#pragma once
// Squelchmoth - Auto-Save, Auto Backup and session recovery.
//
// Pro Tools' Auto Backup (Preferences > Operation) writes numbered copies of the session file into a
// "Session File Backups" folder next to the session, keeps the N most recent, and only when something changed.
// Squelchmoth does the same and fixes the complaints people have about it:
//  - Auto-Save of the session itself (optional), written atomically: a temporary file is written, re-read and
//    checked, then renamed over the session - a crash mid-write can never leave a half-written session.
//  - Nothing is ever written while the transport is playing or recording: the save waits for Stop, so it can
//    never cause a glitch or a dropout.
//  - A second copy of every backup on the system drive, so a failing session drive doesn't take the backups too.
//  - File > Restore from Backup... lists every copy with its date, size and track count and opens the chosen one
//    as a new session file (the original is never overwritten).
//  - After a crash, the next launch offers the newest backup if it is newer than the saved session.

#include "Session.h"

class AutoSaver : private juce::Timer,
                  private juce::ValueTree::Listener,
                  private juce::ChangeListener
{
public:
    explicit AutoSaver (Session& s) : session (s)
    {
        instance() = this;
        session.addChangeListener (this);
        attach();
        startTimer (5000);
    }

    ~AutoSaver() override
    {
        stopTimer();
        session.removeChangeListener (this);
        if (watched.isValid()) watched.removeListener (this);
        if (instance() == this) instance() = nullptr;
    }

    static AutoSaver*& instance()   { static AutoSaver* p = nullptr; return p; }

    //==========================================================================
    struct Backup { juce::File file; juce::Time time; juce::int64 bytes = 0; int tracks = 0; bool mirror = false; };

    static juce::File backupFolderFor (const juce::File& sessionFile)  { return sessionFile.getParentDirectory().getChildFile ("Session File Backups"); }

    static juce::File mirrorFolderFor (const juce::File& sessionFile)
    {
        auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   #if JUCE_MAC
                    .getChildFile ("Application Support")
                   #endif
                    .getChildFile ("Squelchmoth").getChildFile ("Session Backups");
        auto key = juce::String::toHexString (sessionFile.getFullPathName().hashCode64()).substring (0, 8);
        return root.getChildFile (juce::File::createLegalFileName (sessionFile.getFileNameWithoutExtension()) + " - " + key);
    }

    /** Every backup of a session (session folder and system-drive copy), newest first. */
    static std::vector<Backup> listBackups (const juce::File& sessionFile)
    {
        std::vector<Backup> out;
        auto base = sessionFile.getFileNameWithoutExtension();
        for (auto folder : { backupFolderFor (sessionFile), mirrorFolderFor (sessionFile) })
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.squelch"))
                if (f.getFileName().startsWith (base + ".bak."))
                {
                    // the system-drive copy is only listed when the session-folder copy is gone (drive lost, folder deleted)
                    if (folder == mirrorFolderFor (sessionFile) && std::any_of (out.begin(), out.end(), [&] (auto& o) { return o.file.getFileName() == f.getFileName(); }))
                        continue;
                    Backup b { f, f.getLastModificationTime(), f.getSize(), 0, folder == mirrorFolderFor (sessionFile) };
                    if (auto xml = juce::parseXML (f)) b.tracks = xml->getNumChildElements() > 0 ? countTracks (*xml) : 0;
                    out.push_back (b);
                }
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.time > b.time; });
        return out;
    }

    //==========================================================================
    /** Atomic save of the open session: temp file, verify it parses as a Squelchmoth session, then rename. */
    static bool saveAtomically (Session& s, juce::String* error = nullptr)
    {
        if (s.edit == nullptr || ! s.savingAllowed()) { if (error) *error = "saving is not allowed (trial expired or no session)"; return false; }
        auto f = s.getFile();
        if (f == juce::File()) { if (error) *error = "the session has no file"; return false; }
        auto tmp = f.getSiblingFile ("." + f.getFileName() + ".saving");
        tmp.deleteFile();
        if (! te::EditFileOperations (*s.edit).writeToFile (tmp, false)) { if (error) *error = "could not write " + tmp.getFullPathName(); tmp.deleteFile(); return false; }
        if (! isValidSessionFile (tmp)) { if (error) *error = "the written file failed verification"; tmp.deleteFile(); return false; }
        if (! tmp.moveFileTo (f)) { if (error) *error = "could not replace " + f.getFullPathName(); tmp.deleteFile(); return false; }
        s.edit->resetChangedStatus();
        return true;
    }

    static bool isValidSessionFile (const juce::File& f)
    {
        if (f.getSize() < 64) return false;
        auto xml = juce::parseXML (f);
        return xml != nullptr && xml->hasTagName ("EDIT");
    }

    /** Writes a numbered backup now (and the system-drive copy), then trims to the "keep N" setting. */
    bool backupNow (juce::String* error = nullptr)
    {
        if (session.edit == nullptr) return false;
        auto f = session.getFile();
        if (f == juce::File()) return false;
        auto folder = backupFolderFor (f);
        if (! folder.createDirectory()) { if (error) *error = "could not create " + folder.getFullPathName(); return false; }
        const int n = nextNumber (f);
        auto name = f.getFileNameWithoutExtension() + ".bak." + juce::String (n).paddedLeft ('0', 3) + ".squelch";
        auto dest = folder.getChildFile (name);
        auto tmp = folder.getChildFile ("." + name + ".saving");
        const bool wasChanged = session.edit->hasChangedSinceSaved();
        const bool written = te::EditFileOperations (*session.edit).writeToFile (tmp, false);
        if (! wasChanged) session.edit->resetChangedStatus();   // writing a backup copy must not make the session look unsaved
        if (! written || ! isValidSessionFile (tmp) || ! tmp.moveFileTo (dest))
        {
            tmp.deleteFile();
            if (error) *error = "could not write the backup";
            return false;
        }
        if (Prefs::autoBackupMirror())
        {
            auto mirror = mirrorFolderFor (f);
            if (mirror.createDirectory()) dest.copyFileTo (mirror.getChildFile (name));
        }
        trim (f);
        lastBackupMs = juce::Time::currentTimeMillis();
        backupDirty = false;
        lastBackupFile = dest;
        session.log.add ("Squelchmoth", "Backup saved: " + name);
        return true;
    }

    /** Copies a backup next to the session as "<name> (restored <date>).squelch" and opens it. */
    bool restore (const juce::File& backup, juce::String* error = nullptr)
    {
        if (! isValidSessionFile (backup)) { if (error) *error = "that backup can't be read"; return false; }
        auto original = session.getFile();
        auto folder = original != juce::File() ? original.getParentDirectory() : session.sessionsDir;
        auto base = original != juce::File() ? original.getFileNameWithoutExtension() : backup.getFileNameWithoutExtension().upToFirstOccurrenceOf (".bak.", false, false);
        auto dest = folder.getNonexistentChildFile (base + " (restored " + backup.getLastModificationTime().formatted ("%Y-%m-%d %H-%M") + ")", ".squelch", false);
        if (! backup.copyFileTo (dest)) { if (error) *error = "could not copy the backup"; return false; }
        if (! session.openSession (dest)) { if (error) *error = "could not open the restored session"; return false; }
        session.log.add ("Squelchmoth", "Restored " + backup.getFileName() + " as " + dest.getFileName());
        return true;
    }

    juce::var status() const
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("auto_save", Prefs::autoSaveSession());
        o->setProperty ("auto_save_minutes", Prefs::autoSaveMinutes());
        o->setProperty ("auto_backup", Prefs::autoBackup());
        o->setProperty ("backup_minutes", Prefs::autoBackupMinutes());
        o->setProperty ("keep", Prefs::autoBackupKeep());
        o->setProperty ("system_drive_copy", Prefs::autoBackupMirror());
        o->setProperty ("unsaved_changes", session.edit != nullptr && session.edit->hasChangedSinceSaved());
        o->setProperty ("waiting_for_stop", pendingUntilStop);
        o->setProperty ("last_auto_save", lastSaveMs > 0 ? juce::Time (lastSaveMs).toString (true, true) : juce::String ("not yet"));
        o->setProperty ("last_backup", lastBackupMs > 0 ? juce::Time (lastBackupMs).toString (true, true) : juce::String ("not yet"));
        auto f = session.getFile();
        o->setProperty ("backup_folder", f != juce::File() ? backupFolderFor (f).getFullPathName() : juce::String());
        o->setProperty ("system_drive_folder", f != juce::File() ? mirrorFolderFor (f).getFullPathName() : juce::String());
        return juce::var (o);
    }

    /** Called at launch after a crash: if the newest backup is newer than the saved session, offer it. */
    static void offerRecovery (Session& s)
    {
        auto f = s.getFile();
        if (f == juce::File()) return;
        auto list = listBackups (f);
        if (list.empty() || list.front().time <= f.getLastModificationTime() + juce::RelativeTime::seconds (5)) return;
        auto newest = list.front().file;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Recover your work?",
            "Squelchmoth closed unexpectedly. A backup of \"" + f.getFileNameWithoutExtension() + "\" from "
            + list.front().time.toString (true, true) + " is newer than the saved session.\n\nOpen the backup as a new session file? "
            "(Your original session is not changed.)", "Open Backup", "Keep Current", nullptr,
            juce::ModalCallbackFunction::create ([&s, newest] (int r) { if (r == 1 && instance() != nullptr) instance()->restore (newest); }));
    }

private:
    Session& session;
    juce::ValueTree watched;
    bool backupDirty = false, pendingUntilStop = false;
    juce::int64 lastSaveMs = 0, lastBackupMs = 0;
    juce::File lastBackupFile;

    void attach()
    {
        if (watched.isValid()) watched.removeListener (this);
        watched = session.edit != nullptr ? session.edit->state : juce::ValueTree();
        if (watched.isValid()) watched.addListener (this);
        backupDirty = false;
        lastBackupMs = lastSaveMs = juce::Time::currentTimeMillis();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (! watched.isValid() || session.edit == nullptr || watched != session.edit->state) attach();
    }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { backupDirty = true; }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { backupDirty = true; }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { backupDirty = true; }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { backupDirty = true; }

    void timerCallback() override
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        const auto now = juce::Time::currentTimeMillis();
        const bool saveDue = Prefs::autoSaveSession() && session.edit->hasChangedSinceSaved() && now - lastSaveMs >= Prefs::autoSaveMinutes() * 60000LL;
        const bool backupDue = Prefs::autoBackup() && backupDirty && now - lastBackupMs >= Prefs::autoBackupMinutes() * 60000LL;
        if (! saveDue && ! backupDue) { pendingUntilStop = false; return; }

        // never touch the disk while audio is running - wait for Stop
        if (tr.isPlaying() || tr.isRecording() || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) { pendingUntilStop = true; return; }
        pendingUntilStop = false;

        if (saveDue)
        {
            juce::String err;
            if (saveAtomically (session, &err)) lastSaveMs = now;
            else { lastSaveMs = now; session.log.add ("Squelchmoth", "Auto-Save skipped: " + err); }
        }
        if (backupDue) backupNow();
    }

    int nextNumber (const juce::File& f) const
    {
        int maxN = 0;
        for (auto& b : listBackups (f))
            maxN = juce::jmax (maxN, b.file.getFileNameWithoutExtension().fromLastOccurrenceOf (".bak.", false, false).getIntValue());
        return maxN + 1;
    }

    void trim (const juce::File& f)
    {
        const int keep = Prefs::autoBackupKeep();
        for (auto folder : { backupFolderFor (f), mirrorFolderFor (f) })
        {
            juce::Array<juce::File> mine;
            for (auto& b : folder.findChildFiles (juce::File::findFiles, false, "*.squelch"))
                if (b.getFileName().startsWith (f.getFileNameWithoutExtension() + ".bak.")) mine.add (b);
            std::sort (mine.begin(), mine.end(), [] (auto& a, auto& b) { return a.getLastModificationTime() > b.getLastModificationTime(); });
            for (int i = keep; i < mine.size(); ++i) mine[i].deleteFile();
        }
    }

    static int countTracks (const juce::XmlElement& x)
    {
        int n = 0;
        for (auto* c : x.getChildIterator()) if (c->hasTagName ("TRACK") || c->hasTagName ("FOLDERTRACK")) ++n;
        return n;
    }
};

//==============================================================================
/** File > Restore from Backup... */
class BackupBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit BackupBrowser (Session& s) : session (s)
    {
        if (s.getFile() != juce::File()) items = AutoSaver::listBackups (s.getFile());
        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (26);
        list.setColour (juce::ListBox::backgroundColourId, Theme::bg());
        addAndMakeVisible (openBtn);
        addAndMakeVisible (backupBtn);
        addAndMakeVisible (folderBtn);
        openBtn.onClick = [this] { openSelected(); };
        backupBtn.onClick = [this] { if (auto a = AutoSaver::instance()) { a->backupNow(); items = AutoSaver::listBackups (session.getFile()); list.updateContent(); repaint(); } };
        folderBtn.onClick = [this] { if (session.getFile() != juce::File()) AutoSaver::backupFolderFor (session.getFile()).revealToUser(); };
        info.setFont (Theme::font (12.0f));
        info.setColour (juce::Label::textColourId, Theme::dim());
        info.setText (items.empty() ? "No backups of this session yet. Backups are written every few minutes when something changed "
                                      "(Setup > Preferences > Operation > Auto Backup), or now with Back Up Now."
                                    : "Pick a backup and click Open. It opens as a new session file - your original session is not changed.",
                      juce::dontSendNotification);
        info.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (info);
        setSize (640, 440);
    }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::panel()); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        info.setBounds (r.removeFromTop (40));
        auto b = r.removeFromBottom (32);
        openBtn.setBounds (b.removeFromRight (120)); b.removeFromRight (8);
        backupBtn.setBounds (b.removeFromRight (130)); b.removeFromRight (8);
        folderBtn.setBounds (b.removeFromLeft (170));
        r.removeFromBottom (8);
        list.setBounds (r);
    }

    std::function<void()> onDone;

private:
    Session& session;
    std::vector<AutoSaver::Backup> items;
    juce::ListBox list;
    juce::Label info;
    juce::TextButton openBtn { "Open" }, backupBtn { "Back Up Now" }, folderBtn { "Show Backups Folder" };

    int getNumRows() override   { return (int) items.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) items.size()) return;
        auto& b = items[(size_t) row];
        if (selected) g.fillAll (Theme::violet().withAlpha (0.4f));
        g.setColour (Theme::text());
        g.setFont (Theme::font (12.5f, true));
        g.drawText (b.time.toString (true, true, true, true), 10, 0, 230, h, juce::Justification::centredLeft);
        g.setFont (Theme::font (11.5f));
        g.setColour (Theme::dim());
        g.drawText (b.file.getFileName(), 240, 0, w - 420, h, juce::Justification::centredLeft);
        g.drawText (juce::String (b.tracks) + " tracks  " + juce::File::descriptionOfSizeInBytes (b.bytes) + (b.mirror ? "  (system drive)" : ""),
                    w - 220, 0, 210, h, juce::Justification::centredRight);
    }

    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override   { openSelected(); }

    void openSelected()
    {
        auto row = list.getSelectedRow();
        if (row < 0 || row >= (int) items.size() || AutoSaver::instance() == nullptr) return;
        juce::String err;
        if (! AutoSaver::instance()->restore (items[(size_t) row].file, &err))
            Ui::message (juce::MessageBoxIconType::WarningIcon, "Restore from Backup", err);
        else if (onDone) onDone();
    }
};

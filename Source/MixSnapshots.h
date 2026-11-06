#pragma once
// Squelchmoth - Mix Snapshots A-H: store and recall whole-mix states (every fader, pan, mute, solo, polarity, send level,
// insert bypass and every plug-in parameter, master included), compare two mixes instantly, and undo a recall.
// Saved inside the session. Pro Tools has no built-in mix snapshots; Studio One has Mix Scenes - this does both jobs.

#include "Session.h"
#include "TrackOps.h"

namespace MixSnap
{
    inline juce::Identifier rootID()    { return "SQM_SNAPSHOTS"; }
    constexpr int numSlots = 8;
    constexpr int undoSlot = 8;          // hidden: the mix as it was before the last recall
    inline juce::String slotName (int i)  { return i == undoSlot ? juce::String ("Before recall") : juce::String::charToString ((juce::juce_wchar) ('A' + i)); }

    inline juce::ValueTree root (te::Edit& e)       { return e.state.getOrCreateChildWithName (rootID(), nullptr); }
    inline juce::ValueTree slot (te::Edit& e, int i)
    {
        auto r = root (e);
        for (auto c : r) if ((int) c.getProperty ("slot", -1) == i) return c;
        return {};
    }

    /** Every channel strip whose state is captured: tracks (any kind) and the master. */
    inline std::vector<std::pair<juce::String, te::PluginList*>> strips (Session& s, std::vector<te::Track*>* tracksOut = nullptr)
    {
        std::vector<std::pair<juce::String, te::PluginList*>> out;
        for (auto t : s.getRows())
        {
            out.push_back ({ t->itemID.toString(), &t->pluginList });
            if (tracksOut) tracksOut->push_back (t);
        }
        out.push_back ({ "master", &s.edit->getMasterPluginList() });
        if (tracksOut) tracksOut->push_back (nullptr);
        return out;
    }

    inline juce::ValueTree capture (Session& s)
    {
        juce::ValueTree snap ("SNAPSHOT");
        std::vector<te::Track*> tracks;
        auto list = strips (s, &tracks);
        for (size_t i = 0; i < list.size(); ++i)
        {
            juce::ValueTree st ("STRIP");
            st.setProperty ("id", list[i].first, nullptr);
            if (auto t = tracks[i])
            {
                st.setProperty ("name", t->getName(), nullptr);
                st.setProperty ("mute", t->isMuted (false), nullptr);
                st.setProperty ("solo", t->isSolo (false), nullptr);
                st.setProperty ("polarity", TrackOps::isPolarityInverted (*t), nullptr);
            }
            for (auto p : list[i].second->getPlugins())
            {
                if (dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr) continue;
                juce::ValueTree pt ("PLUGIN");
                pt.setProperty ("id", p->itemID.toString(), nullptr);
                pt.setProperty ("enabled", p->isEnabled(), nullptr);
                for (auto ap : p->getAutomatableParameters())
                {
                    juce::ValueTree v ("P");
                    v.setProperty ("id", ap->paramID, nullptr);
                    v.setProperty ("v", ap->getCurrentValue(), nullptr);
                    pt.appendChild (v, nullptr);
                }
                st.appendChild (pt, nullptr);
            }
            snap.appendChild (st, nullptr);
        }
        return snap;
    }

    /** Glides parameters to a recalled snapshot over a few seconds (an equal-time S-curve), for mix-scene transitions. */
    class Glider : private juce::Timer
    {
    public:
        static Glider& get()    { static Glider g; return g; }
        void clear()            { moves.clear(); stopTimer(); }
        void add (te::AutomatableParameter* p, float from, float to)   { moves.push_back ({ p, from, to }); }
        void start (int ms)     { durationMs = juce::jmax (1, ms); startMs = juce::Time::getMillisecondCounterHiRes(); if (! moves.empty()) startTimerHz (60); }
        bool isGliding() const  { return isTimerRunning(); }
    private:
        struct Move { te::AutomatableParameter::Ptr p; float from, to; };
        std::vector<Move> moves;
        double startMs = 0; int durationMs = 1000;
        void timerCallback() override
        {
            const double x = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - startMs) / durationMs);
            const float k = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * x));
            for (auto& m : moves) if (m.p != nullptr) m.p->setParameter (m.from + (m.to - m.from) * k, juce::sendNotification);
            if (x >= 1.0) clear();
        }
    };

    /** Applies a captured mix. Returns how many controls changed. Plug-ins added since the snapshot are left alone.
        glideMs > 0 moves continuous controls there smoothly; switches (mute, solo, polarity, bypass) change at once. */
    inline int apply (Session& s, const juce::ValueTree& snap, int glideMs = 0)
    {
        Glider::get().clear();
        int changed = 0;
        std::vector<te::Track*> tracks;
        auto list = strips (s, &tracks);
        for (size_t i = 0; i < list.size(); ++i)
        {
            auto st = snap.getChildWithProperty ("id", list[i].first);
            if (! st.isValid()) continue;
            if (auto t = tracks[i])
            {
                if (st.hasProperty ("mute") && (bool) st["mute"] != t->isMuted (false))  { s.setTrackMute (*t, st["mute"], false); ++changed; }
                if (st.hasProperty ("solo") && (bool) st["solo"] != t->isSolo (false))    { s.setTrackSolo (*t, st["solo"], false); ++changed; }
                if (st.hasProperty ("polarity") && (bool) st["polarity"] != TrackOps::isPolarityInverted (*t)) { TrackOps::setPolarityInverted (*t, st["polarity"]); ++changed; }
            }
            for (auto p : list[i].second->getPlugins())
            {
                auto pt = st.getChildWithProperty ("id", p->itemID.toString());
                if (! pt.isValid()) continue;
                if ((bool) pt["enabled"] != p->isEnabled()) { p->setEnabled (pt["enabled"]); ++changed; }
                for (auto ap : p->getAutomatableParameters())
                {
                    auto v = pt.getChildWithProperty ("id", ap->paramID);
                    if (! v.isValid()) continue;
                    const float val = (float) v["v"];
                    if (std::abs (val - ap->getCurrentValue()) > 1.0e-6f)
                    {
                        if (glideMs > 0 && ! ap->isDiscrete()) Glider::get().add (ap, ap->getCurrentValue(), val);
                        else ap->setParameter (val, juce::sendNotification);
                        ++changed;
                    }
                }
            }
        }
        if (glideMs > 0) Glider::get().start (glideMs);
        return changed;
    }

    inline void store (Session& s, int i, const juce::String& name = {})
    {
        auto& e = *s.edit;
        auto r = root (e);
        auto old = slot (e, i);
        juce::String keepName = old.isValid() ? old["name"].toString() : juce::String();
        if (old.isValid()) r.removeChild (old, nullptr);
        auto snap = capture (s);
        snap.setProperty ("slot", i, nullptr);
        snap.setProperty ("name", name.isNotEmpty() ? name : (keepName.isNotEmpty() ? keepName : "Mix " + slotName (i)), nullptr);
        snap.setProperty ("time", juce::Time::getCurrentTime().toISO8601 (true), nullptr);
        r.appendChild (snap, nullptr);
        e.markAsChanged();
    }

    /** Recalls slot i (the current mix is kept as "Before recall" so it can be undone). */
    inline int recall (Session& s, int i, int glideMs = 0)
    {
        auto& e = *s.edit;
        auto snap = slot (e, i);
        if (! snap.isValid()) return -1;
        if (i != undoSlot) store (s, undoSlot);
        const int n = apply (s, snap, glideMs);
        root (e).setProperty ("lastRecalled", i, nullptr);
        if (i != undoSlot) root (e).setProperty ("previousRecalled", (int) root (e).getProperty ("currentRecalled", -1), nullptr);
        root (e).setProperty ("currentRecalled", i, nullptr);
        return n;
    }

    inline void clear (Session& s, int i)    { auto sn = slot (*s.edit, i); if (sn.isValid()) { root (*s.edit).removeChild (sn, nullptr); s.edit->markAsChanged(); } }
    inline void rename (Session& s, int i, const juce::String& n) { auto sn = slot (*s.edit, i); if (sn.isValid()) { sn.setProperty ("name", n, nullptr); s.edit->markAsChanged(); } }

    /** Compare: flips between the last two snapshots recalled. */
    inline int compare (Session& s)
    {
        auto r = root (*s.edit);
        const int prev = (int) r.getProperty ("previousRecalled", -1);
        if (prev < 0 || ! slot (*s.edit, prev).isValid()) return -1;
        recall (s, prev);
        return prev;
    }

    inline juce::var describe (Session& s)
    {
        juce::Array<juce::var> out;
        auto r = root (*s.edit);
        for (int i = 0; i < numSlots; ++i)
        {
            auto sn = slot (*s.edit, i);
            auto o = new juce::DynamicObject();
            o->setProperty ("slot", slotName (i));
            o->setProperty ("stored", sn.isValid());
            if (sn.isValid())
            {
                o->setProperty ("name", sn["name"]);
                o->setProperty ("saved", juce::Time::fromISO8601 (sn["time"].toString()).toString (true, true));
                o->setProperty ("strips", sn.getNumChildren());
            }
            o->setProperty ("current", (int) r.getProperty ("currentRecalled", -1) == i);
            out.add (juce::var (o));
        }
        return out;
    }

    inline int slotFromText (const juce::String& t)
    {
        auto c = t.trim().toUpperCase();
        if (c.length() == 1 && c[0] >= 'A' && c[0] < 'A' + numSlots) return c[0] - 'A';
        if (c.containsOnly ("0123456789") && c.getIntValue() >= 1 && c.getIntValue() <= numSlots) return c.getIntValue() - 1;
        return -1;
    }
}

//==============================================================================
/** Window > Mix Snapshots: eight big recall buttons, Store / Rename / Clear per slot, Compare and Undo Recall. */
class MixSnapshotsContent : public juce::Component, private juce::Timer
{
public:
    explicit MixSnapshotsContent (Session& s) : session (s)
    {
        for (int i = 0; i < MixSnap::numSlots; ++i)
        {
            auto& r = rows[(size_t) i];
            r.recall.setButtonText (MixSnap::slotName (i));
            r.recall.setTooltip ("Recall this mix (click). The mix before the recall is kept - Undo Recall brings it back.");
            r.recall.onClick = [this, i] { doRecall (i); };
            r.store.onClick = [this, i] { MixSnap::store (session, i); session.log.add ("You", "Stored mix snapshot " + MixSnap::slotName (i)); refreshRows(); };
            r.name.setEditable (false, true);
            r.name.onTextChange = [this, i] { MixSnap::rename (session, i, rows[(size_t) i].name.getText()); };
            r.name.setTooltip ("Double-click to rename");
            r.clear.onClick = [this, i] { MixSnap::clear (session, i); refreshRows(); };
            for (juce::Component* c : { (juce::Component*) &r.recall, (juce::Component*) &r.store, (juce::Component*) &r.name, (juce::Component*) &r.info, (juce::Component*) &r.clear })
                addAndMakeVisible (c);
            r.name.setFont (Theme::font (13.0f, true));
            r.info.setFont (Theme::font (10.5f));
            r.info.setColour (juce::Label::textColourId, Theme::dim());
            r.name.setColour (juce::Label::textColourId, Theme::text());
        }
        for (auto* b : { &compareBtn, &undoBtn }) addAndMakeVisible (b);
        addAndMakeVisible (glide);
        glide.addItemList ({ "Instant recall", "Glide 0.5 s", "Glide 1 s", "Glide 2 s", "Glide 4 s", "Glide 8 s" }, 1);
        glide.setSelectedId (juce::jlimit (1, 6, Prefs::getInt ("mx_snapGlide", 1)), juce::dontSendNotification);
        glide.setTooltip ("Glide: faders, pans, sends and plug-in settings move smoothly to the recalled mix (great for transitions and A/B)");
        glide.onChange = [this] { Prefs::set ("mx_snapGlide", glide.getSelectedId()); };
        compareBtn.setTooltip ("Flip between the last two snapshots you recalled - hear mix A against mix B");
        compareBtn.onClick = [this] { auto p = MixSnap::compare (session); if (p >= 0) session.log.add ("You", "Compare: now hearing " + MixSnap::slotName (p)); refreshRows(); };
        undoBtn.onClick = [this] { if (MixSnap::recall (session, MixSnap::undoSlot) >= 0) session.log.add ("You", "Undo recall"); refreshRows(); };
        addAndMakeVisible (help);
        help.setFont (Theme::font (11.0f));
        help.setColour (juce::Label::textColourId, Theme::dim());
        help.setText ("A snapshot holds every fader, pan, mute, solo, polarity, send level, insert bypass and plug-in setting (master too). "
                      "Automated controls follow their automation while playing.", juce::dontSendNotification);
        setSize (520, 70 + MixSnap::numSlots * 40 + 46);
        refreshRows();
        startTimerHz (2);
    }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::panel()); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        help.setBounds (r.removeFromTop (36));
        r.removeFromTop (6);
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (34);
            r.removeFromTop (6);
            row.recall.setBounds (line.removeFromLeft (52));
            line.removeFromLeft (8);
            row.clear.setBounds (line.removeFromRight (54));
            line.removeFromRight (6);
            row.store.setBounds (line.removeFromRight (64));
            line.removeFromRight (8);
            row.name.setBounds (line.removeFromTop (18));
            row.info.setBounds (line);
        }
        r.removeFromTop (4);
        auto b = r.removeFromTop (28);
        compareBtn.setBounds (b.removeFromLeft (200));
        b.removeFromLeft (8);
        undoBtn.setBounds (b.removeFromLeft (140));
        b.removeFromLeft (8);
        glide.setBounds (b);
    }

private:
    struct Row { juce::TextButton recall, store { "Store" }, clear { "Clear" }; juce::Label name, info; };
    Session& session;
    std::array<Row, MixSnap::numSlots> rows;
    juce::TextButton compareBtn { "Compare (A/B last two)" }, undoBtn { "Undo Recall" };
    juce::ComboBox glide;
    int glideMs() const   { static const int ms[] = { 0, 500, 1000, 2000, 4000, 8000 }; return ms[juce::jlimit (0, 5, glide.getSelectedId() - 1)]; }
    juce::Label help;
    juce::String lastSig;

    void doRecall (int i)
    {
        if (session.edit == nullptr) return;
        auto n = MixSnap::recall (session, i, glideMs());
        if (n < 0) { MixSnap::store (session, i); session.log.add ("You", "Snapshot " + MixSnap::slotName (i) + " was empty - stored the current mix in it"); }
        else session.log.add ("You", "Recalled mix snapshot " + MixSnap::slotName (i) + " (" + juce::String (n) + " controls changed)");
        refreshRows();
    }

    /** Changes whenever a slot is stored, renamed, cleared or recalled - from here or from Claude. */
    juce::String signature()
    {
        auto r = MixSnap::root (*session.edit);
        juce::String sig (r.getProperty ("currentRecalled", -1).toString() + "/" + r.getProperty ("previousRecalled", -1).toString());
        for (auto c : r) sig << "|" << c["slot"].toString() << c["time"].toString() << c["name"].toString();
        return sig;
    }

    void timerCallback() override
    {
        if (session.edit == nullptr) return;
        if (signature() != lastSig) refreshRows();
    }

    void refreshRows()
    {
        if (session.edit == nullptr) return;
        auto& e = *session.edit;
        const int cur = (int) MixSnap::root (e).getProperty ("currentRecalled", -1);
        lastSig = signature();
        for (int i = 0; i < MixSnap::numSlots; ++i)
        {
            auto& r = rows[(size_t) i];
            auto sn = MixSnap::slot (e, i);
            const bool has = sn.isValid();
            if (! r.name.isBeingEdited()) r.name.setText (has ? sn["name"].toString() : "(empty - click the letter or Store to save the mix)", juce::dontSendNotification);
            r.name.setEditable (false, has);
            r.info.setText (has ? "Saved " + juce::Time::fromISO8601 (sn["time"].toString()).toString (true, true) + "   " + juce::String (sn.getNumChildren()) + " channel strips" : juce::String(),
                            juce::dontSendNotification);
            r.clear.setEnabled (has);
            r.recall.setColour (juce::TextButton::buttonColourId, cur == i ? Theme::acid().darker (0.3f) : (has ? Theme::violet().darker (0.35f) : Theme::panel2()));
        }
        undoBtn.setEnabled (MixSnap::slot (e, MixSnap::undoSlot).isValid());
        compareBtn.setEnabled ((int) MixSnap::root (e).getProperty ("previousRecalled", -1) >= 0);
    }
};

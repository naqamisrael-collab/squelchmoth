#pragma once
// Squelchmoth - the one "new track" menu used everywhere (Track > Quick New, right-click on + TRACK, right-click on a
// track name, right-click on empty space in the Edit and Mix windows). Every track type that has a format offers
// Mono and Stereo, exactly like the Pro Tools New Tracks dialog; "New Tracks..." opens the full dialog.

#include "TrackOps.h"
#include "PluginMenu.h"
#include "DrumKit.h"

namespace QuickNew
{
    enum Ids
    {
        dialog = 1,
        audioMono = 10, audioStereo, auxMono, auxStereo, instStereo, instMono, samplerStereo, midi,
        masterFader, vca, basicFolder, routingFolder, drum, audioMono4, audioStereo4, bass808, blossom
    };

    /** base: offset added to the small ids so the menu can live inside a bigger menu (plug-in ids are unchanged). */
    inline juce::PopupMenu build (Session& s, int base = 0, bool withPluginSubmenu = true)
    {
        juce::PopupMenu m;
        m.addItem (base + dialog, "New Tracks...  (" SQ_CMD "+Shift+N)");
        m.addSeparator();
        m.addSectionHeader ("Audio Track");
        m.addItem (base + audioMono, "Mono Audio Track");
        m.addItem (base + audioStereo, "Stereo Audio Track");
        m.addItem (base + audioMono4, "4 Mono Audio Tracks");
        m.addItem (base + audioStereo4, "4 Stereo Audio Tracks");
        m.addSectionHeader ("Aux Input");
        m.addItem (base + auxStereo, "Stereo Aux Input");
        m.addItem (base + auxMono, "Mono Aux Input");
        m.addSectionHeader ("Instrument / MIDI");
        m.addItem (base + instStereo, "Stereo Instrument Track (Mothsynth)");
        m.addItem (base + instMono, "Mono Instrument Track (Mothsynth)");
        m.addItem (base + samplerStereo, "Stereo Instrument Track (Chrysalis Sampler)");
        if (withPluginSubmenu) m.addSubMenu ("Stereo Instrument Track with...", PluginMenu::build (s.getEngine(), PluginMenu::instruments));
        m.addItem (base + midi, "MIDI Track");
        m.addItem (base + drum, "Drum Track (Beat Maker + kit)");
        m.addItem (base + bass808, "808 Track (Moth 808 - glides)");
        m.addItem (base + blossom, "Strings Track (Blossom physical model)");
        m.addSectionHeader ("Mix");
        m.addItem (base + masterFader, "Master Fader", ! TrackOps::hasMasterFader (s));
        m.addItem (base + vca, "VCA Master");
        m.addItem (base + routingFolder, "Routing Folder (stereo)");
        m.addItem (base + basicFolder, "Basic Folder");
        return m;
    }

    /** Runs a result from build(). Returns the tracks it made. */
    inline bool owns (int r, int base = 0)   { return (r - base >= dialog && r - base <= blossom) || r >= PluginMenu::signatureBase; }

    inline juce::Array<te::Track*> handle (Session& s, int r, te::Track* after,
                                           std::function<void()> openDialog,
                                           std::function<void (te::StepClip&)> openStepClip = {}, int base = 0)
    {
        juce::Array<te::Track*> made;
        if (r <= 0 || s.edit == nullptr) return made;
        if (PluginMenu::isSearch (r))
        {
            te::SafeSelectable<te::Track> safeAfter (after);
            PluginMenu::showSearch (s.getEngine(), PluginMenu::instruments, true, [&s, safeAfter, openDialog, openStepClip] (int id)
            {
                if (s.quickNewHook) s.quickNewHook (id, 0);
                else handle (s, id, safeAfter.get(), openDialog, openStepClip, 0);
            });
            return made;
        }
        if (r < PluginMenu::signatureBase) r -= base;
        if (r == dialog) { if (openDialog) openDialog(); return made; }

        TrackOps::NewSpec sp;
        bool useSpec = true;
        switch (r)
        {
            case audioMono:     sp.type = TrackOps::NewType::audio; break;
            case audioStereo:   sp.type = TrackOps::NewType::audio; sp.stereo = true; break;
            case audioMono4:    sp.type = TrackOps::NewType::audio; sp.count = 4; break;
            case audioStereo4:  sp.type = TrackOps::NewType::audio; sp.stereo = true; sp.count = 4; break;
            case auxMono:       sp.type = TrackOps::NewType::aux; break;
            case auxStereo:     sp.type = TrackOps::NewType::aux; sp.stereo = true; break;
            case instStereo:    sp.type = TrackOps::NewType::instrument; sp.stereo = true; sp.ticks = true; break;
            case instMono:      sp.type = TrackOps::NewType::instrument; sp.ticks = true; break;
            case samplerStereo: sp.type = TrackOps::NewType::instrument; sp.stereo = true; sp.ticks = true; sp.instrument = "sampler"; break;
            case midi:          sp.type = TrackOps::NewType::midi; sp.ticks = true; break;
            case masterFader:   sp.type = TrackOps::NewType::masterFader; sp.stereo = true; break;
            case vca:           sp.type = TrackOps::NewType::vca; break;
            case routingFolder: sp.type = TrackOps::NewType::routingFolder; sp.stereo = true; break;
            case basicFolder:   sp.type = TrackOps::NewType::basicFolder; break;
            case bass808:       sp.type = TrackOps::NewType::instrument; sp.stereo = true; sp.ticks = true; sp.instrument = Sq808Plugin::xmlTypeName; sp.name = "808"; break;
            case blossom:       sp.type = TrackOps::NewType::instrument; sp.stereo = true; sp.ticks = true; sp.instrument = SqBlossomPlugin::xmlTypeName; sp.name = "Strings"; break;
            default:            useSpec = false; break;
        }

        if (useSpec)
        {
            juce::String note;
            made = TrackOps::createTracks (s, sp, after, note);
            if (note.isNotEmpty()) Ui::message (juce::MessageBoxIconType::InfoIcon, "New Track", note);
        }
        else if (r == drum)
        {
            if (auto t = DrumKit::createDrumTrack (s, {}, 4))
            {
                made.add (t);
                if (auto sc = DrumKit::findStepClip (*t)) if (openStepClip) openStepClip (*sc);
            }
        }
        else if (auto p = PluginMenu::createFromResult (s, r))
        {
            // any instrument from the plug-in menu: a stereo Instrument track with that plug-in in insert A
            sp.type = TrackOps::NewType::instrument; sp.stereo = true; sp.ticks = true; sp.instrument = {};
            sp.name = p->getName();
            juce::String note;
            made = TrackOps::createTracks (s, sp, after, note);
            if (auto at = made.isEmpty() ? nullptr : dynamic_cast<te::AudioTrack*> (made.getFirst()))
                at->pluginList.insertPlugin (p, 0, nullptr);
        }
        if (! made.isEmpty()) s.log.add ("You", "New track: " + made.getFirst()->getName() + (made.size() > 1 ? " (+" + juce::String (made.size() - 1) + ")" : juce::String()));
        return made;
    }
}

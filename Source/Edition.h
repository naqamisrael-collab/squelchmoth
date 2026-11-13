#pragma once
// Squelchmoth editions (1.9 "Store"). One place decides what the running copy may use:
//   Demo (no key)  - free forever: 16 tracks, the basic plug-ins and instruments, kits 1-5, save and bounce mix
//   Squelchmoth    - the core program: unlimited tracks, every core plug-in and instrument, kits 1-17, Mastering,
//                    Cut Lab, stems / digital release / DDP, Molt
//   Packs          - Vocal Suite, Mastering Suite, Instrument Collection, Beat Lab Expansion, Creative FX
//   Complete       - all of the above (owner keys count as Complete)
// The engine asks isTypeAllowed() for every built-in plug-in it creates (the PluginManager gate), so menus, sessions,
// presets, the rack and Claude all meet the same rule; a locked type loads as a placeholder that keeps its settings.
#include <JuceHeader.h>

namespace Edition
{
    enum class Pack { none, vocal, mastering, instruments, beat, creative, color };

    struct Product { const char* code; const char* name; const char* price; const char* list; const char* blurb; };
    // codes match the SQM2 key product byte: 1 CORE, 2 COMPLETE, 3 VOCAL, 4 MASTER, 5 INSTR, 6 BEAT, 7 CREATIVE, 8 COLOR
    inline const std::vector<Product>& products()
    {
        static const std::vector<Product> p {
            { "CORE",     "Squelchmoth",           "$189.99", "$299.99", "The full program: unlimited tracks, every core plug-in and instrument, Mastering, Cut Lab, stems and DDP" },
            { "COMPLETE", "Squelchmoth Complete",  "$399.99", "$699.99", "Squelchmoth and every add-on pack, Color & Space and the whole Vocal Suite included" },
            { "VOCAL",    "Vocal Suite",           "$149",    "$249",    "The whole Voice family: Moth Tune, Moth Tune FX 2, Moth Key, Siren Comp, Siren EQ, Sibyl De-Ess, Hush De-Ess 2, Siren Verb, Mic Closet, Larynx, Exhale, Hive Choir, Double Wing 2, Double Wing ADT, Mutant, Vox Moth, Talk Wing and Clean Room" },
            { "MASTER",   "Mastering Suite",       "$99",     "$149",    "Spectra EQ, Torque Comp, Summit Limiter, Apex Multiband 2, Portcullis Gate, the Apex series, Pulse EQ and Opto Leveler" },
            { "INSTR",    "Instrument Collection", "$99",     "$149",    "Hexwing, Toxin FM, Lantern, Lantern Exo, Pollen, Blossom 2, Thorn Bass, Larva Bass, Chrysalis 2 and Chrysalis SF" },
            { "BEAT",     "Beat Lab Expansion",    "$49",     "$79",     "Carapace, Live Drum Synth, Drum Pad Model, Thump Live, Mandible, Mandible Lite, ten more kits and 51 genre loops" },
            { "CREATIVE", "Creative FX",           "$49",     "$79",     "Time Weaver, Larva Filter, Oil Can Delay, Bucket Brigade, Halo Ring, Twelve Phase and Silk Ladder" },
            { "COLOR",    "Color & Space",         "$179",    "$299",    "Cinder, Wingbeat Echo, Foil Plate, Radiant, Maw, Vellum EQ, Tremor, Orbit Pan, Mirror Shift, Shutter Delay, Shard, Murmuration, Masquerade, Larva Filter 2, Twelve Phase 2, Hive Rack, Atrium Verb and Kiln" } };
        return p;
    }
    inline const Product* productFor (Pack p)
    {
        switch (p) { case Pack::vocal: return &products()[2]; case Pack::mastering: return &products()[3]; case Pack::instruments: return &products()[4];
                     case Pack::beat: return &products()[5]; case Pack::creative: return &products()[6]; case Pack::color: return &products()[7]; default: return &products()[0]; }
    }

    /** The store. One page per product so a lock can send people straight to the thing they need. */
    inline juce::String storeUrl (const juce::String& productCode = {})
    {
        static const juce::String base = "https://squelchmoth.com/";
        if (productCode.isEmpty() || productCode == "CORE" || productCode == "COMPLETE") return base + "compare.html";
        return base + "addons.html#" + productCode.toLowerCase();
    }

    //==========================================================================
    // What the running copy owns (set by Licence from the stored keys)
    struct Owned { bool core = false; bool vocal = false, mastering = false, instruments = false, beat = false, creative = false, color = false; };
    inline Owned& owned()   { static Owned o; return o; }
    inline bool isDemo()    { return ! owned().core; }
    inline bool hasPack (Pack p)
    {
        auto& o = owned();
        switch (p) { case Pack::vocal: return o.vocal; case Pack::mastering: return o.mastering; case Pack::instruments: return o.instruments;
                     case Pack::beat: return o.beat; case Pack::creative: return o.creative; case Pack::color: return o.color; default: return true; }
    }
    inline juce::String editionName()
    {
        auto& o = owned();
        if (o.core && o.vocal && o.mastering && o.instruments && o.beat && o.creative && o.color) return "Squelchmoth Complete";
        if (! o.core) return "Squelchmoth Demo";
        juce::StringArray packs;
        for (auto p : { Pack::vocal, Pack::mastering, Pack::instruments, Pack::beat, Pack::creative, Pack::color }) if (hasPack (p)) packs.add (productFor (p)->name);
        return packs.isEmpty() ? juce::String ("Squelchmoth") : "Squelchmoth + " + packs.joinIntoString (", ");
    }

    //==========================================================================
    // Plug-in types
    inline Pack packOf (const juce::String& type)
    {
        static const std::map<juce::String, Pack> m {
            { "sqmSibylDeEss", Pack::vocal }, { "sqmTune", Pack::vocal }, { "sqmTuneFx", Pack::vocal }, { "airwindows_adt", Pack::vocal },
            { "sqmMothKey", Pack::vocal }, { "sqmSirenComp", Pack::vocal }, { "sqmSirenEq", Pack::vocal }, { "sqmHushDeEss2", Pack::vocal },
            { "sqmSirenVerb", Pack::vocal }, { "sqmMicCloset", Pack::vocal }, { "sqmLarynx", Pack::vocal }, { "sqmExhale", Pack::vocal },
            { "sqmHiveChoir", Pack::vocal }, { "sqmDoubleWing2", Pack::vocal }, { "sqmMutant", Pack::vocal },
            { "sqmVoxMoth", Pack::vocal }, { "sqmTalkWing", Pack::vocal }, { "sqmCleanRoom", Pack::vocal },
            { "sqmSpectraEq", Pack::mastering }, { "sqmTorqueComp", Pack::mastering }, { "sqmSummitLim", Pack::mastering }, { "sqmApexMB2", Pack::mastering }, { "sqmPortcullis", Pack::mastering }, { "sqmApexDyn", Pack::mastering }, { "sqmApexLim", Pack::mastering }, { "sqmApexMB", Pack::mastering }, { "sqmApexSub", Pack::mastering },
            { "sqmApexTriad", Pack::mastering }, { "sqmPulse", Pack::mastering }, { "sqmOpto", Pack::mastering },
            { "sqmHexwing", Pack::instruments }, { "sqmToxin", Pack::instruments }, { "sqmLantern", Pack::instruments }, { "sqmLanternExo", Pack::instruments },
            { "sqmPollen", Pack::instruments }, { "sqmBlossom", Pack::instruments }, { "sqmThornBass", Pack::instruments }, { "sqmLarvaBass", Pack::instruments },
            { "sqmChrysalis2", Pack::instruments }, { "sqmChrysalisSF", Pack::instruments },
            { "sqmCarapace", Pack::beat }, { "sqmLiveDrumSynth", Pack::beat }, { "sqmDrumPadModel", Pack::beat }, { "sqmThumpLive", Pack::beat },
            { "sqmMandible", Pack::beat }, { "sqmMandibleLite", Pack::beat },
            { "sqmTimeWeaver", Pack::creative }, { "sqmLarvaFilter", Pack::creative }, { "sqmOilCan", Pack::creative }, { "sqmBBD", Pack::creative },
            { "sqmRing", Pack::creative }, { "sqmPhase12", Pack::creative }, { "sqmLadder", Pack::creative },
            { "sqmCinder", Pack::color }, { "sqmWingbeat", Pack::color }, { "sqmFoilPlate", Pack::color }, { "sqmRadiant", Pack::color }, { "sqmMaw", Pack::color },
            { "sqmVellum", Pack::color }, { "sqmTremor", Pack::color }, { "sqmOrbitPan", Pack::color }, { "sqmMirrorShift", Pack::color }, { "sqmShutter", Pack::color },
            { "sqmShard", Pack::color }, { "sqmMurmuration", Pack::color }, { "sqmMasquerade", Pack::color },
            { "sqmLarvaFilter2", Pack::color }, { "sqmPhase12v2", Pack::color }, { "sqmHiveRack", Pack::color }, { "sqmAtriumVerb", Pack::color }, { "sqmKiln", Pack::color } };
        auto it = m.find (type); return it != m.end() ? it->second : Pack::none;
    }
    /** The Demo's basic set: 16 effects, 6 instruments and what the rack needs to run. */
    inline bool isDemoType (const juce::String& type)
    {
        static const juce::StringArray demo { "4bandEq", "airwindows_biquad2", "lowpass", "airwindows_toneslant", "compressor", "airwindows_gatelope", "airwindows_deess",
                                              "airwindows_drive", "reverb", "delay", "chorus", "phaser", "pitchShifter", "airwindows_wider", "airwindows_monoam",
                                              "airwindows_purestgain", "midiModifier", "sqmRackChannel", "4osc", "sqmMiniMoth", "sqmSimMoth", "sqmThump", "sqm808",
                                              "sampler", "sqmHivePads", "sqmWingbeatJr", "sqmFoilPlateLite" };
        return demo.contains (type);
    }
    /** Product types are the Signature bundle and the AirWindows collection; everything else (meters, panners, sends,
        racks, the hidden helpers) is part of the engine and never locked. Filled in by Signature at start-up. */
    inline juce::StringArray& productTypes()   { static juce::StringArray t; return t; }
    inline bool isProductType (const juce::String& type)   { return type.startsWith ("airwindows_") || productTypes().contains (type); }

    inline bool isTypeAllowed (const juce::String& type)
    {
        if (! isProductType (type)) return true;
        const auto pk = packOf (type);
        if (pk != Pack::none) return hasPack (pk);
        return ! isDemo() || isDemoType (type);
    }
    /** "Part of the Vocal Suite" / "Part of Squelchmoth" - who a locked type belongs to. */
    inline const Product* productForType (const juce::String& type)   { return productFor (packOf (type)); }
    inline juce::String lockReason (const juce::String& type)
    {
        auto* p = productForType (type);
        return juce::String ("Part of ") + p->name + (packOf (type) != Pack::none ? " (" + juce::String (p->price) + ")" : juce::String (" - the Demo has the basic set"));
    }

    //==========================================================================
    // Kits, tracks and features
    inline bool isKitAllowed (int kit)
    {
        if (kit >= 17) return hasPack (Pack::beat);
        if (isDemo()) return kit < 5;
        return true;
    }
    inline juce::String kitLockReason (int kit)   { return kit >= 17 ? "Part of the Beat Lab Expansion" : "Part of Squelchmoth - the Demo has the first five kits"; }

    static constexpr int demoMaxTracks = 16;
    inline int maxTracks()   { return isDemo() ? demoMaxTracks : 100000; }

    enum class Feature { mastering, cutLab, stems, digitalRelease, ddp, molt, airwindowsMenu };
    inline bool allows (Feature)   { return ! isDemo(); }
    inline juce::String featureName (Feature f)
    {
        switch (f) { case Feature::mastering: return "The Mastering window"; case Feature::cutLab: return "Cut Lab"; case Feature::stems: return "Stem export";
                     case Feature::digitalRelease: return "Digital release"; case Feature::ddp: return "DDP export"; case Feature::molt: return "Molt (time and pitch)";
                     default: return "The AirWindows collection"; }
    }
    inline juce::String featureLockMessage (Feature f)   { return featureName (f) + " is part of Squelchmoth. The Demo has the basics - get the full program at " + storeUrl ("CORE"); }

    /** UI hook: set by MainComponent to show the unlock window (product code); null in headless runs. */
    inline std::function<void (const juce::String&)>& showUnlock()   { static std::function<void (const juce::String&)> f; return f; }
}

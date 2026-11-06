#pragma once
// Squelchmoth - organises every plug-in the way Pro Tools does.
//
//  * Every plug-in (yours, the Signature bundle, the built-ins) is placed in a Pro Tools category:
//    EQ, Dynamics, Pitch Shift, Reverb, Delay, Modulation, Harmonic, Dither, Sound Field, Noise Reduction,
//    Channel Strip, Instrument, Effect, Other.
//    The category comes from the plug-in's own VST3 sub-category when it gives one, then from a catalogue of
//    well-known products (Waves, FabFilter, Universal Audio, Soundtoys, iZotope, Plugin Alliance, Slate,
//    Valhalla, Native Instruments...), then from words in the name.
//  * Shell plug-ins that publish one entry per channel layout ("H-Reverb Mono", "H-Reverb Stereo",
//    "H-Reverb Mono/Stereo"...) are shown once; the right layout is picked for the track it goes on.
//  * When a plug-in is installed as both VST3 and Audio Unit it is listed once, in the format chosen in
//    Setup > Preferences > Plugins (VST3 by default), unless you ask to see both.

#include <JuceHeader.h>
#include "Prefs.h"

namespace PluginCatalog
{
    inline const juce::StringArray& categories()
    {
        static const juce::StringArray c { "EQ", "Dynamics", "Pitch Shift", "Reverb", "Delay", "Modulation", "Harmonic", "Dither",
                                           "Sound Field", "Noise Reduction", "Channel Strip", "Instrument", "Effect", "Other" };
        return c;
    }

    //==========================================================================
    // Channel-layout variants published by shell plug-ins
    enum class Layout { none, mono, stereo, monoToStereo, surround };

    struct NameParts { juce::String base; Layout layout = Layout::none; };

    inline NameParts splitName (const juce::String& fullName)
    {
        NameParts p { fullName.trim(), Layout::none };
        struct Suffix { const char* text; Layout layout; };
        static const Suffix suffixes[] = {
            { " Mono/Stereo", Layout::monoToStereo }, { " Mono-Stereo", Layout::monoToStereo }, { " Mono>Stereo", Layout::monoToStereo },
            { " (m->s)", Layout::monoToStereo }, { " (m>s)", Layout::monoToStereo }, { " M>S", Layout::monoToStereo }, { " mono/stereo", Layout::monoToStereo },
            { " Stereo", Layout::stereo }, { " (s)", Layout::stereo }, { " (Stereo)", Layout::stereo }, { " stereo", Layout::stereo },
            { " Mono", Layout::mono }, { " (m)", Layout::mono }, { " (Mono)", Layout::mono }, { " mono", Layout::mono },
            { " 5.0", Layout::surround }, { " 5.1", Layout::surround }, { " 7.0", Layout::surround }, { " 7.1", Layout::surround },
            { " 7.0.2", Layout::surround }, { " 7.1.2", Layout::surround }, { " LCR", Layout::surround }, { " Quad", Layout::surround } };
        for (auto& s : suffixes)
            if (p.base.endsWith (s.text) && p.base.length() > (int) strlen (s.text) + 1)
            {
                p.base = p.base.dropLastCharacters ((int) strlen (s.text)).trim();
                p.layout = s.layout;
                break;
            }
        return p;
    }

    //==========================================================================
    // Well-known products whose names don't say what they are. Keys are lower-case name prefixes.
    struct Known { const char* prefix; const char* category; };

    inline const std::vector<Known>& knownProducts()
    {
        static const std::vector<Known> k = {
            // ---- Waves: dynamics
            { "cla-2a", "Dynamics" }, { "cla-3a", "Dynamics" }, { "cla-76", "Dynamics" }, { "c1 ", "Dynamics" }, { "c1comp", "Dynamics" }, { "c1 comp", "Dynamics" },
            { "c1 gate", "Dynamics" }, { "c4", "Dynamics" }, { "c6", "Dynamics" }, { "l1", "Dynamics" }, { "l2", "Dynamics" }, { "l3", "Dynamics" }, { "l4", "Dynamics" },
            { "maxxvolume", "Dynamics" }, { "rcomp", "Dynamics" }, { "renaissance compressor", "Dynamics" }, { "rvox", "Dynamics" }, { "renaissance vox", "Dynamics" },
            { "raxx", "Dynamics" }, { "renaissance axx", "Dynamics" }, { "rdeesser", "Dynamics" }, { "renaissance deesser", "Dynamics" },
            { "api 2500", "Dynamics" }, { "api2500", "Dynamics" }, { "dbx 160", "Dynamics" }, { "dbx160", "Dynamics" }, { "ssl g-master", "Dynamics" }, { "sslcomp", "Dynamics" },
            { "ssl comp", "Dynamics" }, { "kramer pie", "Dynamics" }, { "pie compressor", "Dynamics" }, { "puigchild", "Dynamics" }, { "v-comp", "Dynamics" }, { "vcomp", "Dynamics" },
            { "vocal rider", "Dynamics" }, { "bass rider", "Dynamics" }, { "mv2", "Dynamics" }, { "deesser", "Dynamics" }, { "sibilance", "Dynamics" },
            { "smack attack", "Dynamics" }, { "trans-x", "Dynamics" }, { "linmb", "Dynamics" }, { "linear phase multiband", "Dynamics" }, { "wlm", "Other" },
            { "abbey road rs124", "Dynamics" }, { "rs124", "Dynamics" }, { "cla mixdown", "Dynamics" }, { "magma channel", "Channel Strip" },
            { "sync vx", "Other" }, { "x-fdbk", "Noise Reduction" }, { "lofi space", "Delay" }, { "curves aq", "EQ" }, { "curves equator", "EQ" }, { "curves", "EQ" },
            { "submarine", "Harmonic" }, { "maxxbass", "Harmonic" }, { "rbass", "Harmonic" }, { "renaissance bass", "Harmonic" }, { "lofi", "Harmonic" },
            { "silk vocal", "EQ" }, { "f6", "EQ" }, { "h-eq", "EQ" }, { "heq", "EQ" }, { "q10", "EQ" }, { "q-clone", "EQ" }, { "linear phase eq", "EQ" }, { "lineq", "EQ" },
            { "renaissance eq", "EQ" }, { "req", "EQ" }, { "puigtec", "EQ" }, { "eqp1a", "EQ" }, { "meq5", "EQ" }, { "v-eq", "EQ" }, { "veq", "EQ" },
            { "scheps 73", "EQ" }, { "api 550", "EQ" }, { "api 560", "EQ" }, { "api550", "EQ" }, { "api560", "EQ" }, { "ssl e-channel", "Channel Strip" },
            { "ssl g-channel", "Channel Strip" }, { "ssl g-equalizer", "EQ" }, { "ssl ev2", "Channel Strip" }, { "eChannel", "Channel Strip" }, { "rchannel", "Channel Strip" },
            { "renaissance channel", "Channel Strip" }, { "scheps omni", "Channel Strip" }, { "cla mixhub", "Channel Strip" }, { "kramer hls", "Channel Strip" },
            { "abbey road tg", "Channel Strip" }, { "tg12345", "Channel Strip" }, { "ssl 4000", "Channel Strip" }, { "cla vocals", "Channel Strip" }, { "cla drums", "Channel Strip" },
            { "cla guitars", "Channel Strip" }, { "cla bass", "Channel Strip" }, { "cla unplugged", "Channel Strip" }, { "cla effects", "Effect" }, { "cla epic", "Delay" },
            { "greg wells mixcentric", "Channel Strip" }, { "greg wells voicecentric", "Effect" }, { "greg wells pianocentric", "Effect" }, { "greg wells tonecentric", "Harmonic" },
            { "jjp vocals", "Channel Strip" }, { "jjp drums", "Channel Strip" }, { "jjp guitars", "Channel Strip" }, { "jjp bass", "Channel Strip" }, { "jjp strings", "Channel Strip" },
            { "tony maserati", "Channel Strip" }, { "maserati", "Channel Strip" }, { "eddie kramer", "Channel Strip" }, { "ek ", "Channel Strip" }, { "butch vig vocals", "Channel Strip" },
            { "andrew scheps", "Channel Strip" }, { "vitamin", "Sound Field" }, { "s1 ", "Sound Field" }, { "s1 imager", "Sound Field" }, { "s1 shuffler", "Sound Field" },
            { "ps22", "Sound Field" }, { "center", "Sound Field" }, { "brauer motion", "Modulation" }, { "inphase", "Sound Field" }, { "nx ", "Sound Field" },
            { "abbey road studio 3", "Sound Field" }, { "dorrough", "Other" }, { "paz", "Other" }, { "vu meter", "Other" },
            { "h-delay", "Delay" }, { "supertap", "Delay" }, { "manny marroquin delay", "Delay" }, { "mm delay", "Delay" }, { "abbey road reel adt", "Modulation" },
            { "reel adt", "Modulation" }, { "golden cassette", "Harmonic" }, { "cobalt saphira", "Harmonic" }, { "kramer master tape", "Harmonic" },
            { "j37", "Harmonic" }, { "abbey road saturator", "Harmonic" }, { "aphex vintage aural exciter", "Harmonic" }, { "aural exciter", "Harmonic" },
            { "abbey road vinyl", "Harmonic" }, { "infected mushroom pusher", "Harmonic" }, { "berzerk", "Harmonic" }, { "retro fi", "Harmonic" },
            { "h-reverb", "Reverb" }, { "rverb", "Reverb" }, { "renaissance reverb", "Reverb" }, { "trueverb", "Reverb" }, { "ir-1", "Reverb" }, { "ir-l", "Reverb" },
            { "ir1", "Reverb" }, { "irl", "Reverb" }, { "abbey road plates", "Reverb" }, { "abbey road chambers", "Reverb" }, { "manny marroquin reverb", "Reverb" },
            { "mm reverb", "Reverb" }, { "magma springs", "Reverb" }, { "space rider", "Reverb" }, { "clarity vx", "Noise Reduction" },
            { "ns1", "Noise Reduction" }, { "wns", "Noise Reduction" }, { "z-noise", "Noise Reduction" }, { "x-noise", "Noise Reduction" }, { "x-click", "Noise Reduction" },
            { "x-crackle", "Noise Reduction" }, { "x-hum", "Noise Reduction" }, { "debreath", "Noise Reduction" }, { "w43", "Noise Reduction" },
            { "waves tune", "Pitch Shift" }, { "ultrapitch", "Pitch Shift" }, { "soundshifter", "Pitch Shift" }, { "morphoder", "Pitch Shift" }, { "ovox", "Pitch Shift" },
            { "torque", "Pitch Shift" }, { "doubler", "Modulation" }, { "mondomod", "Modulation" }, { "enigma", "Modulation" }, { "metaflanger", "Modulation" },
            { "metafilter", "EQ" }, { "doppler", "Modulation" }, { "kaleidoscopes", "Modulation" }, { "flow motion", "Instrument" }, { "codex", "Instrument" },
            { "grand rhapsody", "Instrument" }, { "electric 88", "Instrument" }, { "electric 200", "Instrument" }, { "electric grand", "Instrument" },
            { "clavinet", "Instrument" }, { "bass fingers", "Instrument" }, { "bass slapper", "Instrument" }, { "cr8", "Instrument" }, { "bb tubes", "Harmonic" },
            { "pro-q", "EQ" }, { "pro-c", "Dynamics" }, { "pro-l", "Dynamics" }, { "pro-mb", "Dynamics" }, { "pro-ds", "Dynamics" }, { "pro-g", "Dynamics" },
            { "pro-r", "Reverb" }, { "saturn", "Harmonic" }, { "timeless", "Delay" }, { "volcano", "EQ" }, { "simplon", "EQ" },
            { "decapitator", "Harmonic" }, { "radiator", "Harmonic" }, { "devil-loc", "Dynamics" }, { "echoboy", "Delay" }, { "primaltap", "Delay" }, { "crystallizer", "Pitch Shift" },
            { "little alterboy", "Pitch Shift" }, { "littlealterboy", "Pitch Shift" }, { "microshift", "Modulation" }, { "phasemistress", "Modulation" }, { "filterfreak", "EQ" },
            { "panman", "Sound Field" }, { "tremolator", "Modulation" }, { "sie-q", "EQ" }, { "effectrack", "Effect" }, { "superplate", "Reverb" }, { "spaceblender", "Reverb" },
            { "ozone", "Dynamics" }, { "neutron", "Channel Strip" }, { "nectar", "Channel Strip" }, { "rx ", "Noise Reduction" }, { "rx10", "Noise Reduction" }, { "rx11", "Noise Reduction" },
            { "trash 2", "Harmonic" }, { "vinyl", "Harmonic" }, { "tonal balance", "Other" }, { "neoverb", "Reverb" }, 
            { "valhalla", "Reverb" }, { "valhallafreqecho", "Delay" }, { "valhalladelay", "Delay" }, { "valhallaspacemod", "Modulation" },
            { "bx_", "EQ" }, { "bx_console", "Channel Strip" }, { "bx_digital", "EQ" }, { "bx_xl", "Dynamics" }, { "bx_townhouse", "Dynamics" }, { "bx_opto", "Dynamics" },
            { "bx_rooMS", "Reverb" }, { "bx_stereomaker", "Sound Field" }, { "bx_subsynth", "Harmonic" }, { "spl ", "Dynamics" }, { "shadow hills", "Dynamics" },
            { "lindell", "Channel Strip" }, { "ssl 9000", "Channel Strip" }, { "unfiltered audio", "Effect" }, { "black box", "Harmonic" },
            { "virtual mix rack", "Channel Strip" }, { "vmr", "Channel Strip" }, { "virtual tape", "Harmonic" }, { "virtual buss", "Dynamics" },
            { "virtual console", "Harmonic" }, { "infinity eq", "EQ" }, { "fresh air", "Harmonic" }, { "verbsuite", "Reverb" }, { "repeater", "Delay" },
            { "neve 1073", "EQ" }, { "1073", "EQ" }, { "neve 88", "Channel Strip" }, { "api vision", "Channel Strip" }, { "studer", "Harmonic" }, { "ampex", "Harmonic" },
            { "ocean way", "Reverb" }, { "lexicon", "Reverb" }, { "emt", "Reverb" }, { "capitol chambers", "Reverb" },
            { "melodyne", "Pitch Shift" }, { "kontakt", "Instrument" }, { "massive", "Instrument" }, { "serum", "Instrument" }, { "omnisphere", "Instrument" },
            { "keyscape", "Instrument" }, { "trilian", "Instrument" }, { "battery", "Instrument" }, { "superior drummer", "Instrument" }, 
            { "guitar rig", "Harmonic" }, { "amplitube", "Harmonic" }, { "neural dsp", "Harmonic" }, { "soothe", "Dynamics" }, { "gullfoss", "EQ" },
            { "trackspacer", "Dynamics" }, { "youlean", "Other" }, { "insight 2", "Other" }, { "span plus", "Other" }, { "ott", "Dynamics" }, { "kickstart", "Dynamics" }, { "lfotool", "Modulation" },
            { "shaperbox", "Modulation" }, { "cableguys", "Modulation" }, { "halftime", "Pitch Shift" } };
        return k;
    }

    //==========================================================================
    inline juce::String fromVst3Category (const juce::String& raw)
    {
        auto c = raw.toLowerCase();
        if (c.contains ("instrument") || c.contains ("synth") || c.contains ("sampler") || c.contains ("drum")) return "Instrument";
        if (c.contains ("|eq") || c.contains ("eq|") || c == "eq" || c.contains ("filter")) return "EQ";
        if (c.contains ("dynamics") || c.contains ("mastering")) return "Dynamics";
        if (c.contains ("reverb")) return "Reverb";
        if (c.contains ("delay")) return "Delay";
        if (c.contains ("modulation")) return "Modulation";
        if (c.contains ("distortion")) return "Harmonic";
        if (c.contains ("pitch")) return "Pitch Shift";
        if (c.contains ("restoration")) return "Noise Reduction";
        if (c.contains ("spatial") || c.contains ("surround") || c.contains ("ambisonics")) return "Sound Field";
        if (c.contains ("analyzer") || c.contains ("tools") || c.contains ("generator") || c.contains ("network")) return "Other";
        return {};
    }

    inline bool hasWord (const juce::String& hay, const char* word)   { return hay.contains (word); }

    inline juce::String fromName (const juce::String& lowerName)
    {
        auto n = " " + lowerName + " ";
        struct Rule { const char* category; std::initializer_list<const char*> words; };
        static const Rule rules[] = {
            { "Other",           { "meter", "analy", "scope", "spectr", "lufs", "tuner", "oscillo", "correl" } },
            { "Noise Reduction", { "denois", "de-nois", "noise red", "dehum", "de-hum", " hum ", "declick", "de-click", "decrackle", "de-crackle", "declip", "de-clip",
                                   "restor", "debreath", "de-breath", "dereverb", "de-reverb", "deplosive", "voice isolat", "dialogue isolat", "unveil" } },
            { "Dither",          { "dither", "noise shap", " idr", "pow-r" } },
            { "Channel Strip",   { "channel strip", "channelstrip", " strip ", "console 1", "mixhub", "voice strip" } },
            { "Pitch Shift",     { "pitch", " tune", "tuner", "harmoniz", "octav", "formant", "vocoder", "shifter", "transpos", "detune", "varispeed" } },
            { "Reverb",          { "verb", "reverb", "plate", "chamber", " hall", " room", "convol", "spring", "ambien", "impulse", "shimmer", "space " } },
            { "Delay",           { "delay", "echo", " tap ", "multitap", "multi-tap", "dly", "repeat" } },
            { "Modulation",      { "chorus", "flang", "phaser", "phase shift", "tremol", "vibrat", "rotary", "leslie", "ensemble", " mod ", "modulat", "doubler", " adt", "auto pan", "autopan", "ring mod", "wah" } },
            { "EQ",              { " eq", "eq ", "equal", "filter", "tilt", "1073", "parametric", "graphic", "shelf" } },
            { "Dynamics",        { "comp", "limit", "maxim", "gate", "expand", "de-ess", "deess", "de ess", "sibil", "transient", "rider", "leveler", "leveller", "dynamic", "multiband",
                                   "multi-band", "glue", "squash", "punch", "duck", "opto", "vari-mu", "varimu", "bus comp", "loudness max", "clip" } },
            { "Harmonic",        { "satur", "tape", "distort", "drive", "fuzz", "exciter", "enhanc", "harmonic", "tube", "valve", "bitcrush", "crush", "lofi", "lo-fi", "decimat",
                                   " amp", "amp ", "cabinet", "cab ", "warm", "colour", "color", "vinyl", "radio", "analog" } },
            { "Sound Field",     { "stereo", "imager", "image", "width", "wide", "widen", "mono", "mid/side", "m/s", "binaural", "spatial", "surround", "panner", " pan ", "upmix", "3d" } },
            { "Channel Strip",   { " channel", "-channel" } },
            { "Other",           { "utility", "gain", "trim", "test", "tone gen", "loud", "monitor", "level" } },
        };
        for (auto& r : rules)
            for (auto w : r.words)
                if (n.contains (w)) return r.category;
        return {};
    }

    /** The Pro Tools category for any plug-in. */
    inline juce::String classify (const juce::String& name, const juce::String& rawCategory, bool isInstrument)
    {
        if (isInstrument) return "Instrument";
        auto base = splitName (name).base.toLowerCase();
        if (auto v = fromVst3Category (rawCategory); v.isNotEmpty() && v != "Instrument") return v;
        for (auto& k : knownProducts())
            if (base.startsWith (juce::String (k.prefix).toLowerCase())) return k.category;
        if (auto v = fromName (base); v.isNotEmpty()) return v;
        return "Effect";
    }

    /** Signature bundle / built-in plug-ins: their own grouping mapped to Pro Tools categories. */
    inline juce::String classifyBuiltIn (const juce::String& name, const juce::String& group)
    {
        auto g = group.toLowerCase(), n = name.toLowerCase();
        if (n.contains ("pitch")) return "Pitch Shift";
        if (g.contains ("pitch") || g.contains ("vocal")) return "Pitch Shift";
        if (n.contains ("dither")) return "Dither";
        if (n.contains ("midi")) return "Other";
        if (g.contains ("instrument")) return "Instrument";
        if (g.startsWith ("eq") || g.contains ("filter")) return n.contains ("sub") ? "Harmonic" : "EQ";
        if (g.contains ("dynamics")) return "Dynamics";
        if (g.contains ("saturation") || g.contains ("tape")) return "Harmonic";
        if (g.contains ("space"))  return (n.contains ("delay") || n.contains ("echo")) ? "Delay" : "Reverb";
        if (g.contains ("reverb")) return "Reverb";
        if (g.contains ("delay"))  return "Delay";
        if (g.contains ("modulation")) return "Modulation";
        if (g.contains ("utility")) return (n.contains ("wing") || n.contains ("mono") || n.contains ("wide") || n.contains ("stereo")) ? "Sound Field" : "Other";
        return classify (name, {}, false);
    }

    //==========================================================================
    /** One line in the plug-in menus for a third-party plug-in: the best variant for the destination track. */
    struct Item
    {
        juce::String name, vendor, category, format;
        int typeIndex = -1;          // index into KnownPluginList::getTypes()
        bool isInstrument = false;
    };

    /** Setup > Preferences > Plugins: which format to list when a plug-in is installed as VST3 and Audio Unit. */
    inline juce::String formatPreference()
    {
        static const char* names[] = { "vst3", "au", "both" };
        return names[juce::jlimit (0, 2, Prefs::getInt ("pl_formatPref", 0))];
    }

    inline std::vector<Item> thirdPartyItems (const juce::Array<juce::PluginDescription>& types, bool stereoTarget)
    {
        struct Group { juce::String name, vendor, rawCategory; bool instrument = false; std::map<juce::String, std::vector<std::pair<int, Layout>>> byFormat; };
        std::map<juce::String, Group> groups;
        for (int i = 0; i < types.size(); ++i)
        {
            auto& d = types.getReference (i);
            auto parts = splitName (d.name);
            auto key = d.manufacturerName.toLowerCase() + "|" + parts.base.toLowerCase() + "|" + (d.isInstrument ? "i" : "f");
            auto& g = groups[key];
            if (g.name.isEmpty()) { g.name = parts.base; g.vendor = d.manufacturerName; g.instrument = d.isInstrument; }
            if (g.rawCategory.isEmpty() || g.rawCategory.equalsIgnoreCase ("Fx")) g.rawCategory = d.category;
            g.byFormat[d.pluginFormatName].push_back ({ i, parts.layout });
        }

        auto rank = [stereoTarget] (Layout l)
        {
            // a mono track in Squelchmoth carries the same signal on both sides, so stereo-capable variants are safe
            // everywhere; pure mono variants are only used when nothing else exists
            switch (l)
            {
                case Layout::stereo:       return stereoTarget ? 0 : 1;
                case Layout::monoToStereo: return stereoTarget ? 1 : 0;
                case Layout::none:         return 2;
                case Layout::mono:         return 3;
                case Layout::surround:     return 4;
            }
            return 5;
        };

        const auto pref = formatPreference();
        std::vector<Item> out;
        for (auto& [key, g] : groups)
        {
            juce::StringArray formats;
            for (auto& [f, v] : g.byFormat) formats.add (f);
            juce::StringArray chosen;
            if (pref == "both" || formats.size() == 1) chosen = formats;
            else
            {
                auto want = pref == "au" ? juce::String ("AudioUnit") : juce::String ("VST3");
                chosen.add (formats.contains (want) ? want : formats.contains ("VST3") ? juce::String ("VST3") : formats[0]);
            }
            for (auto& f : chosen)
            {
                auto variants = g.byFormat[f];
                std::sort (variants.begin(), variants.end(), [&] (auto& a, auto& b) { return rank (a.second) < rank (b.second); });
                Item it;
                it.name = g.name;
                it.vendor = g.vendor.isNotEmpty() ? g.vendor : juce::String ("Other");
                it.isInstrument = g.instrument;
                it.category = classify (g.name, g.rawCategory, g.instrument);
                it.format = f == "AudioUnit" ? "AU" : f;
                it.typeIndex = variants.front().first;
                out.push_back (it);
            }
        }
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.name.compareNatural (b.name) < 0; });
        return out;
    }
}

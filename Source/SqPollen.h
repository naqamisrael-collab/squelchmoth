#pragma once
// Pollen - the preset-pack instrument: the Lantern Exo engine (wavetables + analogue oscillators, dual filters, mod matrix)
// behind pack pages with named macros. Packs are categories of the Pollen preset bank; the four macros are wired through
// the mod matrix, so every preset names what its macro knobs do.
#include "SqSynths2.h"

namespace SqSynth
{
    class SqPollenPlugin : public SqLanternExoPlugin
    {
    public:
        static constexpr const char* xmlTypeName = "sqmPollen";
        static const char* getPluginName()   { return NEEDS_TRANS("Pollen"); }
        juce::String getName() const override { return "Pollen"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Pollen"; }
        juce::String getSelectableDescription() override { return "Pollen preset-pack instrument"; }
        SqPollenPlugin (te::PluginCreationInfo info) : SqLanternExoPlugin (info)
        {
            pack.referTo (state, "pollenPack", getUndoManager(), "Keys");
            presetName.referTo (state, "pollenPreset", getUndoManager(), juce::String());
        }
        ~SqPollenPlugin() override {}
        juce::CachedValue<juce::String> pack, presetName;

        struct PackInfo { const char* name; const char* blurb; juce::uint32 colour; int glyph; };
        static const std::vector<PackInfo>& packs()
        {
            static const std::vector<PackInfo> p = {
                { "Keys",           "Pianos, electric pianos, clavs, organs and soft keys",                 0xffffd23a, 0 },
                { "Pads",           "Warm analogue, morphing wavetables, strings, choirs, drones",         0xff7a3aff, 1 },
                { "Plucks",         "Tight synth plucks, mallets, harps, organ stabs",                      0xff3affb0, 2 },
                { "Bass",           "Subs, 808s, reese, acid, wobbles, growls, stabs",                      0xffff3ad1, 3 },
                { "Brass",          "Synth brass, horn sections, trap brass, hoovers",                      0xffff9a3a, 4 },
                { "Strings",        "Ensembles, cellos, violins, cinematic and 80s strings",                0xffc98a4a, 5 },
                { "Choir",          "Ahh and ooh choirs, gospel stacks, synth vox, whispers",               0xff3ad6ff, 6 },
                { "Bells",          "Glass, tubular, music box, gamelan, chime pads",                       0xffb14dff, 7 },
                { "Textures",       "Shimmers, digital rain, drones, wind, risers, alien talk",             0xff58ff3a, 8 },
                { "Trap Melodies",  "Dark bells, flutes, plucked guitars, eerie keys, choir stabs, leads",  0xffff5a3a, 9 },
                { "Drill Melodies", "Drill pianos, bells, strings, flutes, vox, plucks",                    0xff47c8ff, 10 } };
            return p;
        }
        static int packIndex (const juce::String& name)   { auto& p = packs(); for (int i = 0; i < (int) p.size(); ++i) if (name.equalsIgnoreCase (p[(size_t) i].name)) return i; return 0; }
        /** What each macro drives, read from the mod matrix (the pack page labels the knobs with it). */
        juce::String macroLabel (int macro)
        {
            for (int i = 0; i < numModSlots; ++i)
                if (modSourceValue (i).get() == (int) ModSource::macro1 + macro && modDestValue (i).get().isNotEmpty())
                {
                    auto d = modDestValue (i).get();
                    static const std::map<juce::String, juce::String> friendly = { { "pitch", "Pitch" }, { "cutoff", "Cutoff" }, { "resonance", "Resonance" }, { "pw", "Width" }, { "amp", "Level" }, { "pan", "Pan" }, { "fm", "FM" }, { "morph", "Morph" } };
                    auto it = friendly.find (d); return it != friendly.end() ? it->second : d;
                }
            return "Macro " + juce::String (macro + 1);
        }
    };
}

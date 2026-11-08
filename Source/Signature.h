#pragma once
// Squelchmoth Signature Bundle - the flagship built-in instruments and processors: Squelchmoth's own engines
// plus curated builds of Tracktion core DSP and AirWindows algorithms, each with factory presets.
// Squelchmoth's own bundle: every entry is an original Squelchmoth design with its own name, artwork and presets.
//
// The full AirWindows collection (186) stays available under its own menu for people who want the raw tools.

#include <JuceHeader.h>
#include "BuiltinPlugins.h"
#include "SignaturePresets.h"
#include "SqInstruments.h"
#include "SqSynths.h"
#include "SqSynths2.h"
#include "SqDrumSynths.h"
#include "SqSamplers2.h"
#include "SqPollen.h"
#include "SqRackChannel.h"
#include "SqHivePads.h"
#include "SqFactory.h"
#include "SqVocalFx.h"
#include "SqApex.h"
#include "SqFx181.h"
#include "SqColour.h"
#include "SqCinder.h"
#include "SqWingbeat.h"
#include "SqFoilPlate.h"
#include "SqRadiantMaw.h"

namespace te = tracktion;

namespace Signature
{
    struct Entry
    {
        const char* name;          // Squelchmoth brand name
        const char* type;          // engine plugin type (see BuiltinPlugins.h)
        const char* category;      // menu group
        const char* tagline;       // one line shown in the editor header
        juce::uint32 colour;       // header colour
        const char* art;           // faceplate artwork key (Resources/plugart/pa_<art>.jpg)
        std::vector<Preset> presets;
    };

    inline const std::vector<Entry>& entries()
    {
        struct Row { const char* name; const char* type; const char* category; const char* tagline; juce::uint32 colour; const char* art; };
        static const Row rows[] = {
            // ---------------- Instruments
            { "Mothsynth",        "4osc",            "Instruments", "Four-oscillator analogue-style synth: saw/square/sine/noise, resonant filter, two envelopes, LFO, unison", 0xff7a3aff, "mothsynth" },
            { "Chrysalis Sampler","sampler",         "Instruments", "Multi-zone key-mapped sampler for your own sounds - drop WAVs, set root notes, play",   0xff7a3aff, "chrysalis" },
            { "Hive Drums",       "sampler",         "Instruments", "The beat-maker drum machine: 27 sound kits, twelve pads, the Beat Lab channel rack",             0xffffb23a, "hivedrums" },
            { "Moth 808",         "sqm808",          "Instruments", "Dedicated 808 bass: punch, click, sub, 4x-oversampled drive, tone - and FL-style glides on overlapping notes", 0xffff3ad1, "moth808" },
            { "Blossom 2",        "sqmBlossom",      "Instruments", "A physical string model: two coupled strings per voice, pluck / hammer / bow, material pad, 8 bodies, sympathetic strings, vibrato, ethereal chorus + reverb, strum", 0xff3affb0, "blossom2" },
            { "Mothsynth 2",      "sqmMothsynth2",   "Instruments", "The flagship analogue-style synth: 3 anti-aliased oscillators with sync / ring / FM, sub, noise, dual filters, 3 envelopes, 2 LFOs, 8-slot mod matrix, unison, FX", 0xffb14dff, "mothsynth2" },
            { "Venom",            "sqmVenom",        "Instruments", "Modern subtractive lead and bass synth: morphing oscillators, two filters in series or parallel, trance gate, drive, mod matrix, 4 macros, chorus / delay / reverb", 0xff58ff3a, "venom" },
            { "Sim Moth",         "sqmSimMoth",      "Instruments", "The simple three-oscillator synth: levels, pitches, pulse width, one filter, two envelopes and an LFO - classic sounds in seconds", 0xffffd23a, "simmoth" },
            { "Mini Moth",        "sqmMiniMoth",     "Instruments", "One-screen, ultra-light synth: oscillator + sub, filter, envelope, vibrato, chorus - stacks by the dozen", 0xff3ad6ff, "minimoth" },
            { "Thump",            "sqmThump",        "Instruments", "Kick drum synth: pitch sweep with curve, body harmonic, click, sub layer, drive and tone - every kick from 808 to hardstyle", 0xffff5a3a, "thump" },
            { "Hive Pads",        "sqmHivePads",     "Instruments", "The pad sampler: 64 pads in 4 banks, 8 layers per pad (velocity / round-robin / random / crossfade), choke groups, per-pad tune / envelope / filter, the 27 kits in one click, slice a loop to pads", 0xffffc23a, "hivepads" },
            { "Dial 10",          "sqmDial10",       "Instruments", "Two-operator FM in sixteen knobs: ratio, index, feedback, two envelopes, vibrato - bells, basses, keys and plucks", 0xffff9a3a, "dial10" },
            { "Hexwing",          "sqmHexwing",      "Instruments", "Six-operator FM / RM hybrid: per-operator wave, ratio, level, feedback and envelope, 8 algorithms, a filter after the operators, 2 LFOs, mod matrix, unison, FX", 0xff3adfff, "hexwing" },
            { "Toxin FM",         "sqmToxin",        "Instruments", "Six-operator FM with a 16-step pitch / gate / accent / slide sequencer, ladder filter, drive and delay - acid FM, sequenced basses and arps", 0xff7cff3a, "toxin" },
            { "Lantern",          "sqmLantern",      "Instruments", "Vintage polysynth: two drifting oscillators, self-oscillating ladder, ensemble, 8-voice vintage mode, three eras", 0xffffb347, "lantern" },
            { "Lantern Exo",      "sqmLanternExo",   "Instruments", "The vintage synth re-imagined: morphing wavetables plus analogue oscillators, dual filters, 32 voices, mod matrix, drift, FX", 0xff47c8ff, "lanternexo" },
            { "Larva Bass",       "sqmLarvaBass",    "Instruments", "Acid bass: saw / square, diode-style ladder, accent and slide, drive, and a 16-step sequencer with slide and accent per step", 0xffd4ff3a, "larvabass" },
            { "Thorn Bass",       "sqmThornBass",    "Instruments", "Electric bass model: plucked string, pick and pickup position, neck / bridge blend, amp, slap, palm mute, fret noise, round-robin picks", 0xffc98a4a, "thornbass" },
            { "Carapace",         "sqmCarapace",     "Instruments", "16-sound drum synth: membrane, metal, noise, wood and tom models plus the classic two-oscillator engine per sound, choke groups, master tone and drive", 0xffff7a3a, "carapace" },
            { "Live Drum Synth",  "sqmLiveDrumSynth","Instruments", "The acoustic-leaning drum synth: 12 kit pieces with pitch, decay, tone, snap and room per piece - play it live from pads", 0xffe0c23a, "livedrumsynth" },
            { "Drum Pad Model",   "sqmDrumPadModel", "Instruments", "One physically-modelled drum per instance, keytracked across the keyboard - membrane, metal, wood or noise, with pitch envelope and drive", 0xff3affd6, "drumpadmodel" },
            { "Thump Live",       "sqmThumpLive",    "Instruments", "Kick drum synth with drawable pitch, amp and distortion envelopes (four breakpoints each), click, sub, tone, drive, and a built-in 16-step pattern", 0xffff4a6a, "thumplive" },
            { "Chrysalis 2",      "sqmChrysalis2",   "Instruments", "The keyed multi-zone sampler: key and velocity zones, round-robin groups, loops with crossfade, ADSR, filter, drop a folder to auto-map, SFZ and SF2 import", 0xff9a6bff, "chrysalis2" },
            { "Chrysalis SF",     "sqmChrysalisSF",  "Instruments", "SoundFont (SF2) and SFZ bank player: browse the bank's presets, loops, envelopes and velocity layers come through", 0xffc08cff, "chrysalissf" },
            { "Mandible",         "sqmMandible",     "Instruments", "Two-deck slicer: transient or grid slicing, slices on keys (A from C1, B from C4), per-slice pitch / reverse / level / filter / decay, stretch to tempo, dump to piano roll, slice to rack channels, A/B crossfade", 0xffff8a3a, "mandible" },
            { "Mandible Lite",    "sqmMandibleLite", "Instruments", "The quick slicer: drop a loop, pick the slice resolution, play the slices from C1, dump to the piano roll", 0xffffb36b, "mandiblelite" },
            { "Pollen",           "sqmPollen",       "Instruments", "The preset-pack instrument: 11 packs (Keys, Pads, Plucks, Bass, Brass, Strings, Choir, Bells, Textures, Trap Melodies, Drill Melodies) on the wavetable + analogue engine, four named macros per sound, pack browser with covers", 0xffffc23a, "pollen" },
            // ---------------- Pitch & Vocal
            { "Moth Tune",        "sqmTune",         "Pitch & Vocal", "The flagship vocal tuner: Live (<1 ms) or Studio engine, Vibrato Keep, formant & throat, MIDI targets, 4 harmony voices, Graph notes", 0xff19e3ff, "mothtune" },
            { "Moth Tune FX",     "sqmTuneFx",       "Pitch & Vocal", "Creative tuning: editable Motion patterns plus a six-slot X/Y effects rack - Mutate, Duet, Vocode, Talkbox, Octaver, Stutter and more", 0xffff2fd6, "mothtunefx" },
            // ---------------- EQ & filters
            { "Pulse EQ",         "sqmPulse",        "EQ & Filter", "Passive program EQ: the boost-and-cut low end, bandwidth high boost, high attenuation, plus a peak-dip-peak mid section and tube", 0xff2fe0ff, "pulseeq" },
            { "Silk Ladder",      "sqmLadder",       "EQ & Filter", "Four- or two-pole transistor-ladder low-pass with envelope follower, resonance and drive", 0xffff7a1a, "silkladder" },
            { "Prism EQ",         "4bandEq",         "EQ & Filter", "Four-band parametric with low/high shelves - clean, musical, zero-latency",    0xff3ad1ff, "prismeq" },
            { "Scalpel",          "airwindows_biquad2", "EQ & Filter", "Surgical single-band filter/peak with smooth parameter interpolation - find and fix resonances",    0xff3ad1ff, "scalpel" },
            { "Lantern Tilt",     "airwindows_toneslant", "EQ & Filter", "One-knob tilt EQ - brighter or darker without losing balance",      0xff3ad1ff, "lanterntilt" },
            { "Antenna Air",      "airwindows_air",  "EQ & Filter", "Three bands of pure high-frequency sheen that never sound harsh",     0xff3ad1ff, "antennaair" },
            { "Deep Sub",         "airwindows_dubsub", "EQ & Filter", "Sub-harmonic synth and bass shaper - weight for 808s, kicks and bass", 0xff3ad1ff, "deepsub" },
            { "Veil Filter",      "lowpass",         "EQ & Filter", "Resonant low/high-pass with sweepable cutoff - automatable filter sweeps and builds", 0xff3ad1ff, "veilfilter" },
            { "Capacitor",        "airwindows_capacitor", "EQ & Filter", "Analogue-modelled HP/LP with 'non-linear capacitor' colour - DJ-style sweeps with character", 0xff3ad1ff, "capacitor" },
            // ---------------- Dynamics
            { "Apex Dynamics",    "sqmApexDyn",      "Dynamics",    "Compressor, expander, gate, ducker and upward compressor in one: lookahead, negative ratios, Smart detection, key filter", 0xffffc21a, "apexdyn" },
            { "Apex Limiter",     "sqmApexLim",      "Dynamics",    "True-peak limiter with lookahead, Character and a full BS.1770-4 loudness meter with one-click loudness targets", 0xffff4a2a, "apexlim" },
            { "Apex Triad",     "sqmApexTriad",  "Dynamics",    "Three-band maximiser: per-band threshold, knee, attack and release with the envelope view, lookahead ceiling", 0xffff6a3a, "apextriad" },
            { "Apex Multiband",   "sqmApexMB",       "Dynamics",    "Up to six bands, minimum or linear phase, flip, solo, auto gain and a live spectrum with draggable crossovers", 0xff1affc8, "apexmb" },
            { "Apex Sub",         "sqmApexSub",      "EQ & Filter", "Pitch-tracked sub-octave and fifth bands from any source range or MIDI, six drive types, filters and mix", 0xffb03aff, "apexsub" },
            { "Opto Leveler",     "sqmOpto",         "Dynamics",    "Optical leveling amp: program-dependent two-stage release, compress or limit, HF emphasis, tube stage and a real VU", 0xffffe066, "optoleveler" },
            { "Cocoon Compressor","compressor",      "Dynamics",    "Versatile feed-forward compressor/limiter with sidechain - the everyday squeeze", 0xffd4ff3a, "cocoon" },
            { "Moth Press",       "airwindows_pressure4", "Dynamics", "The famous one-knob 'pressure' compressor: speed, mewl, output - glue that breathes",      0xffd4ff3a, "mothpress" },
            { "Pupa Buss Comp",   "airwindows_buttercomp2", "Dynamics", "Smooth bi-polar interleaved buss compressor - transparent mix glue", 0xffd4ff3a, "pupabuss" },
            { "Vari-Mu Tube",     "airwindows_varimu", "Dynamics",  "Variable-mu tube compressor model with dual-stage saturation - vintage warmth", 0xffd4ff3a, "varimu" },
            { "Clamp Limiter",    "airwindows_adclip7", "Dynamics", "Transparent brickwall clipper/limiter for loud masters without smear",            0xffd4ff3a, "clamplimiter" },
            { "Veil Gate",        "airwindows_gatelope", "Dynamics", "Gate with tone-shaping release - cleans drums and tightens tails", 0xffd4ff3a, "veilgate" },
            { "Hush De-Ess",    "airwindows_deess", "Dynamics",   "Zero-latency de-esser with frequency-select - tames sibilance without lisping", 0xffd4ff3a, "nectardeess" },
            { "Snap Transient",   "airwindows_point", "Dynamics",   "Transient designer: more or less attack on drums, bass and plucks", 0xffd4ff3a, "snaptransient" },
            { "Pop Knock",        "airwindows_pop",   "Dynamics",   "Explosive drum compressor that brings kicks and snares forward in the mix",           0xffd4ff3a, "popknock" },
            // ---------------- Saturation, tape & console
            { "Cinder",           "sqmCinder",       "Saturation & Tape", "Ten-model saturation: tape preamp to diode fuzz, oversampled to 16x with anti-aliased curves, Model Blend, Bias, Dynamics, Mid/Side, ducked drive, Delta and a live harmonic readout", 0xffff5a1f, "cinder" },
            { "Radiant",          "sqmRadiant",      "Saturation & Tape", "Two-stage tube mixer colour: 60s mixer, 50s broadcast pre or tube line amp, Baxandall bass / treble, Mic / Line, noise floor, heat VU, Mid/Side", 0xffffb347, "radiant" },
            { "Radiant Pre",      "sqmRadiantPre",   "Saturation & Tape", "Tube mic preamp colour: one hot stage, a Bias switch for choppy breakup, a vintage noise floor", 0xffff9a3a, "radiantpre" },
            { "Maw",              "sqmMaw",          "Dynamics",    "The crushing podium limiter: Crush, Crunch, Darkness, Mix, 1.3 ms attack, a release that stretches as it saturates, external key with HPF, GR history", 0xffff3a5a, "maw" },
            { "Maw Lite",         "sqmMawLite",      "Dynamics",    "Crush and Crunch: instant room-mic attitude", 0xffe83a55, "mawlite" },
            { "Ember Tape",       "airwindows_totape5", "Saturation & Tape", "Full tape machine model: head bump, flutter, softness, bias - the Squelchmoth 'warm' button", 0xffff8a3a, "embertape" },
            { "Scorch Drive",     "airwindows_drive", "Saturation & Tape", "Classic overdrive with highpass and tone - from grit to fuzz",    0xffff8a3a, "scorchdrive" },
            { "Lumen Channel",    "airwindows_console6channel", "Saturation & Tape", "Console channel stage: put one on every track, Lumen Buss on the master, for analogue summing", 0xffff8a3a, "lumenchannel" },
            { "Lumen Buss",       "airwindows_console6buss", "Saturation & Tape", "Console summing stage for busses and the master - pairs with Lumen Channel",       0xffff8a3a, "lumenbuss" },
            { "Ashen Desk",       "airwindows_desk4", "Saturation & Tape", "Desk/op-amp saturation with slew and power-sag modelling - weight and glue",             0xffff8a3a, "ashendesk" },
            { "Furnace Density",  "airwindows_density", "Saturation & Tape", "Density saturation that thickens anything - from subtle to fat",   0xffff8a3a, "furnacedensity" },
            { "Triode Glow",      "airwindows_singeendedtriode", "Saturation & Tape", "Single-ended triode tube stage with second-harmonic warmth", 0xffff8a3a, "triodeglow" },
            { "Vinyl Press",      "airwindows_tovinyl4", "Saturation & Tape", "Cutting-lathe emulation: elliptical bass, groove wear, highs filtering",    0xffff8a3a, "vinylpress" },
            { "Grit Bits",        "airwindows_derez2", "Saturation & Tape", "Bit and sample-rate reduction with analogue softness - lo-fi textures",            0xffff8a3a, "gritbits" },
            // ---------------- Space: reverb & delay
            { "Aurora Verb",      "sqmAurora",       "Space",       "Eight-line FDN reverb: eight room types, early reflections, three-band decay, modulation, ducking, freeze and width", 0xff6a7bff, "auroraverb" },
            { "Wingbeat Echo",    "sqmWingbeat",     "Space",       "The 36-style echo: tape, tube, spring, bucket, oil can, radios and digital racks; Single, Dual, Ping-Pong and 32-tap Rhythm, up to 30 s, ducking, freeze and a live tap map", 0xff3ac8ff, "wingbeat" },
            { "Wingbeat Jr.",     "sqmWingbeatJr",   "Space",       "The quick echo: eight styles, Normal / Wide / Ping-Pong, glide or stretch when the time changes", 0xff3aa8ff, "wingbeatjr" },
            { "Foil Plate",       "sqmFoilPlate",    "Space",       "Plate reverb: seven plates from steel to gold foil, four audible input stages, decay per band, auto-decay that shortens or lengthens (or follows a key), freeze", 0xffd8c27a, "foilplate" },
            { "Foil Plate Lite",  "sqmFoilPlateLite", "Space",      "The steel plate in four knobs: Decay, Low Cut, Mod, Mix", 0xffc8b26a, "foilplatelite" },
            { "Oil Can Delay",    "sqmOilCan",       "Space",       "Electrostatic oil-can echo: wobbling delay line, sustain, tone, drive - echo or doubler", 0xffffa31a, "oilcan" },
            { "Bucket Brigade",   "sqmBBD",          "Space",       "Analog bucket-brigade delay: darkening repeats, compander drive, filtered feedback and chorus-style modulation", 0xffff5a8a, "bucketbrigade" },
            { "Nebula Verb",      "reverb",          "Space",       "Lush algorithmic reverb: room size, damping, width, pre-delay, freeze",           0xff3affb0, "nebulaverb" },
            { "Cavern IR",        "impulseResponse", "Space",       "Convolution reverb - load any impulse response WAV for real spaces and hardware", 0xff3affb0, "cavernir" },
            { "Pocket Spaces",    "airwindows_pocketverbs", "Space", "Compact reverb collection: chamber, spring, tiled, stretch - instant character",   0xff3affb0, "pocketspaces" },
            { "Starfield",        "airwindows_starchild", "Space",  "Otherworldly granular-style ambience and shimmer for pads and vocals",       0xff3affb0, "starfield" },
            { "Dust Delay",       "delay",           "Space",       "Tempo-synced stereo delay with feedback and mix",    0xff3affb0, "dustdelay" },
            { "Echo Wing",        "airwindows_tapedelay", "Space",  "Tape echo with wow, flutter and saturating feedback - dub and vintage slap",        0xff3affb0, "echowing" },
            // ---------------- Modulation & pitch
            { "Halo Ring",        "sqmRing",         "Modulation",  "Ring modulator with sine or square LFO, drive and mix - bells, robots and tremolo", 0xff9dff2a, "haloring" },
            { "Time Weaver",      "sqmTimeWeaver",   "Modulation",  "Time and volume slicer: 32 steps over two bars, 36 factory time slots (half-time, stutter, reverse, tape stop, scratch) and 36 volume slots (gates, pumps, trance gates), MIDI picks the slot", 0xff3ae0c8, "timeweaver" },
            { "Larva Filter",     "sqmLarvaFilter",  "EQ & Filter", "Eight filter stages in series, parallel or 4+4, each with its own type, resonance, LFO, envelope follower and share of the 16-step sequencer", 0xffb8ff3a, "larvafilter" },
            { "Twelve Phase",     "sqmPhase12",      "Modulation",  "Four- to twelve-stage phaser with resonance and stereo spread", 0xffc04dff, "twelvephase" },
            { "Flutter Chorus",   "chorus",          "Modulation",  "Classic stereo chorus - depth, speed, width, mix",       0xffff3ad1, "flutterchorus" },
            { "Swarm Ensemble",   "airwindows_ensemble", "Modulation", "Multi-voice ensemble thickener for pads, strings and guitars", 0xffff3ad1, "swarmensemble" },
            { "Phase Moth",       "phaser",          "Modulation",  "Sweeping analogue phaser - rate, depth, feedback",       0xffff3ad1, "phasemoth" },
            { "Quiver Tremolo",   "airwindows_tremolo", "Modulation", "Tube-style tremolo with organic wobble", 0xffff3ad1, "quivertremolo" },
            { "Helix Pitch",      "pitchShifter",    "Modulation",  "Real-time pitch shifter in semitones with formant-friendly algorithm",         0xffff3ad1, "helixpitch" },
            { "Double Wing ADT",  "airwindows_adt",  "Modulation",  "Automatic double tracking - instant vocal and guitar doubles", 0xffff3ad1, "doublewingadt" },
            // ---------------- Stereo & mastering utilities
            { "Wingspan",         "airwindows_wider", "Utility",    "Stereo widener with mono-safe low end - open up busses and pads", 0xffb0b0c0, "wingspan" },
            { "Mono Maker",       "airwindows_monoam", "Utility",   "Mono below a crossover - keeps bass centred and vinyl/club safe",      0xffb0b0c0, "monomaker" },
            { "Dither Dust",      "airwindows_notjustanotherdither", "Utility", "Mastering dither with noise-shaping for 24-bit and 16-bit bounces",     0xffb0b0c0, "ditherdust" },
            { "Trim Gain",        "airwindows_purestgain", "Utility", "Ultra-clean gain trim for gain staging",             0xffb0b0c0, "trimgain" },
            { "MIDI Shaper",      "midiModifier",    "Utility",     "Transpose, velocity scale and channel remap for MIDI",   0xffb0b0c0, "midishaper" },
            { "Rack Channel",     "sqmRackChannel",  "Utility",     "Channel Settings for any instrument: levels, polyphony, time, tracking, cut groups, arpeggiator, echo, layer - MIDI, in front of the instrument", 0xffffb23a, "rackchannel" } };

        static const std::vector<Entry> e = []
        {
            std::vector<Entry> out;
            for (auto& r : rows)
            {
                out.push_back ({ r.name, r.type, r.category, r.tagline, r.colour, r.art, SignaturePresets::forName (r.name) });
                if (out.back().presets.empty()) out.back().presets = SqFactory::presetsFor (r.name);   // samplers / slicers: factory content built from the kits
            }
            return out;
        }();
        return e;
    }

    inline const Entry* findByName (const juce::String& name)
    {
        auto n = name.trim();
        for (auto& e : entries()) if (juce::String (e.name).equalsIgnoreCase (n)) return &e;
        for (auto& e : entries()) if (juce::String (e.name).containsIgnoreCase (n) && n.length() >= 4) return &e;
        return nullptr;
    }

    /** The Signature entry a plugin instance corresponds to (first match on type), or nullptr. */
    inline const Entry* findForPlugin (const te::Plugin& p)
    {
        auto type = const_cast<te::Plugin&> (p).getPluginType();
        for (auto& e : entries()) if (type == e.type) return &e;
        return nullptr;
    }

    /** Brand name for any plugin: Signature name, else the engine/AirWindows/third-party name. */
    inline juce::String displayName (const te::Plugin& p)
    {
        if (auto e = findForPlugin (p))
        {
            // the Sampler is used both as "Chrysalis Sampler" and "Hive Drums": tell them apart by the kit property on the track
            if (juce::String (e->type) == "sampler" || juce::String (e->type) == "sqmHivePads")
                if (auto t = p.getOwnerTrack())
                {
                    if (t->state.getProperty ("sqmRackType").toString() == "sample") return "Sample Channel";
                    if (juce::String (e->type) == "sampler" && t->state.hasProperty ("sqmKit")) return "Hive Drums";
                }
            return e->name;
        }
        return p.getName();
    }

    inline te::AutomatableParameter* findParam (te::Plugin& p, const juce::String& nameOrID)
    {
        auto params = p.getAutomatableParameters();
        for (auto ap : params) if (ap->paramID.equalsIgnoreCase (nameOrID)) return ap;
        for (auto ap : params) if (ap->getParameterName().equalsIgnoreCase (nameOrID)) return ap;
        for (auto ap : params) if (ap->getParameterName().containsIgnoreCase (nameOrID)) return ap;
        return nullptr;
    }

    inline void applyPreset (te::Plugin& p, const Preset& preset)
    {
        // Squelchmoth's own instruments: start every preset from the defaults so nothing leaks from the last one
        SqDsp::ParamBank* bank = nullptr;
        if (auto e8 = dynamic_cast<Sq808Plugin*> (&p)) bank = &e8->bank;
        if (auto bl = dynamic_cast<SqBlossomPlugin*> (&p)) bank = &bl->bank;
        if (auto bo = dynamic_cast<SqDsp::BankOwner*> (&p)) { bank = &bo->getBank(); bo->resetProps (&p.edit.getUndoManager()); }
        if (bank != nullptr)
            for (size_t i = 0; i < bank->params.size(); ++i)
                bank->params[i]->setParameter (bank->specs[i].def, juce::sendNotification);
        for (auto& pv : preset.values)
        {
            auto match = findParam (p, pv.param);
            if (match == nullptr) continue;
            if (pv.normalised) match->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, pv.value), juce::sendNotification);
            else match->setParameter (match->getValueRange().clipValue (pv.value), juce::sendNotification);
        }

        // raw state properties (wave shapes, modes, delay length...)
        auto* um = &p.edit.getUndoManager();
        for (auto& pr : preset.props)
        {
            juce::Identifier id (pr.id);
            if (dynamic_cast<te::LowPassPlugin*> (&p) != nullptr && id == te::IDs::mode)
                p.state.setProperty (id, pr.value > 0.5f ? "highpass" : "lowpass", um);
            else if (juce::String (pr.id).endsWith ("On") || juce::String (pr.id) == "sidechainTrigger" || juce::String (pr.id) == "ampAnalog")
                p.state.setProperty (id, pr.value > 0.5f, um);
            else if (std::abs (pr.value - std::round (pr.value)) < 1.0e-5f && (juce::String (pr.id).startsWith ("waveShape") || juce::String (pr.id).startsWith ("voices")
                     || id == juce::Identifier ("polyMode") || id == juce::Identifier ("glideMode") || id == juce::Identifier ("unison") || id == juce::Identifier ("maxPoly") || id == juce::Identifier ("bendRange") || id == juce::Identifier ("quality")
                     || juce::String (pr.id).startsWith ("lfoWaveShape") || id == te::IDs::voiceMode || id == te::IDs::filterSlope || id == te::IDs::filterType || id == te::IDs::length))
                p.state.setProperty (id, (int) std::round (pr.value), um);
            else
                p.state.setProperty (id, pr.value, um);
        }

        // 1.8 synths: the mod matrix slots come straight from the preset's mods list
        if (auto sc = dynamic_cast<SqSynth::SynthCommon*> (&p))
        {
            for (int i = 0; i < SqSynth::numModSlots; ++i)
            {
                const bool have = i < (int) preset.mods.size();
                sc->modSourceValue (i).setValue (have ? preset.mods[(size_t) i].source : 0, um);
                sc->modDestValue (i).setValue (have ? juce::String (preset.mods[(size_t) i].paramID) : juce::String(), um);
                sc->modDepthValue (i).setValue (have ? preset.mods[(size_t) i].depth : 0.0f, um);
            }
        }
        // Pollen remembers which pack / sound is loaded so its pack page follows the preset menu and Claude's preset loads
        if (auto po = dynamic_cast<SqSynth::SqPollenPlugin*> (&p))
        {
            juce::String n (preset.name);
            if (n.contains (" | ")) { po->pack.setValue (n.upToFirstOccurrenceOf (" | ", false, false), um); po->presetName.setValue (n.fromFirstOccurrenceOf (" | ", false, false), um); }
        }

        // samplers and slicers: the factory sounds (kits, 808s, SoundFont banks, genre loops) load behind the preset name
        SqFactory::load (p, preset.name);

        // 4OSC modulation routings
        if (auto fo = dynamic_cast<te::FourOscPlugin*> (&p))
        {
            if (! preset.mods.empty() || ! preset.props.empty())
                for (int src = 0; src < 4; ++src)
                    for (auto ap : fo->getAutomatableParameters())
                        fo->clearModulation ((te::FourOscPlugin::ModSource) src, ap);
            for (auto& m : preset.mods)
                if (auto ap = findParam (p, m.paramID))
                    fo->setModulationDepth ((te::FourOscPlugin::ModSource) m.source, ap, m.depth);
        }
    }

    inline juce::String artKey (const te::Plugin& p)
    {
        if (auto e = findForPlugin (p))
        {
            if (displayName (p) == "Hive Drums") return "hivedrums";
            return e->art;
        }
        return {};
    }

    /** The brand name for a plug-in type ("sqmTune" -> "Moth Tune"); AirWindows types by their engine name. */
    inline juce::String displayNameForType (const juce::String& type)
    {
        for (auto& e : entries()) if (type == e.type) return e.name;
        for (auto& b : builtinPlugins) if (type == b.type) return b.name;
        return type;
    }

    /** Creates a Signature plugin by brand name and applies its first preset. */
    inline te::Plugin::Ptr create (te::Edit& edit, const Entry& e)
    {
        auto p = edit.getPluginCache().createNewPlugin (e.type, {});
        if (p != nullptr && ! e.presets.empty() && ! e.presets.front().values.empty())
            applyPreset (*p, e.presets.front());
        return p;
    }
}

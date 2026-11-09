#pragma once
// Squelchmoth Signature Bundle - factory preset banks.
//
// Every Signature plugin ships with a bank of Basic presets (the settings most people reach for) and
// Advanced presets (specialised, aggressive or creative settings). Values are either normalised (0..1,
// the AirWindows convention), natural units (Hz, dB, ms - flagged with N()), raw state properties
// (P(), for non-automatable settings such as oscillator wave shapes or a filter mode) or 4OSC modulation
// routings (M()).
//
// Parameter names must match the plugin's own parameter names (or IDs for 4OSC).

#include <JuceHeader.h>
#include <cmath>

namespace Signature
{
    struct PV   { const char* param; float value; bool normalised = true; };
    struct Prop { const char* id; float value; };
    struct Mod  { int source; const char* paramID; float depth; };   // source: 0 lfo1, 1 lfo2, 2 env1, 3 env2, 7 velocity
    struct Preset
    {
        const char* name;
        std::vector<PV> values;
        bool advanced = false;
        std::vector<Prop> props = {};
        std::vector<Mod> mods = {};
    };
}

namespace SignaturePresets
{
    using Signature::PV;
    using Signature::Preset;
    using Signature::Prop;
    using Signature::Mod;

    inline PV N (const char* p, float v)            { return { p, v, false }; }
    inline float TH (float db)                      { return std::pow (10.0f, db / 20.0f); }   // compressor threshold is stored as gain
    inline float RATIO (float r)                    { return 1.0f / r; }                        // compressor ratio is stored as 1/ratio
    inline Preset B (const char* n, std::vector<PV> v, std::vector<Prop> p = {}, std::vector<Mod> m = {}) { return { n, std::move (v), false, std::move (p), std::move (m) }; }
    inline Preset A (const char* n, std::vector<PV> v, std::vector<Prop> p = {}, std::vector<Mod> m = {}) { return { n, std::move (v), true,  std::move (p), std::move (m) }; }

    // 4OSC note-number helper: filter frequency is stored in MIDI note units (69 = 440 Hz)
    inline float NOTE (float hz)                    { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }

#include "SignaturePresets111.inl"
#include "SignaturePresets111v.inl"
#include "SignaturePresets111d.inl"
#include "SignaturePresets111e.inl"
#include "SignaturePresets111f.inl"
#include "SignaturePresets111g.inl"

    inline const std::map<juce::String, std::vector<Preset>>& bank()
    {
        static const std::map<juce::String, std::vector<Preset>> b = [] { std::map<juce::String, std::vector<Preset>> base {
        //==================================================================== Instruments
        { "Mothsynth", {
            // osc props: waveShape (1 sine 2 tri 3 saw 4 sawDown 5 square 6 noise), voices (unison)
            B ("Init Saw",        { N ("level1", 0), N ("ampAttack", 0.005f), N ("ampDecay", 0.2f), N ("ampSustain", 80), N ("ampRelease", 0.25f), N ("filterFreq", NOTE (12000)), N ("filterResonance", 10), N ("filterAmount", 0), N ("masterLevel", -6) },
                                  { { "waveShape1", 3 }, { "voices1", 1 }, { "waveShape2", 0 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 2 } }),
            B ("Sub Bass",        { N ("level1", 0), N ("tune1", -12), N ("level2", -8), N ("tune2", -24), N ("ampAttack", 0.003f), N ("ampDecay", 0.4f), N ("ampSustain", 90), N ("ampRelease", 0.15f), N ("filterFreq", NOTE (420)), N ("filterResonance", 12), N ("filterAmount", 0.35f), N ("filterAttack", 0.002f), N ("filterDecay", 0.25f), N ("filterSustain", 20), N ("masterLevel", -4) },
                                  { { "waveShape1", 1 }, { "waveShape2", 3 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 0 }, { "filterSlope", 24 } }),
            B ("Reese Bass",      { N ("level1", 0), N ("tune1", -12), N ("detune1", 0.18f), N ("spread1", 60), N ("level2", -3), N ("tune2", -12), N ("fineTune2", 12), N ("ampAttack", 0.01f), N ("ampSustain", 100), N ("ampRelease", 0.2f), N ("filterFreq", NOTE (900)), N ("filterResonance", 18), N ("masterLevel", -6) },
                                  { { "waveShape1", 3 }, { "voices1", 4 }, { "waveShape2", 3 }, { "voices2", 2 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 0 } }),
            B ("Pluck Keys",      { N ("level1", 0), N ("level2", -6), N ("tune2", 12), N ("ampAttack", 0.001f), N ("ampDecay", 0.45f), N ("ampSustain", 0), N ("ampRelease", 0.35f), N ("filterFreq", NOTE (600)), N ("filterResonance", 20), N ("filterAmount", 0.8f), N ("filterAttack", 0.001f), N ("filterDecay", 0.3f), N ("filterSustain", 0), N ("reverbMix", 0.18f), N ("masterLevel", -6) },
                                  { { "waveShape1", 3 }, { "waveShape2", 5 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "reverbOn", 1 } }),
            B ("Warm Pad",        { N ("level1", 0), N ("detune1", 0.22f), N ("spread1", 80), N ("level2", -4), N ("tune2", 12), N ("fineTune2", -8), N ("level3", -10), N ("tune3", -12), N ("ampAttack", 0.9f), N ("ampDecay", 1.0f), N ("ampSustain", 85), N ("ampRelease", 1.6f), N ("filterFreq", NOTE (1800)), N ("filterResonance", 8), N ("filterAmount", 0.25f), N ("filterAttack", 1.2f), N ("filterSustain", 60), N ("chorusMix", 0.35f), N ("reverbMix", 0.3f), N ("masterLevel", -9) },
                                  { { "waveShape1", 3 }, { "voices1", 4 }, { "waveShape2", 3 }, { "voices2", 2 }, { "waveShape3", 2 }, { "waveShape4", 0 }, { "chorusOn", 1 }, { "reverbOn", 1 } }),
            B ("Soft Lead",       { N ("level1", 0), N ("level2", -5), N ("fineTune2", 7), N ("ampAttack", 0.03f), N ("ampDecay", 0.3f), N ("ampSustain", 75), N ("ampRelease", 0.3f), N ("filterFreq", NOTE (2600)), N ("filterResonance", 15), N ("filterAmount", 0.3f), N ("legato", 60), N ("delayMix", 0.2f), N ("delayFeedback", -14), N ("masterLevel", -6) },
                                  { { "waveShape1", 3 }, { "waveShape2", 2 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 1 }, { "delayOn", 1 } }),
            B ("Square Lead",     { N ("level1", 0), N ("pulseWidth1", 0.35f), N ("level2", -9), N ("tune2", -12), N ("ampAttack", 0.005f), N ("ampSustain", 90), N ("ampRelease", 0.15f), N ("filterFreq", NOTE (5000)), N ("filterResonance", 12), N ("masterLevel", -6) },
                                  { { "waveShape1", 5 }, { "waveShape2", 5 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 1 } }),
            B ("Organ",           { N ("level1", 0), N ("level2", -3), N ("tune2", 12), N ("level3", -6), N ("tune3", 19), N ("level4", -9), N ("tune4", 24), N ("ampAttack", 0.004f), N ("ampSustain", 100), N ("ampRelease", 0.05f), N ("filterFreq", NOTE (14000)), N ("chorusMix", 0.25f), N ("masterLevel", -8) },
                                  { { "waveShape1", 1 }, { "waveShape2", 1 }, { "waveShape3", 1 }, { "waveShape4", 1 }, { "chorusOn", 1 } }),
            B ("Bell Keys",       { N ("level1", 0), N ("level2", -4), N ("tune2", 19), N ("level3", -12), N ("tune3", 31), N ("ampAttack", 0.001f), N ("ampDecay", 1.4f), N ("ampSustain", 0), N ("ampRelease", 1.2f), N ("filterFreq", NOTE (9000)), N ("reverbMix", 0.35f), N ("masterLevel", -8) },
                                  { { "waveShape1", 1 }, { "waveShape2", 1 }, { "waveShape3", 2 }, { "waveShape4", 0 }, { "reverbOn", 1 } }),
            B ("Strings",         { N ("level1", 0), N ("detune1", 0.3f), N ("spread1", 100), N ("level2", -2), N ("tune2", 12), N ("detune2", 0.25f), N ("spread2", 100), N ("ampAttack", 0.5f), N ("ampSustain", 100), N ("ampRelease", 0.9f), N ("filterFreq", NOTE (4200)), N ("filterResonance", 5), N ("chorusMix", 0.3f), N ("reverbMix", 0.25f), N ("masterLevel", -9) },
                                  { { "waveShape1", 4 }, { "voices1", 6 }, { "waveShape2", 3 }, { "voices2", 4 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "chorusOn", 1 }, { "reverbOn", 1 } }),
            A ("Wobble Bass",     { N ("level1", 0), N ("tune1", -12), N ("level2", -6), N ("tune2", -12), N ("fineTune2", 15), N ("ampAttack", 0.003f), N ("ampSustain", 100), N ("ampRelease", 0.1f), N ("filterFreq", NOTE (700)), N ("filterResonance", 45), N ("lfoRate1", 4.0f), N ("lfoDepth1", 1.0f), N ("distortion", 0.3f), N ("masterLevel", -6) },
                                  { { "waveShape1", 3 }, { "waveShape2", 5 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 0 }, { "lfoWaveShape1", 1 }, { "distortionOn", 1 }, { "filterSlope", 24 } },
                                  { { 0, "filterFreq", 0.45f } }),
            A ("Acid Line",       { N ("level1", 0), N ("ampAttack", 0.001f), N ("ampDecay", 0.25f), N ("ampSustain", 40), N ("ampRelease", 0.08f), N ("filterFreq", NOTE (300)), N ("filterResonance", 70), N ("filterAmount", 0.75f), N ("filterAttack", 0.001f), N ("filterDecay", 0.22f), N ("filterSustain", 0), N ("legato", 80), N ("distortion", 0.25f), N ("masterLevel", -8) },
                                  { { "waveShape1", 3 }, { "waveShape2", 0 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "voiceMode", 1 }, { "distortionOn", 1 }, { "filterSlope", 24 } },
                                  { { 7, "filterFreq", 0.3f } }),
            A ("Supersaw Anthem", { N ("level1", 0), N ("detune1", 0.35f), N ("spread1", 100), N ("level2", -2), N ("tune2", 12), N ("detune2", 0.3f), N ("spread2", 100), N ("level3", -6), N ("tune3", -12), N ("ampAttack", 0.02f), N ("ampSustain", 100), N ("ampRelease", 0.4f), N ("filterFreq", NOTE (6000)), N ("reverbMix", 0.3f), N ("delayMix", 0.2f), N ("delayFeedback", -12), N ("masterLevel", -10) },
                                  { { "waveShape1", 3 }, { "voices1", 7 }, { "waveShape2", 3 }, { "voices2", 7 }, { "waveShape3", 3 }, { "voices3", 3 }, { "waveShape4", 0 }, { "reverbOn", 1 }, { "delayOn", 1 } }),
            A ("Cinematic Drone", { N ("level1", 0), N ("tune1", -24), N ("detune1", 0.15f), N ("spread1", 100), N ("level2", -4), N ("tune2", -12), N ("level3", -10), N ("tune3", 7), N ("ampAttack", 3.0f), N ("ampSustain", 100), N ("ampRelease", 4.0f), N ("filterFreq", NOTE (900)), N ("filterResonance", 20), N ("lfoRate1", 0.08f), N ("lfoDepth1", 1.0f), N ("reverbMix", 0.5f), N ("reverbSize", 0.95f), N ("masterLevel", -10) },
                                  { { "waveShape1", 3 }, { "voices1", 5 }, { "waveShape2", 2 }, { "waveShape3", 1 }, { "waveShape4", 0 }, { "reverbOn", 1 }, { "lfoWaveShape1", 2 } },
                                  { { 0, "filterFreq", 0.25f } }),
            A ("Noise Riser",     { N ("level1", 0), N ("level2", -12), N ("tune2", 12), N ("detune2", 0.4f), N ("ampAttack", 2.5f), N ("ampSustain", 100), N ("ampRelease", 1.0f), N ("filterFreq", NOTE (400)), N ("filterResonance", 35), N ("filterAmount", 1.0f), N ("filterAttack", 4.0f), N ("filterSustain", 100), N ("masterLevel", -10) },
                                  { { "waveShape1", 6 }, { "waveShape2", 3 }, { "voices2", 5 }, { "waveShape3", 0 }, { "waveShape4", 0 } }),
            A ("Chiptune",        { N ("level1", 0), N ("pulseWidth1", 0.25f), N ("level2", -6), N ("tune2", 12), N ("pulseWidth2", 0.5f), N ("ampAttack", 0.001f), N ("ampDecay", 0.15f), N ("ampSustain", 70), N ("ampRelease", 0.05f), N ("filterFreq", NOTE (16000)), N ("masterLevel", -8) },
                                  { { "waveShape1", 5 }, { "waveShape2", 5 }, { "waveShape3", 0 }, { "waveShape4", 0 }, { "ampAnalog", 0 } }),
            A ("FM-ish Pluck",    { N ("level1", 0), N ("level2", -3), N ("tune2", 24), N ("fineTune2", 3), N ("level3", -9), N ("tune3", 36), N ("ampAttack", 0.001f), N ("ampDecay", 0.35f), N ("ampSustain", 0), N ("ampRelease", 0.3f), N ("filterFreq", NOTE (1200)), N ("filterAmount", 0.9f), N ("filterDecay", 0.18f), N ("filterSustain", 0), N ("delayMix", 0.25f), N ("delayFeedback", -10), N ("masterLevel", -8) },
                                  { { "waveShape1", 1 }, { "waveShape2", 1 }, { "waveShape3", 5 }, { "waveShape4", 0 }, { "delayOn", 1 } }) } },

        { "Chrysalis Sampler", {} },
        { "Hive Drums", {} },

        //==================================================================== EQ & filters
        { "Prism EQ", {
            B ("Flat",            { N ("Low-shelf freq", 100), N ("Low-shelf gain", 0), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 400), N ("Mid gain 1", 0), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 2500), N ("Mid gain 2", 0), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 8000), N ("High-shelf gain", 0), N ("High-shelf Q", 0.7f) }),
            B ("Vocal Presence",  { N ("Low-shelf freq", 110), N ("Low-shelf gain", -2), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 320), N ("Mid gain 1", -1.5f), N ("Mid Q 1", 1.2f), N ("Mid freq 2", 3200), N ("Mid gain 2", 2.5f), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 9000), N ("High-shelf gain", 2), N ("High-shelf Q", 0.7f) }),
            B ("Vocal Air",       { N ("Low-shelf freq", 90), N ("Low-shelf gain", -3), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 250), N ("Mid gain 1", -2), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 5000), N ("Mid gain 2", 1.5f), N ("Mid Q 2", 0.8f), N ("High-shelf freq", 12000), N ("High-shelf gain", 3.5f), N ("High-shelf Q", 0.6f) }),
            B ("Kick Shape",      { N ("Low-shelf freq", 60), N ("Low-shelf gain", 2.5f), N ("Low-shelf Q", 0.8f), N ("Mid freq 1", 350), N ("Mid gain 1", -3), N ("Mid Q 1", 1.4f), N ("Mid freq 2", 3500), N ("Mid gain 2", 2), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 8000), N ("High-shelf gain", -1), N ("High-shelf Q", 0.7f) }),
            B ("Snare Crack",     { N ("Low-shelf freq", 120), N ("Low-shelf gain", 1), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 200), N ("Mid gain 1", 2), N ("Mid Q 1", 1.2f), N ("Mid freq 2", 4500), N ("Mid gain 2", 3), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 9000), N ("High-shelf gain", 1.5f), N ("High-shelf Q", 0.7f) }),
            B ("Bass Guitar",     { N ("Low-shelf freq", 80), N ("Low-shelf gain", 2), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 250), N ("Mid gain 1", -2), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 900), N ("Mid gain 2", 2.5f), N ("Mid Q 2", 1.2f), N ("High-shelf freq", 5000), N ("High-shelf gain", -2), N ("High-shelf Q", 0.7f) }),
            B ("Acoustic Guitar", { N ("Low-shelf freq", 100), N ("Low-shelf gain", -3), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 220), N ("Mid gain 1", -2.5f), N ("Mid Q 1", 1.3f), N ("Mid freq 2", 2800), N ("Mid gain 2", 1.5f), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 10000), N ("High-shelf gain", 2.5f), N ("High-shelf Q", 0.7f) }),
            B ("Electric Guitar", { N ("Low-shelf freq", 120), N ("Low-shelf gain", -2), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 400), N ("Mid gain 1", -2), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 2200), N ("Mid gain 2", 2.5f), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 7000), N ("High-shelf gain", -1.5f), N ("High-shelf Q", 0.7f) }),
            B ("Piano",           { N ("Low-shelf freq", 90), N ("Low-shelf gain", 1), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 300), N ("Mid gain 1", -1.5f), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 3500), N ("Mid gain 2", 1.5f), N ("Mid Q 2", 0.9f), N ("High-shelf freq", 9000), N ("High-shelf gain", 1.5f), N ("High-shelf Q", 0.7f) }),
            B ("Mix Bus Polish",  { N ("Low-shelf freq", 60), N ("Low-shelf gain", 1), N ("Low-shelf Q", 0.6f), N ("Mid freq 1", 300), N ("Mid gain 1", -0.8f), N ("Mid Q 1", 0.8f), N ("Mid freq 2", 3000), N ("Mid gain 2", 0.8f), N ("Mid Q 2", 0.7f), N ("High-shelf freq", 11000), N ("High-shelf gain", 1.2f), N ("High-shelf Q", 0.6f) }),
            A ("De-Mud",          { N ("Low-shelf freq", 100), N ("Low-shelf gain", 0), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 260), N ("Mid gain 1", -4), N ("Mid Q 1", 1.6f), N ("Mid freq 2", 500), N ("Mid gain 2", -2), N ("Mid Q 2", 1.4f), N ("High-shelf freq", 8000), N ("High-shelf gain", 0), N ("High-shelf Q", 0.7f) }),
            A ("De-Harsh",        { N ("Low-shelf freq", 100), N ("Low-shelf gain", 0), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 2800), N ("Mid gain 1", -3), N ("Mid Q 1", 2.0f), N ("Mid freq 2", 4200), N ("Mid gain 2", -2.5f), N ("Mid Q 2", 2.2f), N ("High-shelf freq", 9000), N ("High-shelf gain", 0), N ("High-shelf Q", 0.7f) }),
            A ("Telephone",       { N ("Low-shelf freq", 400), N ("Low-shelf gain", -18), N ("Low-shelf Q", 0.9f), N ("Mid freq 1", 1200), N ("Mid gain 1", 4), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 2500), N ("Mid gain 2", 3), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 3500), N ("High-shelf gain", -18), N ("High-shelf Q", 0.9f) }),
            A ("Lo-Fi Radio",     { N ("Low-shelf freq", 180), N ("Low-shelf gain", -10), N ("Low-shelf Q", 0.8f), N ("Mid freq 1", 800), N ("Mid gain 1", 3), N ("Mid Q 1", 0.8f), N ("Mid freq 2", 2000), N ("Mid gain 2", 2), N ("Mid Q 2", 0.9f), N ("High-shelf freq", 5000), N ("High-shelf gain", -12), N ("High-shelf Q", 0.8f) }),
            A ("808 Focus",       { N ("Low-shelf freq", 45), N ("Low-shelf gain", 3), N ("Low-shelf Q", 0.8f), N ("Mid freq 1", 120), N ("Mid gain 1", -1), N ("Mid Q 1", 1.2f), N ("Mid freq 2", 2500), N ("Mid gain 2", 1.5f), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 6000), N ("High-shelf gain", -4), N ("High-shelf Q", 0.7f) }),
            A ("Hi-Hat Sheen",    { N ("Low-shelf freq", 300), N ("Low-shelf gain", -12), N ("Low-shelf Q", 0.7f), N ("Mid freq 1", 800), N ("Mid gain 1", -3), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 6000), N ("Mid gain 2", 2), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 12000), N ("High-shelf gain", 3), N ("High-shelf Q", 0.7f) }),
            A ("Master Tilt Bright", { N ("Low-shelf freq", 200), N ("Low-shelf gain", -1), N ("Low-shelf Q", 0.5f), N ("Mid freq 1", 500), N ("Mid gain 1", 0), N ("Mid Q 1", 1.0f), N ("Mid freq 2", 2000), N ("Mid gain 2", 0), N ("Mid Q 2", 1.0f), N ("High-shelf freq", 4000), N ("High-shelf gain", 1.2f), N ("High-shelf Q", 0.5f) }) } },

        { "Scalpel", {
            // Type: 0 LP .. (AirWindows Biquad2: lowpass, highpass, bandpass, notch), Freq/Q normalised
            B ("Notch Hunt",      { { "Type", 1.0f }, { "Freq", 0.5f }, { "Q", 0.75f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            B ("Low Cut 80",      { { "Type", 0.34f }, { "Freq", 0.08f }, { "Q", 0.35f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            B ("Low Cut 120",     { { "Type", 0.34f }, { "Freq", 0.12f }, { "Q", 0.35f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            B ("High Cut 10k",    { { "Type", 0.0f }, { "Freq", 0.8f }, { "Q", 0.35f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            B ("High Cut 5k",     { { "Type", 0.0f }, { "Freq", 0.62f }, { "Q", 0.35f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            B ("Band Focus Mids", { { "Type", 0.67f }, { "Freq", 0.45f }, { "Q", 0.3f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            A ("Resonance Killer 250", { { "Type", 1.0f }, { "Freq", 0.3f }, { "Q", 0.9f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            A ("Resonance Killer 2k", { { "Type", 1.0f }, { "Freq", 0.52f }, { "Q", 0.9f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            A ("Resonant Sweep",  { { "Type", 0.0f }, { "Freq", 0.4f }, { "Q", 0.85f }, { "Output", 0.8f }, { "Inv/Wet", 1.0f } }),
            A ("Telephone Band",  { { "Type", 0.67f }, { "Freq", 0.48f }, { "Q", 0.6f }, { "Output", 1.0f }, { "Inv/Wet", 1.0f } }),
            A ("Inverse (listen to what is cut)", { { "Type", 1.0f }, { "Freq", 0.5f }, { "Q", 0.7f }, { "Output", 1.0f }, { "Inv/Wet", 0.0f } }),
            A ("Half Wet Parallel", { { "Type", 0.34f }, { "Freq", 0.2f }, { "Q", 0.4f }, { "Output", 1.0f }, { "Inv/Wet", 0.75f } }) } },

        { "Lantern Tilt", {
            B ("Flat",            { { "Voicing", 0.5f }, { "Highs", 0.5f } }),
            B ("Slightly Brighter", { { "Voicing", 0.6f }, { "Highs", 0.5f } }),
            B ("Brighter",        { { "Voicing", 0.72f }, { "Highs", 0.5f } }),
            B ("Slightly Darker", { { "Voicing", 0.4f }, { "Highs", 0.5f } }),
            B ("Darker",          { { "Voicing", 0.28f }, { "Highs", 0.5f } }),
            B ("Warm Vocal",      { { "Voicing", 0.42f }, { "Highs", 0.58f } }),
            A ("Air Lift",        { { "Voicing", 0.55f }, { "Highs", 0.8f } }),
            A ("Lo-Fi Dark",      { { "Voicing", 0.15f }, { "Highs", 0.3f } }),
            A ("Crisp Hats",      { { "Voicing", 0.8f }, { "Highs", 0.7f } }),
            A ("Thick Bass",      { { "Voicing", 0.2f }, { "Highs", 0.5f } }),
            A ("Mastering Nudge Bright", { { "Voicing", 0.54f }, { "Highs", 0.52f } }),
            A ("Mastering Nudge Dark",   { { "Voicing", 0.46f }, { "Highs", 0.48f } }) } },

        { "Antenna Air", {
            B ("Subtle Air",      { { "22K tap", 0.55f }, { "15K tap", 0.55f }, { "11K tap", 0.5f }, { "filters Q", 0.0f }, { "Output Level", 1.0f } }),
            B ("Vocal Sparkle",   { { "22K tap", 0.65f }, { "15K tap", 0.6f }, { "11K tap", 0.52f }, { "filters Q", 0.2f }, { "Output Level", 1.0f } }),
            B ("Acoustic Shimmer", { { "22K tap", 0.6f }, { "15K tap", 0.65f }, { "11K tap", 0.55f }, { "filters Q", 0.1f }, { "Output Level", 1.0f } }),
            B ("Mix Bus Air",     { { "22K tap", 0.58f }, { "15K tap", 0.54f }, { "11K tap", 0.5f }, { "filters Q", 0.0f }, { "Output Level", 1.0f } }),
            B ("Cymbal Lift",     { { "22K tap", 0.7f }, { "15K tap", 0.6f }, { "11K tap", 0.5f }, { "filters Q", 0.3f }, { "Output Level", 1.0f } }),
            B ("Neutral",         { { "22K tap", 0.5f }, { "15K tap", 0.5f }, { "11K tap", 0.5f }, { "filters Q", 0.0f }, { "Output Level", 1.0f } }),
            A ("Extreme Air",     { { "22K tap", 0.9f }, { "15K tap", 0.8f }, { "11K tap", 0.6f }, { "filters Q", 0.4f }, { "Output Level", 0.9f } }),
            A ("Tame Harsh Top",  { { "22K tap", 0.5f }, { "15K tap", 0.4f }, { "11K tap", 0.38f }, { "filters Q", 0.2f }, { "Output Level", 1.0f } }),
            A ("Dull Source Rescue", { { "22K tap", 0.75f }, { "15K tap", 0.7f }, { "11K tap", 0.62f }, { "filters Q", 0.1f }, { "Output Level", 1.0f } }),
            A ("Presence 11k",    { { "22K tap", 0.5f }, { "15K tap", 0.52f }, { "11K tap", 0.68f }, { "filters Q", 0.5f }, { "Output Level", 1.0f } }),
            A ("Soften Everything", { { "22K tap", 0.35f }, { "15K tap", 0.4f }, { "11K tap", 0.45f }, { "filters Q", 0.0f }, { "Output Level", 1.0f } }),
            A ("Lo-Fi Top Roll",  { { "22K tap", 0.2f }, { "15K tap", 0.25f }, { "11K tap", 0.4f }, { "filters Q", 0.0f }, { "Output Level", 1.0f } }) } },

        { "Deep Sub", {
            B ("808 Weight",      { { "TGrind", 0.3f }, { "Grd/Out", 0.5f }, { "XOver", 0.35f }, { "BsDrive", 0.5f }, { "BsVoice", 0.6f }, { "BassOut", 0.6f }, { "SbDrive", 0.35f }, { "SbVoice", 0.3f }, { "SubOut", 0.55f } }),
            B ("Kick Thump",      { { "TGrind", 0.4f }, { "Grd/Out", 0.5f }, { "XOver", 0.45f }, { "BsDrive", 0.6f }, { "BsVoice", 0.5f }, { "BassOut", 0.6f }, { "SbDrive", 0.2f }, { "SbVoice", 0.2f }, { "SubOut", 0.4f } }),
            B ("Bass Guitar Body", { { "TGrind", 0.5f }, { "Grd/Out", 0.5f }, { "XOver", 0.5f }, { "BsDrive", 0.7f }, { "BsVoice", 0.6f }, { "BassOut", 0.55f }, { "SbDrive", 0.1f }, { "SbVoice", 0.2f }, { "SubOut", 0.3f } }),
            B ("Gentle Sub Add",  { { "TGrind", 0.2f }, { "Grd/Out", 0.5f }, { "XOver", 0.4f }, { "BsDrive", 0.4f }, { "BsVoice", 0.5f }, { "BassOut", 0.5f }, { "SbDrive", 0.25f }, { "SbVoice", 0.25f }, { "SubOut", 0.45f } }),
            B ("Neutral",         { { "TGrind", 0.0f }, { "Grd/Out", 0.5f }, { "XOver", 0.74f }, { "BsDrive", 0.0f }, { "BsVoice", 0.5f }, { "BassOut", 0.5f }, { "SbDrive", 0.0f }, { "SbVoice", 0.2f }, { "SubOut", 0.0f } }),
            A ("Trap Sub Monster", { { "TGrind", 0.5f }, { "Grd/Out", 0.55f }, { "XOver", 0.3f }, { "BsDrive", 0.6f }, { "BsVoice", 0.7f }, { "BassOut", 0.6f }, { "SbDrive", 0.7f }, { "SbVoice", 0.4f }, { "SubOut", 0.75f } }),
            A ("Dub Reggae",      { { "TGrind", 0.6f }, { "Grd/Out", 0.5f }, { "XOver", 0.4f }, { "BsDrive", 0.8f }, { "BsVoice", 0.8f }, { "BassOut", 0.65f }, { "SbDrive", 0.5f }, { "SbVoice", 0.5f }, { "SubOut", 0.6f } }),
            A ("Cinematic Boom",  { { "TGrind", 0.3f }, { "Grd/Out", 0.5f }, { "XOver", 0.25f }, { "BsDrive", 0.4f }, { "BsVoice", 0.6f }, { "BassOut", 0.5f }, { "SbDrive", 0.8f }, { "SbVoice", 0.2f }, { "SubOut", 0.8f } }),
            A ("Grind Only",      { { "TGrind", 0.8f }, { "Grd/Out", 0.7f }, { "XOver", 0.74f }, { "BsDrive", 0.0f }, { "BsVoice", 0.5f }, { "BassOut", 0.5f }, { "SbDrive", 0.0f }, { "SbVoice", 0.2f }, { "SubOut", 0.0f } }),
            A ("Thin Mix Fix",    { { "TGrind", 0.2f }, { "Grd/Out", 0.5f }, { "XOver", 0.5f }, { "BsDrive", 0.5f }, { "BsVoice", 0.45f }, { "BassOut", 0.55f }, { "SbDrive", 0.3f }, { "SbVoice", 0.3f }, { "SubOut", 0.4f } }),
            A ("Small Speakers Bass", { { "TGrind", 0.55f }, { "Grd/Out", 0.55f }, { "XOver", 0.55f }, { "BsDrive", 0.75f }, { "BsVoice", 0.75f }, { "BassOut", 0.6f }, { "SbDrive", 0.0f }, { "SbVoice", 0.2f }, { "SubOut", 0.0f } }),
            A ("Octave Down Sub", { { "TGrind", 0.0f }, { "Grd/Out", 0.5f }, { "XOver", 0.35f }, { "BsDrive", 0.2f }, { "BsVoice", 0.5f }, { "BassOut", 0.5f }, { "SbDrive", 0.9f }, { "SbVoice", 0.35f }, { "SubOut", 0.9f } }) } },

        { "Veil Filter", {
            B ("Low-pass 8 kHz",  { N ("Frequency", 8000) }, { { "mode", 0 } }),
            B ("Low-pass 2 kHz (muffled)", { N ("Frequency", 2000) }, { { "mode", 0 } }),
            B ("Low-pass 600 Hz (under water)", { N ("Frequency", 600) }, { { "mode", 0 } }),
            B ("High-pass 80 Hz", { N ("Frequency", 80) }, { { "mode", 1 } }),
            B ("High-pass 150 Hz", { N ("Frequency", 150) }, { { "mode", 1 } }),
            B ("High-pass 300 Hz (thin)", { N ("Frequency", 300) }, { { "mode", 1 } }),
            A ("Build-up Start (LP 200)", { N ("Frequency", 200) }, { { "mode", 0 } }),
            A ("Build-up End (LP 20k)", { N ("Frequency", 20000) }, { { "mode", 0 } }),
            A ("Drop Filter (HP 1k)", { N ("Frequency", 1000) }, { { "mode", 1 } }),
            A ("Radio Voice (HP 500)", { N ("Frequency", 500) }, { { "mode", 1 } }),
            A ("Sub Only (LP 120)", { N ("Frequency", 120) }, { { "mode", 0 } }),
            A ("Hats Only (HP 4k)", { N ("Frequency", 4000) }, { { "mode", 1 } }) } },

        { "Capacitor", {
            B ("Open",            { { "Lowpass", 1.0f }, { "Highpass", 0.0f } }),
            B ("Gentle Low Cut",  { { "Lowpass", 1.0f }, { "Highpass", 0.12f } }),
            B ("Gentle High Cut", { { "Lowpass", 0.8f }, { "Highpass", 0.0f } }),
            B ("Band Focus",      { { "Lowpass", 0.75f }, { "Highpass", 0.2f } }),
            B ("Warm Vocal",      { { "Lowpass", 0.85f }, { "Highpass", 0.15f } }),
            B ("Tight Bass",      { { "Lowpass", 0.5f }, { "Highpass", 0.05f } }),
            A ("DJ Low-pass Sweep Start", { { "Lowpass", 0.25f }, { "Highpass", 0.0f } }),
            A ("DJ High-pass Sweep Start", { { "Lowpass", 1.0f }, { "Highpass", 0.6f } }),
            A ("Telephone",       { { "Lowpass", 0.55f }, { "Highpass", 0.45f } }),
            A ("Muffled Next Room", { { "Lowpass", 0.3f }, { "Highpass", 0.1f } }),
            A ("Thin Lo-Fi",      { { "Lowpass", 0.7f }, { "Highpass", 0.5f } }),
            A ("Sub Rumble Only", { { "Lowpass", 0.15f }, { "Highpass", 0.0f } }) } },

        //==================================================================== Dynamics
        { "Cocoon Compressor", {
            B ("Vocal Smooth",    { N ("Threshold", TH (-18)), N ("Ratio", RATIO (3)), N ("Attack", 10), N ("Release", 120), N ("Output gain", 3) }),
            B ("Vocal Tight",     { N ("Threshold", TH (-22)), N ("Ratio", RATIO (4)), N ("Attack", 5), N ("Release", 80), N ("Output gain", 5) }),
            B ("Drum Punch",      { N ("Threshold", TH (-14)), N ("Ratio", RATIO (4)), N ("Attack", 30), N ("Release", 80), N ("Output gain", 2) }),
            B ("Drum Bus Glue",   { N ("Threshold", TH (-12)), N ("Ratio", RATIO (2)), N ("Attack", 25), N ("Release", 150), N ("Output gain", 1.5f) }),
            B ("Bass Even",       { N ("Threshold", TH (-16)), N ("Ratio", RATIO (4)), N ("Attack", 15), N ("Release", 120), N ("Output gain", 3) }),
            B ("Acoustic Guitar", { N ("Threshold", TH (-18)), N ("Ratio", RATIO (3)), N ("Attack", 20), N ("Release", 180), N ("Output gain", 2.5f) }),
            B ("Piano Gentle",    { N ("Threshold", TH (-15)), N ("Ratio", RATIO (2)), N ("Attack", 25), N ("Release", 200), N ("Output gain", 1.5f) }),
            B ("Mix Bus Gentle",  { N ("Threshold", TH (-10)), N ("Ratio", RATIO (1.5f)), N ("Attack", 30), N ("Release", 250), N ("Output gain", 1) }),
            B ("Limiter Safety",  { N ("Threshold", TH (-3)), N ("Ratio", RATIO (20)), N ("Attack", 0.5f), N ("Release", 60), N ("Output gain", 0) }),
            A ("Smashed Room Mics", { N ("Threshold", TH (-30)), N ("Ratio", RATIO (10)), N ("Attack", 1), N ("Release", 40), N ("Output gain", 10) }),
            A ("Parallel Crush",  { N ("Threshold", TH (-35)), N ("Ratio", RATIO (20)), N ("Attack", 0.5f), N ("Release", 30), N ("Output gain", 12) }),
            A ("Rap Vocal In Your Face", { N ("Threshold", TH (-24)), N ("Ratio", RATIO (6)), N ("Attack", 3), N ("Release", 60), N ("Output gain", 7) }),
            A ("Snare Snap (slow attack)", { N ("Threshold", TH (-16)), N ("Ratio", RATIO (5)), N ("Attack", 45), N ("Release", 70), N ("Output gain", 3) }),
            A ("808 Control",     { N ("Threshold", TH (-12)), N ("Ratio", RATIO (3)), N ("Attack", 40), N ("Release", 200), N ("Output gain", 2) }),
            A ("Podcast Voice",   { N ("Threshold", TH (-20)), N ("Ratio", RATIO (3.5f)), N ("Attack", 8), N ("Release", 100), N ("Output gain", 5) }),
            A ("Sidechain Duck (bus from kick)", { N ("Threshold", TH (-20)), N ("Ratio", RATIO (8)), N ("Attack", 1), N ("Release", 120), N ("Output gain", 0), N ("Sidechain gain", 6) }, { { "sidechainTrigger", 1 } }) } },

        { "Moth Press", {
            B ("Gentle",          { { "Pressure", 0.25f }, { "Speed", 0.4f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }),
            B ("Vocal Fast Squeeze",  { { "Pressure", 0.45f }, { "Speed", 0.3f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }),
            B ("Bass Hold",       { { "Pressure", 0.4f }, { "Speed", 0.6f }, { "Mewiness", 0.4f }, { "Output Gain", 1.0f } }),
            B ("Drum Room",       { { "Pressure", 0.6f }, { "Speed", 0.2f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }),
            B ("Acoustic Leveller", { { "Pressure", 0.3f }, { "Speed", 0.7f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }),
            B ("Slam",            { { "Pressure", 0.7f }, { "Speed", 0.6f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }),
            A ("All Buttons In",  { { "Pressure", 0.9f }, { "Speed", 0.15f }, { "Mewiness", 0.7f }, { "Output Gain", 1.0f } }),
            A ("Pumping Trap",    { { "Pressure", 0.75f }, { "Speed", 0.9f }, { "Mewiness", 0.3f }, { "Output Gain", 1.0f } }),
            A ("Mewl Vocal",      { { "Pressure", 0.5f }, { "Speed", 0.4f }, { "Mewiness", 0.9f }, { "Output Gain", 1.0f } }),
            A ("Click Through (no mew)", { { "Pressure", 0.5f }, { "Speed", 0.5f }, { "Mewiness", 0.0f }, { "Output Gain", 1.0f } }),
            A ("Parallel Smash",  { { "Pressure", 1.0f }, { "Speed", 0.5f }, { "Mewiness", 0.5f }, { "Output Gain", 0.85f } }),
            A ("Mastering Touch", { { "Pressure", 0.15f }, { "Speed", 0.8f }, { "Mewiness", 0.5f }, { "Output Gain", 1.0f } }) } },

        { "Pupa Buss Comp", {
            B ("Mix Glue",        { { "Compress", 0.35f }, { "Output", 1.0f } }),
            B ("Drum Bus",        { { "Compress", 0.5f }, { "Output", 1.0f } }),
            B ("Vocal Bus",       { { "Compress", 0.4f }, { "Output", 1.0f } }),
            B ("Light Touch",     { { "Compress", 0.2f }, { "Output", 1.0f } }),
            B ("Bass Bus",        { { "Compress", 0.45f }, { "Output", 1.0f } }),
            B ("Keys & Pads",     { { "Compress", 0.3f }, { "Output", 1.0f } }),
            A ("Thick Glue",      { { "Compress", 0.7f }, { "Output", 1.0f } }),
            A ("Maximum Butter",  { { "Compress", 1.0f }, { "Output", 0.9f } }),
            A ("Mastering Kiss",  { { "Compress", 0.12f }, { "Output", 1.0f } }),
            A ("Lo-Fi Beats Pump", { { "Compress", 0.8f }, { "Output", 1.0f } }),
            A ("Podcast Bus",     { { "Compress", 0.55f }, { "Output", 1.0f } }),
            A ("Strings Smooth",  { { "Compress", 0.25f }, { "Output", 1.0f } }) } },

        { "Vari-Mu Tube", {
            B ("Vintage Vocal",   { { "Intensty", 0.35f }, { "Speed", 0.5f }, { "Output", 1.0f } }),
            B ("Mix Bus Tube",    { { "Intensty", 0.25f }, { "Speed", 0.6f }, { "Output", 1.0f } }),
            B ("Bass Warmth",     { { "Intensty", 0.4f }, { "Speed", 0.4f }, { "Output", 1.0f } }),
            B ("Drum Overheads",  { { "Intensty", 0.3f }, { "Speed", 0.7f }, { "Output", 1.0f } }),
            B ("Piano Glow",      { { "Intensty", 0.3f }, { "Speed", 0.5f }, { "Output", 1.0f } }),
            B ("Light",           { { "Intensty", 0.15f }, { "Speed", 0.5f }, { "Output", 1.0f } }),
            A ("Heavy Tube Squash", { { "Intensty", 0.8f }, { "Speed", 0.3f }, { "Output", 0.9f } }),
            A ("Fast Tube Pump",  { { "Intensty", 0.6f }, { "Speed", 1.0f }, { "Output", 1.0f } }),
            A ("Slow Mastering",  { { "Intensty", 0.2f }, { "Speed", 0.1f }, { "Output", 1.0f } }),
            A ("Saturated Guitar", { { "Intensty", 0.7f }, { "Speed", 0.5f }, { "Output", 1.0f } }),
            A ("Soul Vocal",      { { "Intensty", 0.5f }, { "Speed", 0.35f }, { "Output", 1.0f } }),
            A ("Maximum Mu",      { { "Intensty", 1.0f }, { "Speed", 0.5f }, { "Output", 0.8f } }) } },

        { "Clamp Limiter", {
            B ("Master Loud",     { { "Boost", 0.12f }, { "Soften", 0.5f }, { "Enhance", 0.3f }, { "Mode", 0.0f } }),
            B ("Master Transparent", { { "Boost", 0.06f }, { "Soften", 0.7f }, { "Enhance", 0.2f }, { "Mode", 0.0f } }),
            B ("Master Streaming (-1 dB)", { { "Boost", 0.1f }, { "Soften", 0.6f }, { "Enhance", 0.3f }, { "Mode", 0.0f } }),
            B ("Drum Bus Clip",   { { "Boost", 0.2f }, { "Soften", 0.3f }, { "Enhance", 0.4f }, { "Mode", 0.0f } }),
            B ("Vocal Peak Stop", { { "Boost", 0.08f }, { "Soften", 0.8f }, { "Enhance", 0.1f }, { "Mode", 0.0f } }),
            B ("Safety Only",     { { "Boost", 0.0f }, { "Soften", 0.5f }, { "Enhance", 0.0f }, { "Mode", 0.0f } }),
            A ("Club Loud",       { { "Boost", 0.3f }, { "Soften", 0.4f }, { "Enhance", 0.5f }, { "Mode", 0.0f } }),
            A ("Gain-Matched Audition", { { "Boost", 0.2f }, { "Soften", 0.5f }, { "Enhance", 0.3f }, { "Mode", 0.5f } }),
            A ("Clip Only (hard)", { { "Boost", 0.15f }, { "Soften", 0.0f }, { "Enhance", 0.0f }, { "Mode", 1.0f } }),
            A ("808 Clipper",     { { "Boost", 0.25f }, { "Soften", 0.2f }, { "Enhance", 0.6f }, { "Mode", 0.0f } }),
            A ("Trap Master",     { { "Boost", 0.22f }, { "Soften", 0.45f }, { "Enhance", 0.45f }, { "Mode", 0.0f } }),
            A ("Soft Knee Lift",  { { "Boost", 0.1f }, { "Soften", 1.0f }, { "Enhance", 0.3f }, { "Mode", 0.0f } }) } },

        { "Veil Gate", {
            B ("Drum Tight",      { { "Thresh", 0.3f }, { "TrebSus", 0.6f }, { "BassSus", 0.4f }, { "AttackS", 0.0f } }),
            B ("Kick Clean",      { { "Thresh", 0.35f }, { "TrebSus", 0.3f }, { "BassSus", 0.7f }, { "AttackS", 0.0f } }),
            B ("Snare Clean",     { { "Thresh", 0.3f }, { "TrebSus", 0.7f }, { "BassSus", 0.4f }, { "AttackS", 0.0f } }),
            B ("Tom Tails",       { { "Thresh", 0.28f }, { "TrebSus", 0.5f }, { "BassSus", 0.8f }, { "AttackS", 0.0f } }),
            B ("Vocal Noise Floor", { { "Thresh", 0.18f }, { "TrebSus", 0.8f }, { "BassSus", 0.6f }, { "AttackS", 0.0f } }),
            B ("Guitar Hum Off",  { { "Thresh", 0.22f }, { "TrebSus", 0.7f }, { "BassSus", 0.5f }, { "AttackS", 0.0f } }),
            A ("Trance Gate",     { { "Thresh", 0.6f }, { "TrebSus", 0.2f }, { "BassSus", 0.2f }, { "AttackS", 0.0f } }),
            A ("Soft Expand",     { { "Thresh", 0.12f }, { "TrebSus", 0.9f }, { "BassSus", 0.9f }, { "AttackS", 0.3f } }),
            A ("Chopped Bass",    { { "Thresh", 0.5f }, { "TrebSus", 0.5f }, { "BassSus", 0.1f }, { "AttackS", 0.0f } }),
            A ("Bright Tail Only", { { "Thresh", 0.4f }, { "TrebSus", 1.0f }, { "BassSus", 0.0f }, { "AttackS", 0.0f } }),
            A ("Slow Swell",      { { "Thresh", 0.35f }, { "TrebSus", 0.6f }, { "BassSus", 0.6f }, { "AttackS", 0.8f } }),
            A ("Room Mic Killer", { { "Thresh", 0.45f }, { "TrebSus", 0.4f }, { "BassSus", 0.3f }, { "AttackS", 0.0f } }) } },

        { "Hush De-Ess", {
            B ("Female Vocal",    { { "Intense", 0.45f }, { "Max DS", 0.5f }, { "Freq", 0.62f } }),
            B ("Male Vocal",      { { "Intense", 0.45f }, { "Max DS", 0.5f }, { "Freq", 0.5f } }),
            B ("Gentle",          { { "Intense", 0.25f }, { "Max DS", 0.4f }, { "Freq", 0.55f } }),
            B ("Rap Vocal",       { { "Intense", 0.55f }, { "Max DS", 0.55f }, { "Freq", 0.55f } }),
            B ("Podcast",         { { "Intense", 0.4f }, { "Max DS", 0.5f }, { "Freq", 0.52f } }),
            B ("Hi-Hat Tamer",    { { "Intense", 0.5f }, { "Max DS", 0.6f }, { "Freq", 0.7f } }),
            A ("Heavy Lisp Control", { { "Intense", 0.8f }, { "Max DS", 0.8f }, { "Freq", 0.58f } }),
            A ("Cymbal Wash Control", { { "Intense", 0.6f }, { "Max DS", 0.7f }, { "Freq", 0.75f } }),
            A ("Acoustic Guitar Squeak", { { "Intense", 0.5f }, { "Max DS", 0.5f }, { "Freq", 0.65f } }),
            A ("Low Sibilance (dark mic)", { { "Intense", 0.5f }, { "Max DS", 0.5f }, { "Freq", 0.4f } }),
            A ("Broadband Harsh Tamer", { { "Intense", 0.7f }, { "Max DS", 0.6f }, { "Freq", 0.45f } }),
            A ("Barely There",    { { "Intense", 0.12f }, { "Max DS", 0.3f }, { "Freq", 0.55f } }) } },

        { "Snap Transient", {
            B ("Neutral",         { { "Input Trim", 0.5f }, { "Point", 0.5f }, { "Reaction Speed", 0.5f } }),
            B ("Snare Snap",      { { "Input Trim", 0.5f }, { "Point", 0.72f }, { "Reaction Speed", 0.4f } }),
            B ("Kick Click",      { { "Input Trim", 0.5f }, { "Point", 0.68f }, { "Reaction Speed", 0.3f } }),
            B ("Soften Drums",    { { "Input Trim", 0.5f }, { "Point", 0.3f }, { "Reaction Speed", 0.5f } }),
            B ("Pluck Attack",    { { "Input Trim", 0.5f }, { "Point", 0.65f }, { "Reaction Speed", 0.6f } }),
            B ("Bass Pick Definition", { { "Input Trim", 0.5f }, { "Point", 0.62f }, { "Reaction Speed", 0.7f } }),
            A ("Maximum Attack",  { { "Input Trim", 0.45f }, { "Point", 1.0f }, { "Reaction Speed", 0.3f } }),
            A ("Smooth Everything", { { "Input Trim", 0.55f }, { "Point", 0.0f }, { "Reaction Speed", 0.7f } }),
            A ("Room Mic Transient Pull", { { "Input Trim", 0.5f }, { "Point", 0.2f }, { "Reaction Speed", 0.4f } }),
            A ("Trap Hat Crisp",  { { "Input Trim", 0.5f }, { "Point", 0.8f }, { "Reaction Speed", 0.2f } }),
            A ("Acoustic Strum Soften", { { "Input Trim", 0.5f }, { "Point", 0.35f }, { "Reaction Speed", 0.8f } }),
            A ("Percussion Pop",  { { "Input Trim", 0.5f }, { "Point", 0.85f }, { "Reaction Speed", 0.45f } }) } },

        { "Pop Knock", {
            B ("Kick Forward",    { { "Intenst", 0.4f }, { "Output", 1.0f } }),
            B ("Snare Forward",   { { "Intenst", 0.45f }, { "Output", 1.0f } }),
            B ("Drum Bus",        { { "Intenst", 0.3f }, { "Output", 1.0f } }),
            B ("Gentle",          { { "Intenst", 0.15f }, { "Output", 1.0f } }),
            B ("Percussion",      { { "Intenst", 0.5f }, { "Output", 1.0f } }),
            B ("Bass Knock",      { { "Intenst", 0.35f }, { "Output", 1.0f } }),
            A ("Explosive",       { { "Intenst", 0.9f }, { "Output", 0.85f } }),
            A ("Trap Kick Slap",  { { "Intenst", 0.7f }, { "Output", 0.95f } }),
            A ("Parallel Knock",  { { "Intenst", 1.0f }, { "Output", 0.8f } }),
            A ("Lo-Fi Knock",     { { "Intenst", 0.6f }, { "Output", 0.9f } }),
            A ("Acoustic Kick",   { { "Intenst", 0.25f }, { "Output", 1.0f } }),
            A ("Boom Bap",        { { "Intenst", 0.55f }, { "Output", 0.95f } }) } },

        //==================================================================== Saturation & tape
        { "Ember Tape", {
            B ("Mix Bus Tape",    { { "Louder", 0.3f }, { "Softer", 0.3f }, { "Fatter", 0.3f }, { "Flutter", 0.1f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            B ("Vocal Warmth",    { { "Louder", 0.25f }, { "Softer", 0.4f }, { "Fatter", 0.25f }, { "Flutter", 0.05f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            B ("Drum Smash",      { { "Louder", 0.65f }, { "Softer", 0.45f }, { "Fatter", 0.6f }, { "Flutter", 0.15f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            B ("Bass Fat",        { { "Louder", 0.4f }, { "Softer", 0.3f }, { "Fatter", 0.7f }, { "Flutter", 0.0f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            B ("Gentle Glue",     { { "Louder", 0.15f }, { "Softer", 0.2f }, { "Fatter", 0.2f }, { "Flutter", 0.05f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            B ("Keys & Pads",     { { "Louder", 0.3f }, { "Softer", 0.5f }, { "Fatter", 0.4f }, { "Flutter", 0.3f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Old Cassette",    { { "Louder", 0.5f }, { "Softer", 0.8f }, { "Fatter", 0.5f }, { "Flutter", 0.7f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Lo-Fi Wobble",    { { "Louder", 0.4f }, { "Softer", 0.7f }, { "Fatter", 0.6f }, { "Flutter", 1.0f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Hot Tape Master", { { "Louder", 0.8f }, { "Softer", 0.3f }, { "Fatter", 0.5f }, { "Flutter", 0.05f }, { "Output", 0.45f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Parallel Tape Crush", { { "Louder", 1.0f }, { "Softer", 0.6f }, { "Fatter", 0.8f }, { "Flutter", 0.1f }, { "Output", 0.5f }, { "Dry Level", 0.6f }, { "Wet Level", 0.7f } }),
            A ("Soft Head Bump Only", { { "Louder", 0.0f }, { "Softer", 0.0f }, { "Fatter", 0.9f }, { "Flutter", 0.0f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Dark Tape Vocal", { { "Louder", 0.35f }, { "Softer", 0.9f }, { "Fatter", 0.3f }, { "Flutter", 0.1f }, { "Output", 0.5f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }) } },

        { "Scorch Drive", {
            B ("Warm Grit",       { { "Drive", 0.25f }, { "Highpass", 0.15f }, { "Out Level", 0.5f } }),
            B ("Bass Grind",      { { "Drive", 0.4f }, { "Highpass", 0.05f }, { "Out Level", 0.45f } }),
            B ("Vocal Edge",      { { "Drive", 0.2f }, { "Highpass", 0.3f }, { "Out Level", 0.5f } }),
            B ("Guitar Crunch",   { { "Drive", 0.55f }, { "Highpass", 0.25f }, { "Out Level", 0.45f } }),
            B ("Drum Dirt",       { { "Drive", 0.35f }, { "Highpass", 0.1f }, { "Out Level", 0.48f } }),
            B ("Synth Bite",      { { "Drive", 0.45f }, { "Highpass", 0.2f }, { "Out Level", 0.47f } }),
            A ("Fuzz",            { { "Drive", 0.85f }, { "Highpass", 0.3f }, { "Out Level", 0.45f } }),
            A ("Blown Speaker",   { { "Drive", 1.0f }, { "Highpass", 0.5f }, { "Out Level", 0.4f } }),
            A ("Lo-Fi Vocal Phone", { { "Drive", 0.6f }, { "Highpass", 0.7f }, { "Out Level", 0.5f } }),
            A ("808 Distortion",  { { "Drive", 0.7f }, { "Highpass", 0.0f }, { "Out Level", 0.42f } }),
            A ("Parallel Vocal Grit", { { "Drive", 0.5f }, { "Highpass", 0.4f }, { "Out Level", 0.4f }, { "Dry Level", 0.7f }, { "Wet Level", 0.5f } }),
            A ("Subtle Mastering Harmonics", { { "Drive", 0.08f }, { "Highpass", 0.0f }, { "Out Level", 0.5f } }) } },

        { "Lumen Channel", {
            B ("Unity",           { { "Input", 1.0f } }),
            B ("Hot Channel (+3)", { { "Input", 1.3f } }),
            B ("Pushed (+6)",     { { "Input", 1.6f } }),
            B ("Soft (-3)",       { { "Input", 0.7f } }),
            B ("Drums In",        { { "Input", 1.2f } }),
            B ("Vocal In",        { { "Input", 1.1f } }),
            A ("Driven Hard",     { { "Input", 2.0f } }),
            A ("Quiet Source Lift", { { "Input", 1.5f } }),
            A ("Headroom Save",   { { "Input", 0.5f } }),
            A ("Bass Channel",    { { "Input", 1.15f } }),
            A ("Keys Channel",    { { "Input", 0.9f } }),
            A ("Guitar Channel",  { { "Input", 1.25f } }) } },

        { "Lumen Buss", {
            B ("Unity",           { { "Input", 1.0f } }),
            B ("Mix Bus",         { { "Input", 1.0f } }),
            B ("Drum Bus Hot",    { { "Input", 1.2f } }),
            B ("Vocal Bus",       { { "Input", 1.05f } }),
            B ("Soft Summing",    { { "Input", 0.85f } }),
            B ("Master",          { { "Input", 1.0f } }),
            A ("Driven Summing",  { { "Input", 1.5f } }),
            A ("Gentle Summing",  { { "Input", 0.7f } }),
            A ("Parallel Bus",    { { "Input", 1.3f } }),
            A ("Stem Bus",        { { "Input", 1.1f } }),
            A ("Aux Return Bus",  { { "Input", 0.95f } }),
            A ("Loud Summing",    { { "Input", 1.8f } }) } },

        { "Ashen Desk", {
            B ("Desk Default",    { { "Overdrive", 0.27f }, { "Hi Choke", 0.18f }, { "Power Sag", 0.26f }, { "Frequency", 0.54f }, { "Output Trim", 0.84f } }),
            B ("Mix Bus Weight",  { { "Overdrive", 0.3f }, { "Hi Choke", 0.2f }, { "Power Sag", 0.3f }, { "Frequency", 0.5f }, { "Output Trim", 0.84f } }),
            B ("Vocal Desk",      { { "Overdrive", 0.2f }, { "Hi Choke", 0.25f }, { "Power Sag", 0.2f }, { "Frequency", 0.6f }, { "Output Trim", 0.84f } }),
            B ("Drum Desk",       { { "Overdrive", 0.45f }, { "Hi Choke", 0.15f }, { "Power Sag", 0.4f }, { "Frequency", 0.45f }, { "Output Trim", 0.8f } }),
            B ("Bass Desk",       { { "Overdrive", 0.35f }, { "Hi Choke", 0.3f }, { "Power Sag", 0.35f }, { "Frequency", 0.35f }, { "Output Trim", 0.82f } }),
            B ("Subtle",          { { "Overdrive", 0.12f }, { "Hi Choke", 0.1f }, { "Power Sag", 0.15f }, { "Frequency", 0.5f }, { "Output Trim", 0.86f } }),
            A ("Sagging Power Supply", { { "Overdrive", 0.4f }, { "Hi Choke", 0.2f }, { "Power Sag", 0.9f }, { "Frequency", 0.4f }, { "Output Trim", 0.84f } }),
            A ("Choked Highs Dark", { { "Overdrive", 0.3f }, { "Hi Choke", 0.8f }, { "Power Sag", 0.3f }, { "Frequency", 0.5f }, { "Output Trim", 0.84f } }),
            A ("Overdriven Desk", { { "Overdrive", 0.85f }, { "Hi Choke", 0.2f }, { "Power Sag", 0.4f }, { "Frequency", 0.5f }, { "Output Trim", 0.7f } }),
            A ("Vintage Soul",    { { "Overdrive", 0.5f }, { "Hi Choke", 0.4f }, { "Power Sag", 0.5f }, { "Frequency", 0.45f }, { "Output Trim", 0.8f } }),
            A ("Lo-Fi Beat Desk", { { "Overdrive", 0.6f }, { "Hi Choke", 0.6f }, { "Power Sag", 0.7f }, { "Frequency", 0.35f }, { "Output Trim", 0.78f } }),
            A ("Clean Desk",      { { "Overdrive", 0.0f }, { "Hi Choke", 0.0f }, { "Power Sag", 0.0f }, { "Frequency", 0.5f }, { "Output Trim", 0.84f } }) } },

        { "Furnace Density", {
            // Density 0.2 = neutral; above thickens, below thins
            B ("Light Thicken",   { { "Density", 0.3f }, { "Highpass", 0.0f }, { "Out Level", 1.0f } }),
            B ("Vocal Body",      { { "Density", 0.35f }, { "Highpass", 0.1f }, { "Out Level", 0.95f } }),
            B ("Bass Fatten",     { { "Density", 0.45f }, { "Highpass", 0.0f }, { "Out Level", 0.9f } }),
            B ("Drum Density",    { { "Density", 0.4f }, { "Highpass", 0.05f }, { "Out Level", 0.92f } }),
            B ("Mix Bus Warmth",  { { "Density", 0.26f }, { "Highpass", 0.0f }, { "Out Level", 1.0f } }),
            B ("Neutral",         { { "Density", 0.2f }, { "Highpass", 0.0f }, { "Out Level", 1.0f } }),
            A ("Thin Out (negative)", { { "Density", 0.1f }, { "Highpass", 0.0f }, { "Out Level", 1.0f } }),
            A ("Heavy Saturation", { { "Density", 0.7f }, { "Highpass", 0.1f }, { "Out Level", 0.75f } }),
            A ("Maximum Density", { { "Density", 1.0f }, { "Highpass", 0.2f }, { "Out Level", 0.6f } }),
            A ("Guitar Amp-ish",  { { "Density", 0.6f }, { "Highpass", 0.3f }, { "Out Level", 0.8f } }),
            A ("Parallel Thickness", { { "Density", 0.8f }, { "Highpass", 0.1f }, { "Out Level", 0.7f }, { "Dry Level", 0.8f }, { "Wet Level", 0.4f } }),
            A ("Clean HP Only",   { { "Density", 0.2f }, { "Highpass", 0.25f }, { "Out Level", 1.0f } }) } },

        { "Triode Glow", {
            B ("Gentle Tube",     { { "Triode", 0.2f }, { "Clas AB", 0.0f }, { "Clas B", 0.0f } }),
            B ("Vocal Glow",      { { "Triode", 0.3f }, { "Clas AB", 0.1f }, { "Clas B", 0.0f } }),
            B ("Bass Tube",       { { "Triode", 0.4f }, { "Clas AB", 0.0f }, { "Clas B", 0.0f } }),
            B ("Mix Bus Tube",    { { "Triode", 0.15f }, { "Clas AB", 0.05f }, { "Clas B", 0.0f } }),
            B ("Keys Warmth",     { { "Triode", 0.25f }, { "Clas AB", 0.0f }, { "Clas B", 0.0f } }),
            B ("Clean",           { { "Triode", 0.0f }, { "Clas AB", 0.0f }, { "Clas B", 0.0f } }),
            A ("Hot Triode",      { { "Triode", 0.8f }, { "Clas AB", 0.0f }, { "Clas B", 0.0f } }),
            A ("Class AB Crunch", { { "Triode", 0.3f }, { "Clas AB", 0.6f }, { "Clas B", 0.0f } }),
            A ("Class B Crossover Grit", { { "Triode", 0.2f }, { "Clas AB", 0.0f }, { "Clas B", 0.5f } }),
            A ("Everything Glowing", { { "Triode", 0.6f }, { "Clas AB", 0.4f }, { "Clas B", 0.2f } }),
            A ("Guitar Preamp",   { { "Triode", 0.7f }, { "Clas AB", 0.3f }, { "Clas B", 0.0f } }),
            A ("Lo-Fi Tube Radio", { { "Triode", 0.5f }, { "Clas AB", 0.2f }, { "Clas B", 0.7f } }) } },

        { "Vinyl Press", {
            B ("Clean Cut",       { { "Mid HiP", 0.2f }, { "SideHiP", 0.34f }, { "H Limit", 0.32f }, { "Gv Wear", 0.06f } }),
            B ("Club 12-inch",    { { "Mid HiP", 0.15f }, { "SideHiP", 0.45f }, { "H Limit", 0.3f }, { "Gv Wear", 0.05f } }),
            B ("Mono Bass Safe",  { { "Mid HiP", 0.1f }, { "SideHiP", 0.6f }, { "H Limit", 0.3f }, { "Gv Wear", 0.0f } }),
            B ("Soft Vinyl",      { { "Mid HiP", 0.2f }, { "SideHiP", 0.35f }, { "H Limit", 0.5f }, { "Gv Wear", 0.15f } }),
            B ("Lo-Fi Beats",     { { "Mid HiP", 0.25f }, { "SideHiP", 0.4f }, { "H Limit", 0.6f }, { "Gv Wear", 0.35f } }),
            B ("Mastering Elliptical", { { "Mid HiP", 0.12f }, { "SideHiP", 0.4f }, { "H Limit", 0.25f }, { "Gv Wear", 0.0f } }),
            A ("Worn Record",     { { "Mid HiP", 0.3f }, { "SideHiP", 0.4f }, { "H Limit", 0.7f }, { "Gv Wear", 0.8f } }),
            A ("Thrift Store 45", { { "Mid HiP", 0.35f }, { "SideHiP", 0.5f }, { "H Limit", 0.8f }, { "Gv Wear", 1.0f } }),
            A ("Dark Inner Groove", { { "Mid HiP", 0.2f }, { "SideHiP", 0.35f }, { "H Limit", 0.9f }, { "Gv Wear", 0.5f } }),
            A ("Bright Outer Groove", { { "Mid HiP", 0.18f }, { "SideHiP", 0.3f }, { "H Limit", 0.1f }, { "Gv Wear", 0.05f } }),
            A ("Thin 78 rpm",     { { "Mid HiP", 0.6f }, { "SideHiP", 0.8f }, { "H Limit", 0.75f }, { "Gv Wear", 0.6f } }),
            A ("Wide Stereo Cut", { { "Mid HiP", 0.2f }, { "SideHiP", 0.1f }, { "H Limit", 0.3f }, { "Gv Wear", 0.05f } }) } },

        { "Grit Bits", {
            B ("Clean",           { { "Rate", 1.0f }, { "Rez", 1.0f }, { "Hard", 1.0f } }),
            B ("12-bit Sampler",  { { "Rate", 0.9f }, { "Rez", 0.5f }, { "Hard", 0.5f } }),
            B ("8-bit Crunch",    { { "Rate", 0.8f }, { "Rez", 0.33f }, { "Hard", 0.7f } }),
            B ("Soft Lo-Fi",      { { "Rate", 0.7f }, { "Rez", 0.6f }, { "Hard", 0.0f } }),
            B ("Old Sampler Drums", { { "Rate", 0.6f }, { "Rez", 0.5f }, { "Hard", 0.3f } }),
            B ("Telephone Bits",  { { "Rate", 0.5f }, { "Rez", 0.45f }, { "Hard", 0.5f } }),
            A ("Destroyed",       { { "Rate", 0.25f }, { "Rez", 0.15f }, { "Hard", 1.0f } }),
            A ("Aliased Lead",    { { "Rate", 0.35f }, { "Rez", 0.9f }, { "Hard", 1.0f } }),
            A ("4-bit Noise",     { { "Rate", 0.8f }, { "Rez", 0.12f }, { "Hard", 0.8f } }),
            A ("Smooth Downsample", { { "Rate", 0.3f }, { "Rez", 1.0f }, { "Hard", 0.0f } }),
            A ("Game Console",    { { "Rate", 0.45f }, { "Rez", 0.3f }, { "Hard", 0.9f } }),
            A ("Parallel Crunch", { { "Rate", 0.5f }, { "Rez", 0.3f }, { "Hard", 0.8f }, { "Dry Level", 0.8f }, { "Wet Level", 0.4f } }) } },

        //==================================================================== Space
        { "Nebula Verb", {
            B ("Vocal Plate",     { { "Room Size", 0.55f }, { "Damping", 0.45f }, { "Wet Level", 0.6f }, { "Dry Level", 0.8f }, { "Width", 0.85f }, { "Freeze", 0.0f } }),
            B ("Tight Room",      { { "Room Size", 0.25f }, { "Damping", 0.6f }, { "Wet Level", 0.5f }, { "Dry Level", 0.85f }, { "Width", 0.7f }, { "Freeze", 0.0f } }),
            B ("Medium Hall",     { { "Room Size", 0.7f }, { "Damping", 0.4f }, { "Wet Level", 0.55f }, { "Dry Level", 0.8f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            B ("Big Hall",        { { "Room Size", 0.9f }, { "Damping", 0.3f }, { "Wet Level", 0.65f }, { "Dry Level", 0.78f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            B ("Drum Room",       { { "Room Size", 0.35f }, { "Damping", 0.5f }, { "Wet Level", 0.45f }, { "Dry Level", 0.9f }, { "Width", 0.9f }, { "Freeze", 0.0f } }),
            B ("Send (100% wet) Plate", { { "Room Size", 0.55f }, { "Damping", 0.45f }, { "Wet Level", 1.0f }, { "Dry Level", 0.0f }, { "Width", 0.9f }, { "Freeze", 0.0f } }),
            B ("Send (100% wet) Hall", { { "Room Size", 0.85f }, { "Damping", 0.35f }, { "Wet Level", 1.0f }, { "Dry Level", 0.0f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            B ("Ambience",        { { "Room Size", 0.2f }, { "Damping", 0.7f }, { "Wet Level", 0.3f }, { "Dry Level", 0.95f }, { "Width", 0.6f }, { "Freeze", 0.0f } }),
            A ("Cathedral",       { { "Room Size", 1.0f }, { "Damping", 0.15f }, { "Wet Level", 0.7f }, { "Dry Level", 0.7f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            A ("Dark Chamber",    { { "Room Size", 0.6f }, { "Damping", 0.9f }, { "Wet Level", 0.55f }, { "Dry Level", 0.8f }, { "Width", 0.8f }, { "Freeze", 0.0f } }),
            A ("Bright Shimmer Pad", { { "Room Size", 0.95f }, { "Damping", 0.05f }, { "Wet Level", 0.8f }, { "Dry Level", 0.5f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            A ("Mono Vintage",    { { "Room Size", 0.5f }, { "Damping", 0.6f }, { "Wet Level", 0.5f }, { "Dry Level", 0.8f }, { "Width", 0.0f }, { "Freeze", 0.0f } }),
            A ("Infinite Freeze", { { "Room Size", 0.9f }, { "Damping", 0.2f }, { "Wet Level", 0.9f }, { "Dry Level", 0.3f }, { "Width", 1.0f }, { "Freeze", 1.0f } }),
            A ("Snare Splash",    { { "Room Size", 0.45f }, { "Damping", 0.3f }, { "Wet Level", 0.6f }, { "Dry Level", 0.85f }, { "Width", 1.0f }, { "Freeze", 0.0f } }),
            A ("Lo-Fi Spring-ish", { { "Room Size", 0.3f }, { "Damping", 0.85f }, { "Wet Level", 0.55f }, { "Dry Level", 0.8f }, { "Width", 0.4f }, { "Freeze", 0.0f } }) } },

        { "Cavern IR", {
            B ("Natural (loaded IR)", { N ("Mix", 0.35f), N ("Gain", 0), N ("Low Cut", 80), N ("High Cut", 16000), N ("Filter Q", 0.7f) }),
            B ("Send (100% wet)", { N ("Mix", 1.0f), N ("Gain", 0), N ("Low Cut", 100), N ("High Cut", 14000), N ("Filter Q", 0.7f) }),
            B ("Vocal",           { N ("Mix", 0.25f), N ("Gain", 0), N ("Low Cut", 150), N ("High Cut", 10000), N ("Filter Q", 0.7f) }),
            B ("Drums",           { N ("Mix", 0.3f), N ("Gain", 0), N ("Low Cut", 120), N ("High Cut", 12000), N ("Filter Q", 0.7f) }),
            B ("Guitar Cab-ish",  { N ("Mix", 1.0f), N ("Gain", 0), N ("Low Cut", 60), N ("High Cut", 8000), N ("Filter Q", 0.7f) }),
            B ("Subtle Space",    { N ("Mix", 0.15f), N ("Gain", 0), N ("Low Cut", 200), N ("High Cut", 9000), N ("Filter Q", 0.7f) }),
            A ("Dark Cave",       { N ("Mix", 0.5f), N ("Gain", 0), N ("Low Cut", 60), N ("High Cut", 3500), N ("Filter Q", 0.9f) }),
            A ("Bright Tiled",    { N ("Mix", 0.4f), N ("Gain", 0), N ("Low Cut", 300), N ("High Cut", 18000), N ("Filter Q", 0.7f) }),
            A ("Telephone Space", { N ("Mix", 0.6f), N ("Gain", 0), N ("Low Cut", 500), N ("High Cut", 3000), N ("Filter Q", 1.2f) }),
            A ("Low Rumble",      { N ("Mix", 0.5f), N ("Gain", 2), N ("Low Cut", 20), N ("High Cut", 800), N ("Filter Q", 0.7f) }),
            A ("Loud IR (+6)",    { N ("Mix", 0.5f), N ("Gain", 6), N ("Low Cut", 80), N ("High Cut", 16000), N ("Filter Q", 0.7f) }),
            A ("Resonant Filtered", { N ("Mix", 0.5f), N ("Gain", 0), N ("Low Cut", 400), N ("High Cut", 4000), N ("Filter Q", 4.0f) }) } },

        { "Pocket Spaces", {
            // Type: 0 Chamber, 0.2 Spring, 0.4 Tiled, 0.6 Room, 0.8 Stretch, 1 Zarathustra (approx.)
            B ("Chamber",         { { "Type", 0.0f }, { "Size", 0.5f }, { "Gating", 0.0f } }),
            B ("Spring",          { { "Type", 0.2f }, { "Size", 0.5f }, { "Gating", 0.0f } }),
            B ("Tiled Bathroom",  { { "Type", 0.4f }, { "Size", 0.4f }, { "Gating", 0.0f } }),
            B ("Room",            { { "Type", 0.6f }, { "Size", 0.45f }, { "Gating", 0.0f } }),
            B ("Stretch",         { { "Type", 0.8f }, { "Size", 0.6f }, { "Gating", 0.0f } }),
            B ("Small Chamber",   { { "Type", 0.0f }, { "Size", 0.2f }, { "Gating", 0.0f } }),
            A ("Gated 80s Snare", { { "Type", 0.6f }, { "Size", 0.7f }, { "Gating", 0.7f } }),
            A ("Big Spring Surf", { { "Type", 0.2f }, { "Size", 0.9f }, { "Gating", 0.0f } }),
            A ("Huge Stretch",    { { "Type", 0.8f }, { "Size", 1.0f }, { "Gating", 0.0f } }),
            A ("Zarathustra",     { { "Type", 1.0f }, { "Size", 0.7f }, { "Gating", 0.0f } }),
            A ("Tight Gated Room", { { "Type", 0.6f }, { "Size", 0.3f }, { "Gating", 0.9f } }),
            A ("Tiny Tiled",      { { "Type", 0.4f }, { "Size", 0.1f }, { "Gating", 0.0f } }) } },

        { "Starfield", {
            B ("Shimmer Pad",     { { "Sustain", 1.0f }, { "Grain", 0.7f } }),
            B ("Vocal Halo",      { { "Sustain", 0.8f }, { "Grain", 0.6f } }),
            B ("Ambient Wash",    { { "Sustain", 1.0f }, { "Grain", 0.9f } }),
            B ("Short Sparkle",   { { "Sustain", 0.4f }, { "Grain", 0.5f } }),
            B ("Guitar Dream",    { { "Sustain", 0.7f }, { "Grain", 0.65f } }),
            B ("Medium Halo",     { { "Sustain", 0.6f }, { "Grain", 0.6f } }),
            A ("Infinite Drone",  { { "Sustain", 1.0f }, { "Grain", 1.0f } }),
            A ("Grainy Cloud",    { { "Sustain", 0.9f }, { "Grain", 0.3f } }),
            A ("Pitchy Sparkle",  { { "Sustain", 0.5f }, { "Grain", 0.95f } }),
            A ("Send Shimmer (wet)", { { "Sustain", 0.9f }, { "Grain", 0.7f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }),
            A ("Subtle Dust",     { { "Sustain", 0.3f }, { "Grain", 0.4f } }),
            A ("Sci-Fi Tail",     { { "Sustain", 0.85f }, { "Grain", 0.2f } }) } },

        { "Dust Delay", {
            // lengthMs is a property (not automatable): set per preset; Feedback is dB (-30..0), Mix 0..1
            B ("Slapback 110 ms", { N ("Feedback", -24), N ("Mix proportion", 0.3f) }, { { "length", 110 } }),
            B ("1/8 at 120 BPM (250 ms)", { N ("Feedback", -10), N ("Mix proportion", 0.3f) }, { { "length", 250 } }),
            B ("1/4 at 120 BPM (500 ms)", { N ("Feedback", -8), N ("Mix proportion", 0.28f) }, { { "length", 500 } }),
            B ("Dotted 1/8 at 120 (375 ms)", { N ("Feedback", -9), N ("Mix proportion", 0.3f) }, { { "length", 375 } }),
            B ("1/8 at 90 BPM (333 ms)", { N ("Feedback", -10), N ("Mix proportion", 0.3f) }, { { "length", 333 } }),
            B ("1/4 at 90 BPM (667 ms)", { N ("Feedback", -8), N ("Mix proportion", 0.28f) }, { { "length", 667 } }),
            B ("1/8 at 140 BPM (214 ms)", { N ("Feedback", -10), N ("Mix proportion", 0.3f) }, { { "length", 214 } }),
            B ("1/4 at 140 BPM (429 ms)", { N ("Feedback", -8), N ("Mix proportion", 0.28f) }, { { "length", 429 } }),
            B ("Gentle Echo",     { N ("Feedback", -14), N ("Mix proportion", 0.22f) }, { { "length", 380 } }),
            A ("Dub Trails",      { N ("Feedback", -2), N ("Mix proportion", 0.4f) }, { { "length", 450 } }),
            A ("Send (100% wet) 1/4", { N ("Feedback", -8), N ("Mix proportion", 1.0f) }, { { "length", 500 } }),
            A ("Doubler 25 ms",   { N ("Feedback", -30), N ("Mix proportion", 0.45f) }, { { "length", 25 } }),
            A ("Haas Width 15 ms", { N ("Feedback", -30), N ("Mix proportion", 0.5f) }, { { "length", 15 } }),
            A ("Long Ambient 1.2 s", { N ("Feedback", -5), N ("Mix proportion", 0.35f) }, { { "length", 1200 } }),
            A ("Runaway Feedback", { N ("Feedback", 0), N ("Mix proportion", 0.5f) }, { { "length", 300 } }) } },

        { "Echo Wing", {
            B ("Tape Slap",       { { "Dry", 1.0f }, { "Wet", 0.35f }, { "Delay", 0.12f }, { "Feedbak", 0.1f }, { "Lean/Ft", 0.6f }, { "Depth", 0.05f } }),
            B ("Vintage Echo",    { { "Dry", 1.0f }, { "Wet", 0.4f }, { "Delay", 0.35f }, { "Feedbak", 0.4f }, { "Lean/Ft", 0.5f }, { "Depth", 0.1f } }),
            B ("Dub Echo",        { { "Dry", 1.0f }, { "Wet", 0.5f }, { "Delay", 0.45f }, { "Feedbak", 0.7f }, { "Lean/Ft", 0.4f }, { "Depth", 0.15f } }),
            B ("Vocal Throw",     { { "Dry", 1.0f }, { "Wet", 0.45f }, { "Delay", 0.4f }, { "Feedbak", 0.3f }, { "Lean/Ft", 0.55f }, { "Depth", 0.08f } }),
            B ("Guitar Rockabilly", { { "Dry", 1.0f }, { "Wet", 0.4f }, { "Delay", 0.1f }, { "Feedbak", 0.15f }, { "Lean/Ft", 0.7f }, { "Depth", 0.05f } }),
            B ("Send (wet only)", { { "Dry", 0.0f }, { "Wet", 1.0f }, { "Delay", 0.35f }, { "Feedbak", 0.4f }, { "Lean/Ft", 0.5f }, { "Depth", 0.1f } }),
            A ("Worn Tape Wobble", { { "Dry", 1.0f }, { "Wet", 0.45f }, { "Delay", 0.4f }, { "Feedbak", 0.5f }, { "Lean/Ft", 0.3f }, { "Depth", 0.6f } }),
            A ("Self-Oscillate",  { { "Dry", 1.0f }, { "Wet", 0.5f }, { "Delay", 0.3f }, { "Feedbak", 1.0f }, { "Lean/Ft", 0.5f }, { "Depth", 0.2f } }),
            A ("Dark Dub Cave",   { { "Dry", 1.0f }, { "Wet", 0.6f }, { "Delay", 0.6f }, { "Feedbak", 0.8f }, { "Lean/Ft", 0.1f }, { "Depth", 0.25f } }),
            A ("Chorus-ish Short", { { "Dry", 1.0f }, { "Wet", 0.5f }, { "Delay", 0.03f }, { "Feedbak", 0.0f }, { "Lean/Ft", 0.5f }, { "Depth", 0.8f } }),
            A ("Bright Slap",     { { "Dry", 1.0f }, { "Wet", 0.35f }, { "Delay", 0.15f }, { "Feedbak", 0.1f }, { "Lean/Ft", 1.0f }, { "Depth", 0.0f } }),
            A ("Long Tape Loop",  { { "Dry", 1.0f }, { "Wet", 0.4f }, { "Delay", 0.9f }, { "Feedbak", 0.6f }, { "Lean/Ft", 0.4f }, { "Depth", 0.3f } }) } },

        //==================================================================== Modulation
        { "Flutter Chorus", {
            // properties of the chorus plugin: depthMs, speedHz, width, mixProportion
            B ("Classic Chorus",  {}, { { "depthMs", 3.0f }, { "speedHz", 1.0f }, { "width", 0.5f }, { "mixProportion", 0.5f } }),
            B ("Subtle Vocal",    {}, { { "depthMs", 1.5f }, { "speedHz", 0.6f }, { "width", 0.4f }, { "mixProportion", 0.3f } }),
            B ("Wide Guitar",     {}, { { "depthMs", 4.0f }, { "speedHz", 0.8f }, { "width", 1.0f }, { "mixProportion", 0.5f } }),
            B ("80s Synth",       {}, { { "depthMs", 5.0f }, { "speedHz", 0.5f }, { "width", 0.9f }, { "mixProportion", 0.55f } }),
            B ("Pad Thickener",   {}, { { "depthMs", 6.0f }, { "speedHz", 0.3f }, { "width", 1.0f }, { "mixProportion", 0.5f } }),
            B ("Bass Light",      {}, { { "depthMs", 1.0f }, { "speedHz", 0.4f }, { "width", 0.2f }, { "mixProportion", 0.25f } }),
            A ("Seasick",         {}, { { "depthMs", 12.0f }, { "speedHz", 2.5f }, { "width", 1.0f }, { "mixProportion", 0.6f } }),
            A ("Vibrato (wet)",   {}, { { "depthMs", 4.0f }, { "speedHz", 5.5f }, { "width", 0.0f }, { "mixProportion", 1.0f } }),
            A ("Slow Drift",      {}, { { "depthMs", 8.0f }, { "speedHz", 0.1f }, { "width", 1.0f }, { "mixProportion", 0.5f } }),
            A ("Leslie-ish",      {}, { { "depthMs", 3.0f }, { "speedHz", 6.5f }, { "width", 1.0f }, { "mixProportion", 0.5f } }),
            A ("Doubler",         {}, { { "depthMs", 10.0f }, { "speedHz", 0.15f }, { "width", 0.8f }, { "mixProportion", 0.5f } }),
            A ("Lo-Fi Warble",    {}, { { "depthMs", 7.0f }, { "speedHz", 1.8f }, { "width", 0.3f }, { "mixProportion", 0.7f } }) } },

        { "Swarm Ensemble", {
            B ("Strings",         { { "Ensemble", 0.5f }, { "Fullness", 0.3f }, { "Brighten", 1.0f } }),
            B ("Pad Swarm",       { { "Ensemble", 0.7f }, { "Fullness", 0.5f }, { "Brighten", 0.9f } }),
            B ("Guitar Shimmer",  { { "Ensemble", 0.4f }, { "Fullness", 0.2f }, { "Brighten", 1.0f } }),
            B ("Vocal Stack",     { { "Ensemble", 0.35f }, { "Fullness", 0.4f }, { "Brighten", 0.8f } }),
            B ("Subtle",          { { "Ensemble", 0.2f }, { "Fullness", 0.1f }, { "Brighten", 1.0f } }),
            B ("Keys Width",      { { "Ensemble", 0.45f }, { "Fullness", 0.3f }, { "Brighten", 0.9f } }),
            A ("Maximum Swarm",   { { "Ensemble", 1.0f }, { "Fullness", 1.0f }, { "Brighten", 1.0f } }),
            A ("Dark Ensemble",   { { "Ensemble", 0.6f }, { "Fullness", 0.6f }, { "Brighten", 0.2f } }),
            A ("Thin Chorus",     { { "Ensemble", 0.5f }, { "Fullness", 0.0f }, { "Brighten", 1.0f } }),
            A ("Orchestral Full", { { "Ensemble", 0.8f }, { "Fullness", 0.8f }, { "Brighten", 0.7f } }),
            A ("Lo-Fi Warm",      { { "Ensemble", 0.5f }, { "Fullness", 0.5f }, { "Brighten", 0.0f } }),
            A ("Send Ensemble (wet)", { { "Ensemble", 0.6f }, { "Fullness", 0.5f }, { "Brighten", 1.0f }, { "Dry Level", 0.0f }, { "Wet Level", 1.0f } }) } },

        { "Phase Moth", {
            // properties: depth, rate, feedbackGain
            B ("Classic Phaser",  {}, { { "depth", 5.0f }, { "rate", 0.4f }, { "feedback", 0.7f } }),
            B ("Slow Sweep",      {}, { { "depth", 6.0f }, { "rate", 0.12f }, { "feedback", 0.6f } }),
            B ("Fast Swirl",      {}, { { "depth", 4.0f }, { "rate", 2.0f }, { "feedback", 0.5f } }),
            B ("Guitar Funk",     {}, { { "depth", 5.0f }, { "rate", 0.8f }, { "feedback", 0.75f } }),
            B ("Rhodes Vibe",     {}, { { "depth", 3.5f }, { "rate", 0.5f }, { "feedback", 0.4f } }),
            B ("Subtle Motion",   {}, { { "depth", 2.0f }, { "rate", 0.25f }, { "feedback", 0.3f } }),
            A ("Jet Plane",       {}, { { "depth", 8.0f }, { "rate", 0.08f }, { "feedback", 0.95f } }),
            A ("Resonant Scream", {}, { { "depth", 6.0f }, { "rate", 0.6f }, { "feedback", 0.98f } }),
            A ("Psychedelic Fast", {}, { { "depth", 7.0f }, { "rate", 4.0f }, { "feedback", 0.8f } }),
            A ("Deep Slow Pad",   {}, { { "depth", 9.0f }, { "rate", 0.05f }, { "feedback", 0.5f } }),
            A ("Drum Phase",      {}, { { "depth", 4.0f }, { "rate", 1.2f }, { "feedback", 0.6f } }),
            A ("No Feedback Soft", {}, { { "depth", 5.0f }, { "rate", 0.3f }, { "feedback", 0.0f } }) } },

        { "Quiver Tremolo", {
            B ("Classic Amp",     { { "Speed", 0.45f }, { "Depth", 0.6f } }),
            B ("Slow Pulse",      { { "Speed", 0.2f }, { "Depth", 0.7f } }),
            B ("Fast Shiver",     { { "Speed", 0.8f }, { "Depth", 0.5f } }),
            B ("Subtle Movement", { { "Speed", 0.4f }, { "Depth", 0.25f } }),
            B ("Rhodes Trem",     { { "Speed", 0.5f }, { "Depth", 0.5f } }),
            B ("Surf",            { { "Speed", 0.6f }, { "Depth", 0.8f } }),
            A ("Helicopter",      { { "Speed", 1.0f }, { "Depth", 1.0f } }),
            A ("Deep Chop",       { { "Speed", 0.55f }, { "Depth", 1.0f } }),
            A ("Slow Swell",      { { "Speed", 0.05f }, { "Depth", 0.9f } }),
            A ("Vocal Shimmer",   { { "Speed", 0.7f }, { "Depth", 0.3f } }),
            A ("Pad Breathing",   { { "Speed", 0.12f }, { "Depth", 0.6f } }),
            A ("Stutter",         { { "Speed", 0.95f }, { "Depth", 0.85f } }) } },

        { "Helix Pitch", {
            B ("Unity",           { N ("Semitones", 0) }),
            B ("Octave Down",     { N ("Semitones", -12) }),
            B ("Octave Up",       { N ("Semitones", 12) }),
            B ("Fifth Up",        { N ("Semitones", 7) }),
            B ("Fourth Down",     { N ("Semitones", -5) }),
            B ("Minor Third Up",  { N ("Semitones", 3) }),
            B ("Major Third Up",  { N ("Semitones", 4) }),
            B ("Whole Step Down", { N ("Semitones", -2) }),
            A ("Chipmunk (+9)",   { N ("Semitones", 9) }),
            A ("Monster (-9)",    { N ("Semitones", -9) }),
            A ("Two Octaves Down", { N ("Semitones", -24) }),
            A ("Two Octaves Up",  { N ("Semitones", 24) }),
            A ("Detune Thick (+0.15)", { N ("Semitones", 0.15f) }),
            A ("Detune Thick (-0.15)", { N ("Semitones", -0.15f) }) } },

        { "Double Wing ADT", {
            B ("Classic Double",  { { "Headrm", 0.5f }, { "A Delay", 0.45f }, { "A Level", 0.5f }, { "B Delay", 0.55f }, { "B Level", 0.5f }, { "Output", 0.5f } }),
            B ("Subtle Thicken",  { { "Headrm", 0.5f }, { "A Delay", 0.48f }, { "A Level", 0.35f }, { "B Delay", 0.52f }, { "B Level", 0.35f }, { "Output", 0.5f } }),
            B ("Wide Vocal",      { { "Headrm", 0.5f }, { "A Delay", 0.4f }, { "A Level", 0.5f }, { "B Delay", 0.6f }, { "B Level", 0.5f }, { "Output", 0.5f } }),
            B ("Guitar Double",   { { "Headrm", 0.5f }, { "A Delay", 0.42f }, { "A Level", 0.5f }, { "B Delay", 0.58f }, { "B Level", 0.45f }, { "Output", 0.5f } }),
            B ("Short Flange-ish", { { "Headrm", 0.5f }, { "A Delay", 0.49f }, { "A Level", 0.5f }, { "B Delay", 0.51f }, { "B Level", 0.5f }, { "Output", 0.5f } }),
            B ("Backing Vocal Spread", { { "Headrm", 0.5f }, { "A Delay", 0.38f }, { "A Level", 0.5f }, { "B Delay", 0.62f }, { "B Level", 0.5f }, { "Output", 0.5f } }),
            A ("Maximum Width",   { { "Headrm", 0.5f }, { "A Delay", 0.0f }, { "A Level", 0.5f }, { "B Delay", 1.0f }, { "B Level", 0.5f }, { "Output", 0.5f } }),
            A ("One Side Only",   { { "Headrm", 0.5f }, { "A Delay", 0.45f }, { "A Level", 0.5f }, { "B Delay", 0.5f }, { "B Level", 0.0f }, { "Output", 0.5f } }),
            A ("Hot Headroom Push", { { "Headrm", 0.9f }, { "A Delay", 0.45f }, { "A Level", 0.6f }, { "B Delay", 0.55f }, { "B Level", 0.6f }, { "Output", 0.45f } }),
            A ("Lo-Fi Triple",    { { "Headrm", 0.4f }, { "A Delay", 0.3f }, { "A Level", 0.7f }, { "B Delay", 0.7f }, { "B Level", 0.7f }, { "Output", 0.45f } }),
            A ("Tight Stack",     { { "Headrm", 0.5f }, { "A Delay", 0.47f }, { "A Level", 0.6f }, { "B Delay", 0.53f }, { "B Level", 0.6f }, { "Output", 0.5f } }),
            A ("Slapback Pair",   { { "Headrm", 0.5f }, { "A Delay", 0.2f }, { "A Level", 0.4f }, { "B Delay", 0.8f }, { "B Level", 0.4f }, { "Output", 0.5f } }) } },

        //==================================================================== Utility
        { "Wingspan", {
            B ("Neutral",         { { "Width", 0.5f }, { "Center", 0.5f } }),
            B ("Slightly Wider",  { { "Width", 0.6f }, { "Center", 0.5f } }),
            B ("Wide Pads",       { { "Width", 0.75f }, { "Center", 0.5f } }),
            B ("Narrow Bass",     { { "Width", 0.3f }, { "Center", 0.5f } }),
            B ("Mono",            { { "Width", 0.0f }, { "Center", 0.5f } }),
            B ("Vocal Centre Focus", { { "Width", 0.5f }, { "Center", 0.6f } }),
            A ("Super Wide",      { { "Width", 1.0f }, { "Center", 0.5f } }),
            A ("Wide + Less Centre", { { "Width", 0.8f }, { "Center", 0.4f } }),
            A ("Sides Only",      { { "Width", 1.0f }, { "Center", 0.0f } }),
            A ("Centre Only",     { { "Width", 0.0f }, { "Center", 1.0f } }),
            A ("Drum Bus Air Width", { { "Width", 0.65f }, { "Center", 0.55f } }),
            A ("Mastering Touch", { { "Width", 0.55f }, { "Center", 0.5f } }) } },

        { "Mono Maker", {
            B ("Off",             { { "Monotor", 0.0f } }),
            B ("Mono Below 60 Hz", { { "Monotor", 0.15f } }),
            B ("Mono Below 100 Hz", { { "Monotor", 0.25f } }),
            B ("Mono Below 150 Hz", { { "Monotor", 0.35f } }),
            B ("Club Safe (120 Hz)", { { "Monotor", 0.3f } }),
            B ("Vinyl Safe (200 Hz)", { { "Monotor", 0.45f } }),
            A ("Mono Below 300 Hz", { { "Monotor", 0.6f } }),
            A ("Mono Below 500 Hz", { { "Monotor", 0.75f } }),
            A ("Almost Mono",     { { "Monotor", 0.95f } }),
            A ("Full Mono",       { { "Monotor", 1.0f } }),
            A ("Sub Only Mono (40 Hz)", { { "Monotor", 0.08f } }),
            A ("Bass Bus Mono (90 Hz)", { { "Monotor", 0.22f } }) } },

        { "Dither Dust", {
            B ("24-bit Master",   {}),
            B ("16-bit CD Master", {}),
            B ("Streaming Master", {}),
            B ("Stems",           {}),
            A ("Quiet Classical", {}),
            A ("Loud Modern",     {}) } },

        { "Trim Gain", {
            // Gain 0.5 = 0 dB (+-40 dB range)
            B ("Unity",           { { "Gain", 0.5f }, { "Slow Fade", 1.0f } }),
            B ("+3 dB",           { { "Gain", 0.5375f }, { "Slow Fade", 1.0f } }),
            B ("+6 dB",           { { "Gain", 0.575f }, { "Slow Fade", 1.0f } }),
            B ("-3 dB",           { { "Gain", 0.4625f }, { "Slow Fade", 1.0f } }),
            B ("-6 dB",           { { "Gain", 0.425f }, { "Slow Fade", 1.0f } }),
            B ("-12 dB",          { { "Gain", 0.35f }, { "Slow Fade", 1.0f } }),
            A ("+12 dB",          { { "Gain", 0.65f }, { "Slow Fade", 1.0f } }),
            A ("+20 dB (quiet source)", { { "Gain", 0.75f }, { "Slow Fade", 1.0f } }),
            A ("-20 dB",          { { "Gain", 0.25f }, { "Slow Fade", 1.0f } }),
            A ("Gain Stage -18 dBFS Target", { { "Gain", 0.45f }, { "Slow Fade", 1.0f } }),
            A ("Fast Automation", { { "Gain", 0.5f }, { "Slow Fade", 0.0f } }),
            A ("Mute (-40 dB)",   { { "Gain", 0.0f }, { "Slow Fade", 1.0f } }) } },

        //==================================================================== Pitch & Vocal
        // Moth Tune props: engine (0 Live, 1 Studio), character (0 Modern, 1 Classic), scale, formantPreserve, harmMode1-4
        // (0 off, 1 interval in semitones, 2 scale steps, 3 MIDI notes), midiMode (0 off, 1 this track, 2 named track).
        { "Moth Tune", {
            B ("Natural Pop",          { N ("Retune Speed", 25), N ("Humanize", 30), N ("Flex Tune", 10), N ("Vibrato Keep", 100) }),
            B ("Gentle Ballad",        { N ("Retune Speed", 60), N ("Humanize", 50), N ("Flex Tune", 35), N ("Vibrato Keep", 100), N ("Natural Vibrato", 10) }),
            B ("R&B Smooth",           { N ("Retune Speed", 15), N ("Humanize", 25), N ("Flex Tune", 5), N ("Vibrato Keep", 85), N ("Natural Vibrato", 15) }),
            B ("Hard Tune",            { N ("Retune Speed", 0), N ("Humanize", 0), N ("Flex Tune", 0), N ("Vibrato Keep", 0) }, { { "character", 1 } }),
            B ("Trap Lead",            { N ("Retune Speed", 0), N ("Humanize", 0), N ("Vibrato Keep", 0), N ("Formant", 0.5f) }, { { "character", 1 } }),
            B ("Rap Ad-libs",          { N ("Retune Speed", 5), N ("Humanize", 10), N ("Vibrato Keep", 30) }, { { "character", 1 } }),
            B ("Live Stage (low latency)", { N ("Retune Speed", 20), N ("Humanize", 20), N ("Vibrato Keep", 100), N ("Flex Tune", 10) }, { { "engine", 0 } }),
            B ("Tight Doubles",        { N ("Retune Speed", 15), N ("Vibrato Keep", 100), N ("Voice 1 Interval", 0), N ("Voice 1 Level", -4), N ("Voice 1 Pan", -75),
                                         N ("Voice 2 Interval", 0), N ("Voice 2 Level", -4), N ("Voice 2 Pan", 75), N ("Harmony Detune", 9), N ("Harmony Timing", 22) },
                                       { { "harmMode1", 1 }, { "harmMode2", 1 } }),
            A ("Robot",                { N ("Retune Speed", 0), N ("Humanize", 0), N ("Vibrato Keep", 0), N ("Natural Vibrato", -100) }, { { "character", 1 } }),
            A ("Harmony Stack (3rd + 5th)", { N ("Retune Speed", 15), N ("Vibrato Keep", 100), N ("Voice 1 Interval", 2), N ("Voice 1 Level", -6), N ("Voice 1 Pan", -45),
                                         N ("Voice 2 Interval", 4), N ("Voice 2 Level", -8), N ("Voice 2 Pan", 45), N ("Harmony Detune", 6), N ("Harmony Timing", 12) },
                                       { { "harmMode1", 2 }, { "harmMode2", 2 } }),
            A ("Choir Stack",          { N ("Retune Speed", 20), N ("Vibrato Keep", 100), N ("Voice 1 Interval", 2), N ("Voice 1 Level", -8), N ("Voice 1 Pan", -60),
                                         N ("Voice 2 Interval", 4), N ("Voice 2 Level", -9), N ("Voice 2 Pan", 60), N ("Voice 3 Interval", -3), N ("Voice 3 Level", -10), N ("Voice 3 Pan", -30),
                                         N ("Voice 4 Interval", 7), N ("Voice 4 Level", -12), N ("Voice 4 Pan", 30), N ("Harmony Detune", 14), N ("Harmony Timing", 24) },
                                       { { "harmMode1", 2 }, { "harmMode2", 2 }, { "harmMode3", 2 }, { "harmMode4", 2 } }),
            A ("MIDI Chord Harmonies", { N ("Retune Speed", 15), N ("Vibrato Keep", 100), N ("Voice 1 Level", -6), N ("Voice 1 Pan", -50), N ("Voice 2 Level", -7), N ("Voice 2 Pan", 50),
                                         N ("Voice 3 Level", -8), N ("Voice 3 Pan", -20), N ("Voice 4 Level", -9), N ("Voice 4 Pan", 20) },
                                       { { "harmMode1", 3 }, { "harmMode2", 3 }, { "harmMode3", 3 }, { "harmMode4", 3 }, { "midiMode", 1 }, { "midiLead", 0 } }),
            A ("Octave Up Chipmunk",   { N ("Retune Speed", 10), N ("Transpose", 12) }, { { "formantPreserve", 0 } }),
            A ("Deep Voice",           { N ("Retune Speed", 20), N ("Transpose", -12), N ("Formant", -3), N ("Throat", 118) }),
            A ("Female to Male",       { N ("Retune Speed", 25), N ("Transpose", -12), N ("Formant", -2.5f), N ("Throat", 114) }),
            A ("Male to Female",       { N ("Retune Speed", 25), N ("Transpose", 12), N ("Formant", 2.5f), N ("Throat", 88) }),
            A ("Vibrato Lift",         { N ("Retune Speed", 10), N ("Vibrato Keep", 0), N ("Vibrato Depth", 35), N ("Vibrato Rate", 5.6f), N ("Vibrato Delay", 0.25f), N ("Vibrato Onset", 0.35f), N ("Vibrato Variation", 25) }),
            A ("Talkbox Glide",        { N ("Retune Speed", 45), N ("Humanize", 0), N ("Vibrato Keep", 0), N ("Formant", 1.5f) }, { { "character", 0 } }) } },

        // Moth Tune FX props: slotTypeN (0 empty, 1 Mutate, 2 Duet, 3 Vocode, 4 Tube, 5 Breath, 6 Reverb, 7 Echo, 8 Chorus, 9 Compress,
        // 10 Low-pass, 11 High-pass, 12 Tilt EQ, 13 Octaver, 14 Bit-crush, 15 Talkbox, 16 Stutter), motionMode (0 off 1 auto 2 momentary 3 toggle 4 always),
        // motionPattern (1-based factory pattern), engine (0 Live, 1 Studio).
        { "Moth Tune FX", {
            B ("Clean Hard Tune",      { N ("Retune Speed", 0), N ("Humanize", 0) }),
            B ("Robot Voice",          { N ("Retune Speed", 0), N ("Slot 1 X", 0.5f), N ("Slot 1 Y", 0.15f), N ("Slot 1 Mix", 55), N ("Slot 2 X", 0.35f), N ("Slot 2 Y", 0.6f), N ("Slot 2 Mix", 100) },
                                       { { "slotType1", 1 }, { "slotType2", 4 } }),
            B ("Telephone Talk",       { N ("Retune Speed", 10), N ("Slot 1 X", 0.47f), N ("Slot 1 Y", 0.2f), N ("Slot 2 X", 0.66f), N ("Slot 2 Y", 0.25f), N ("Slot 3 X", 0.45f), N ("Slot 3 Y", 0.12f), N ("Slot 3 Mix", 70) },
                                       { { "slotType1", 11 }, { "slotType2", 10 }, { "slotType3", 14 } }),
            B ("Doubled Lead",         { N ("Retune Speed", 15), N ("Slot 1 X", 0.45f), N ("Slot 1 Y", 0.35f), N ("Slot 2 X", 0.45f), N ("Slot 2 Y", 0.5f), N ("Slot 2 Mix", 22) },
                                       { { "slotType1", 2 }, { "slotType2", 6 } }),
            B ("Vocoder Choir",        { N ("Retune Speed", 0), N ("Slot 1 X", 0.5f), N ("Slot 1 Y", 0.85f), N ("Slot 2 X", 0.35f), N ("Slot 2 Y", 0.6f), N ("Slot 2 Mix", 60), N ("Slot 3 X", 0.7f), N ("Slot 3 Y", 0.4f), N ("Slot 3 Mix", 30) },
                                       { { "slotType1", 3 }, { "slotType2", 8 }, { "slotType3", 6 } }),
            B ("Up Arp Motion",        { N ("Retune Speed", 0), N ("Motion Glide", 15), N ("Slot 1 X", 0.4f), N ("Slot 1 Y", 0.3f), N ("Slot 1 Mix", 30) }, { { "slotType1", 7 }, { "motionPattern", 1 }, { "motionMode", 1 } }),
            A ("Trap Flip Motion",     { N ("Retune Speed", 0), N ("Motion Glide", 8) }, { { "motionPattern", 8 }, { "motionMode", 1 } }),
            A ("Yodel Steps",          { N ("Retune Speed", 0), N ("Motion Glide", 30) }, { { "motionPattern", 9 }, { "motionMode", 1 } }),
            A ("Talkbox Funk",         { N ("Retune Speed", 0), N ("Slot 1 X", 0.3f), N ("Slot 1 Y", 0.6f), N ("Slot 2 X", 0.3f), N ("Slot 2 Y", 0.5f), N ("Slot 3 X", 0.4f), N ("Slot 3 Y", 0.3f), N ("Slot 3 Mix", 30) },
                                       { { "slotType1", 15 }, { "slotType2", 4 }, { "slotType3", 7 } }),
            A ("Octave Monster",       { N ("Retune Speed", 0), N ("Slot 1 X", 0.85f), N ("Slot 1 Y", 0.3f), N ("Slot 2 X", 0.5f), N ("Slot 2 Y", 0.45f) }, { { "slotType1", 13 }, { "slotType2", 4 } }),
            A ("Stutter Gate",         { N ("Retune Speed", 0), N ("Slot 1 X", 0.7f), N ("Slot 1 Y", 1.0f), N ("Slot 2 X", 0.3f), N ("Slot 2 Y", 0.35f), N ("Slot 2 Mix", 35) }, { { "slotType1", 16 }, { "slotType2", 7 } }),
            A ("Alien Mutation",       { N ("Retune Speed", 0), N ("Formant", 5), N ("Slot 1 X", 0.8f), N ("Slot 1 Y", 0.8f), N ("Slot 1 Mix", 70), N ("Slot 2 X", 0.7f), N ("Slot 2 Y", 0.8f), N ("Slot 3 X", 0.8f), N ("Slot 3 Y", 0.2f), N ("Slot 3 Mix", 40) },
                                       { { "slotType1", 1 }, { "slotType2", 8 }, { "slotType3", 6 } }),
            A ("Breathy Air",          { N ("Retune Speed", 20), N ("Humanize", 30), N ("Slot 1 X", 0.5f), N ("Slot 1 Y", 0.6f), N ("Slot 2 X", 0.62f), N ("Slot 2 Y", 0.6f) }, { { "slotType1", 5 }, { "slotType2", 12 } }),
            A ("Lo-Fi Radio",          { N ("Retune Speed", 10), N ("Slot 1 X", 0.55f), N ("Slot 1 Y", 0.25f), N ("Slot 2 X", 0.42f), N ("Slot 2 Y", 0.3f), N ("Slot 3 X", 0.62f), N ("Slot 3 Y", 0.3f), N ("Slot 4 X", 0.4f), N ("Slot 4 Y", 0.6f) },
                                       { { "slotType1", 14 }, { "slotType2", 11 }, { "slotType3", 10 }, { "slotType4", 9 } }) } },

        //==================================================================== Apex Series
        // Apex Dynamics props: mode (0 compress 1 expand 2 gate 3 duck 4 upward), detection (0 smart 1 rms 2 average 3 peak 4 fast),
        // scFilter (0 off 1 HP 2 LP 3 BP 4 notch), autoMakeup, externalKey, link. Ratio: 1-20 = n:1, 21 = LMTR, 22-41 = negative ratios.
        { "Apex Dynamics", {
            B ("Vocal Leveler",       { N ("Threshold", -18), N ("Ratio", 3), N ("Knee", 8), N ("Attack", 5), N ("Release", 80), N ("Depth", 24) }, { { "autoMakeup", 1 } }),
            B ("Drum Bus Glue",       { N ("Threshold", -14), N ("Ratio", 2), N ("Knee", 6), N ("Attack", 30), N ("Release", 100), N ("Depth", 12) }, { { "autoMakeup", 1 } }),
            B ("Bass Control",        { N ("Threshold", -20), N ("Ratio", 4), N ("Knee", 4), N ("Attack", 3), N ("Release", 60), N ("Depth", 20) }, { { "autoMakeup", 1 }, { "detection", 1 } }),
            B ("Mix Bus Gentle",      { N ("Threshold", -10), N ("Ratio", 1.5f), N ("Knee", 10), N ("Attack", 30), N ("Release", 200), N ("Depth", 6) }, { { "autoMakeup", 1 } }),
            B ("Noise Gate",          { N ("Threshold", -45), N ("Attack", 0.5f), N ("Hold", 40), N ("Release", 120), N ("Hysteresis", 4), N ("Depth", 60) }, { { "mode", 2 }, { "detection", 3 } }),
            B ("Kick Duck (key)",     { N ("Threshold", -30), N ("Depth", 8), N ("Knee", 6), N ("Attack", 2), N ("Release", 120) }, { { "mode", 3 }, { "externalKey", 1 }, { "detection", 4 } }),
            A ("Brickwall LMTR",      { N ("Threshold", -6), N ("Ratio", 21), N ("Knee", 0), N ("Attack", 0.1f), N ("Release", 50), N ("Lookahead", 5), N ("Depth", 30) }, { { "detection", 3 } }),
            A ("Over-compression (-20:1)", { N ("Threshold", -24), N ("Ratio", 22), N ("Knee", 6), N ("Attack", 5), N ("Release", 150), N ("Depth", 30) }),
            A ("Parallel Smash",      { N ("Threshold", -40), N ("Ratio", 20), N ("Attack", 1), N ("Release", 60), N ("Makeup", 18), N ("Dry Mix", 60), N ("Depth", 40) }, { { "detection", 4 } }),
            A ("De-ess (key band 7 kHz)", { N ("Threshold", -30), N ("Ratio", 6), N ("Attack", 0.5f), N ("Release", 50), N ("Key Filter Freq", 7000), N ("Key Filter Q", 2), N ("Depth", 10) }, { { "scFilter", 3 }, { "detection", 4 } }),
            A ("Upward Detail",       { N ("Threshold", -30), N ("Ratio", 2), N ("Depth", 10), N ("Attack", 10), N ("Release", 200) }, { { "mode", 4 } }),
            A ("Downward Expander",   { N ("Threshold", -40), N ("Ratio", 2), N ("Release", 200), N ("Depth", 20), N ("Knee", 6) }, { { "mode", 1 } }),
            A ("Lookahead Vocal Rider", { N ("Threshold", -22), N ("Ratio", 2.5f), N ("Knee", 12), N ("Attack", 15), N ("Release", 300), N ("Lookahead", 10), N ("Depth", 9) }, { { "autoMakeup", 1 }, { "detection", 1 } }) } },
        // Apex Limiter props: target (0 Streaming, 1 Apple, 2 Podcast, 3 EBU R128, 4 ATSC, 5 Club, 6 CD loud), autoRelease, link
        { "Apex Limiter", {
            B ("Streaming Master",    { N ("Input", 4), N ("Ceiling", -1), N ("Release", 80) }, { { "target", 0 }, { "autoRelease", 1 } }),
            B ("Transparent Safety",  { N ("Input", 0), N ("Ceiling", -0.3f), N ("Release", 100) }, { { "autoRelease", 1 } }),
            B ("Apple Music Master",  { N ("Input", 3), N ("Ceiling", -1), N ("Release", 80) }, { { "target", 1 }, { "autoRelease", 1 } }),
            B ("Loud Club",           { N ("Input", 9), N ("Ceiling", -0.3f), N ("Character", 30) }, { { "target", 5 }, { "autoRelease", 1 } }),
            A ("Broadcast EBU R128",  { N ("Input", 0), N ("Ceiling", -1), N ("Release", 200) }, { { "target", 3 }, { "autoRelease", 0 } }),
            A ("ATSC A/85 (TV)",      { N ("Input", 0), N ("Ceiling", -2), N ("Release", 200) }, { { "target", 4 }, { "autoRelease", 0 } }),
            A ("Warm Push",           { N ("Input", 6), N ("Ceiling", -1), N ("Character", 60) }, { { "autoRelease", 1 } }),
            A ("Podcast Voice",       { N ("Input", 6), N ("Ceiling", -1), N ("Release", 150) }, { { "target", 2 }, { "autoRelease", 1 } }) } },
        // Apex Multiband props: bands (1-6), linearPhase, detection, soloN / bypassN / flipN / autoN
        { "Apex Multiband", {
            B ("Mastering Gentle (4 bands)", { N ("Crossover 1", 120), N ("Crossover 2", 600), N ("Crossover 3", 4000), N ("Band 1 Threshold", -24), N ("Band 1 Depth", -3), N ("Band 2 Threshold", -24), N ("Band 2 Depth", -2),
                                               N ("Band 3 Threshold", -26), N ("Band 3 Depth", -2), N ("Band 4 Threshold", -30), N ("Band 4 Depth", -3) }, { { "bands", 4 } }),
            B ("Vocal De-harsh",      { N ("Crossover 1", 2500), N ("Crossover 2", 8000), N ("Band 2 Threshold", -30), N ("Band 2 Depth", -6), N ("Band 2 Slope", 60), N ("Band 2 Attack", 2), N ("Band 2 Release", 80),
                                        N ("Band 1 Depth", 0), N ("Band 3 Depth", -3), N ("Band 3 Threshold", -34) }, { { "bands", 3 } }),
            B ("Bass Tamer",          { N ("Crossover 1", 150), N ("Band 1 Threshold", -24), N ("Band 1 Depth", -8), N ("Band 1 Slope", 50), N ("Band 1 Attack", 15), N ("Band 1 Release", 120), N ("Band 2 Depth", 0) }, { { "bands", 2 } }),
            A ("Upward Air (flip)",   { N ("Crossover 1", 300), N ("Crossover 2", 8000), N ("Band 3 Threshold", -40), N ("Band 3 Depth", 6), N ("Band 3 Slope", 30), N ("Band 1 Depth", 0), N ("Band 2 Depth", 0) }, { { "bands", 3 }, { "flip3", 1 } }),
            A ("6-band Mastering (linear phase)", { N ("Crossover 1", 80), N ("Crossover 2", 250), N ("Crossover 3", 1000), N ("Crossover 4", 4000), N ("Crossover 5", 10000) }, { { "bands", 6 }, { "linearPhase", 1 } }),
            A ("Drum Punch Split",    { N ("Crossover 1", 100), N ("Crossover 2", 2000), N ("Band 1 Threshold", -20), N ("Band 1 Depth", -6), N ("Band 1 Attack", 20), N ("Band 2 Depth", 0), N ("Band 3 Threshold", -26), N ("Band 3 Depth", -4), N ("Band 3 Attack", 1) }, { { "bands", 3 } }) } },
        // Apex Sub props: range (0 20-40, 1 30-60, 2 40-80, 3 55-110, 4 75-150, 5 100-200 Hz), midiTune, driveType (0 clean 1 soft 2 warm 3 tube 4 fuzz 5 fold)
        { "Apex Sub", {
            B ("Kick Weight",         { N ("Lower", -6), N ("Upper", -60), N ("Direct", 0), N ("Low-pass", 120), N ("Mix", 40) }, { { "range", 2 } }),
            B ("808 Boost",           { N ("Lower", -3), N ("Upper", -24), N ("Drive", 30), N ("Mix", 50) }, { { "range", 1 }, { "driveType", 1 } }),
            B ("Bass Octave",         { N ("Lower", -4), N ("Upper", -14), N ("Low-pass", 200), N ("Mix", 50) }, { { "range", 3 } }),
            A ("MIDI Sub (fixed)",    { N ("Lower", -3), N ("Upper", -60), N ("Mix", 50) }, { { "midiTune", 1 } }),
            A ("Fuzz Sub",            { N ("Lower", -4), N ("Upper", -10), N ("Drive", 60), N ("Low-pass", 400), N ("Mix", 45) }, { { "range", 3 }, { "driveType", 4 } }) } },

        // Aurora Verb props: roomType (0 Hall 1 Chamber 2 Room 3 Plate 4 Church 5 Cathedral 6 Studio 7 Ambience), freeze
        { "Aurora Verb", {
            B ("Vocal Plate",        { N ("Pre-Delay", 30), N ("Size", 90), N ("Decay", 1.8f), N ("High Decay", 0.6f), N ("Low Cut", 150), N ("High Cut", 9000), N ("Mix", 22) }, { { "roomType", 3 } }),
            B ("Big Hall",           { N ("Pre-Delay", 25), N ("Size", 120), N ("Decay", 3.2f), N ("Low Decay", 1.3f), N ("High Decay", 0.45f), N ("Modulation", 25), N ("Mix", 28) }, { { "roomType", 0 } }),
            B ("Drum Room",          { N ("Pre-Delay", 5), N ("Size", 80), N ("Decay", 0.7f), N ("Early Level", 0), N ("Tail Level", -4), N ("Mix", 20) }, { { "roomType", 2 } }),
            B ("Studio Ambience",    { N ("Pre-Delay", 0), N ("Size", 100), N ("Decay", 0.4f), N ("Early Level", 0), N ("Tail Level", -8), N ("Mix", 15) }, { { "roomType", 7 } }),
            B ("Ducked Vocal Hall",  { N ("Pre-Delay", 60), N ("Size", 110), N ("Decay", 2.6f), N ("High Decay", 0.5f), N ("Ducking", 9), N ("Low Cut", 200), N ("Mix", 30) }, { { "roomType", 0 } }),
            A ("Cathedral",          { N ("Pre-Delay", 40), N ("Size", 140), N ("Decay", 7.5f), N ("Low Decay", 1.4f), N ("High Decay", 0.4f), N ("Modulation", 30), N ("Mix", 35) }, { { "roomType", 5 } }),
            A ("Chamber Strings",    { N ("Pre-Delay", 20), N ("Size", 100), N ("Decay", 2.0f), N ("Diffusion", 90), N ("Mix", 25) }, { { "roomType", 1 } }),
            A ("Church Organ",       { N ("Pre-Delay", 30), N ("Size", 130), N ("Decay", 4.5f), N ("Low Decay", 1.5f), N ("Mix", 35) }, { { "roomType", 4 } }),
            A ("Snare Plate",        { N ("Pre-Delay", 10), N ("Size", 70), N ("Decay", 1.2f), N ("High Decay", 0.8f), N ("Low Cut", 250), N ("Mix", 25) }, { { "roomType", 3 } }),
            A ("Infinite Pad",       { N ("Size", 150), N ("Decay", 18), N ("Modulation", 60), N ("Width", 140), N ("Mix", 50) }, { { "roomType", 0 } }) } },

        // Opto Leveler props: limitMode, meterMode (0 gain reduction, 1 output)
        { "Opto Leveler", {
            B ("Vocal Leveler",      { N ("Peak Reduction", 45), N ("Gain", 10), N ("Emphasis", 10), N ("Tube", 30) }, { { "limitMode", 0 } }),
            B ("Bass Smooth",        { N ("Peak Reduction", 50), N ("Gain", 9), N ("Tube", 40) }, { { "limitMode", 0 } }),
            B ("Gentle Bus",         { N ("Peak Reduction", 25), N ("Gain", 4), N ("Tube", 20), N ("Mix", 100) }, { { "limitMode", 0 } }),
            A ("Limit Mode Vocal",   { N ("Peak Reduction", 55), N ("Gain", 12), N ("Emphasis", 25), N ("Tube", 30) }, { { "limitMode", 1 } }),
            A ("Parallel Squash",    { N ("Peak Reduction", 80), N ("Gain", 18), N ("Tube", 50), N ("Mix", 40) }, { { "limitMode", 1 } }),
            A ("De-Harsh Guitar",    { N ("Peak Reduction", 40), N ("Gain", 8), N ("Emphasis", 70) }, { { "limitMode", 0 } }) } },

        // Pulse EQ props (index into the stepped switches): lowFreq (20 30 60 100), highFreq (3k 4k 5k 8k 10k 12k 16k),
        // attenFreq (5k 10k 20k), midLowFreq (200 300 500 700 1k), dipFreq (200..7k), midHighFreq (1.5k 2k 3k 4k 5k)
        { "Pulse EQ", {
            B ("Kick Trick",         { N ("Low Boost", 6), N ("Low Atten", 4), N ("Tube", 30) }, { { "lowFreq", 2 } }),
            B ("Vocal Air",          { N ("High Boost", 4), N ("Bandwidth", 8), N ("High Atten", 1) }, { { "highFreq", 5 }, { "attenFreq", 2 } }),
            B ("Bass Weight",        { N ("Low Boost", 5), N ("Mid Dip", 3), N ("Tube", 35) }, { { "lowFreq", 3 }, { "dipFreq", 2 } }),
            B ("Mix Bus Sheen",      { N ("Low Boost", 2), N ("High Boost", 2), N ("Bandwidth", 9), N ("High Atten", 0.5f), N ("Tube", 20) }, { { "lowFreq", 1 }, { "highFreq", 6 } }),
            A ("Snare Crack",        { N ("Mid High Peak", 4), N ("Mid Dip", 3), N ("High Boost", 2) }, { { "midHighFreq", 2 }, { "dipFreq", 3 }, { "highFreq", 3 } }),
            A ("Boxy Fix",           { N ("Mid Dip", 6) }, { { "dipFreq", 2 } }),
            A ("Guitar Body",        { N ("Mid Low Peak", 4), N ("Mid Dip", 2), N ("Mid High Peak", 3) }, { { "midLowFreq", 1 }, { "dipFreq", 4 }, { "midHighFreq", 3 } }),
            A ("Master Polish",      { N ("Low Boost", 1.5f), N ("Low Atten", 1), N ("High Boost", 1.5f), N ("Bandwidth", 10), N ("Tube", 15) }, { { "lowFreq", 1 }, { "highFreq", 6 } }) } },

        // Oil Can Delay props: mode (0 echo, 1 doubler), sync (0 free, 1 1/16, 2 1/8, 3 dotted 1/8, 4 1/4, 5 1/2)
        { "Oil Can Delay", {
            B ("Classic Oil Can",    { N ("Delay", 180), N ("Variation", 40), N ("Sustain", 35), N ("Tone", 50), N ("Mix", 35) }, { { "mode", 0 } }),
            B ("Surf Slap",          { N ("Delay", 110), N ("Variation", 30), N ("Sustain", 15), N ("Tone", 65), N ("Mix", 30) }, { { "mode", 0 } }),
            B ("Wobble Double",      { N ("Variation", 60), N ("Wobble Rate", 4.5f), N ("Mix", 45) }, { { "mode", 1 } }),
            A ("Dub Swirl",          { N ("Delay", 280), N ("Variation", 70), N ("Sustain", 70), N ("Drive", 40), N ("Tone", 35), N ("Mix", 40) }, { { "mode", 0 } }),
            A ("Quarter Sync",       { N ("Variation", 35), N ("Sustain", 45), N ("Mix", 30) }, { { "mode", 0 }, { "sync", 4 } }) } },

        // Silk Ladder props: poles (0 two-pole, 1 four-pole), envFast
        { "Silk Ladder", {
            B ("Warm Lowpass",       { N ("Cutoff", 1800), N ("Resonance", 15), N ("Env Amount", 0), N ("Drive", 15) }, { { "poles", 1 } }),
            B ("Funky Envelope",     { N ("Cutoff", 300), N ("Resonance", 60), N ("Env Amount", 80), N ("Drive", 25) }, { { "poles", 1 }, { "envFast", 1 } }),
            B ("Squelch Bass",       { N ("Cutoff", 220), N ("Resonance", 75), N ("Env Amount", 65), N ("Drive", 45) }, { { "poles", 1 }, { "envFast", 1 } }),
            A ("Two-Pole Gentle",    { N ("Cutoff", 3000), N ("Resonance", 20), N ("Env Amount", 20) }, { { "poles", 0 } }),
            A ("Dark Drive",         { N ("Cutoff", 700), N ("Resonance", 35), N ("Drive", 80), N ("Output", -4) }, { { "poles", 1 } }) } },

        // Halo Ring props: lfoShape (0 sine, 1 square)
        { "Halo Ring", {
            B ("Bell Tones",         { N ("Frequency", 440), N ("Mix", 60) }, { { "lfoShape", 0 } }),
            B ("Robot Voice",        { N ("Frequency", 60), N ("Mix", 100) }, { { "lfoShape", 0 } }),
            B ("Tremolo",            { N ("Frequency", 6), N ("Mix", 50) }, { { "lfoShape", 0 } }),
            A ("Sci-Fi Sweep",       { N ("Frequency", 300), N ("LFO Rate", 0.3f), N ("LFO Amount", 60), N ("Mix", 80) }, { { "lfoShape", 0 } }),
            A ("Square Warble",      { N ("Frequency", 180), N ("LFO Rate", 6), N ("LFO Amount", 25), N ("Mix", 70) }, { { "lfoShape", 1 } }) } },

        // Twelve Phase props: stages (0 four, 1 six, 2 eight, 3 twelve)
        { "Twelve Phase", {
            B ("Slow Twelve",        { N ("Rate", 0.25f), N ("Sweep", 75), N ("Resonance", 55), N ("Mix", 100) }, { { "stages", 3 } }),
            B ("Classic Four",       { N ("Rate", 0.6f), N ("Sweep", 60), N ("Resonance", 30), N ("Mix", 100) }, { { "stages", 0 } }),
            B ("Wide Stereo",        { N ("Rate", 0.35f), N ("Sweep", 70), N ("Stereo Spread", 180), N ("Mix", 100) }, { { "stages", 2 } }),
            A ("Jet Resonance",      { N ("Rate", 0.12f), N ("Sweep", 90), N ("Resonance", 90), N ("Mix", 100) }, { { "stages", 3 } }),
            A ("Fast Wobble",        { N ("Rate", 5), N ("Sweep", 40), N ("Resonance", 40), N ("Mix", 100) }, { { "stages", 1 } }) } },

        // Bucket Brigade props: sync (0 free, 1 1/16, 2 1/8, 3 dotted 1/8, 4 1/4, 5 1/2)
        { "Bucket Brigade", {
            B ("Analog Echo",        { N ("Time", 350), N ("Feedback", 40), N ("Filter", 50), N ("Mix", 35) }, { { "sync", 0 } }),
            B ("Slapback",           { N ("Time", 95), N ("Feedback", 10), N ("Filter", 70), N ("Mix", 30) }, { { "sync", 0 } }),
            B ("Dotted Eighth",      { N ("Feedback", 45), N ("Filter", 45), N ("Mix", 30) }, { { "sync", 3 } }),
            A ("Chorus Echo",        { N ("Time", 25), N ("Feedback", 20), N ("Mod Rate", 0.8f), N ("Mod Depth", 60), N ("Mix", 50) }, { { "sync", 0 } }),
            A ("Runaway",            { N ("Time", 420), N ("Feedback", 92), N ("Drive", 50), N ("Filter", 35), N ("Mix", 40) }, { { "sync", 0 } }) } },

        { "Moth 808", {
            // props: driveType 0 tube 1 tape 2 hard 3 fold, glideMode 0 slide (overlapping notes) 1 always 2 off, polyMode 0 mono 1 poly
            B ("Classic 808",       { N ("punch", 12), N ("punchTime", 0.045f), N ("decay", 1.6f), N ("release", 0.18f), N ("click", 0.2f), N ("harmonics", 0.15f), N ("sub", 0), N ("drive", 0.15f), N ("tone", 9000), N ("glide", 0.12f), N ("level", 0) },
                                    { { "driveType", 0 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            B ("Long Trap 808",     { N ("punch", 14), N ("punchTime", 0.05f), N ("decay", 3.0f), N ("release", 0.25f), N ("click", 0.25f), N ("harmonics", 0.35f), N ("sub", 0), N ("drive", 0.35f), N ("tone", 7000), N ("glide", 0.15f), N ("level", -1) },
                                    { { "driveType", 0 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            B ("Drill Slide",       { N ("punch", 7), N ("punchTime", 0.03f), N ("decay", 3.5f), N ("release", 0.3f), N ("click", 0.15f), N ("harmonics", 0.4f), N ("drive", 0.45f), N ("tone", 6500), N ("glide", 0.18f), N ("level", -2) },
                                    { { "driveType", 1 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            B ("Punchy Short",      { N ("punch", 18), N ("punchTime", 0.035f), N ("decay", 0.55f), N ("release", 0.1f), N ("click", 0.45f), N ("harmonics", 0.25f), N ("drive", 0.3f), N ("tone", 10000), N ("glide", 0.08f), N ("level", 0) },
                                    { { "driveType", 0 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            B ("Clean Sub",         { N ("punch", 4), N ("punchTime", 0.04f), N ("decay", 2.5f), N ("release", 0.2f), N ("click", 0.05f), N ("harmonics", 0), N ("sub", 0), N ("drive", 0), N ("tone", 2000), N ("glide", 0.1f), N ("level", 0) },
                                    { { "driveType", 0 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            B ("Distorted 808",     { N ("punch", 12), N ("punchTime", 0.045f), N ("decay", 2.2f), N ("release", 0.2f), N ("click", 0.3f), N ("harmonics", 0.5f), N ("drive", 0.7f), N ("tone", 6000), N ("glide", 0.12f), N ("level", -4) },
                                    { { "driveType", 2 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            A ("Phonk Crusher",     { N ("punch", 16), N ("punchTime", 0.04f), N ("decay", 2.0f), N ("click", 0.35f), N ("harmonics", 0.6f), N ("drive", 0.9f), N ("tone", 5000), N ("glide", 0.1f), N ("level", -6) },
                                    { { "driveType", 3 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            A ("Tape Warm",         { N ("punch", 10), N ("punchTime", 0.05f), N ("decay", 2.2f), N ("click", 0.15f), N ("harmonics", 0.3f), N ("drive", 0.5f), N ("tone", 4000), N ("glide", 0.12f), N ("level", -2) },
                                    { { "driveType", 1 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            A ("Always Glide",      { N ("punch", 10), N ("decay", 2.4f), N ("harmonics", 0.3f), N ("drive", 0.3f), N ("tone", 8000), N ("glide", 0.2f), N ("level", -1) },
                                    { { "driveType", 0 }, { "glideMode", 1 }, { "polyMode", 0 } }),
            A ("Kick-808 Hybrid",   { N ("punch", 24), N ("punchTime", 0.03f), N ("decay", 1.2f), N ("click", 0.8f), N ("harmonics", 0.2f), N ("drive", 0.35f), N ("tone", 12000), N ("glide", 0.05f), N ("level", -1) },
                                    { { "driveType", 0 }, { "glideMode", 2 }, { "polyMode", 0 } }),
            A ("Sub Octave Monster", { N ("punch", 9), N ("decay", 3.0f), N ("sub", 0.7f), N ("harmonics", 0.2f), N ("drive", 0.3f), N ("tone", 5000), N ("glide", 0.14f), N ("level", -3) },
                                    { { "driveType", 0 }, { "glideMode", 0 }, { "polyMode", 0 } }),
            A ("Poly Bass Keys",    { N ("punch", 5), N ("decay", 1.0f), N ("release", 0.3f), N ("harmonics", 0.35f), N ("drive", 0.2f), N ("tone", 9000), N ("glide", 0), N ("level", -3) },
                                    { { "driveType", 0 }, { "glideMode", 2 }, { "polyMode", 1 } }) } },

        { "Blossom 2", {
            // props: exciterType 0 pluck 1 strike 2 bow; bodyType 0 guitar 1 violin 2 cello 3 koto 4 kalimba 5 piano 6 glass 7 none
            B ("Nylon Guitar",      { N ("position", 0.14f), N ("hardness", 0.45f), N ("noise", 0.15f), N ("stiffness", 0.05f), N ("damping", 0.45f), N ("decay", 4), N ("s2Ratio", 1), N ("s2Detune", 3), N ("s2Mix", 0.3f), N ("coupling", 0.15f), N ("pickup", 0.35f), N ("bodySize", 1), N ("bodyMix", 0.5f), N ("sympathetic", 0.15f), N ("attack", 0), N ("release", 0.6f), N ("vibDepth", 0), N ("chorus", 0), N ("delayMix", 0), N ("reverbMix", 0.15f), N ("reverbSize", 0.5f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 0 } }),
            B ("Steel Guitar",      { N ("position", 0.1f), N ("hardness", 0.75f), N ("noise", 0.2f), N ("stiffness", 0.12f), N ("damping", 0.25f), N ("decay", 6), N ("s2Ratio", 1), N ("s2Detune", 5), N ("s2Mix", 0.45f), N ("coupling", 0.2f), N ("pickup", 0.2f), N ("bodyMix", 0.45f), N ("sympathetic", 0.25f), N ("attack", 0), N ("release", 0.7f), N ("reverbMix", 0.18f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 0 } }),
            B ("Koto",              { N ("position", 0.06f), N ("hardness", 0.8f), N ("noise", 0.1f), N ("stiffness", 0.2f), N ("damping", 0.3f), N ("decay", 3.5f), N ("s2Ratio", 1), N ("s2Detune", 8), N ("s2Mix", 0.25f), N ("pickup", 0.15f), N ("bodyMix", 0.55f), N ("sympathetic", 0.2f), N ("attack", 0), N ("release", 0.5f), N ("reverbMix", 0.25f), N ("reverbSize", 0.6f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 3 } }),
            B ("Harp",              { N ("position", 0.2f), N ("hardness", 0.35f), N ("noise", 0.05f), N ("stiffness", 0.08f), N ("damping", 0.4f), N ("decay", 7), N ("s2Mix", 0.2f), N ("s2Detune", 2), N ("pickup", 0.5f), N ("bodyMix", 0.3f), N ("sympathetic", 0.35f), N ("release", 1.2f), N ("chorus", 0.1f), N ("reverbMix", 0.35f), N ("reverbSize", 0.7f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 5 } }),
            B ("Violin",            { N ("position", 0.12f), N ("bowPressure", 0.55f), N ("bowSpeed", 0.6f), N ("noise", 0.1f), N ("stiffness", 0.05f), N ("damping", 0.55f), N ("decay", 2), N ("s2Mix", 0.2f), N ("s2Detune", 6), N ("pickup", 0.25f), N ("bodyMix", 0.6f), N ("sympathetic", 0.1f), N ("attack", 0.08f), N ("release", 0.25f), N ("vibDepth", 0.25f), N ("vibRate", 5.5f), N ("reverbMix", 0.25f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 2 }, { "bodyType", 1 } }),
            B ("Cello",             { N ("position", 0.13f), N ("bowPressure", 0.6f), N ("bowSpeed", 0.55f), N ("noise", 0.1f), N ("stiffness", 0.04f), N ("damping", 0.6f), N ("decay", 2.5f), N ("s2Mix", 0.15f), N ("s2Detune", 5), N ("pickup", 0.3f), N ("bodyMix", 0.65f), N ("sympathetic", 0.12f), N ("attack", 0.12f), N ("release", 0.4f), N ("vibDepth", 0.2f), N ("vibRate", 5), N ("reverbMix", 0.3f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 2 }, { "bodyType", 2 } }),
            B ("Kalimba",           { N ("position", 0.18f), N ("hardness", 0.7f), N ("stiffness", 0.45f), N ("damping", 0.15f), N ("decay", 1.6f), N ("s2Ratio", 2.7f), N ("s2Detune", 0), N ("s2Mix", 0.25f), N ("coupling", 0), N ("pickup", 0.2f), N ("bodyMix", 0.7f), N ("sympathetic", 0.05f), N ("release", 0.4f), N ("reverbMix", 0.2f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 1 }, { "bodyType", 4 } }),
            B ("Glass Bells",       { N ("position", 0.22f), N ("hardness", 0.9f), N ("stiffness", 0.6f), N ("damping", 0.05f), N ("decay", 6), N ("s2Ratio", 2.0f), N ("s2Detune", 3), N ("s2Mix", 0.3f), N ("pickup", 0.1f), N ("bodyMix", 0.5f), N ("release", 2.0f), N ("chorus", 0.2f), N ("reverbMix", 0.4f), N ("reverbSize", 0.75f), N ("tune", 0), N ("level", -2) },
                                    { { "exciterType", 1 }, { "bodyType", 6 } }),
            B ("Hammered Dulcimer", { N ("position", 0.16f), N ("hardness", 0.65f), N ("stiffness", 0.15f), N ("damping", 0.25f), N ("decay", 5), N ("s2Ratio", 1), N ("s2Detune", 6), N ("s2Mix", 1.0f), N ("coupling", 0.4f), N ("pickup", 0.2f), N ("bodyMix", 0.4f), N ("sympathetic", 0.4f), N ("release", 1.5f), N ("reverbMix", 0.25f), N ("tune", 0), N ("level", -2) },
                                    { { "exciterType", 1 }, { "bodyType", 0 } }),
            B ("Pizzicato",         { N ("position", 0.25f), N ("hardness", 0.3f), N ("noise", 0.05f), N ("stiffness", 0.04f), N ("damping", 0.7f), N ("decay", 0.8f), N ("s2Mix", 0.15f), N ("pickup", 0.3f), N ("bodyMix", 0.6f), N ("release", 0.2f), N ("reverbMix", 0.3f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 1 } }),
            A ("Ambient Bow Pad",   { N ("position", 0.15f), N ("bowPressure", 0.35f), N ("bowSpeed", 0.45f), N ("damping", 0.5f), N ("decay", 6), N ("s2Mix", 0.7f), N ("s2Detune", 9), N ("coupling", 0.2f), N ("bodyMix", 0.3f), N ("attack", 1.2f), N ("release", 3.5f), N ("vibDepth", 0.1f), N ("chorus", 0.5f), N ("delayMix", 0.25f), N ("delayTime", 0.5f), N ("delayFb", 0.5f), N ("reverbMix", 0.6f), N ("reverbSize", 0.85f), N ("tune", 0), N ("level", 6) },
                                    { { "exciterType", 2 }, { "bodyType", 2 } }),
            A ("Sitar Drone",       { N ("position", 0.04f), N ("hardness", 0.9f), N ("noise", 0.3f), N ("stiffness", 0.3f), N ("damping", 0.15f), N ("decay", 9), N ("s2Mix", 0.5f), N ("s2Detune", 2), N ("pickup", 0.1f), N ("bodyMix", 0.4f), N ("sympathetic", 1.0f), N ("release", 2.0f), N ("reverbMix", 0.2f), N ("tune", 0), N ("level", -3) },
                                    { { "exciterType", 0 }, { "bodyType", 3 } }),
            A ("Music Box",         { N ("position", 0.2f), N ("hardness", 0.95f), N ("stiffness", 0.7f), N ("damping", 0.05f), N ("decay", 2.2f), N ("s2Ratio", 3.0f), N ("s2Mix", 0.2f), N ("bodyMix", 0.3f), N ("release", 1.0f), N ("reverbMix", 0.35f), N ("tune", 12), N ("level", -3) },
                                    { { "exciterType", 1 }, { "bodyType", 4 } }),
            A ("Shamisen",          { N ("position", 0.05f), N ("hardness", 1.0f), N ("noise", 0.35f), N ("stiffness", 0.25f), N ("damping", 0.2f), N ("decay", 1.5f), N ("s2Mix", 0.2f), N ("s2Detune", 4), N ("pickup", 0.1f), N ("bodyMix", 0.7f), N ("release", 0.3f), N ("reverbMix", 0.15f), N ("tune", 0), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 3 } }),
            A ("Celestial Harp",    { N ("position", 0.2f), N ("hardness", 0.3f), N ("damping", 0.3f), N ("decay", 10), N ("s2Mix", 0.3f), N ("s2Detune", 7), N ("bodyMix", 0.25f), N ("sympathetic", 0.4f), N ("release", 2.5f), N ("chorus", 0.4f), N ("delayMix", 0.3f), N ("delayTime", 0.375f), N ("delayFb", 0.45f), N ("reverbMix", 0.55f), N ("reverbSize", 0.8f), N ("tune", 0), N ("level", -3) },
                                    { { "exciterType", 0 }, { "bodyType", 5 } }),
            A ("Upright Bass Pluck", { N ("position", 0.2f), N ("hardness", 0.3f), N ("noise", 0.1f), N ("stiffness", 0.03f), N ("damping", 0.6f), N ("decay", 2.5f), N ("s2Mix", 0.1f), N ("pickup", 0.5f), N ("bodyMix", 0.7f), N ("release", 0.3f), N ("reverbMix", 0.12f), N ("tune", -12), N ("level", 0) },
                                    { { "exciterType", 0 }, { "bodyType", 2 } }),
            A ("Bowed Glass",       { N ("position", 0.18f), N ("bowPressure", 0.4f), N ("bowSpeed", 0.5f), N ("stiffness", 0.55f), N ("damping", 0.1f), N ("decay", 5), N ("s2Ratio", 2.0f), N ("s2Mix", 0.3f), N ("bodyMix", 0.6f), N ("attack", 0.6f), N ("release", 2.0f), N ("chorus", 0.3f), N ("reverbMix", 0.5f), N ("reverbSize", 0.8f), N ("tune", 0), N ("level", 5) },
                                    { { "exciterType", 2 }, { "bodyType", 6 } }) ,
#include "SignaturePresetsBlossom2.inl"
        } },

#include "SignaturePresets18.inl"
#include "SignaturePresets181.inl"
#include "SignaturePresets181fx.inl"
        { "MIDI Shaper", {
            B ("Unity",           { N ("Semitones", 0) }),
            B ("Octave Down",     { N ("Semitones", -12) }),
            B ("Octave Up",       { N ("Semitones", 12) }),
            B ("Fifth Up",        { N ("Semitones", 7) }),
            B ("Fourth Up",       { N ("Semitones", 5) }),
            B ("Minor Third Up",  { N ("Semitones", 3) }),
            A ("Two Octaves Down", { N ("Semitones", -24) }),
            A ("Two Octaves Up",  { N ("Semitones", 24) }),
            A ("Tritone",         { N ("Semitones", 6) }),
            A ("Whole Step Up",   { N ("Semitones", 2) }),
            A ("Half Step Down",  { N ("Semitones", -1) }),
            A ("Major Sixth Up",  { N ("Semitones", 9) }) } } };
            add111 (base);
            add111v (base);   // 1.11 Voice, first wave (Moth Tune 2 presets go in front of the 1.6 ones)
            add111d (base);   // 1.11 Color & Space, second wave (part 1)
            add111e (base);   // 1.11 Color & Space, second wave (part 2)
            add111f (base);   // 1.11 Color & Space, second wave (part 3)
            add111g (base);   // 1.11 Voice, second wave (part 1)
            return base; }();
        return b;
    }

    inline const std::vector<Preset>& forName (const juce::String& name)
    {
        static const std::vector<Preset> none;
        auto it = bank().find (name);
        return it != bank().end() ? it->second : none;
    }
}

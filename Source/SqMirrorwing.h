#pragma once
// Mirrorwing (1.14 "Studio Line", phase K): the flagship synth. DSP: SqMirrorwingDsp.h (tests/mirrorwing_test.cpp), on the
// shared synth scaffolding (SqSynthCore.h: MIDI, voices, poly / mono / legato, glide, HD). Mirrorwing's own 16-slot matrix
// replaces the scaffolding's 8-slot one. Face: SkinsMirrorwing.inl (drag a source onto any knob). Claude: `mirrorwing`.

#include "SqSynthCore.h"
#include "SqMirrorwingDsp.h"
#include <deque>

namespace SqSynth
{
    struct MirrorVoice : public VoiceBase
    {
        SqMirror::Voice dsp; const SqMirror::Settings* set = nullptr;
        void prepare (double rate) { VoiceBase::prepare (rate); dsp.prepare (rate); }
        void start (int n, float vel, float glideSecs, float fromHz) { const bool fresh = ! active; startBase (n, vel, glideSecs, fromHz); if (set != nullptr) dsp.start (*set, random + vel, fresh); }
        void restartEnvelopes() override { VoiceBase::restartEnvelopes(); for (auto& e : dsp.env) e.noteOn(); }
        void release() override { VoiceBase::release(); dsp.release(); }
        void kill() override { VoiceBase::kill(); dsp.kill(); }
        inline void render (float* l, float* r, int n)
        {
            if (set == nullptr) return;
            const double hz = chunkFrequency (bend, 0.0f);
            dsp.render (l, r, n, hz, *set, velocity, note);
            for (int e = 0; e < 3; ++e) env[e].level = dsp.envNow[(size_t) e];
            if (! dsp.active()) active = false;
        }
    };

    class SqMirrorwingPlugin : public SynthPluginBase<MirrorVoice, 16>
    {
    public:
        using Base = SynthPluginBase<MirrorVoice, 16>;
        static constexpr int numOsc = SqMirror::numOsc, numF = SqMirror::numFilters, numEnv = SqMirror::numEnv, numLfo = SqMirror::numLfo, numSlots = SqMirror::numSlots;
        enum OP { oShape = 0, oWidth, oFold, oOct, oSemi, oFine, oLevel, oPan, oUniDetune, oUniSpread, oFm, oPhase, perOsc };
        enum FP { fCut = 0, fRes, fDrive, fGain, fKey, perF };
        enum EP { eDelay = 0, eAttack, eHold, eDecay, eSustain, eRelease, perEnv };
        enum LP { lRate = 0, lPhase, lFade, perLfo };
        static constexpr int oscBase = 0, noise = oscBase + numOsc * perOsc, noiseColour = noise + 1, fBase = noiseColour + 1, eBase = fBase + numF * perF, lBase = eBase + numEnv * perEnv,
                             macroBase = lBase + numLfo * perLfo, modBase = macroBase + SqMirror::numMacros, glide = modBase + numSlots, ampVel = glide + 1,
                             fxChorus = ampVel + 1, fxDelay = fxChorus + 1, fxDelayFb = fxDelay + 1, fxReverb = fxDelayFb + 1, fxSize = fxReverb + 1, width = fxSize + 1, level = width + 1, numParams = level + 1;

        static constexpr const char* xmlTypeName = "sqmMirrorwing";
        static const char* getPluginName()   { return NEEDS_TRANS("Mirrorwing"); }
        juce::String getName() const override { return "Mirrorwing"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Mirror"; }
        juce::String getSelectableDescription() override { return "Mirrorwing flagship synth"; }

        static const std::vector<SqDsp::ParamSpec>& paramSpecs()
        {
            static std::deque<std::string> names;
            static const std::vector<SqDsp::ParamSpec> v = []
            {
                std::vector<SqDsp::ParamSpec> s;
                auto S = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
                for (int k = 1; k <= numOsc; ++k)
                {
                    const auto n = std::to_string (k), o = "o" + n, N = "Osc " + n + " ";
                    s.push_back ({ S (o + "shape"), S (N + "Shape"), 0.0f, 3.0f, k == 1 ? 2.0f : (k == 2 ? 3.0f : 0.0f), 1.0f, "", 2 });
                    s.push_back ({ S (o + "width"), S (N + "Width"), 5.0f, 50.0f, 50.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (o + "fold"),  S (N + "Fold"),  0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (o + "oct"),   S (N + "Octave"), -3.0f, 3.0f, k == 3 ? -1.0f : 0.0f, 1.0f, "", 0 });
                    s.push_back ({ S (o + "semi"),  S (N + "Semitone"), -12.0f, 12.0f, 0.0f, 1.0f, " st", 0 });
                    s.push_back ({ S (o + "fine"),  S (N + "Fine"), -100.0f, 100.0f, k == 2 ? 7.0f : 0.0f, 1.0f, " ct", 0 });
                    s.push_back ({ S (o + "level"), S (N + "Level"), 0.0f, 100.0f, k <= 2 ? 80.0f : 60.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (o + "pan"),   S (N + "Pan"), -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (o + "udet"),  S (N + "Unison Detune"), 0.0f, 100.0f, 15.0f, 0.6f, " ct", 1 });
                    s.push_back ({ S (o + "uspr"),  S (N + "Unison Spread"), 0.0f, 100.0f, 70.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (o + "fm"),    S (N + "FM"), 0.0f, 200.0f, 0.0f, 0.6f, " %", 0 });
                    s.push_back ({ S (o + "phase"), S (N + "Phase"), 0.0f, 360.0f, 0.0f, 1.0f, " deg", 0 });
                }
                s.push_back ({ "noise", "Noise Level", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                s.push_back ({ "noiseColour", "Noise Colour", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                for (int k = 1; k <= numF; ++k)
                {
                    const auto n = std::to_string (k), f = "f" + n, N = "Filter " + n + " ";
                    s.push_back ({ S (f + "cut"),   S (N + "Cutoff"), 20.0f, 20000.0f, k == 1 ? 2200.0f : 8000.0f, 0.25f, " Hz", 0 });
                    s.push_back ({ S (f + "res"),   S (N + "Resonance"), 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 });
                    s.push_back ({ S (f + "drive"), S (N + "Drive"), 0.0f, 36.0f, 0.0f, 1.0f, " dB", 1 });
                    s.push_back ({ S (f + "gain"),  S (N + "Gain"), -24.0f, 24.0f, 6.0f, 1.0f, " dB", 1 });
                    s.push_back ({ S (f + "key"),   S (N + "Key Track"), 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 });
                }
                static const float defs[numEnv][6] = { { 0, 3, 0, 500, 80, 300 }, { 0, 2, 0, 400, 25, 300 }, { 0, 200, 0, 800, 60, 600 }, { 0, 1000, 0, 2000, 100, 2000 } };
                for (int k = 1; k <= numEnv; ++k)
                {
                    const auto n = std::to_string (k), e = "e" + n, N = "Env " + n + " ";
                    s.push_back ({ S (e + "del"), S (N + "Delay"), 0.0f, 5000.0f, defs[k - 1][0], 0.3f, " ms", 0 });
                    s.push_back ({ S (e + "att"), S (N + "Attack"), 0.5f, 15000.0f, defs[k - 1][1], 0.25f, " ms", 1 });
                    s.push_back ({ S (e + "hold"), S (N + "Hold"), 0.0f, 5000.0f, defs[k - 1][2], 0.3f, " ms", 0 });
                    s.push_back ({ S (e + "dec"), S (N + "Decay"), 1.0f, 15000.0f, defs[k - 1][3], 0.25f, " ms", 0 });
                    s.push_back ({ S (e + "sus"), S (N + "Sustain"), 0.0f, 100.0f, defs[k - 1][4], 1.0f, " %", 0 });
                    s.push_back ({ S (e + "rel"), S (N + "Release"), 1.0f, 20000.0f, defs[k - 1][5], 0.25f, " ms", 0 });
                }
                for (int k = 1; k <= numLfo; ++k)
                {
                    const auto n = std::to_string (k), l = "l" + n, N = "LFO " + n + " ";
                    s.push_back ({ S (l + "rate"), S (N + "Rate"), 0.02f, 40.0f, k == 1 ? 4.0f : (k == 2 ? 0.5f : (k == 3 ? 0.1f : 7.0f)), 0.3f, " Hz", 2 });
                    s.push_back ({ S (l + "phase"), S (N + "Phase"), 0.0f, 360.0f, 0.0f, 1.0f, " deg", 0 });
                    s.push_back ({ S (l + "fade"), S (N + "Fade In"), 0.0f, 5000.0f, 0.0f, 0.4f, " ms", 0 });
                }
                for (int k = 1; k <= SqMirror::numMacros; ++k) s.push_back ({ S ("macro" + std::to_string (k)), S ("Macro " + std::to_string (k)), 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                for (int k = 1; k <= numSlots; ++k) s.push_back ({ S ("mod" + std::to_string (k)), S ("Matrix " + std::to_string (k) + " Amount"), -1.0f, 1.0f, 0.0f, 1.0f, "", 2 });
                s.push_back ({ "glide", "Glide", 0.0f, 2000.0f, 0.0f, 0.4f, " ms", 0 });
                s.push_back ({ "ampVel", "Velocity", 0.0f, 100.0f, 60.0f, 1.0f, " %", 0 });
                s.push_back ({ "fxChorus", "FX Chorus", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                s.push_back ({ "fxDelay", "FX Delay", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                s.push_back ({ "fxDelayFb", "FX Delay Feedback", 0.0f, 95.0f, 35.0f, 1.0f, " %", 0 });
                s.push_back ({ "fxReverb", "FX Reverb", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                s.push_back ({ "fxSize", "FX Reverb Size", 0.0f, 100.0f, 60.0f, 1.0f, " %", 0 });
                s.push_back ({ "width", "Stereo Width", 0.0f, 200.0f, 100.0f, 1.0f, " %", 0 });
                s.push_back ({ "level", "Level", -30.0f, 6.0f, -8.0f, 1.0f, " dB", 1 });
                return s;
            }();
            return v;
        }
        static juce::StringArray routeNames()   { return { "Filter 1", "Filter 2", "Both" }; }
        static juce::StringArray syncNames()    { return { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" }; }
        static double syncBeats (int i)         { static const double b[12] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 1.5, 0.75, 2.0 / 3, 1.0 / 3 }; return b[juce::jlimit (0, 11, i)]; }
        static juce::StringArray delayNames()   { return { "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2" }; }
        static double delayBeats (int i)        { static const double b[6] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }; return b[juce::jlimit (0, 5, i)]; }

        // properties
        std::array<juce::CachedValue<bool>, numOsc> oscOn, oscSync, oscFree; std::array<juce::CachedValue<int>, numOsc> oscRoute, oscUnison;
        std::array<juce::CachedValue<bool>, numF> fOn; std::array<juce::CachedValue<int>, numF> fType, fSlope; juce::CachedValue<bool> parallel;
        std::array<juce::CachedValue<int>, numLfo> lfoShape, lfoSync; std::array<juce::CachedValue<bool>, numLfo> lfoRetrig;
        std::array<juce::CachedValue<int>, numSlots> modSrc, modDst; juce::CachedValue<int> delayTime;

        SqMirrorwingPlugin (te::PluginCreationInfo info) : Base (info)
        {
            bank.create (*this, paramSpecs());
            auto um = getUndoManager();
            for (int k = 0; k < numOsc; ++k)
            {
                const auto n = juce::String (k + 1);
                oscOn[(size_t) k].referTo (state, juce::Identifier ("oscOn" + n), um, k < 2); oscSync[(size_t) k].referTo (state, juce::Identifier ("oscSync" + n), um, false);
                oscFree[(size_t) k].referTo (state, juce::Identifier ("oscFree" + n), um, false); oscRoute[(size_t) k].referTo (state, juce::Identifier ("oscRoute" + n), um, 0);
                oscUnison[(size_t) k].referTo (state, juce::Identifier ("oscUnison" + n), um, 1);
            }
            for (int f = 0; f < numF; ++f)
            {
                const auto n = juce::String (f + 1);
                fOn[(size_t) f].referTo (state, juce::Identifier ("fOn" + n), um, f == 0); fType[(size_t) f].referTo (state, juce::Identifier ("fType" + n), um, f == 0 ? SqFissure::lowPass : SqFissure::highPass);
                fSlope[(size_t) f].referTo (state, juce::Identifier ("fSlope" + n), um, f == 0 ? 24 : 12);
            }
            parallel.referTo (state, "fParallel", um, false);
            for (int l = 0; l < numLfo; ++l)
            {
                const auto n = juce::String (l + 1);
                lfoShape[(size_t) l].referTo (state, juce::Identifier ("lfoShape" + n), um, 0); lfoSync[(size_t) l].referTo (state, juce::Identifier ("lfoSync" + n), um, 0);
                lfoRetrig[(size_t) l].referTo (state, juce::Identifier ("lfoRetrig" + n), um, true);
            }
            for (int m = 0; m < numSlots; ++m)
            {
                const auto n = juce::String (m + 1);
                modSrc[(size_t) m].referTo (state, juce::Identifier ("mSrc" + n), um, 0); modDst[(size_t) m].referTo (state, juce::Identifier ("mDst" + n), um, 0);
            }
            delayTime.referTo (state, "fxDelayTime", um, 2);
            for (auto& v : voices) v.set = &settings;
        }
        ~SqMirrorwingPlugin() override {}

        std::vector<juce::CachedValue<int>*> intProps()
        {
            std::vector<juce::CachedValue<int>*> v;
            for (auto* a : { &oscRoute, &oscUnison }) for (auto& c : *a) v.push_back (&c);
            for (auto* a : { &fType, &fSlope }) for (auto& c : *a) v.push_back (&c);
            for (auto* a : { &lfoShape, &lfoSync }) for (auto& c : *a) v.push_back (&c);
            for (auto* a : { &modSrc, &modDst }) for (auto& c : *a) v.push_back (&c);
            v.push_back (&delayTime); return v;
        }
        std::vector<juce::CachedValue<bool>*> boolProps()
        {
            std::vector<juce::CachedValue<bool>*> v;
            for (auto* a : { &oscOn, &oscSync, &oscFree }) for (auto& c : *a) v.push_back (&c);
            for (auto& c : fOn) v.push_back (&c);
            for (auto& c : lfoRetrig) v.push_back (&c);
            v.push_back (&parallel); return v;
        }
        void resetProps (juce::UndoManager* um) override
        {
            Base::resetProps (um);
            for (int k = 0; k < numOsc; ++k) { oscOn[(size_t) k].setValue (k < 2, um); oscSync[(size_t) k].setValue (false, um); oscFree[(size_t) k].setValue (false, um); oscRoute[(size_t) k].setValue (0, um); oscUnison[(size_t) k].setValue (1, um); }
            for (int f = 0; f < numF; ++f) { fOn[(size_t) f].setValue (f == 0, um); fType[(size_t) f].setValue (f == 0 ? SqFissure::lowPass : SqFissure::highPass, um); fSlope[(size_t) f].setValue (f == 0 ? 24 : 12, um); }
            parallel.setValue (false, um);
            for (int l = 0; l < numLfo; ++l) { lfoShape[(size_t) l].setValue (0, um); lfoSync[(size_t) l].setValue (0, um); lfoRetrig[(size_t) l].setValue (true, um); }
            for (int m = 0; m < numSlots; ++m) { modSrc[(size_t) m].setValue (0, um); modDst[(size_t) m].setValue (0, um); }
            delayTime.setValue (2, um);
        }
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            Base::restorePluginStateFromValueTree (v);
            for (auto* cv : intProps()) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
            for (auto* cv : boolProps()) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
        }
        juce::StringArray modDestinations() const override { return {}; }   // Mirrorwing's own 16-slot matrix is used instead

        int po (int k, int p) const { return oscBase + k * perOsc + p; }
        int pf (int f, int p) const { return fBase + f * perF + p; }
        int pe (int e, int p) const { return eBase + e * perEnv + p; }
        int pl (int l, int p) const { return lBase + l * perLfo + p; }
        double lfoHz (int l) const { const int s = lfoSync[(size_t) l].get(); return s > 0 ? juce::jmax (20.0, bpm) / 60.0 / syncBeats (s) : (double) bank[(size_t) pl (l, lRate)]; }
        SqDsp::ParamBank& theBank() { return bank; }

        /** The parameter a matrix target moves (for the face's rings and drag-and-drop); -1 for pitch / none. */
        static int paramOfTarget (int t)
        {
            using namespace SqMirror;
            if (t >= tOscBase && t < tFilBase)
            {
                const int k = (t - tOscBase) / perOscT, q = (t - tOscBase) % perOscT;
                static const int map[] = { oSemi, oShape, oWidth, oFold, oLevel, oPan, oFm };
                return oscBase + k * perOsc + map[q];
            }
            if (t >= tFilBase && t < tAmp) { const int f = (t - tFilBase) / perFilT, q = (t - tFilBase) % perFilT; static const int map[] = { fCut, fRes, fDrive }; return fBase + f * perF + map[q]; }
            if (t == tAmp) return level; if (t == tNoise) return noise;
            return -1;
        }
        static int targetOfParam (int p) { for (int t = 1; t < SqMirror::numTargets; ++t) if (paramOfTarget (t) == p) return t; return 0; }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            for (int k = 0; k < numOsc; ++k)
            {
                auto& o = s.osc[(size_t) k];
                o.on = oscOn[(size_t) k].get(); o.sync = k > 0 && oscSync[(size_t) k].get(); o.freePhase = oscFree[(size_t) k].get();
                o.route = juce::jlimit (0, 2, oscRoute[(size_t) k].get()); o.unison = juce::jlimit (1, SqMirror::maxUnison, oscUnison[(size_t) k].get());
                o.shape = bank[(size_t) po (k, oShape)]; o.width = bank[(size_t) po (k, oWidth)] / 100.0; o.fold = bank[(size_t) po (k, oFold)] / 100.0;
                o.semis = bank[(size_t) po (k, oOct)] * 12.0 + bank[(size_t) po (k, oSemi)]; o.cents = bank[(size_t) po (k, oFine)];
                o.level = bank[(size_t) po (k, oLevel)] / 100.0; o.pan = bank[(size_t) po (k, oPan)] / 100.0;
                o.uniDetune = bank[(size_t) po (k, oUniDetune)]; o.uniSpread = bank[(size_t) po (k, oUniSpread)] / 100.0;
                o.fm = bank[(size_t) po (k, oFm)] / 100.0; o.phase = bank[(size_t) po (k, oPhase)] / 360.0;
            }
            s.noise = bank[(size_t) noise] / 100.0; s.noiseColour = bank[(size_t) noiseColour] / 100.0;
            for (int f = 0; f < numF; ++f)
            {
                auto& x = s.filter[(size_t) f];
                x.on = fOn[(size_t) f].get(); x.type = juce::jlimit (0, SqFissure::numTypes - 1, fType[(size_t) f].get()); x.slope = fSlope[(size_t) f].get() >= 48 ? 48 : fSlope[(size_t) f].get() >= 24 ? 24 : 12;
                x.cutoffHz = bank[(size_t) pf (f, fCut)]; x.res = bank[(size_t) pf (f, fRes)] / 100.0; x.driveDb = bank[(size_t) pf (f, fDrive)]; x.gainDb = bank[(size_t) pf (f, fGain)]; x.keyTrack = bank[(size_t) pf (f, fKey)] / 100.0;
            }
            s.parallel = parallel.get();
            for (int e = 0; e < numEnv; ++e)
                s.env[(size_t) e] = { bank[(size_t) pe (e, eDelay)] / 1000.0, bank[(size_t) pe (e, eAttack)] / 1000.0, bank[(size_t) pe (e, eHold)] / 1000.0,
                                      bank[(size_t) pe (e, eDecay)] / 1000.0, bank[(size_t) pe (e, eSustain)] / 100.0, bank[(size_t) pe (e, eRelease)] / 1000.0 };
            for (int l = 0; l < numLfo; ++l)
            {
                auto& x = s.lfo[(size_t) l];
                x.shape = juce::jlimit (0, SqMirror::numLfoShapes - 1, lfoShape[(size_t) l].get()); x.retrig = lfoRetrig[(size_t) l].get();
                x.hz = lfoHz (l); x.startPhase = bank[(size_t) pl (l, lPhase)] / 360.0; x.fade = bank[(size_t) pl (l, lFade)] / 1000.0;
                freePh[(size_t) l] += x.hz * chunk / sr; freePh[(size_t) l] -= std::floor (freePh[(size_t) l]); s.freePhase[(size_t) l] = freePh[(size_t) l];
            }
            for (int m = 0; m < numSlots; ++m) s.slot[(size_t) m] = { juce::jlimit (0, SqMirror::numSources - 1, modSrc[(size_t) m].get()), juce::jlimit (0, SqMirror::numTargets - 1, modDst[(size_t) m].get()), (double) bank[(size_t) (modBase + m)] };
            for (int k = 0; k < SqMirror::numMacros; ++k) s.macro[(size_t) k] = bank[(size_t) (macroBase + k)] / 100.0;
            s.wheel = modWheel; s.aftertouch = aftertouch; s.ampVel = bank[(size_t) ampVel] / 100.0; s.gain = 0.6;
            glideTime = bank[(size_t) glide] / 1000.0f; unisonSpread = 0.0f;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float ch = bank[(size_t) fxChorus] / 100.0f, dl = bank[(size_t) fxDelay] / 100.0f, fb = bank[(size_t) fxDelayFb] / 100.0f, w = bank[(size_t) width] / 100.0f;
            const float dt = juce::jlimit (0.02f, 2.0f, (float) (60.0 / bpm * delayBeats (delayTime.get())));
            for (int i = 0; i < n; ++i)
            {
                if (ch > 0.001f) chorus.process (l[i], r[i], 0.5f, 0.7f, ch);
                if (dl > 0.001f) delay.process (l[i], r[i], dt, dt, fb, 5500.0f, dl, true);
                if (std::abs (w - 1.0f) > 0.001f) { const float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]) * w; l[i] = m + sd; r[i] = m - sd; }
            }
            reverb.set (bank[(size_t) fxSize] / 100.0f, 0.45f, 1.0f, bank[(size_t) fxReverb] / 100.0f); reverb.process (l, r, n);
            const float g = juce::Decibels::decibelsToGain (bank[(size_t) level]); float pl_ = 0, pr_ = 0;
            for (int i = 0; i < n; ++i) { l[i] *= g; r[i] *= g; pl_ = juce::jmax (pl_, std::abs (l[i])); pr_ = juce::jmax (pr_, std::abs (r[i])); }
            outPeak = juce::jmax (pl_, pr_); peakL = juce::jmax (peakL.load(), pl_); peakR = juce::jmax (peakR.load(), pr_);
        }
        void takePeaks (float& a, float& b) { a = peakL.exchange (0.0f); b = peakR.exchange (0.0f); }
        /** Live values of the newest sounding voice, for the face. */
        bool newest (std::array<float, numEnv>& env, std::array<float, numLfo>& lfo, std::array<float, numF>& cut, int& note) const
        {
            const MirrorVoice* best = nullptr;
            for (auto& v : voices) if (v.active && (best == nullptr || v.ageCounter > best->ageCounter)) best = &v;
            if (best == nullptr) { note = -1; return false; }
            env = best->dsp.envNow; lfo = best->dsp.lfoNow; cut = best->dsp.cutNow; note = best->note; return true;
        }
        const SqMirror::Settings& currentSettings() const { return settings; }
        double sampleRate() const { return sr; }
        std::atomic<float> outPeak { 0 }, peakL { 0 }, peakR { 0 };

    protected:
        SqMirror::Settings settings;
        std::array<double, numLfo> freePh {};
    };
}
using SqMirrorwingPlugin = SqSynth::SqMirrorwingPlugin;

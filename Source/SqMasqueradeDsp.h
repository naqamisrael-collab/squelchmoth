#pragma once
// Masquerade (1.11 Color & Space, C15): the voice changer, on Moth Tune's tracker, PSOLA / live shifter and formant engine
// (SqVocalDsp.h) plus the shared Analog Style drive (SqEngine111Dsp.h). Measured by tests/masquerade_test.cpp (JUCE core).
//
//   Transpose  every voiced sound moved by Pitch (+-24 st), formant held unless Link is on
//   Quantize   pitch corrected to the key (Moth Key's key, or a key set here), then moved by Pitch on the scale
//   Robot      one fixed note (Robot Note + Pitch), no glide, no vibrato: the monotone voice
//   MIDI       the lead follows the lowest held MIDI note; up to three more held notes become extra voices (chords)
// Formant +-24 st on its own; Live engine (under 1 ms) or Studio engine (PSOLA, range-dependent latency); stereo in.

#include "SqVocalDsp.h"
#include "SqEngine111Dsp.h"

namespace Masquerade
{
    enum Mode { transpose, quantize, robot, midi, numModes };
    inline const char* modeName (int m) { static const char* n[] = { "Transpose", "Quantize", "Robot", "MIDI" }; return n[juce::jlimit (0, 3, m)]; }

    struct Params
    {
        int mode = transpose; float pitchSt = 0, formantSt = 0; bool link = false;
        int engine = 0, voiceType = 1;              // 0 Live, 1 Studio; SqVocal::voiceTypes()
        int keyRoot = 0, scaleMask = 0xAB5; int robotNote = 48;
        float retuneMs = 0, chordLevel = 0.7f, chordSpread = 0.6f;
        int driveVoicing = 0; float drive = 0;      // 0 = off, 1.. = Analog Style voicing + 1; amount 0..1
        float mix = 1, outputDb = 0;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = juce::jmax (16, juce::jmin (maxBlock, 512));
            eng = juce::jlimit (0, 1, p.engine); vt = juce::jlimit (0, 5, p.voiceType);
            tune.prepare (sr, eng, vt);
            drive.prepare (sr, block, 1); driveOn = -1;
            dry[0].assign ((size_t) (sr * 0.25) + 8, 0.0f); dry[1] = dry[0]; dw = 0;
            held.fill (false);
            reset();
        }
        void reset() { tune.reset(); drive.reset(); for (auto& d : dry) std::fill (d.begin(), d.end(), 0.0f); dw = 0; }
        /** The engine / range is fixed per prepare (it sets the latency): call when either changes, off the audio thread. */
        bool needsPrepare() const { return juce::jlimit (0, 1, p.engine) != eng || juce::jlimit (0, 5, p.voiceType) != vt; }
        int latencySamples() const { return tune.latency() + (p.driveVoicing > 0 ? juce::roundToInt (drive.latencySamples()) : 0); }
        double driveLatency() const { return drive.latencySamples(); }
        void setHeld (const std::array<bool, 128>& h) { held = h; }
        float inputHz() const { return tune.latestHz(); }
        const SqVocal::FrameResult& latest() const { return tune.latestResult(); }

        SqVocal::TuneSettings settings() const
        {
            SqVocal::TuneSettings s;
            s.retuneMs = juce::jmax (0.0f, p.retuneMs); s.classic = true; s.vibKeep = 1; s.flex = 0; s.humanize = 0;
            s.preserve = ! p.link; s.formantSt = juce::jlimit (-24.0f, 24.0f, p.formantSt); s.throat = 1;
            const float pitch = juce::jlimit (-24.0f, 24.0f, p.pitchSt);
            s.allowedMask = 0xfff; s.keyRoot = p.keyRoot;
            switch (p.mode)
            {
                case transpose: s.strength = 0; s.transpose = pitch; break;
                case quantize:  s.strength = 1; s.allowedMask = rotate (p.scaleMask, p.keyRoot); s.transpose = pitch; s.conform = true; s.vibKeep = 0.4f; break;
                case robot:     s.strength = 1; s.vibKeep = 0; s.midiTargets = true; s.allOctaves = false; s.midiPassThrough = false; s.transpose = 0; break;
                case midi:
                {
                    s.strength = 1; s.vibKeep = 0.5f; s.midiTargets = true; s.allOctaves = false; s.midiPassThrough = false; s.transpose = pitch;
                    for (int i = 0; i < 3; ++i)
                    {
                        auto& h = s.harm[(size_t) i]; h.mode = 3; h.gain = juce::jlimit (0.0f, 1.0f, p.chordLevel);
                        h.pan = (i == 0 ? -1.0f : i == 1 ? 1.0f : 0.0f) * juce::jlimit (0.0f, 1.0f, p.chordSpread); h.formantSt = 0;
                    }
                    s.harmDetune = 4; s.harmTimingMs = 6;
                    break;
                }
                default: break;
            }
            return s;
        }
        /** Pitch classes allowed for a scale mask written relative to the root (bit 0 = the root). */
        static int rotate (int relMask, int root)
        {
            int m = 0; for (int i = 0; i < 12; ++i) if ((relMask >> i) & 1) m |= 1 << ((i + root) % 12);
            return m;
        }

        void process (float* const* ch, int nch, int n)
        {
            nch = juce::jlimit (1, 2, nch);
            if (p.driveVoicing != driveOn) { driveOn = p.driveVoicing; if (driveOn > 0) { drive.setVoicing (driveOn - 1); drive.reset(); } }
            auto s = settings();
            std::array<bool, 128> h {};
            if (p.mode == robot) h[(size_t) juce::jlimit (0, 127, p.robotNote + juce::roundToInt (p.pitchSt))] = true;
            else if (p.mode == midi) h = held;
            const float out = juce::Decibels::decibelsToGain (p.outputDb), mx = juce::jlimit (0.0f, 1.0f, p.mix);
            const int L = latencySamples(), dsz = (int) dry[0].size();
            for (int off = 0; off < n; off += block)
            {
                const int m = juce::jmin (block, n - off);
                float* io[2] = { ch[0] + off, ch[nch - 1] + off };
                // keep the dry input, delayed by the engine latency, for Mix
                for (int i = 0; i < m; ++i) { dry[0][(size_t) ((dw + i) % dsz)] = io[0][i]; dry[1][(size_t) ((dw + i) % dsz)] = io[1][i]; }
                SqVocal::TuneProcessor::Context ctx; ctx.heldIn = (p.mode == robot || p.mode == midi) ? &h : nullptr; ctx.mix = 1; ctx.outGain = 1;
                if (nch == 1) { float* one[1] = { io[0] }; tune.process (one, 1, m, s, ctx); }
                else tune.process (io, 2, m, s, ctx);
                if (p.driveVoicing > 0) { drive.setAmount (p.drive); drive.process (io, nch, m); }
                for (int i = 0; i < m; ++i)
                {
                    const int di = ((dw + i - L) % dsz + dsz) % dsz;
                    for (int c = 0; c < nch; ++c) io[c][i] = (dry[(size_t) c][(size_t) di] + mx * (io[c][i] - dry[(size_t) c][(size_t) di])) * out;
                }
                dw = (dw + m) % dsz;
            }
        }

    private:
        double sr = 48000; int block = 256, eng = 0, vt = 1, driveOn = -1, dw = 0;
        SqVocal::TuneProcessor tune; Eng::AnalogStyle drive;
        std::array<bool, 128> held {};
        std::vector<float> dry[2];
    };
}

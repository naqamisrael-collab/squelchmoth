#pragma once
// Squelchmoth 1.11 Voice, second wave, part 2 (super prompt V4, V14, V16). Measured by tests/voice111f_test.cpp.
//
//   Ensemble     the shared unison engine: 1-32 singers per voice, each a delay tap with its own timing, slow pitch
//                drift, vibrato, timbre (a brighter or darker voice, not just a copy) and place in the stereo field
//   HiveChoir    four harmony voices on Moth Tune's engine (interval, scale step, MIDI chord, MIDI omni), each with
//                Throat (formant), level, pan, vibrato and its own choir of 1-32 singers; the lead at its own level,
//                consonant / breath leakage, humanised drift and timing, true stereo
//   DoubleWing2  the auto-doubler: 1-4 doubles with voice range, pitch / timing / timbre variation and vibrato that
//                can be added only where the singer holds a straight note
//   Mutant       the extreme voice designer: Masquerade's pitch and Throat, a pitch-tracking ring modulator,
//                tempo-synced Alienization (pitch and Throat jumps) and grain stutter
//
// Real-time safe after prepare(). JUCE core only (the voice engines).

#include "SqVoiceCore.h"
#include "SqMasqueradeDsp.h"
#include "SqColor111bDsp.h"

namespace Ensemble
{
    using namespace Eng;
    struct Rand { uint32_t s = 0x1234567u; float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 8388608.0f - 1.0f; } };   // -1..1

    class Engine
    {
    public:
        static constexpr int maxSingers = 32;
        void prepare (double sampleRate)
        {
            sr = sampleRate; int n = 1; while (n < (int) (0.12 * sr) + 8) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
            for (auto& s : sg) s = {};
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); for (auto& s : sg) { s.lp = 0; } }
        /** Sets up the singers (message thread or once per block when a setting changed). Variation: pitch in cents,
            time in ms, timbre 0..1; spread 0..1 across the stereo field; seed decides who is who. */
        void setup (int count, float pitchCents, float timeMs, float timbre, float spread, uint32_t seed, bool allLate = false)
        {
            const bool solo = count <= 1 && ! allLate;
            n = clamp (count, 1, maxSingers); Rand r; r.s = seed * 2654435761u + 12345u;
            for (int i = 0; i < n; ++i)
            {
                auto& s = sg[i];
                const float pos = n == 1 ? 0.0f : -1.0f + 2.0f * (float) i / (float) (n - 1);
                s.pan = clamp (pos * spread + 0.15f * spread * r.next(), -1.0f, 1.0f);
                s.gl = std::cos ((s.pan + 1.0f) * 0.785398f); s.gr = std::sin ((s.pan + 1.0f) * 0.785398f);
                s.time = solo ? 0.0f : (allLate ? 0.35f + 0.65f * (0.5f + 0.5f * r.next()) : 0.5f + 0.5f * r.next()) * timeMs;   // a lone choir voice sits on the beat; doubles are always late
                s.driftHz = 0.13f + 0.25f * (0.5f + 0.5f * r.next());
                s.driftPh = 3.14159f * (1.0f + r.next());
                // the drift depth that gives 'pitchCents' of pitch movement at that rate (pitch = -d delay / dt)
                const float c = (solo ? 0.0f : pitchCents) * (0.6f + 0.4f * std::abs (r.next()));
                s.driftMs = (std::pow (2.0f, c / 1200.0f) - 1.0f) / (2.0f * 3.14159f * s.driftHz) * 1000.0f;
                s.vibPh = 3.14159f * (1.0f + r.next()); s.vibRateMul = 1.0f + 0.08f * r.next();
                s.tilt = solo ? 0.0f : timbre * (i % 2 ? 1.0f : -1.0f) * (0.5f + 0.5f * std::abs (r.next()));   // half the singers brighter, half darker
                s.gain = 1.0f + (solo ? 0.0f : 0.15f * r.next());
            }
            norm = 1.0f / std::sqrt ((float) n);
        }
        /** in (mono) -> adds the ensemble into outL / outR. vibCents at vibHz on every singer, scaled by vibAmt (0..1). */
        void process (const float* in, float* outL, float* outR, int num, float level, float vibHz, float vibCents, const float* vibAmt)
        {
            const float lpG = (float) (1.0 - std::exp (-2.0 * pi * 2500.0 / sr));
            const float vibA = (std::pow (2.0f, std::max (0.0f, vibCents) / 1200.0f) - 1.0f) / (2.0f * 3.14159f * std::max (0.5f, vibHz)) * 1000.0f;
            for (int i = 0; i < num; ++i)
            {
                buf[(size_t) (w & mask)] = in[i];
                float l = 0, r = 0;
                const float va = vibAmt != nullptr ? vibAmt[i] : 1.0f;
                for (int k = 0; k < n; ++k)
                {
                    auto& s = sg[k];
                    s.driftPh += (float) (2.0 * pi * s.driftHz / sr); if (s.driftPh > 6.2831853f) s.driftPh -= 6.2831853f;
                    s.vibPh += (float) (2.0 * pi * vibHz * s.vibRateMul / sr); if (s.vibPh > 6.2831853f) s.vibPh -= 6.2831853f;
                    const float dMs = 1.0f + s.time + s.driftMs * (1.0f + std::sin (s.driftPh)) + vibA * va * (1.0f + std::sin (s.vibPh));
                    const double pos = (double) w - (double) dMs * 0.001 * sr;
                    const int ip = (int) std::floor (pos); const float f = (float) (pos - ip);
                    const float a = buf[(size_t) (ip & mask)], b = buf[(size_t) ((ip + 1) & mask)];
                    float v = a + (b - a) * f;
                    s.lp += lpG * (v - s.lp);
                    v = v + s.tilt * 0.9f * (v - s.lp);                                          // tilt > 0 brighter (more above 2.5 kHz), < 0 darker
                    v *= s.gain;
                    l += v * s.gl; r += v * s.gr;
                }
                outL[i] += l * norm * level; outR[i] += r * norm * level;
                ++w;
            }
        }
        int count() const { return n; }
        float panOf (int k) const { return sg[clamp (k, 0, n - 1)].pan; }
        float timeOf (int k) const { return sg[clamp (k, 0, n - 1)].time; }
    private:
        struct Singer { float pan = 0, gl = 0.7071f, gr = 0.7071f, time = 0, driftHz = 0.2f, driftPh = 0, driftMs = 0, vibPh = 0, vibRateMul = 1, tilt = 0, gain = 1, lp = 0; };
        double sr = 48000; std::vector<float> buf; int mask = 0; juce::int64 w = 0; int n = 1; float norm = 1;
        Singer sg[maxSingers];
    };
}

//==============================================================================================================
namespace HiveChoir
{
    using namespace Eng;
    enum Mode { off, interval, scaleStep, midiChord, midiOmni, numModes };
    inline const char* modeName (int m) { static const char* n[] = { "Off", "Interval", "Scale Step", "MIDI Chord", "MIDI Omni" }; return n[clamp (m, 0, (int) numModes - 1)]; }
    struct Voice
    {
        int mode = off; float interval = 0;          // semitones (Interval) or scale steps (Scale Step)
        float levelDb = -3, pan = 0, throatSt = 0;    // Throat: the voice's formant, semitones
        int unison = 1; float pitchVar = 8, timeVar = 15, timbreVar = 0.3f, spread = 0.5f;
        float vibHz = 5.2f, vibCents = 0;
    };
    struct Params
    {
        int engine = 1, voiceType = 1, keyRoot = 0, scaleMask = 0xAB5;
        Voice v[4];
        float leadDb = 0, humanCents = 6, humanMs = 12, consonants = 0.35f, width = 1, mix = 1, outputDb = 0;
        uint32_t seed = 1;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            eng = clamp (p.engine, 0, 1); vt = clamp (p.voiceType, 0, 5);
            tune.prepare (sr, eng, vt);
            for (auto& e : ens) e.prepare (sr);
            for (auto& t : tap) t.assign ((size_t) block, 0.0f);
            accL.assign ((size_t) block, 0.0f); accR.assign ((size_t) block, 0.0f);
            dry[0].assign ((size_t) (sr * 0.3) + 8, 0.0f); dry[1] = dry[0];
            applied = {}; for (auto& a : applied.v) a.unison = -1;
            reset();
        }
        void reset() { tune.reset(); for (auto& e : ens) e.reset(); for (auto& d : dry) std::fill (d.begin(), d.end(), 0.0f); dw = 0; }
        bool needsPrepare() const { return clamp (p.engine, 0, 1) != eng || clamp (p.voiceType, 0, 5) != vt; }
        int latencySamples() const { return tune.latency(); }
        void setHeld (const std::array<bool, 128>& h) { held = h; }
        float inputHz() const { return tune.latestHz(); }
        const SqVocal::FrameResult& latest() const { return tune.latestResult(); }
        const Ensemble::Engine& ensemble (int v) const { return ens[clamp (v, 0, 3)]; }

        SqVocal::TuneSettings settings() const
        {
            SqVocal::TuneSettings s;
            s.strength = 0; s.retuneMs = 0; s.classic = true; s.vibKeep = 1; s.preserve = true; s.throat = 1;
            s.allowedMask = Masquerade::Processor::rotate (p.scaleMask, p.keyRoot); s.keyRoot = p.keyRoot;
            s.harmDetune = std::max (0.0f, p.humanCents); s.harmTimingMs = std::max (0.0f, p.humanMs); s.harmUnvoiced = clamp (p.consonants, 0.0f, 1.0f);
            bool anyMidi = false;
            for (int i = 0; i < 4; ++i)
            {
                auto& h = s.harm[(size_t) i]; const auto& v = p.v[i];
                h.gain = 0; h.pan = 0; h.formantSt = clamp (v.throatSt, -12.0f, 12.0f);
                switch (v.mode)
                {
                    case interval:  h.mode = 1; h.interval = clamp (v.interval, -24.0f, 24.0f); break;
                    case scaleStep: h.mode = 2; h.interval = (float) juce::roundToInt (clamp (v.interval, -14.0f, 14.0f)); break;
                    case midiChord: h.mode = 3; anyMidi = true; break;
                    case midiOmni:  h.mode = 3; anyMidi = true; break;
                    default:        h.mode = 0; break;
                }
            }
            // MIDI Chord: the voices take the held notes other than the one the lead sings; Omni: every held note
            // the lead never follows MIDI here; MIDI Chord voices skip the held note the lead is singing, Omni takes them all
            s.midiTargets = false;
            s.harmSkipLead = anyMidi && ! (p.v[0].mode == midiOmni || p.v[1].mode == midiOmni || p.v[2].mode == midiOmni || p.v[3].mode == midiOmni);
            return s;
        }

        void process (float* const* ch, int nch, int n)
        {
            nch = clamp (nch, 1, 2);
            for (int k = 0; k < 4; ++k)
            {
                const auto& v = p.v[k]; const auto& a = applied.v[k];
                if (v.unison != a.unison || v.pitchVar != a.pitchVar || v.timeVar != a.timeVar || v.timbreVar != a.timbreVar || v.spread != a.spread || p.seed != applied.seed)
                    ens[k].setup (v.unison, v.pitchVar, v.timeVar, v.timbreVar, v.spread, p.seed * 7u + (uint32_t) k * 101u);
            }
            applied = p;
            const auto s = settings();
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f), lead = p.leadDb <= -59.5f ? 0.0f : dbToGain (p.leadDb);
            const int L = latencySamples(), dsz = (int) dry[0].size();
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* io[2] = { ch[0] + off, ch[nch - 1] + off };
                for (int i = 0; i < m; ++i) { dry[0][(size_t) ((dw + i) % dsz)] = io[0][i]; dry[1][(size_t) ((dw + i) % dsz)] = io[nch - 1][i]; }
                for (auto& t : tap) std::fill (t.begin(), t.begin() + m, 0.0f);
                SqVocal::TuneProcessor::Context ctx; ctx.heldIn = &held; ctx.mix = 1; ctx.outGain = 1; ctx.leadGain = lead;
                for (int k = 0; k < 4; ++k) ctx.harmTap[k] = tap[k].data();
                if (nch == 1) { float* one[1] = { io[0] }; tune.process (one, 1, m, s, ctx); }
                else tune.process (io, 2, m, s, ctx);
                // the lead (as the engine returned it) plus each voice through its choir
                std::fill (accL.begin(), accL.begin() + m, 0.0f); std::fill (accR.begin(), accR.begin() + m, 0.0f);
                for (int k = 0; k < 4; ++k)
                {
                    const auto& v = p.v[k]; if (v.mode == off) continue;
                    const float lvl = dbToGain (clamp (v.levelDb, -60.0f, 12.0f));
                    // the voice's own pan moves its whole choir; the choir's spread is around it
                    float tl[512], tr[512]; std::fill (tl, tl + m, 0.0f); std::fill (tr, tr + m, 0.0f);
                    ens[k].process (tap[k].data(), tl, tr, m, lvl, v.vibHz, v.vibCents, nullptr);
                    const float pp = clamp (v.pan * p.width, -1.0f, 1.0f);
                    for (int i = 0; i < m; ++i)
                    {
                        if (pp >= 0) { accL[(size_t) i] += tl[i] * (1.0f - pp); accR[(size_t) i] += tr[i] + tl[i] * pp; }
                        else         { accL[(size_t) i] += tl[i] + tr[i] * -pp; accR[(size_t) i] += tr[i] * (1.0f + pp); }
                    }
                }
                for (int i = 0; i < m; ++i)
                {
                    io[0][i] += accL[(size_t) i];
                    if (nch > 1) io[1][i] += accR[(size_t) i]; else io[0][i] += accR[(size_t) i];
                    const int di = ((dw + i - L) % dsz + dsz) % dsz;
                    io[0][i] = (dry[0][(size_t) di] + mx * (io[0][i] - dry[0][(size_t) di])) * out;
                    if (nch > 1) io[1][i] = (dry[1][(size_t) di] + mx * (io[1][i] - dry[1][(size_t) di])) * out;
                }
                dw = (dw + m) % dsz;
            }
        }
    private:
        double sr = 48000; int block = 256, eng = 1, vt = 1, dw = 0;
        SqVocal::TuneProcessor tune; Ensemble::Engine ens[4];
        std::vector<float> tap[4], accL, accR, dry[2];
        std::array<bool, 128> held {};
        Params applied;
    };
}

//==============================================================================================================
namespace DoubleWing2
{
    using namespace Eng;
    struct Params
    {
        int doubles = 2, voiceRange = 1;            // SqVocal::voiceTypes() index for the vibrato detector
        float pitchVar = 9, timeVar = 22, timbreVar = 0.3f, spread = 0.8f;
        float vibHz = 5.4f, vibCents = 0; bool vibStraightOnly = true;
        float doublesDb = -3, leadDb = 0, width = 1, outputDb = 0;
        uint32_t seed = 3;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            ens.prepare (sr); core.prepare (sr, block);
            mono.assign ((size_t) block, 0.0f); vib.assign ((size_t) block, 0.0f); tl.assign ((size_t) block, 0.0f); tr.assign ((size_t) block, 0.0f);
            applied = {}; applied.doubles = -1; reset();
        }
        void reset() { ens.reset(); core.reset(); straight = 0; vibAmt = 0; lastCents = 0; }
        int latencySamples() const { return 0; }
        float straightness() const { return straight; }
        float vibratoAmount() const { return vibAmt; }
        const Ensemble::Engine& ensemble() const { return ens; }

        void process (float* const* ch, int nch, int n)
        {
            nch = clamp (nch, 1, 2);
            if (p.doubles != applied.doubles || p.pitchVar != applied.pitchVar || p.timeVar != applied.timeVar || p.timbreVar != applied.timbreVar || p.spread != applied.spread || p.seed != applied.seed)
            {
                // the doubles are the singers of a small ensemble, but none of them is "on the beat": each double is late
                ens.setup (clamp (p.doubles, 1, 4), p.pitchVar, p.timeVar, p.timbreVar, p.spread, p.seed, true);
            }
            if (p.voiceRange != applied.voiceRange) { const auto& v = SqVocal::voiceTypes()[(size_t) clamp (p.voiceRange, 0, 5)]; core.setRange (v.lo, v.hi); }
            applied = p;
            const float dbl = p.doublesDb <= -59.5f ? 0.0f : dbToGain (p.doublesDb), lead = p.leadDb <= -59.5f ? 0.0f : dbToGain (p.leadDb), out = dbToGain (p.outputDb);
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* io[2] = { ch[0] + off, ch[nch - 1] + off };
                for (int i = 0; i < m; ++i) mono[(size_t) i] = 0.5f * (io[0][i] + io[nch - 1][i]);
                core.analyse (mono.data(), m);
                // straight-note detector: how little the pitch moves (cents per frame, smoothed)
                const auto& sn = core.latest();
                if (sn.voiced && sn.hz > 0)
                {
                    const float c = 1200.0f * std::log2 (sn.hz / 440.0f);
                    const float mv = std::abs (c - lastCents); lastCents = c;
                    const float k = (float) m / (float) (0.12 * sr);
                    movement += k * (std::min (mv, 100.0f) - movement);
                    straight += k * ((movement < 6.0f ? 1.0f : 0.0f) - straight);
                }
                else straight += (float) m / (float) (0.08 * sr) * (0.0f - straight);
                const float want = p.vibStraightOnly ? clamp (straight, 0.0f, 1.0f) : 1.0f;
                for (int i = 0; i < m; ++i) { vibAmt += (want - vibAmt) * (float) (1.0 / (0.15 * sr)); vib[(size_t) i] = vibAmt; }
                std::fill (tl.begin(), tl.begin() + m, 0.0f); std::fill (tr.begin(), tr.begin() + m, 0.0f);
                ens.process (mono.data(), tl.data(), tr.data(), m, dbl, p.vibHz, p.vibCents, vib.data());
                const float wd = clamp (p.width, 0.0f, 2.0f);
                for (int i = 0; i < m; ++i)
                {
                    const float mid = 0.5f * (tl[(size_t) i] + tr[(size_t) i]), side = 0.5f * (tl[(size_t) i] - tr[(size_t) i]) * wd;
                    if (nch > 1) { io[0][i] = (io[0][i] * lead + mid + side) * out; io[1][i] = (io[1][i] * lead + mid - side) * out; }
                    else io[0][i] = (io[0][i] * lead + mid) * out;
                }
            }
        }
    private:
        double sr = 48000; int block = 256; Ensemble::Engine ens; SqVoiceCore::Core core;
        std::vector<float> mono, vib, tl, tr; Params applied;
        float straight = 0, vibAmt = 0, lastCents = 0, movement = 50;
    };
}

//==============================================================================================================
namespace Mutant
{
    using namespace Eng;
    struct Params
    {
        float pitchSt = 0, throatSt = 0; int engine = 0, voiceType = 1;
        float ringMix = 0, ringRatio = 1, ringFallbackHz = 220;        // carrier = the singer's pitch x ratio (or the fallback when unvoiced)
        float alien = 0; int alienDiv = 9; float alienGlideMs = 30;     // Alienization: random pitch / Throat jumps each division
        float stutter = 0; int stutterDiv = 6; int stutterRepeats = 4;  // grain stutter: chance per division, slice length = division
        int driveVoicing = 0; float drive = 0.3f;
        float mix = 1, outputDb = 0; uint32_t seed = 5;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            masq.p.engine = clamp (p.engine, 0, 1); masq.p.voiceType = clamp (p.voiceType, 0, 5); masq.prepare (sr, block);
            for (auto& b : cap) b.assign ((size_t) (sr * 2.2) + 8, 0.0f);
            for (auto& d : dry) d.assign ((size_t) (sr * 0.3) + 8, 0.0f);
            reset();
        }
        void reset() { masq.reset(); for (auto& b : cap) std::fill (b.begin(), b.end(), 0.0f); for (auto& d : dry) std::fill (d.begin(), d.end(), 0.0f); cw = 0; dw = 0; ringPh = 0; stepIdx = -1; alienP = alienT = 0; alienPs = alienTs = 0; stutterLeft = 0; freeBeats = 0; rnd.s = p.seed * 977u + 1u; }
        bool needsPrepare() const { return clamp (p.engine, 0, 1) != masq.p.engine || clamp (p.voiceType, 0, 5) != masq.p.voiceType; }
        int latencySamples() const { return masq.latencySamples(); }
        void setTransport (double b, double q, bool pl) { bpm = b > 1 ? b : 120.0; ppq = q; playing = pl; }
        float inputHz() const { return masq.inputHz(); }
        float alienPitchNow() const { return alienPs; }
        bool stuttering() const { return stutterLeft > 0; }

        void process (float* const* ch, int nch, int n)
        {
            nch = clamp (nch, 1, 2);
            if (p.seed != seedApplied) { seedApplied = p.seed; rnd.s = p.seed * 977u + 1u; }   // a new mutation: a new jump sequence
            const int L = latencySamples(), dsz = (int) dry[0].size(), csz = (int) cap[0].size();
            const double beatsPerSample = bpm / 60.0 / sr;
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* io[2] = { ch[0] + off, ch[nch - 1] + off };
                for (int i = 0; i < m; ++i) { dry[0][(size_t) ((dw + i) % dsz)] = io[0][i]; dry[1][(size_t) ((dw + i) % dsz)] = io[nch - 1][i]; }
                // ---- the transport position decides the Alienization and stutter steps
                const double beatNow = playing ? ppq + (double) off * beatsPerSample : freeBeats;
                const double divA = WingMod::syncBeats (clamp (p.alienDiv, 0, WingMod::numSyncDivisions() - 1));
                const int64_t step = (int64_t) std::floor (beatNow / std::max (1e-6, divA));
                if (step != stepIdx)
                {
                    stepIdx = step;
                    alienP = p.alien > 0.001f ? p.alien * 12.0f * rnd.next() : 0.0f;
                    alienT = p.alien > 0.001f ? p.alien * 8.0f * rnd.next() : 0.0f;
                }
                const float gk = (float) (1.0 - std::exp (-(double) m / (std::max (1.0f, p.alienGlideMs) * 0.001 * sr)));
                alienPs += gk * (alienP - alienPs); alienTs += gk * (alienT - alienTs);
                // ---- pitch and Throat (Masquerade's engine)
                masq.p.mode = Masquerade::transpose; masq.p.pitchSt = clamp (p.pitchSt + alienPs, -24.0f, 24.0f); masq.p.formantSt = clamp (p.throatSt + alienTs, -24.0f, 24.0f);
                masq.p.link = false; masq.p.driveVoicing = p.driveVoicing; masq.p.drive = p.drive; masq.p.mix = 1; masq.p.outputDb = 0;
                masq.process (io, nch, m);
                // ---- the ring modulator follows the singer
                const float hz = masq.inputHz() > 0 ? masq.inputHz() * std::pow (2.0f, (p.pitchSt + alienPs) / 12.0f) * clamp (p.ringRatio, 0.125f, 8.0f) : p.ringFallbackHz;
                ringHz += 0.2f * (hz - ringHz);
                const float rm = clamp (p.ringMix, 0.0f, 1.0f);
                for (int i = 0; i < m; ++i)
                {
                    ringPh += 2.0 * pi * ringHz / sr; if (ringPh > 2.0 * pi) ringPh -= 2.0 * pi;
                    const float c = (float) std::sin (ringPh);
                    for (int k = 0; k < nch; ++k) io[k][i] = io[k][i] * (1.0f - rm) + io[k][i] * c * rm * 1.41f;
                }
                // ---- grain stutter: capture, and when a step fires, loop the last slice a few times
                const double divS = WingMod::syncBeats (clamp (p.stutterDiv, 0, WingMod::numSyncDivisions() - 1));
                const int sliceLen = clamp ((int) (divS * 60.0 / bpm * sr), 64, csz / 2 - 1);
                const int64_t sStep = (int64_t) std::floor (beatNow / std::max (1e-6, divS));
                if (sStep != stutterStep)
                {
                    stutterStep = sStep;
                    if (stutterLeft <= 0 && p.stutter > 0.001f && (0.5f + 0.5f * rnd.next()) < p.stutter) { stutterLeft = sliceLen * clamp (p.stutterRepeats, 1, 16); sliceStart = cw - sliceLen; slicePos = 0; }
                }
                for (int i = 0; i < m; ++i)
                {
                    for (int k = 0; k < nch; ++k) cap[(size_t) k][(size_t) (cw % csz)] = io[k][i];
                    ++cw;
                    if (stutterLeft > 0)
                    {
                        const int pos = slicePos % sliceLen;
                        const float edge = std::min (1.0f, std::min ((float) pos, (float) (sliceLen - pos)) / 96.0f);   // short fades at the slice ends
                        for (int k = 0; k < nch; ++k) io[k][i] = cap[(size_t) k][(size_t) (((sliceStart + pos) % csz + csz) % csz)] * edge;
                        ++slicePos; --stutterLeft;
                    }
                }
                // ---- mix against the dry (delayed by the engine's latency)
                const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
                for (int i = 0; i < m; ++i)
                {
                    const int di = ((dw + i - L) % dsz + dsz) % dsz;
                    for (int k = 0; k < nch; ++k) io[k][i] = (dry[(size_t) k][(size_t) di] + mx * (io[k][i] - dry[(size_t) k][(size_t) di])) * out;
                }
                dw = (dw + m) % dsz;
                if (! playing) freeBeats += (double) m * beatsPerSample;
            }
            if (playing) ppq += (double) n * beatsPerSample;
        }
    private:
        double sr = 48000, bpm = 120, ppq = 0, freeBeats = 0, ringPh = 0; bool playing = false; int block = 256;
        Masquerade::Processor masq;
        std::vector<float> cap[2], dry[2]; int64_t cw = 0; int dw = 0;
        Ensemble::Rand rnd;
        int64_t stepIdx = -1, stutterStep = -1; float alienP = 0, alienT = 0, alienPs = 0, alienTs = 0, ringHz = 220;
        int stutterLeft = 0; int64_t sliceStart = 0; int slicePos = 0; uint32_t seedApplied = 0;
    };
}

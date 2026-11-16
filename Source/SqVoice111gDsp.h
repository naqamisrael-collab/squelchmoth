#pragma once
// Squelchmoth 1.11 Voice, second wave, part 3 (super prompt V9, V10, V15). Measured by tests/voice111g_test.cpp.
//
//   VoxMoth     vocoder: 4-64 bands between a low and a high edge, four filter shapes, resonance, Stretch and Slide of
//               the synthesis bank, band shift, attack / release, Smear (across bands and time), a 16-point band level
//               curve plus tilt, a whitened carrier (every band speaks at the modulator's level whatever the carrier's
//               own spectrum), unvoiced / sibilance pass-through and noise in the carrier, 20 voicings; the carrier is an
//               internal 8-voice synth (2 oscillators, saw or variable pulse, octave, interval, detune, glide) played by
//               MIDI, by the singer's own pitch through a built-in tuner (key, scale, retune speed), or a sidechain;
//               output ring mod, drive, chorus and width; an X/Y pad (formant slide, resonance)
//   TalkWing    talk box: the voice's formants and level put onto another sound in the spectral domain (cepstral
//               envelopes: the carrier is whitened, the mouth's envelope applied), with formant shift, precision,
//               articulation, a sibilance path for consonants, a gate, the tube's own colour and brightness; the voice
//               can arrive on the key input (the instrument on this track) or be this track (the instrument on the key
//               input); with no key the voice drives a built-in buzz that follows its pitch
//   CleanRoom   vocal cleaner: hum (50 / 60 Hz, found automatically, and harmonics), buzz (the upper harmonics), hiss,
//               broadband noise (decision-directed Wiener gain on a noise floor tracked by minimum statistics, or a
//               learned profile), room (late-reverb spectral estimation from the decay time) and clicks (detected and
//               repaired across the gap), under one Clean knob; artefact guard (gain floor and smoothing), residual listen,
//               Studio (2048-point) and Live (512-point) engines
//
// Real-time safe after prepare(). JUCE core only.

#include "SqVoice111fDsp.h"
#include "SqColor111cDsp.h"
#include <complex>

namespace VoiceKit
{
    using namespace Eng;
    /** PolyBLEP saw / pulse oscillator. */
    struct Osc
    {
        double ph = 0;
        static float blep (double t, double dt)
        {
            if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }
        float saw (double dt)   { const float v = (float) (2.0 * ph - 1.0) - blep (ph, dt); ph += dt; if (ph >= 1.0) ph -= 1.0; return v; }
        float pulse (double dt, float pw)
        {
            float v = ph < pw ? 1.0f : -1.0f; v += blep (ph, dt);
            double t2 = ph - pw; if (t2 < 0) t2 += 1.0; v -= blep (t2, dt);
            ph += dt; if (ph >= 1.0) ph -= 1.0;
            return v - (2.0f * pw - 1.0f);
        }
    };
    inline float noteHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
    inline float hzNote (float hz)   { return 69.0f + 12.0f * std::log2 (std::max (1.0f, hz) / 440.0f); }
    /** The nearest note whose pitch class is in mask (bits = pitch classes from C). */
    inline float snapNote (float n, int mask)
    {
        if ((mask & 0xFFF) == 0) return std::round (n);
        const int c = (int) std::round (n);
        for (int d = 0; d <= 6; ++d)
        {
            const bool up = SqVocal::pcIn (mask, c + d), dn = SqVocal::pcIn (mask, c - d);
            if (up && dn) return (float) (std::abs ((float) (c + d) - n) <= std::abs ((float) (c - d) - n) ? c + d : c - d);
            if (up) return (float) (c + d);
            if (dn) return (float) (c - d);
        }
        return (float) c;
    }
    struct Rand { uint32_t s = 0x9e3779b9u; float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 8388608.0f - 1.0f; } };
    /** The 32 most recent values of something the face draws, written on the audio thread. */
    template <int N> struct Live { std::array<std::atomic<float>, N> v; Live() { for (auto& x : v) x = 0; } };
}

//==============================================================================================================
namespace VoxMoth
{
    using namespace Eng;
    using VoiceKit::Osc;
    constexpr int maxBands = 64, maxVoices = 8, curvePoints = 16;
    enum Carrier { midiOrVoice, voiceTrack, midiOnly, sidechain, numCarriers };
    inline const char* carrierName (int c) { static const char* n[] = { "MIDI, else Voice", "Voice Track", "MIDI Only", "Key Input" }; return n[clamp (c, 0, (int) numCarriers - 1)]; }
    enum Shape { classic, steep, resonant, wide, numShapes };
    inline const char* shapeName (int s) { static const char* n[] = { "Classic 2-Pole", "Steep 4-Pole", "Resonant", "Wide Flat" }; return n[clamp (s, 0, (int) numShapes - 1)]; }
    inline const char* waveName (int w) { return w == 0 ? "Saw" : "Pulse"; }

    struct Params
    {
        int bands = 16; float loHz = 110, hiHz = 7500; int shape = classic; float resonance = 40;     // 0..100
        float stretch = 1, slideSt = 0; int bandShift = 0;
        float attackMs = 3, releaseMs = 50, smear = 0.1f, tiltDb = 0;                                // tilt: dB from the lowest band to the highest
        std::array<float, curvePoints> curve {};                                                      // dB per point, low to high
        float unvoiced = 0.6f, noiseCarrier = 0.35f;
        int carrier = midiOrVoice, voices = 8, wave1 = 0, wave2 = 1, octave2 = 0;
        float pw = 0.5f, osc2Db = -6, interval2 = 0, detuneCents = 7, glideMs = 30;
        float tune = 1, retuneMs = 15; int keyRoot = 0, scaleMask = 0xAB5, voiceType = 1;
        float ring = 0, ringHz = 440, drive = 0; int driveVoicing = 0; float chorus = 0;
        float width = 1, mix = 1, outputDb = 0, xyX = 0.5f, xyY = 0.5f;
    };

    /** A historic vocoder archetype, described by what it does (bands, edges, filter, speed, carrier). */
    struct Voicing { const char* name; int bands; float lo, hi; int shape; float resonance, attackMs, releaseMs, smear, unvoiced, noise; int wave1, wave2; float pw, osc2Db, detune; const char* about; };
    inline const std::array<Voicing, 20>& voicings()
    {
        static const std::array<Voicing, 20> v { {
            { "Studio Ten",        10,  150, 6000, classic,  35,  4,  70, 0.10f, 0.6f, 0.30f, 0, 1, 0.50f,  -8, 6,  "ten wide bands, the late-70s studio rack: warm, a little vague, very musical" },
            { "Sixteen Classic",   16,  100, 7500, classic,  40,  3,  50, 0.10f, 0.6f, 0.35f, 0, 1, 0.50f,  -6, 7,  "the sixteen-band standard: the robot voice everyone knows" },
            { "Twenty Analog",     20,   90, 8500, steep,    45,  3,  45, 0.08f, 0.6f, 0.35f, 0, 0, 0.50f,  -6, 9,  "twenty steep analog bands: clear words, round top" },
            { "String Machine",     8,  200, 5000, wide,     20,  8, 120, 0.25f, 0.3f, 0.15f, 0, 0, 0.50f,  -3, 14, "eight wide bands for strings and pads: soft and blurred" },
            { "Robot Radio",        8,  300, 3400, steep,    60,  2,  35, 0.05f, 0.4f, 0.25f, 1, 1, 0.35f, -10, 3,  "a narrow speech band: the transmission robot" },
            { "Speech Lab",        12,  120, 6500, steep,    55,  2,  30, 0.05f, 0.8f, 0.50f, 0, 1, 0.50f,  -9, 4,  "a laboratory speech coder: crisp consonants, fast tracking" },
            { "Polyphonic 32",     32,   80, 10000, classic, 45,  2,  40, 0.10f, 0.6f, 0.30f, 0, 0, 0.50f,  -6, 8,  "thirty-two bands for chords: the modern polyphonic sound" },
            { "Digital 64",        64,   60, 14000, steep,   60,  1,  25, 0.04f, 0.7f, 0.30f, 0, 0, 0.50f,  -6, 5,  "sixty-four digital bands: near speech, very detailed" },
            { "Space Choir",       24,  120, 9000, resonant, 70,  6, 160, 0.30f, 0.5f, 0.20f, 0, 0, 0.50f,  -4, 18, "resonant bands and long release: a choir in a hall" },
            { "Funk Box",          14,  140, 6000, resonant, 65,  1,  25, 0.05f, 0.5f, 0.30f, 1, 0, 0.30f,  -8, 5,  "punchy resonant bands with a fast envelope: the funk talker" },
            { "Phone Line",         6,  350, 3300, steep,    50,  3,  40, 0.05f, 0.5f, 0.30f, 1, 1, 0.50f, -12, 0,  "six bands inside the phone band" },
            { "Glass Array",       40,  150, 12000, resonant, 85, 2,  90, 0.12f, 0.4f, 0.20f, 1, 1, 0.20f,  -6, 10, "forty very resonant bands: glassy, ringing" },
            { "Wide Pad",          48,   80, 12000, wide,    25,  8, 200, 0.35f, 0.4f, 0.15f, 0, 0, 0.50f,  -3, 16, "forty-eight wide bands, slow: a lush pad that breathes words" },
            { "Sibilant Twenty",   20,  100, 11000, classic, 40,  2,  40, 0.08f, 1.0f, 0.60f, 0, 1, 0.50f,  -6, 6,  "twenty bands with full sibilance: every S and T comes through" },
            { "Chip Four",          4,  200, 4000, steep,    40,  1,  20, 0.00f, 0.2f, 0.10f, 1, 1, 0.25f, -60, 0,  "four bands on a square wave: the game console" },
            { "Warm Eighteen",     18,   90, 6500, classic,  30,  5,  80, 0.18f, 0.5f, 0.25f, 0, 0, 0.50f,  -6, 11, "eighteen soft bands, a darker top: warm and close" },
            { "Formant Ten",       10,  250, 4500, resonant, 75,  3,  60, 0.06f, 0.5f, 0.25f, 0, 1, 0.45f,  -8, 5,  "ten bands on the vowel formants: the talking synth lead" },
            { "Bright Twenty-Four", 24, 150, 13000, steep,   50,  2,  35, 0.06f, 0.7f, 0.35f, 0, 1, 0.50f,  -6, 7,  "twenty-four steep bands reaching high: modern and bright" },
            { "Dark Hall Twelve",  12,   70, 4500, classic,  35,  6, 140, 0.20f, 0.3f, 0.15f, 0, 0, 0.50f,  -4, 12, "twelve low bands, long tails: a dark cinematic voice" },
            { "Hybrid Forty",      40,  100, 11000, classic, 50,  2,  45, 0.10f, 0.6f, 0.30f, 0, 1, 0.50f,  -6, 8,  "forty clean bands: detail with warmth" },
        } };
        return v;
    }

    struct BandFilter
    {
        Biquad a, b; int shape = classic; float g = 1;
        void set (int sh, double sr, double hz, double q, double spreadOct)
        {
            shape = sh;
            switch (sh)
            {
                case steep:    a.set (Biquad::bandpass, sr, hz, q * 0.75); b.set (Biquad::bandpass, sr, hz, q * 0.75); g = 1.15f; break;
                case resonant: a.set (Biquad::bandpass, sr, hz, q * 2.6); b.setFlat(); g = 1.6f; break;
                case wide:     a.set (Biquad::bandpass, sr, hz * std::pow (2.0, -0.3 * spreadOct), q * 0.9); b.set (Biquad::bandpass, sr, hz * std::pow (2.0, 0.3 * spreadOct), q * 0.9); g = 0.75f; break;
                default:       a.set (Biquad::bandpass, sr, hz, q); b.setFlat(); g = 1.0f; break;
            }
        }
        void reset() { a.reset(); b.reset(); }
        float process (float x)
        {
            if (shape == steep) return b.process (a.process (x)) * g;
            if (shape == wide) return (a.process (x) + b.process (x)) * g;
            return a.process (x) * g;
        }
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            core.prepare (sr, block);
            mono.assign ((size_t) block, 0.0f); car.assign ((size_t) block, 0.0f); carR.assign ((size_t) block, 0.0f);
            drive.prepare (sr, block, 1);
            const int cd = (int) (0.05 * sr) + 8; for (auto& d : chD) d.assign ((size_t) cd, 0.0f);
            hpMod.set (Biquad::highpass, sr, 4200.0, 0.707); preEmph.set (Biquad::highShelf, sr, 2000.0, 0.7, 6.0);
            applied = {}; applied.bands = -1; rangeSet = -1; reset();
        }
        void reset()
        {
            core.reset(); for (auto& f : ana) f.reset(); for (auto& f : syn) f.reset();
            std::fill (std::begin (env), std::end (env), 0.0f); std::fill (std::begin (cenv), std::end (cenv), 0.0f);
            for (auto& v : vs) v = {}; hpMod.reset(); preEmph.reset(); drive.reset();
            for (auto& d : chD) std::fill (d.begin(), d.end(), 0.0f); cw = 0; chPh[0] = 0; chPh[1] = 0.37; ringPh = 0;
            uvG = 0; tracked = 48; haveTrack = false; rng.s = 0x51ed; lastDrive = -1;
        }
        int latencySamples() const { return 0; }
        void setHeld (const std::array<bool, 128>& h) { held = h; }
        float inputHz() const { return core.latest().voiced ? core.latest().hz : 0.0f; }
        float bandLevel (int k) const { return live[(size_t) clamp (k, 0, maxBands - 1)].load(); }
        float bandHz (int k) const { return (float) fAna[clamp (k, 0, maxBands - 1)]; }
        int bandCount() const { return nb; }
        float carrierNote (int v) const { return liveNote[(size_t) clamp (v, 0, maxVoices - 1)].load(); }
        bool usingKey() const { return keyUsed.load(); }
        float unvoicedNow() const { return uvG; }

        /** Writes a voicing's settings into p (bands, edges, filter, speed, unvoiced, carrier oscillators). */
        static void applyVoicing (Params& q, int idx)
        {
            const auto& v = voicings()[(size_t) clamp (idx, 0, 19)];
            q.bands = v.bands; q.loHz = v.lo; q.hiHz = v.hi; q.shape = v.shape; q.resonance = v.resonance; q.attackMs = v.attackMs; q.releaseMs = v.releaseMs;
            q.smear = v.smear; q.unvoiced = v.unvoiced; q.noiseCarrier = v.noise; q.wave1 = v.wave1; q.wave2 = v.wave2; q.pw = v.pw; q.osc2Db = v.osc2Db; q.detuneCents = v.detune;
        }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = clamp (nch, 1, 2);
            if (p.voiceType != rangeSet) { rangeSet = p.voiceType; const auto& v = SqVocal::voiceTypes()[(size_t) clamp (p.voiceType, 0, 5)]; core.setRange (v.lo, v.hi); }
            setupBands();
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            const float atk = (float) (1.0 - std::exp (-1.0 / (std::max (0.2f, p.attackMs) * 0.001 * sr)));
            const float rel = (float) (1.0 - std::exp (-1.0 / (std::max (2.0f, p.releaseMs) * (1.0f + 3.0f * clamp (p.smear, 0.0f, 1.0f))* 0.001 * sr)));
            const float catk = (float) (1.0 - std::exp (-1.0 / (0.004 * sr))), crel = (float) (1.0 - std::exp (-1.0 / (0.040 * sr)));
            const float sm = 0.5f * clamp (p.smear, 0.0f, 1.0f), wd = clamp (p.width, 0.0f, 2.0f) * 0.5f;
            const float uvAmt = clamp (p.unvoiced, 0.0f, 1.0f) * 1.4f, nzAmt = clamp (p.noiseCarrier, 0.0f, 1.0f);
            const bool keyCarrier = p.carrier == sidechain && key != nullptr;
            keyUsed = keyCarrier;
            if (p.driveVoicing != lastDrive) { lastDrive = p.driveVoicing; if (p.driveVoicing > 0) drive.setVoicing (p.driveVoicing - 1, true); }
            drive.setAmount (clamp (p.drive, 0.0f, 1.0f));
            const float rg = clamp (p.ring, 0.0f, 1.0f), chMix = clamp (p.chorus, 0.0f, 1.0f);
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* io[2] = { ch[0] + off, ch[nch - 1] + off };
                for (int i = 0; i < m; ++i) mono[(size_t) i] = 0.5f * (io[0][i] + io[nch - 1][i]);
                core.analyse (mono.data(), m);
                const auto& sn = core.latest();
                const bool unv = (! sn.voiced || sn.sibilant) && sn.rmsDb > -62.0f;
                uvG += ((unv ? 1.0f : 0.0f) - uvG) * std::min (1.0f, (float) m / (float) (0.025 * sr));
                // the carrier
                if (keyCarrier) for (int i = 0; i < m; ++i) car[(size_t) i] = 0.5f * (key[0][off + i] + key[1][off + i]) * 2.0f;
                else synth (sn, m);
                const float nz = nzAmt * uvG;
                if (nz > 0.001f) for (int i = 0; i < m; ++i) car[(size_t) i] = car[(size_t) i] * (1.0f - 0.6f * nz) + nz * 1.2f * rng.next();
                // the bank
                for (int i = 0; i < m; ++i)
                {
                    const float x = preEmph.process (mono[(size_t) i]), c = car[(size_t) i];
                    for (int k = 0; k < nb; ++k)
                    {
                        const float a = std::abs (ana[k].process (x));
                        env[k] += (a > env[k] ? atk : rel) * (a - env[k]);
                    }
                    float ceSum = 0;
                    float L = 0, R = 0;
                    for (int k = 0; k < nb; ++k)
                    {
                        const float y = syn[k].process (c);
                        cenv[k] += (std::abs (y) > cenv[k] ? catk : crel) * (std::abs (y) - cenv[k]);
                        ceSum += cenv[k];
                        const int src = clamp (k - p.bandShift, 0, nb - 1);
                        float e = env[src];
                        if (sm > 0) e = (1.0f - sm) * e + sm * 0.5f * (env[clamp (src - 1, 0, nb - 1)] + env[clamp (src + 1, 0, nb - 1)]);
                        const float v = y * e * lvl[k] * std::min (12.0f, 1.0f / (cenv[k] + ceFloor));
                        const float s = (k & 1) ? wd : -wd;
                        L += v * (1.0f - s); R += v * (1.0f + s);
                    }
                    ceFloor = 1.0e-4f + 0.25f * ceSum / (float) std::max (1, nb);
                    const float uv = uvAmt * uvG * hpMod.process (mono[(size_t) i]) * 1.3f;
                    L += uv; R += uv;
                    if (rg > 0.001f)
                    {
                        ringPh += p.ringHz / sr; if (ringPh >= 1.0) ringPh -= 1.0;
                        const float s = (float) std::sin (2.0 * pi * ringPh);
                        L = L + rg * (L * s * 1.41f - L); R = R + rg * (R * s * 1.41f - R);
                    }
                    car[(size_t) i] = L; carR[(size_t) i] = R;
                }
                float* wet[2] = { car.data(), carR.data() };
                if (p.driveVoicing > 0 && p.drive > 0.001f) drive.process (wet, 2, m);
                if (chMix > 0.001f) chorus (wet, m, chMix);
                for (int i = 0; i < m; ++i)
                {
                    const float wl = car[(size_t) i], wr = carR[(size_t) i];
                    if (nch > 1) { io[0][i] = (io[0][i] + mx * (wl - io[0][i])) * out; io[1][i] = (io[1][i] + mx * (wr - io[1][i])) * out; }
                    else io[0][i] = (io[0][i] + mx * (0.5f * (wl + wr) - io[0][i])) * out;
                }
                for (int k = 0; k < nb; ++k) live[(size_t) k] = env[k];
            }
        }

    private:
        struct Voice { Osc o1, o2; float note = 60, cur = 60, amp = 0; bool on = false; };
        double sr = 48000; int block = 256, nb = 16, lastDrive = -1, rangeSet = -1;
        SqVoiceCore::Core core; AnalogStyle drive;
        std::vector<float> mono, car, carR; std::array<std::vector<float>, 2> chD; int cw = 0; double chPh[2] { 0, 0.37 }, ringPh = 0;
        BandFilter ana[maxBands], syn[maxBands]; float env[maxBands] {}, cenv[maxBands] {}, lvl[maxBands] {}; double fAna[maxBands] {};
        float ceFloor = 1.0e-4f, uvG = 0, tracked = 48; bool haveTrack = false;
        Biquad hpMod, preEmph; std::array<bool, 128> held {}; Voice vs[maxVoices]; VoiceKit::Rand rng;
        Params applied; std::array<std::atomic<float>, maxBands> live {}; std::array<std::atomic<float>, maxVoices> liveNote {}; std::atomic<bool> keyUsed { false };

        void setupBands()
        {
            const auto& a = applied;
            bool same = a.bands == p.bands && a.loHz == p.loHz && a.hiHz == p.hiHz && a.shape == p.shape && a.resonance == p.resonance && a.stretch == p.stretch
                        && a.slideSt == p.slideSt && a.xyX == p.xyX && a.xyY == p.xyY && a.tiltDb == p.tiltDb;
            for (int i = 0; i < curvePoints && same; ++i) same = a.curve[(size_t) i] == p.curve[(size_t) i];
            if (same) return;
            const bool resetState = a.bands != p.bands || a.shape != p.shape;
            nb = clamp (p.bands, 4, maxBands);
            const double lo = clamp ((double) p.loHz, 30.0, 2000.0), hi = clamp ((double) p.hiHz, lo * 2.0, std::min (18000.0, sr * 0.45));
            const double span = std::log2 (hi / lo), ob = span / std::max (1, nb - 1);
            const double qBase = std::sqrt (std::pow (2.0, ob)) / (std::pow (2.0, ob) - 1.0);
            const double qMul = 0.55 * std::pow (2.0, 3.0 * clamp (p.resonance, 0.0f, 100.0f) / 100.0) * std::pow (2.0, (clamp (p.xyY, 0.0f, 1.0f) - 0.5) * 2.0);
            const double slide = clamp (p.slideSt + (clamp (p.xyX, 0.0f, 1.0f) - 0.5f) * 24.0f, -24.0f, 24.0f) / 12.0;
            const double st = clamp ((double) p.stretch, 0.25, 2.5);
            for (int k = 0; k < nb; ++k)
            {
                const double u = nb > 1 ? (double) k / (nb - 1) : 0.5;
                fAna[k] = lo * std::pow (hi / lo, u);
                const double us = 0.5 + (u - 0.5) * st;
                const double fs = clamp (lo * std::pow (hi / lo, us) * std::pow (2.0, slide), 25.0, sr * 0.46);
                const double q = clamp (qBase * qMul, 0.4, 120.0);
                ana[k].set (p.shape, sr, fAna[k], q, ob); syn[k].set (p.shape, sr, fs, q, ob * st);
                // the level curve: 16 points across the bands, plus the tilt; and the band's own width (narrow bands are quieter)
                const float pos = (float) u * (curvePoints - 1); const int i0 = clamp ((int) pos, 0, curvePoints - 2); const float fr = pos - (float) i0;
                const float db = p.curve[(size_t) i0] + fr * (p.curve[(size_t) i0 + 1] - p.curve[(size_t) i0]) + p.tiltDb * ((float) u - 0.5f);
                lvl[k] = dbToGain (clamp (db, -48.0f, 24.0f)) * (float) std::sqrt (16.0 / nb);
                if (resetState) { ana[k].reset(); syn[k].reset(); env[k] = cenv[k] = 0; }
            }
            applied = p;
        }

        void synth (const SqVoiceCore::Snapshot& sn, int m)
        {
            // which notes: the held MIDI notes (lowest first), or the singer's own pitch through the built-in tuner
            int want[maxVoices]; int nw = 0;
            const int maxV = clamp (p.voices, 1, maxVoices);
            const bool anyHeld = std::any_of (held.begin(), held.end(), [] (bool b) { return b; });
            const bool useMidi = (p.carrier == midiOnly) || (p.carrier == midiOrVoice && anyHeld);
            if (useMidi) { for (int k = 0; k < 128 && nw < maxV; ++k) if (held[(size_t) k]) want[nw++] = k; }
            if (sn.voiced && sn.hz > 40.0f)
            {
                const float raw = VoiceKit::hzNote (sn.hz), target = raw + clamp (p.tune, 0.0f, 1.0f) * (VoiceKit::snapNote (raw, p.scaleMask) - raw);
                if (! haveTrack) { tracked = target; haveTrack = true; }
                const float k = p.retuneMs <= 0.5f ? 1.0f : std::min (1.0f, (float) m / (float) (p.retuneMs * 0.001 * sr));
                tracked += k * (target - tracked);
            }
            const bool track = ! useMidi && p.carrier != midiOnly;
            const float glK = p.glideMs <= 0.5f ? 1.0f : (float) (1.0 - std::exp (-1.0 / (p.glideMs * 0.001 * sr)));
            for (int v = 0; v < maxVoices; ++v)
            {
                auto& x = vs[v];
                const bool on = track ? (v == 0) : (v < nw);
                const float note = track ? tracked : (v < nw ? (float) want[v] : x.note);
                if (on && ! x.on) { if (x.amp < 0.01f) x.cur = note; }
                x.on = on; x.note = note;
                liveNote[(size_t) v] = on ? note : -1.0f;
            }
            const float pw = clamp (p.pw, 0.05f, 0.95f), g2 = p.osc2Db <= -59.5f ? 0.0f : dbToGain (p.osc2Db);
            const float off2 = 12.0f * (float) clamp (p.octave2, -2, 2) + clamp (p.interval2, -12.0f, 12.0f) + clamp (p.detuneCents, -50.0f, 50.0f) / 100.0f;
            const float aA = (float) (1.0 - std::exp (-1.0 / (0.004 * sr))), aR = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
            int active = 0; for (auto& x : vs) active += (x.on || x.amp > 0.001f) ? 1 : 0;
            const float norm = 1.0f / std::sqrt ((float) std::max (1, active));
            for (int i = 0; i < m; ++i)
            {
                float s = 0;
                for (auto& x : vs)
                {
                    x.amp += ((x.on ? 1.0f : 0.0f) - x.amp) * (x.on ? aA : aR);
                    if (x.amp < 1.0e-4f && ! x.on) continue;
                    x.cur += glK * (x.note - x.cur);
                    const double dt1 = std::min (0.45, VoiceKit::noteHz (x.cur) / sr), dt2 = std::min (0.45, VoiceKit::noteHz (x.cur + off2) / sr);
                    const float a = p.wave1 == 0 ? x.o1.saw (dt1) : x.o1.pulse (dt1, pw);
                    const float b = g2 > 0 ? (p.wave2 == 0 ? x.o2.saw (dt2) : x.o2.pulse (dt2, pw)) : 0.0f;
                    s += (a + g2 * b) * x.amp;
                }
                car[(size_t) i] = s * norm * 0.5f;
            }
        }

        void chorus (float* const* w, int m, float amt)
        {
            const int sz = (int) chD[0].size();
            for (int i = 0; i < m; ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    chD[(size_t) c][(size_t) cw] = w[c][i];
                    chPh[c] += (c == 0 ? 0.63 : 0.81) / sr; if (chPh[c] >= 1.0) chPh[c] -= 1.0;
                    const double d = (0.012 + 0.0035 * std::sin (2.0 * pi * chPh[c])) * sr;
                    double rp = (double) cw - d; while (rp < 0) rp += sz;
                    const int i0 = (int) rp; const float f = (float) (rp - i0);
                    const float a = chD[(size_t) c][(size_t) i0], b = chD[(size_t) c][(size_t) ((i0 + 1) % sz)];
                    const float y = a + f * (b - a);
                    w[c][i] = w[c][i] * (1.0f - 0.35f * amt) + amt * 0.7f * y;
                }
                cw = (cw + 1) % sz;
            }
        }
    };
}

//==============================================================================================================
namespace TalkWing
{
    using namespace Eng;
    enum Source { voiceOnKey, voiceOnTrack, buzz, numSources };
    inline const char* sourceName (int s) { static const char* n[] = { "Voice on Key Input", "Voice on This Track", "Built-in Buzz" }; return n[clamp (s, 0, (int) numSources - 1)]; }
    struct Params
    {
        int source = voiceOnKey, engine = 1, voiceType = 1, buzzOctave = 0;
        float mouth = 1, formantSt = 0, precision = 0.5f, articulation = 0.3f, follow = 1, sibilance = 0.5f, gateDb = -60;
        float tube = 0.4f, brightness = 0, buzzTone = 0.5f, glideMs = 25, mix = 1, outputDb = 0;
    };
    constexpr int maxN = 1024, envPoints = 96;

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; N = p.engine == 0 ? 512 : 1024; hop = N / 4; eng = p.engine == 0 ? 0 : 1;
            block = std::max (16, std::min (maxBlock, 512));
            core.prepare (sr, block); monoMod.assign ((size_t) block, 0.0f);
            win.resize ((size_t) N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) pi * i / N);
            for (auto& b : carIn) b.assign ((size_t) N, 0.0f);
            modIn.assign ((size_t) N, 0.0f);
            for (auto& b : outBuf) b.assign ((size_t) N * 2, 0.0f);
            for (auto& b : dry) b.assign ((size_t) N * 2, 0.0f);
            spec.assign ((size_t) N, {}); cep.assign ((size_t) N, {}); modSpec.assign ((size_t) N, {});
            envM.assign ((size_t) N / 2 + 1, 0.0f); envSm.assign ((size_t) N / 2 + 1, 0.0f); envC.assign ((size_t) N / 2 + 1, 0.0f); tubeG.assign ((size_t) N / 2 + 1, 1.0f);
            tubeApplied = -1; rangeSet = -1; reset();
        }
        void reset()
        {
            core.reset(); for (auto& b : carIn) std::fill (b.begin(), b.end(), 0.0f); std::fill (modIn.begin(), modIn.end(), 0.0f);
            for (auto& b : outBuf) std::fill (b.begin(), b.end(), 0.0f); for (auto& b : dry) std::fill (b.begin(), b.end(), 0.0f);
            std::fill (envSm.begin(), envSm.end(), -12.0f); inW = 0; outR = 0; hopCount = 0; dw = 0; lvl = 0; buzzNote = 48; buzzCur = 48; haveNote = false; gateG = 0;
        }
        bool needsPrepare() const { return (p.engine == 0 ? 0 : 1) != eng; }
        int latencySamples() const { return N - 1; }
        static int latencyFor (int engine) { return (engine == 0 ? 512 : 1024) - 1; }
        float voiceHz() const { return core.latest().voiced ? core.latest().hz : 0.0f; }
        bool talking() const { return gateG > 0.5f; }
        int mode() const { return modeNow.load(); }      // 0 voice on key, 1 voice on track, 2 buzz
        float mouthEnv (int i) const { return liveM[(size_t) clamp (i, 0, envPoints - 1)].load(); }
        float carrierEnv (int i) const { return liveC[(size_t) clamp (i, 0, envPoints - 1)].load(); }
        float sibilanceNow() const { return sibNow.load(); }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = clamp (nch, 1, 2);
            if (p.voiceType != rangeSet) { rangeSet = p.voiceType; const auto& v = SqVocal::voiceTypes()[(size_t) clamp (p.voiceType, 0, 5)]; core.setRange (v.lo, v.hi); }
            int md = clamp (p.source, 0, 2);
            if (md != buzz && key == nullptr) md = buzz;                 // no voice on the key input yet: the voice drives the buzz
            modeNow = md;
            const float mx = clamp (p.mix, 0.0f, 1.0f);
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                // the modulator (the voice) and the carrier
                for (int i = 0; i < m; ++i) monoMod[(size_t) i] = md == voiceOnKey ? 0.5f * (key[0][off + i] + key[1][off + i]) : 0.5f * (ch[0][off + i] + ch[nch - 1][off + i]);
                core.analyse (monoMod.data(), m);
                if (md == buzz) buzzSetup (m);
                for (int i = 0; i < m; ++i)
                {
                    float c[2];
                    if (md == voiceOnKey) { c[0] = ch[0][off + i]; c[1] = ch[nch - 1][off + i]; }
                    else if (md == voiceOnTrack) { c[0] = key[0][off + i]; c[1] = key[1][off + i]; }
                    else { const float b = buzzSample(); c[0] = c[1] = b; }
                    modIn[(size_t) inW] = monoMod[(size_t) i];
                    for (int k = 0; k < 2; ++k) { carIn[(size_t) k][(size_t) inW] = c[k]; dry[(size_t) k][(size_t) dw] = md == buzz ? (k == 0 ? ch[0][off + i] : ch[nch - 1][off + i]) : c[k]; }
                    inW = (inW + 1) % N; dw = (dw + 1) % (2 * N);
                    if (++hopCount >= hop) { hopCount = 0; frame(); }
                    const int dr = ((dw - 1 - latencySamples()) % (2 * N) + 2 * N) % (2 * N);
                    const float out = dbToGain (p.outputDb);
                    for (int k = 0; k < nch; ++k)
                    {
                        const float y = outBuf[(size_t) k][(size_t) outR];
                        const float d = dry[(size_t) k][(size_t) dr];
                        ch[k][off + i] = (d + mx * (y - d)) * out;
                    }
                    outBuf[0][(size_t) outR] = 0; outBuf[1][(size_t) outR] = 0; outR = (outR + 1) % (2 * N);
                }
            }
        }

    private:
        double sr = 48000; int rangeSet = -1, N = 1024, hop = 256, eng = 1, block = 256, inW = 0, outR = 0, hopCount = 0, dw = 0;
        SqVoiceCore::Core core; std::vector<float> monoMod, win, modIn, envM, envSm, envC, tubeG; std::array<std::vector<float>, 2> carIn, outBuf, dry;
        std::vector<std::complex<float>> spec, cep, modSpec; float lvl = 0, gateG = 0; float tubeApplied = -1, brightApplied = -100;
        VoiceKit::Osc bo1, bo2; float buzzNote = 48, buzzCur = 48; bool haveNote = false; VoiceKit::Rand rng;
        std::array<std::atomic<float>, envPoints> liveM {}, liveC {}; std::atomic<float> sibNow { 0 }; std::atomic<int> modeNow { 0 };

        void buzzSetup (int m)
        {
            const auto& sn = core.latest();
            if (sn.voiced && sn.hz > 40.0f) { buzzNote = VoiceKit::hzNote (sn.hz) + 12.0f * (float) clamp (p.buzzOctave, -2, 1); if (! haveNote) { buzzCur = buzzNote; haveNote = true; } }
            juce::ignoreUnused (m);
        }
        float buzzSample()
        {
            const float g = p.glideMs <= 0.5f ? 1.0f : (float) (1.0 - std::exp (-1.0 / (p.glideMs * 0.001 * sr)));
            buzzCur += g * (buzzNote - buzzCur);
            const double dt = std::min (0.45, VoiceKit::noteHz (buzzCur) / sr);
            const float t = clamp (p.buzzTone, 0.0f, 1.0f);
            return 0.35f * ((1.0f - t) * bo1.saw (dt) + t * bo2.pulse (dt, 0.18f));
        }
        /** The cepstral (smoothed) log-magnitude envelope of spec into e, keeping lifter coefficients. */
        void envelope (const std::vector<std::complex<float>>& s, std::vector<float>& e, int lifter)
        {
            for (int b = 0; b < N; ++b) cep[(size_t) b] = std::log (std::abs (s[(size_t) b]) + 1.0e-6f);
            Murm::fft (cep.data(), N, true);
            for (int q = lifter; q <= N - lifter; ++q) cep[(size_t) q] = 0;
            Murm::fft (cep.data(), N, false);
            for (int b = 0; b <= N / 2; ++b) e[(size_t) b] = cep[(size_t) b].real();
        }
        void setupTube()
        {
            if (tubeApplied == p.tube && brightApplied == p.brightness) return;
            tubeApplied = p.tube; brightApplied = p.brightness;
            const float t = clamp (p.tube, 0.0f, 1.0f), tau = 1.0f / (2.0f * 230.0f), fc = 10000.0f - 5500.0f * t;
            for (int b = 0; b <= N / 2; ++b)
            {
                const float hz = (float) b * (float) sr / (float) N;
                const float ripple = 1.0f + 0.45f * t * std::cos (2.0f * (float) pi * hz * tau);
                const float lp = 1.0f / std::sqrt (1.0f + (hz / fc) * (hz / fc) * t * 1.5f);
                const float br = dbToGain (clamp (p.brightness * std::log2 (std::max (80.0f, hz) / 1000.0f), -9.0f, 7.0f));
                tubeG[(size_t) b] = ripple * lp * br;
            }
        }
        void frame()
        {
            setupTube();
            const int bins = N / 2 + 1; const float scale = (float) N / 1024.0f;
            // the mouth: the voice's envelope
            double eM = 0, eHi = 0;
            for (int i = 0; i < N; ++i) { const float v = modIn[(size_t) ((inW + i) % N)]; modSpec[(size_t) i] = v * win[(size_t) i]; eM += (double) v * v * win[(size_t) i] * win[(size_t) i]; }
            Murm::fft (modSpec.data(), N, false);
            const int hiBin = (int) (3000.0 * N / sr);
            double eAll = 0; for (int b = 1; b < bins; ++b) { const double pw = std::norm (modSpec[(size_t) b]); eAll += pw; if (b >= hiBin) eHi += pw; }
            envelope (modSpec, envM, std::max (6, (int) ((16.0f + 44.0f * clamp (p.precision, 0.0f, 1.0f)) * scale)));
            // formant shift and articulation (how fast the mouth moves)
            const float sh = std::pow (2.0f, -clamp (p.formantSt, -12.0f, 12.0f) / 12.0f);
            const float a = 1.0f - 0.85f * clamp (p.articulation, 0.0f, 1.0f);
            for (int b = 0; b < bins; ++b)
            {
                const float src = (float) b * sh; const int i0 = std::min ((int) src, bins - 2); const float f = std::min (1.0f, src - (float) i0);
                const float e = src < (float) (bins - 1) ? envM[(size_t) i0] + f * (envM[(size_t) i0 + 1] - envM[(size_t) i0]) : envM[(size_t) bins - 1] - 0.03f * (src - (float) (bins - 1));
                envSm[(size_t) b] += a * (e - envSm[(size_t) b]);
            }
            // the gate: a silent mouth closes the sound
            const float rmsDb = 10.0f * std::log10 ((float) (eM / (0.375 * N)) + 1.0e-12f);
            gateG += 0.4f * ((rmsDb > p.gateDb ? 1.0f : 0.0f) - gateG);
            const float sib = clamp (((float) (eHi / (eAll + 1.0e-12)) - 0.3f) / 0.4f, 0.0f, 1.0f) * clamp (p.sibilance, 0.0f, 1.0f);
            sibNow = sib;
            // the carrier: whitened by its own (smooth) envelope, from the mid of both sides
            for (int i = 0; i < N; ++i) { const float v = 0.5f * (carIn[0][(size_t) ((inW + i) % N)] + carIn[1][(size_t) ((inW + i) % N)]); spec[(size_t) i] = v * win[(size_t) i]; }
            Murm::fft (spec.data(), N, false);
            envelope (spec, envC, std::max (4, (int) (12.0f * scale)));
            const float mouth = clamp (p.mouth, 0.0f, 1.0f);
            // only the shapes are transferred here (each envelope about its own mean over 100 Hz - 8 kHz); the level follows below
            double mM = 0, mC = 0; int mN = 0;
            for (int b = std::max (1, (int) (100.0 * N / sr)); b < std::min (bins, (int) (8000.0 * N / sr)); ++b) { mM += envSm[(size_t) b]; mC += envC[(size_t) b]; ++mN; }
            const float meanM = (float) (mM / std::max (1, mN)), meanC = (float) (mC / std::max (1, mN));
            for (int k = 0; k < envPoints; ++k)
            {
                const int b = clamp ((int) (std::pow (2.0f, 5.5f * (float) k / (envPoints - 1)) * 80.0f * (float) N / (float) sr), 1, bins - 1);
                liveM[(size_t) k] = envSm[(size_t) b]; liveC[(size_t) k] = envC[(size_t) b];
            }
            double eOutTot = 0;
            for (int c = 0; c < 2; ++c)
            {
                for (int i = 0; i < N; ++i) spec[(size_t) i] = carIn[(size_t) c][(size_t) ((inW + i) % N)] * win[(size_t) i];
                Murm::fft (spec.data(), N, false);
                for (int b = 0; b < bins; ++b)
                {
                    const float g = std::exp (clamp (mouth * ((envSm[(size_t) b] - meanM) - 0.55f * (envC[(size_t) b] - meanC)), -7.0f, 2.3f)) * tubeG[(size_t) b];
                    auto v = spec[(size_t) b] * g;
                    const float hz = (float) b * (float) sr / (float) N;
                    if (sib > 0.001f && hz > 2500.0f) v += modSpec[(size_t) b] * (sib * 1.5f * clamp ((hz - 2500.0f) / 1500.0f, 0.0f, 1.0f));
                    spec[(size_t) b] = v;
                    if (b > 0 && b < N / 2) spec[(size_t) (N - b)] = std::conj (v);
                }
                Murm::fft (spec.data(), N, true);
                for (int i = 0; i < N; ++i) { const float v = spec[(size_t) i].real() * win[(size_t) i]; eOutTot += 0.5 * (double) v * v; tmp[(size_t) c][(size_t) i] = v; }
            }
            // amplitude transfer: the frame's level follows the voice (Follow), always under a +18 dB correction
            float want = 1;
            if (eOutTot > 1.0e-14) want = (float) clamp (std::sqrt (eM / eOutTot), 0.0, 4.0);
            // and its peaks stay near the voice's own (a whitened buzz can be far peakier than the voice it copies)
            float pkM = 0, pkO = 0;
            for (int i = 0; i < N; ++i) { pkM = std::max (pkM, std::abs (modIn[(size_t) ((inW + i) % N)]) * win[(size_t) i]); pkO = std::max (pkO, std::max (std::abs (tmp[0][(size_t) i]), std::abs (tmp[1][(size_t) i]))); }
            if (pkO * want > 1.6f * pkM && pkO > 1.0e-9f) want = 1.6f * pkM / pkO;
            const float f = clamp (p.follow, 0.0f, 1.0f);
            const float target = (f * want + (1.0f - f)) * gateG;
            lvl += 0.5f * (target - lvl);
            const float g = lvl / 1.5f;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) outBuf[(size_t) c][(size_t) ((outR + i) % (2 * N))] += tmp[(size_t) c][(size_t) i] * g;
        }
        std::array<std::array<float, maxN>, 2> tmp {};
    };
}

//==============================================================================================================
namespace CleanRoom
{
    using namespace Eng;
    constexpr int maxN = 2048, profileBands = 96, maxHarm = 24, clickDelay = 128;
    inline const char* humName (int h) { static const char* n[] = { "Auto", "50 Hz", "60 Hz" }; return n[clamp (h, 0, 2)]; }
    struct Params
    {
        int engine = 1, humMode = 0; bool residual = false;
        float clean = 0.5f, hum = 0.8f, buzz = 0.5f, hiss = 0.6f, noise = 0.7f, room = 0.4f, clicks = 0.5f, roomDecay = 0.8f, guard = 0.5f, mix = 1, outputDb = 0;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; eng = p.engine == 0 ? 0 : 1; N = eng == 0 ? 512 : 2048; hop = N / 4;
            juce::ignoreUnused (maxBlock);
            win.resize ((size_t) N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) pi * i / N);
            const int bins = N / 2 + 1;
            for (auto& b : in) b.assign ((size_t) N, 0.0f);
            for (auto& b : outBuf) b.assign ((size_t) N * 2, 0.0f);
            const int dsz = N * 2 + clickDelay * 2; for (auto& b : dryH) b.assign ((size_t) dsz, 0.0f);
            for (auto& b : cd) b.assign ((size_t) clickDelay * 2, 0.0f);
            for (auto& s : spec) s.assign ((size_t) N, {});
            psm.assign ((size_t) bins, 0.0f); pmin.assign ((size_t) bins, 1.0e-3f); subMin.assign ((size_t) bins, 1.0e9f); noiseEst.assign ((size_t) bins, 0.0f); rawMin.assign ((size_t) bins, 0.0f); pNow.assign ((size_t) bins, 0.0f);
            for (auto& m : mins) m.assign ((size_t) bins, 1.0e9f);
            gPrev.assign ((size_t) bins, 1.0f); pPrev.assign ((size_t) bins, 0.0f); gNow.assign ((size_t) bins, 1.0f); learnSum.assign ((size_t) bins, 0.0);
            for (auto& h : hist) h.assign ((size_t) bins, 0.0f);
            hpClick.set (Biquad::highpass, sr, 2500.0, 0.707); clickDecay = (float) std::exp (-1.0 / (0.02 * sr));
            for (auto& g : gz) g = {};
            presK = (float) (1.0 - std::exp (-1.0 / (0.35 * sr)));
            mains = 60; mainsSet = -1; setNotches();
            reset();
        }
        void reset()
        {
            for (auto& b : in) std::fill (b.begin(), b.end(), 0.0f); for (auto& b : outBuf) std::fill (b.begin(), b.end(), 0.0f);
            for (auto& b : dryH) std::fill (b.begin(), b.end(), 0.0f); for (auto& b : cd) std::fill (b.begin(), b.end(), 0.0f);
            std::fill (psm.begin(), psm.end(), 0.0f); std::fill (pmin.begin(), pmin.end(), 1.0e-3f); std::fill (subMin.begin(), subMin.end(), 1.0e9f);
            for (auto& m : mins) std::fill (m.begin(), m.end(), 1.0e9f); std::fill (gPrev.begin(), gPrev.end(), 1.0f); std::fill (pPrev.begin(), pPrev.end(), 0.0f);
            for (auto& h : hist) std::fill (h.begin(), h.end(), 0.0f);
            for (auto& c : notch) for (auto& f : c) f.reset(); for (auto& f : bpN) f.reset(); for (auto& f : bpW) f.reset(); eN.fill (0); eW.fill (0); presence.fill (0); hpClick.reset();
            inW = outR = hopCount = dw = cw = 0; frames = 0; subCount = 0; subIdx = 0; histW = 0; clickPk = 0; pending = -1; clickCount = 0; gzCount = 0;
            for (auto& g : gz) g = {};
        }
        bool needsPrepare() const { return (p.engine == 0 ? 0 : 1) != eng; }
        int latencySamples() const { return N - 1 + clickDelay; }
        static int latencyFor (int engine) { return (engine == 0 ? 512 : 2048) - 1 + clickDelay; }
        int mainsHz() const { return mains; }
        float humPresence (int h) const { return presence[(size_t) clamp (h, 0, maxHarm - 1)]; }
        int clicksRepaired() const { return clickCount.load(); }
        bool hasProfile() const { return profileOn; }
        bool learning() const { return learn.load(); }
        float reductionDb (int band) const { return liveRed[(size_t) clamp (band, 0, profileBands - 1)].load(); }
        float noiseDb (int band) const { return liveNoise[(size_t) clamp (band, 0, profileBands - 1)].load(); }
        float inputDb (int band) const { return liveIn[(size_t) clamp (band, 0, profileBands - 1)].load(); }
        static float bandHz (int band) { return 40.0f * std::pow (2.0f, 9.0f * (float) band / (profileBands - 1)); }

        /** Learn the noise from what plays now (start), then use it (stop). */
        void startLearning() { learnReq = 1; }
        void stopLearning()  { learnReq = 2; }
        /** The learned profile as 96 band levels (dB), for saving with the song; set it back with setProfile. */
        juce::String profileText() const
        {
            if (! profileOn) return {};
            juce::StringArray a; for (int k = 0; k < profileBands; ++k) a.add (juce::String (profDb[(size_t) k], 1)); return a.joinIntoString (",");
        }
        void setProfile (const juce::String& s)
        {
            juce::StringArray a; a.addTokens (s, ",", ""); a.removeEmptyStrings();
            if (a.size() != profileBands) { profileOn = false; return; }
            for (int k = 0; k < profileBands; ++k) profDb[(size_t) k] = a[k].getFloatValue();
            profileDirty = true; profileOn = true;
        }
        void clearProfile() { profileOn = false; }

        void process (float* const* ch, int nch, const float* const*, int n) { process (ch, nch, n); }
        void process (float* const* ch, int nch, int n)
        {
            nch = clamp (nch, 1, 2);
            if (p.humMode != mainsSet) { mainsSet = p.humMode; if (p.humMode == 1) mains = 50; else if (p.humMode == 2) mains = 60; setNotches(); }
            if (learnReq == 1) { std::fill (learnSum.begin(), learnSum.end(), 0.0); learnFrames = 0; learn = true; learnReq = 0; }
            if (learnReq == 2) { learn = false; learnReq = 0; if (learnFrames > 4) finishLearn(); }
            const float cl = clamp (p.clean, 0.0f, 1.0f) * 2.0f;
            const float aHum = clamp (p.hum * cl, 0.0f, 1.0f), aBuzz = clamp (p.buzz * cl, 0.0f, 1.0f), aClick = clamp (p.clicks * cl, 0.0f, 1.0f);
            const float mx = clamp (p.mix, 0.0f, 1.0f), out = dbToGain (p.outputDb);
            const int dsz = (int) dryH[0].size(), csz = (int) cd[0].size();
            for (int i = 0; i < n; ++i)
            {
                float x[2] = { ch[0][i], ch[nch - 1][i] };
                for (int c = 0; c < 2; ++c) dryH[(size_t) c][(size_t) dw] = x[c];
                if (p.humMode == 0) detectMains (0.5f * (x[0] + x[1]));
                // hum and buzz: notches on the mains and its harmonics, each blended by its amount
                if (aHum > 0.001f || aBuzz > 0.001f)
                {
                    // each harmonic is notched only as far as it is really there: a steady tone fills its narrow band as
                    // much as the wide one around it, a voice (or noise) does not
                    const float xm = 0.5f * (x[0] + x[1]);
                    for (int h = 0; h < nHarm; ++h)
                    {
                        const float a = h < 4 ? aHum : aBuzz;
                        if (a <= 0.001f) { presence[(size_t) h] = 0; continue; }
                        const float yn = bpN[(size_t) h].process (xm), yw = bpW[(size_t) h].process (xm);
                        eN[(size_t) h] += presK * (yn * yn - eN[(size_t) h]); eW[(size_t) h] += presK * (yw * yw - eW[(size_t) h]);
                        presence[(size_t) h] = clamp ((eN[(size_t) h] / (eW[(size_t) h] + 1.0e-14f) - 0.2f) / 0.45f, 0.0f, 1.0f) * (eW[(size_t) h] > 1.0e-11f ? 1.0f : 0.0f);
                    }
                    for (int c = 0; c < 2; ++c)
                        for (int h = 0; h < nHarm; ++h)
                        {
                            const float a = (h < 4 ? aHum : aBuzz) * presence[(size_t) h];
                            if (a <= 0.001f) continue;
                            const float y = notch[(size_t) c][(size_t) h].process (x[c]);
                            x[c] = x[c] + a * (y - x[c]);
                        }
                }
                // clicks: an isolated spike on the high band, far above the peaks of the last 20 ms (a voice's own pulses
                // repeat at about the same height every period, a click does not); repaired across the gap 128 samples later
                for (int c = 0; c < 2; ++c) cd[(size_t) c][(size_t) cw] = x[c];
                if (aClick > 0.001f)
                {
                    const float hp = std::abs (hpClick.process (0.5f * (x[0] + x[1])));
                    const float thr = (3.4f - 1.5f * aClick) * clickPk + 0.003f;
                    if (pending < 0 && hp > thr && clickPk > 0) { pending = 40; clickAt = cw; clickThr = thr; clickLong = 0; }
                    else if (pending < 0) clickPk = std::max (hp, clickPk * clickDecay);
                    else if (pending < 34 && hp > 0.5f * clickThr) ++clickLong;          // still loud after 0.15 ms: a drum hit or a consonant, not a click
                    if (pending >= 0 && --pending < 0) { if (clickLong <= 4) repair (csz); clickPk = std::max (clickPk, hp); }
                }
                const int rd = (cw - clickDelay + csz) % csz;
                float y[2] = { cd[0][(size_t) rd], cd[1][(size_t) rd] };
                cw = (cw + 1) % csz;
                // the spectral stage
                for (int c = 0; c < 2; ++c) in[(size_t) c][(size_t) inW] = y[c];
                inW = (inW + 1) % N;
                if (++hopCount >= hop) { hopCount = 0; frame(); }
                const int dr = ((dw - latencySamples()) % dsz + dsz) % dsz;
                for (int c = 0; c < nch; ++c)
                {
                    const float wet = outBuf[(size_t) c][(size_t) outR], d = dryH[(size_t) c][(size_t) dr];
                    const float o = p.residual ? d - wet : wet;
                    ch[c][i] = (d + mx * (o - d)) * out;
                }
                outBuf[0][(size_t) outR] = 0; outBuf[1][(size_t) outR] = 0; outR = (outR + 1) % (2 * N);
                dw = (dw + 1) % dsz;
            }
        }

    private:
        double sr = 48000; int eng = 1, N = 2048, hop = 512, inW = 0, outR = 0, hopCount = 0, dw = 0, cw = 0;
        std::vector<float> win; std::array<std::vector<float>, 2> in, outBuf, dryH, cd; std::array<std::vector<std::complex<float>>, 2> spec;
        std::vector<float> psm, pmin, subMin, noiseEst, gPrev, pPrev, gNow, rawMin, pNow; std::array<std::vector<float>, 8> mins, hist; std::vector<double> learnSum;
        int frames = 0, subCount = 0, subIdx = 0, histW = 0, learnFrames = 0, mains = 60, mainsSet = -1, nHarm = 0, pending = -1, clickAt = 0;
        std::atomic<int> learnReq { 0 }; std::atomic<bool> learn { false }; bool profileOn = false, profileDirty = false; std::array<float, profileBands> profDb {}; std::vector<float> profBins;
        std::array<std::array<Biquad, maxHarm>, 2> notch; std::array<Biquad, maxHarm> bpN, bpW; std::array<float, maxHarm> eN {}, eW {}, presence {}; float presK = 1.0e-4f; Biquad hpClick; float clickPk = 0, clickDecay = 0.999f, clickThr = 0; int clickLong = 0; std::atomic<int> clickCount { 0 };
        struct Gz { double s1 = 0, s2 = 0, e = 0; }; std::array<Gz, 6> gz; int gzCount = 0;
        std::array<std::atomic<float>, profileBands> liveRed {}, liveNoise {}, liveIn {};

        void setNotches()
        {
            nHarm = 0;
            for (int h = 1; h <= maxHarm; ++h)
            {
                const double f = (double) mains * h; if (f > sr * 0.45) break;
                for (int c = 0; c < 2; ++c) notch[(size_t) c][(size_t) h - 1].set (Biquad::notch, sr, f, h == 1 ? 14.0 : (h <= 4 ? 40.0 : 50.0));
                bpN[(size_t) h - 1].set (Biquad::bandpass, sr, f, 60.0); bpW[(size_t) h - 1].set (Biquad::bandpass, sr, f, 4.0);
                nHarm = h;
            }
        }
        /** Every 0.5 s: is the hum at 50 Hz (and 100, 150) or at 60 Hz (and 120, 180)? */
        void detectMains (float x)
        {
            static const double fs[6] = { 50, 100, 150, 60, 120, 180 };
            for (int k = 0; k < 6; ++k)
            {
                auto& g = gz[(size_t) k]; const double c = 2.0 * std::cos (2.0 * pi * fs[k] / sr);
                const double s0 = x + c * g.s1 - g.s2; g.s2 = g.s1; g.s1 = s0;
            }
            if (++gzCount >= (int) (0.5 * sr))
            {
                double e50 = 0, e60 = 0;
                for (int k = 0; k < 6; ++k) { auto& g = gz[(size_t) k]; const double c = 2.0 * std::cos (2.0 * pi * fs[k] / sr); const double pw = g.s1 * g.s1 + g.s2 * g.s2 - c * g.s1 * g.s2; (k < 3 ? e50 : e60) += pw; g.s1 = g.s2 = 0; }
                if (e50 > 3.0 * e60 && mains != 50) { mains = 50; setNotches(); }
                else if (e60 > 3.0 * e50 && mains != 60) { mains = 60; setNotches(); }
                gzCount = 0;
            }
        }
        /** Replaces the clicked span (16 samples before the hit to 40 after) with a straight line between its edges. */
        void repair (int csz)
        {
            const int a = (clickAt - 16 + csz) % csz, b = (clickAt + 40) % csz, len = 56;
            for (int c = 0; c < 2; ++c)
            {
                auto& buf = cd[(size_t) c];
                const float y0 = buf[(size_t) ((a - 1 + csz) % csz)], y1 = buf[(size_t) b];
                for (int k = 0; k < len; ++k) { const float t = (float) (k + 1) / (float) (len + 1); buf[(size_t) ((a + k) % csz)] = y0 + t * (y1 - y0); }
            }
            clickCount = clickCount.load() + 1;
        }
        void finishLearn()
        {
            const int bins = N / 2 + 1;
            for (int k = 0; k < profileBands; ++k)
            {
                const int b = clamp ((int) (bandHz (k) * (float) N / (float) sr), 1, bins - 1);
                double s = 0; int c = 0; for (int j = std::max (1, b - 1); j <= std::min (bins - 1, b + 1); ++j) { s += learnSum[(size_t) j]; ++c; }
                profDb[(size_t) k] = 10.0f * std::log10 ((float) (s / std::max (1, c) / learnFrames) + 1.0e-20f);
            }
            profileDirty = true; profileOn = true;
        }
        void profileToBins()
        {
            const int bins = N / 2 + 1; profBins.assign ((size_t) bins, 0.0f);
            for (int b = 0; b < bins; ++b)
            {
                const float hz = std::max (40.0f, (float) b * (float) sr / (float) N);
                const float pos = clamp (std::log2 (hz / 40.0f) / 9.0f * (profileBands - 1), 0.0f, (float) (profileBands - 1));
                const int i0 = std::min ((int) pos, profileBands - 2); const float f = pos - (float) i0;
                profBins[(size_t) b] = std::pow (10.0f, 0.1f * (profDb[(size_t) i0] + f * (profDb[(size_t) i0 + 1] - profDb[(size_t) i0])));
            }
            profileDirty = false;
        }
        void frame()
        {
            const int bins = N / 2 + 1;
            for (int c = 0; c < 2; ++c)
            {
                for (int i = 0; i < N; ++i) spec[(size_t) c][(size_t) i] = in[(size_t) c][(size_t) ((inW + i) % N)] * win[(size_t) i];
                Murm::fft (spec[(size_t) c].data(), N, false);
            }
            const float cl = clamp (p.clean, 0.0f, 1.0f) * 2.0f;
            const float aNoise = clamp (p.noise * cl, 0.0f, 1.5f), aHiss = clamp (p.hiss * cl, 0.0f, 1.5f), aRoom = clamp (p.room * cl, 0.0f, 1.5f);
            const float guard = clamp (p.guard, 0.0f, 1.0f);
            const float floorG = dbToGain (-34.0f + 22.0f * guard);
            // the late reverb: what the room still holds of the sound Td ago, decayed by the decay time
            const double td = 0.05; const int dFr = clamp ((int) std::round (td * sr / hop), 1, 7);
            const double delta = 3.0 * std::log (10.0) / clamp ((double) p.roomDecay, 0.15, 4.0);
            const float lateK = (float) std::exp (-2.0 * delta * td), tailDb = std::min (-0.5f, 10.0f * std::log10 (lateK));
            if (profileDirty) profileToBins();
            // minimum statistics: the noise floor is the lowest smoothed power over ~1.5 s (8 sub-windows)
            const int subLen = std::max (1, (int) (0.19 * sr / hop));
            const auto& late = hist[(size_t) ((histW - dFr + 8) % 8)];
            for (int b = 0; b < bins; ++b)
            {
                const float P = 0.5f * (std::norm (spec[0][(size_t) b]) + std::norm (spec[1][(size_t) b]));
                pNow[(size_t) b] = P;
                if (learn) learnSum[(size_t) b] += P;
                psm[(size_t) b] = frames == 0 ? P : 0.8f * psm[(size_t) b] + 0.2f * P;
                subMin[(size_t) b] = std::min (subMin[(size_t) b], psm[(size_t) b]);
                float N0 = 1.0e9f; for (auto& m : mins) N0 = std::min (N0, m[(size_t) b]); N0 = std::min (N0, subMin[(size_t) b]);
                rawMin[(size_t) b] = N0;
            }
            // the floor's lower envelope across frequency: a held note's harmonics are narrow peaks that minimum
            // statistics would take for noise if the note lasts; noise is broad, so the lowest value within +-70 Hz is kept
            const int rad = std::max (1, (int) std::round (70.0 * N / sr));
            for (int b = 0; b < bins; ++b)
            {
                float m = rawMin[(size_t) b]; for (int j = std::max (0, b - rad); j <= std::min (bins - 1, b + rad); ++j) m = std::min (m, rawMin[(size_t) j]);
                noiseEst[(size_t) b] = (profileOn && ! profBins.empty() ? profBins[(size_t) b] : m * 2.2f);
            }
            for (int b = 0; b < bins; ++b)
            {
                const float P = pNow[(size_t) b];
                const float hz = (float) b * (float) sr / (float) N;
                const float hf = clamp ((hz - 2500.0f) / 4500.0f, 0.0f, 1.0f);
                // the room only where the sound is dying away at the room's rate: a held or rising sound is the singer
                const float decayDb = 10.0f * std::log10 ((psm[(size_t) b] + 1.0e-20f) / (late[(size_t) b] + 1.0e-20f));
                const float tailW = clamp (decayDb / tailDb, 0.0f, 1.0f);
                const float I = noiseEst[(size_t) b] * (2.2f * aNoise + 4.0f * aHiss * hf) + aRoom * 1.6f * lateK * late[(size_t) b] * tailW;
                // decision-directed a-priori SNR (Ephraim-Malah), Wiener gain
                const float gamma = P / (I + 1.0e-20f);
                const float xi = 0.96f * gPrev[(size_t) b] * gPrev[(size_t) b] * pPrev[(size_t) b] / (I + 1.0e-20f) + 0.04f * std::max (gamma - 1.0f, 0.0f);
                float G = I <= 1.0e-20f ? 1.0f : xi / (1.0f + xi);
                G = std::max (G, floorG);
                gNow[(size_t) b] = G; pPrev[(size_t) b] = P;
            }
            if (learn) ++learnFrames;
            ++frames;
            if (++subCount >= subLen) { subCount = 0; mins[(size_t) subIdx] = subMin; subIdx = (subIdx + 1) % 8; std::fill (subMin.begin(), subMin.end(), 1.0e9f); }
            // artefact guard: smooth the gains across neighbouring bins and in time (gain may fall fast, rises a little slower)
            const float rise = 0.9f - 0.35f * guard;
            for (int b = 0; b < bins; ++b)
            {
                const float s = 0.25f * gNow[(size_t) std::max (0, b - 1)] + 0.5f * gNow[(size_t) b] + 0.25f * gNow[(size_t) std::min (bins - 1, b + 1)];
                const float g = s > gPrev[(size_t) b] ? gPrev[(size_t) b] + rise * (s - gPrev[(size_t) b]) : s;
                gPrev[(size_t) b] = g;
            }
            // history of the cleaned power, for the late-reverb estimate
            auto& h = hist[(size_t) histW];
            for (int b = 0; b < bins; ++b) h[(size_t) b] = psm[(size_t) b];
            histW = (histW + 1) % 8;
            // the face: reduction, noise floor and input in 96 bands
            for (int k = 0; k < profileBands; ++k)
            {
                const int b = clamp ((int) (bandHz (k) * (float) N / (float) sr), 1, bins - 1);
                liveRed[(size_t) k] = gainToDb (gPrev[(size_t) b]);
                liveNoise[(size_t) k] = 10.0f * std::log10 (noiseEst[(size_t) b] / (0.375f * N * N) + 1.0e-20f) + 3.0f;
                liveIn[(size_t) k] = 10.0f * std::log10 (psm[(size_t) b] / (0.375f * N * N) + 1.0e-20f) + 3.0f;
            }
            const float norm = 1.0f / 1.5f;
            for (int c = 0; c < 2; ++c)
            {
                auto& s = spec[(size_t) c];
                for (int b = 0; b < bins; ++b) { s[(size_t) b] *= gPrev[(size_t) b]; if (b > 0 && b < N / 2) s[(size_t) (N - b)] = std::conj (s[(size_t) b]); }
                Murm::fft (s.data(), N, true);
                for (int i = 0; i < N; ++i) outBuf[(size_t) c][(size_t) ((outR + i) % (2 * N))] += s[(size_t) i].real() * win[(size_t) i] * norm;
            }
        }
    };
}

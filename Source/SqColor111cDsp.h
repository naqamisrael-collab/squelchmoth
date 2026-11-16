#pragma once
// Squelchmoth 1.11 Color & Space, second wave, part 2 (super prompt C3, C5). Standard C++ only, measured in isolation by
// tests/color111c_test.cpp. (Masquerade, C15, runs on Moth Tune's engine: SqMasqueradeDsp.h.)
//
//   Shard  granular reverse pitch echo: splices up to 8 s read forward or reversed (per-splice probability) after a
//          delay up to 8 s, pitched +-48 st by a Vintage resampling engine (artefacts kept) or a Clean micro-grain
//          engine (time kept, pitch moved), grain spray / density / size jitter, Recycle (Mixed / Dual / Ping-Pong) with
//          low / high cut, scale-quantised pitch, Gate / Duck with a key, L/R offsets, two LFOs and an envelope in a
//          modulation matrix
//   Murm   tap-cloud space (Murmuration): 256-2048 taps scattered over 10 ms - 90 s, an envelope drawn by X/Y (or a
//          custom 16-point shape), Texture from discrete echoes to a dense wash, Warp (tape-style glide on a time change),
//          Color that tilts further along the tail, editable Mod, Shimmer +-12 / +-24 st with fade-in, Blur, Grain,
//          loop or spectral Freeze, an Infinite tail that still takes input, and a Tweak panel (tilt, LP, HP, width,
//          ducking, pre-delay)
//
// Real-time safe after prepare(): no allocation, no locks on the audio thread.

#include "SqColor111bDsp.h"
#include <complex>

//==============================================================================================================
namespace Shard
{
    using namespace Eng;
    enum Engine { vintage, clean };
    enum FbMode { mixed, dual, pingPong };
    enum Dyn { dynOff, dynGate, dynDuck };
    enum Target { tNone, tPitch, tSplice, tDelay, tRecycle, tSpray, tMix, tReverse, numTargets };
    inline const char* targetName (int t) { static const char* n[] = { "Off", "Pitch", "Splice", "Delay", "Recycle", "Spray", "Mix", "Reverse" }; return n[clamp (t, 0, numTargets - 1)]; }
    inline const char* engineName (int e) { return e == clean ? "Clean" : "Vintage"; }
    inline const char* fbName (int f)     { static const char* n[] = { "Mixed", "Dual", "Ping-Pong" }; return n[clamp (f, 0, 2)]; }
    constexpr double maxSpliceMs = 8000, maxDelayMs = 8000;

    struct Lfo { int target = tNone; float depth = 0, rateHz = 0.5f; int shape = WingMod::sine; bool sync = false; int syncIndex = 12; };
    struct Params
    {
        float spliceMs = 250, delayMs = 0, pitchSt = 12, recycle = 0.3f, mix = 0.5f;
        int engine = vintage; float reverseProb = 1;
        int spliceSync = -1, delaySync = -1;                  // WingMod division or -1 = free
        float smoothing = 0.25f, spray = 0, density = 1, sizeJitter = 0;
        float pitchOffsetLCents = 0, pitchOffsetRCents = 0, spliceOffsetR = 0, delayOffsetRMs = 0;
        float lowCutHz = 20, highCutHz = 20000; int fbMode = mixed;
        int dyn = dynOff; float dynThreshDb = -30, dynReleaseMs = 200; bool keyOn = false;
        bool quantize = false; int keyRoot = 0, scaleMask = 0xAB5;
        Lfo lfo[2]; int envTarget = tNone; float envDepth = 0, envAttackMs = 10, envReleaseMs = 250;
        float outputDb = 0;
    };

    /** The interval the splices are pitched to: with Quantize, the nearest semitone that lands in the key (the input
        is taken as sitting on the root). */
    inline float quantisedPitch (float st, bool q, int root, int mask)
    {
        if (! q || (mask & 0xfff) == 0) return st;
        const int c = (int) std::lround (st); int best = c; float bd = 1e9f;
        for (int d = -12; d <= 12; ++d)
        {
            const int n = c + d, pc = (((root + n) % 12) + 12) % 12;
            if (((mask >> pc) & 1) && std::abs ((float) n - st) < bd) { bd = std::abs ((float) n - st); best = n; }
        }
        return (float) best;
    }

    class Processor
    {
    public:
        Params p;
        static constexpr int maxGrains = 24;
        void prepare (double sampleRate, int)
        {
            sr = sampleRate;
            size = (int) (sr * 20.0) + 16;
            for (auto& b : buf) b.assign ((size_t) size, 0.0f);
            reset();
        }
        void reset()
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            w = 0; for (auto& c : grains) for (auto& g : c) g.active = false;
            countdown[0] = countdown[1] = 0; fb[0] = fb[1] = 0; env = 0; dynEnv = 0; dynG = 1; rnd = {}; lfoPh[0] = lfoPh[1] = 0; spawned = 0;
            for (auto& c : flt) for (auto& f : c) f.reset();
            lastLc = lastHc = -1;
        }
        void setTransport (double bpm, double ppqAtStart, bool isPlaying) { tBpm = bpm > 1 ? bpm : 120; tPpq = ppqAtStart; playing = isPlaying; }
        int activeGrains() const { int a = 0; for (auto& c : grains) for (auto& g : c) a += g.active ? 1 : 0; return a; }
        long long splicesStarted() const { return spawned; }
        float modValue (int k) const { return modNow[clamp (k, 0, 2)]; }
        double spliceMsNow() const { return spliceNow; }
        double delayMsNow() const { return delayNow; }
        float pitchNow() const { return pitchNowSt; }
        float wetGateGain() const { return dynG; }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            if (p.lowCutHz != lastLc || p.highCutHz != lastHc)
            {
                lastLc = p.lowCutHz; lastHc = p.highCutHz;
                for (int c = 0; c < 2; ++c) { flt[c][0].set (Biquad::highpass, sr, std::max (1.0f, p.lowCutHz), 0.7071); flt[c][1].set (Biquad::lowpass, sr, std::min (20000.0f, p.highCutHz), 0.7071); }
            }
            // ---- modulation matrix (control rate: once per block)
            float mod[numTargets] {};
            const float blockSec = (float) n / (float) sr;
            for (int k = 0; k < 2; ++k)
            {
                const auto& l = p.lfo[k];
                double phs;
                if (l.sync && playing) phs = tPpq / WingMod::syncBeats (l.syncIndex);
                else { lfoPh[k] += (l.sync ? tBpm / 60.0 / WingMod::syncBeats (l.syncIndex) : (double) l.rateHz) * blockSec; lfoPh[k] -= std::floor (lfoPh[k]); phs = lfoPh[k]; }
                long long idx = (long long) std::floor (phs);
                if (idx != lfoCycle[k]) { lfoCycle[k] = idx; lfoHeld[k] = rnd.next(); }
                const float v = ModShapes::shape (l.shape, (float) (phs - std::floor (phs)), 0.5f, defaultCustom(), 16, lfoHeld[k]);
                modNow[k] = v;
                if (l.target > tNone && l.target < numTargets) mod[l.target] += l.depth * v;
            }
            // envelope follower (input or key)
            {
                float pk = 0; for (int i = 0; i < n; ++i) for (int c = 0; c < nch; ++c) pk = std::max (pk, std::abs ((key != nullptr && p.keyOn) ? key[c][i] : ch[c][i]));
                const float a = 1.0f - std::exp (-blockSec * 1000.0f / std::max (0.1f, p.envAttackMs)), r = 1.0f - std::exp (-blockSec * 1000.0f / std::max (1.0f, p.envReleaseMs));
                env += (pk > env ? a : r) * (pk - env);
                const float e = clamp ((gainToDb (env) + 48.0f) / 48.0f, 0.0f, 1.0f);
                modNow[2] = e;
                if (p.envTarget > tNone && p.envTarget < numTargets) mod[p.envTarget] += p.envDepth * e;
            }
            const double beatMs = 60000.0 / tBpm;
            double splice = p.spliceSync >= 0 ? WingMod::syncBeats (p.spliceSync) * beatMs : (double) p.spliceMs;
            double delay  = p.delaySync  >= 0 ? WingMod::syncBeats (p.delaySync)  * beatMs : (double) p.delayMs;
            splice = clamp (splice * (1.0 + mod[tSplice]), 10.0, maxSpliceMs);
            delay = clamp (delay * (1.0 + mod[tDelay]) + (mod[tDelay] != 0 && delay < 1 ? 200.0 * mod[tDelay] : 0.0), 0.0, maxDelayMs);
            const float pitch = clamp (quantisedPitch (p.pitchSt + 24.0f * mod[tPitch], p.quantize, p.keyRoot, p.scaleMask), -48.0f, 48.0f);
            const float recycle = clamp (p.recycle + 0.5f * mod[tRecycle], 0.0f, 0.99f);
            const float spray = clamp (p.spray + mod[tSpray], 0.0f, 1.0f), mixV = clamp (p.mix + mod[tMix], 0.0f, 1.0f), revP = clamp (p.reverseProb + mod[tReverse], 0.0f, 1.0f);
            spliceNow = splice; delayNow = delay; pitchNowSt = pitch;
            // ---- gate / duck
            const float thr = dbToGain (p.dynThreshDb), rel = (float) (1.0 - std::exp (-1.0 / (std::max (5.0f, p.dynReleaseMs) * 0.001 * sr))), att = (float) (1.0 - std::exp (-1.0 / (0.003 * sr)));
            const float out = dbToGain (p.outputDb);
            for (int i = 0; i < n; ++i)
            {
                const float in0 = ch[0][i], in1 = nch > 1 ? ch[1][i] : in0;
                // write the input plus the recycled wet
                float f0 = fb[0], f1 = fb[1];
                if (p.fbMode == mixed) f0 = f1 = 0.5f * (fb[0] + fb[1]); else if (p.fbMode == pingPong) std::swap (f0, f1);
                buf[0][(size_t) w] = in0 + recycle * soft (f0);
                buf[1][(size_t) w] = in1 + recycle * soft (f1);
                // schedule splices
                for (int c = 0; c < 2; ++c)
                {
                    if (--countdown[c] > 0) continue;
                    const double sMs = splice * (c == 1 ? 1.0 + clamp (p.spliceOffsetR, -0.5f, 0.5f) : 1.0);
                    const double dMs = std::max (0.0, delay + (c == 1 ? p.delayOffsetRMs : 0.0));
                    const float cents = c == 0 ? p.pitchOffsetLCents : p.pitchOffsetRCents;
                    const double period = sMs * 0.001 * sr / clamp (p.density, 1.0f, 4.0f);
                    countdown[c] = std::max (16, (int) (period * (1.0 + p.sizeJitter * 0.5 * rnd.next())));
                    spawn (c, sMs, dMs, std::pow (2.0, (pitch + cents * 0.01) / 12.0), rnd.next() * 0.5f + 0.5f < revP, spray);
                }
                // render the grains
                float wet[2] = { 0, 0 };
                const float norm = 1.0f / std::sqrt (clamp (p.density, 1.0f, 4.0f));
                for (int c = 0; c < 2; ++c)
                    for (auto& g : grains[c])
                        if (g.active) wet[c] += render (c, g) * norm;
                // gate / duck (the input or the key)
                const float det = (key != nullptr && p.keyOn) ? std::max (std::abs (key[0][i]), std::abs (key[1][i])) : std::max (std::abs (in0), std::abs (in1));
                dynEnv += (det > dynEnv ? att : rel) * (det - dynEnv);
                if (p.dyn == dynGate) { const float want = dynEnv > thr ? 1.0f : 0.0f; dynG += (want > dynG ? att : rel) * (want - dynG); }
                else if (p.dyn == dynDuck) dynG = dbToGain (-std::min (24.0f, std::max (0.0f, 2.0f * (gainToDb (dynEnv) - p.dynThreshDb))));
                else dynG = 1;
                for (int c = 0; c < 2; ++c)
                {
                    if (p.lowCutHz > 5.5f) wet[c] = flt[c][0].process (wet[c]);
                    if (p.highCutHz < 19900.0f) wet[c] = flt[c][1].process (wet[c]);
                    fb[c] = undenorm (wet[c]);
                }
                ch[0][i] = (in0 + mixV * (wet[0] * dynG - in0)) * out;
                if (nch > 1) ch[1][i] = (in1 + mixV * (wet[1] * dynG - in1)) * out;
                w = (w + 1) % size;
            }
        }

    private:
        struct Micro { double start = 0; int age = -1; };
        struct Grain { bool active = false, reverse = false; double pos = 0, step = 1, src = 0, phi = 0; int age = 0, len = 0, fade = 1; float ratio = 1; Micro mg[2]; int nextMg = 0, mgCount = 0; };
        double sr = 48000, tBpm = 120, tPpq = 0, lfoPh[2] {}; bool playing = false;
        std::vector<float> buf[2]; int size = 1, w = 0, countdown[2] {};
        Grain grains[2][maxGrains];
        float fb[2] {}, env = 0, dynEnv = 0, dynG = 1, modNow[3] {}, lfoHeld[2] {}, lastLc = -1, lastHc = -1, pitchNowSt = 0; long long lfoCycle[2] { -1, -1 }, spawned = 0;
        double spliceNow = 0, delayNow = 0;
        Biquad flt[2][2]; ModShapes::Rand rnd;
        static const float* defaultCustom() { static const float c[16] { 0, .38f, .71f, .92f, 1, .92f, .71f, .38f, 0, -.38f, -.71f, -.92f, -1, -.92f, -.71f, -.38f }; return c; }
        static float soft (float x) { return std::abs (x) < 0.8f ? x : (x > 0 ? 1.0f : -1.0f) * (0.8f + 0.2f * std::tanh ((std::abs (x) - 0.8f) / 0.2f)); }

        float at (int c, double pos) const
        {
            while (pos < 0) pos += size; while (pos >= size) pos -= size;
            const int i0 = (int) pos; const float fr = (float) (pos - i0);
            const float a = buf[c][(size_t) i0], b = buf[c][(size_t) ((i0 + 1) % size)];
            return a + (b - a) * fr;
        }
        void spawn (int c, double sMs, double dMs, double ratio, bool reverse, float spray)
        {
            Grain* g = nullptr; for (auto& x : grains[c]) if (! x.active) { g = &x; break; }
            if (g == nullptr) return;
            const double S = sMs * 0.001 * sr, D = dMs * 0.001 * sr;
            const double fadeS = std::max (0.002 * sr, clamp ((double) p.smoothing, 0.01, 0.5) * S);
            const double sprayOff = spray * S * (0.5 + 0.5 * rnd.next());
            g->active = true; g->reverse = reverse; g->age = 0; g->ratio = (float) ratio;
            g->len = (int) (S / clamp ((double) p.density, 1.0, 4.0) + fadeS); g->len = std::max (g->len, (int) (S + fadeS)); g->fade = std::max (1, (int) fadeS);
            const double maxSpan = (double) size - 2.0 * sr;                     // what the buffer can supply behind the delay
            if (p.engine == vintage)
            {
                double span = (double) g->len * ratio;
                if (span + D > maxSpan) { g->len = std::max (64, (int) ((maxSpan - D) / ratio)); span = (double) g->len * ratio; g->fade = std::min (g->fade, g->len / 2); }
                g->step = reverse ? -ratio : ratio;
                g->pos = reverse ? (double) w - D - sprayOff - 2.0 : (double) w - D - sprayOff - span - 2.0;
            }
            else
            {
                // Clean: the splice's own timeline is read at 1x (forward or reversed) by 40 ms micro-grains that each play
                // at the new pitch; every micro-grain is placed (+-8 ms) where its waveform lines up with the one fading
                // out (WSOLA), so the pitch is exact and the splice keeps its length
                g->step = reverse ? -1.0 : 1.0;
                const double G = 0.04 * sr, span = G * std::max (1.0, ratio) + 0.01 * sr;
                g->src = reverse ? (double) w - D - sprayOff - 2.0 - span : (double) w - D - sprayOff - (double) g->len - 2.0 - span;
                if (! reverse) g->src = std::max (g->src, (double) w - maxSpan);
                g->mg[0].age = g->mg[1].age = -1; g->nextMg = 0; g->mgCount = 0;
            }
            ++spawned;
        }
        float render (int c, Grain& g)
        {
            float y;
            if (p.engine == vintage) { y = at (c, g.pos); g.pos += g.step; }
            else
            {
                const int H = std::max (32, (int) (0.02 * sr)), G = 2 * H;
                const double dir = g.reverse ? -1.0 : 1.0, r = (double) g.ratio;
                if (g.age % H == 0)
                {
                    // start the next micro-grain where the waveform matches the continuation of the previous one
                    auto& prev = g.mg[(g.nextMg + 1) & 1];
                    double s0 = g.src + dir * (double) g.mgCount * H;
                    if (prev.age >= 0)
                    {
                        const double cont = prev.start + dir * (double) prev.age * r;
                        const int range = (int) (0.008 * sr), M = H / 2;
                        double best = -1e30; int bestD = 0;
                        for (int dd = -range; dd <= range; dd += 2)
                        {
                            double acc = 0;
                            for (int j = 0; j < M; j += 4) acc += (double) at (c, s0 + dd + dir * j * r) * at (c, cont + dir * j * r);
                            if (acc > best) { best = acc; bestD = dd; }
                        }
                        s0 += bestD;
                    }
                    g.mg[g.nextMg] = { s0, 0 }; g.nextMg ^= 1; ++g.mgCount;
                }
                y = 0;
                for (auto& m : g.mg)
                {
                    if (m.age < 0) continue;
                    const float win = 0.5f - 0.5f * std::cos (2.0f * (float) pi * (float) m.age / (float) G);
                    y += win * at (c, m.start + dir * (double) m.age * r);
                    if (++m.age >= G) m.age = -1;
                }
            }
            // Tukey window (Smoothing sets the fades)
            float win = 1.0f;
            if (g.age < g.fade) win = 0.5f - 0.5f * std::cos ((float) pi * (float) g.age / (float) g.fade);
            else if (g.age > g.len - g.fade) win = 0.5f - 0.5f * std::cos ((float) pi * (float) (g.len - g.age) / (float) g.fade);
            if (++g.age >= g.len) g.active = false;
            return y * win;
        }
    };
}

//==============================================================================================================
namespace Murm
{
    using namespace Eng;
    constexpr int maxTaps = 1024, groups = 8;
    struct Params
    {
        float timeMs = 2000; int sync = -1;                     // WingMod division (1/64 .. 4 bars) or -1 = free; timeBeats overrides
        float timeBeats = 0;                                    // 1..32 beats when > 0
        float x = 0.25f, y = 0.5f;                              // X: where the envelope peaks (0 start .. 1 end); Y: how shaped it is
        bool customEnv = false; float env[16] { 1, .9f, .8f, .7f, .6f, .5f, .42f, .35f, .28f, .22f, .17f, .12f, .08f, .05f, .03f, .01f };
        float texture = 0.6f, warp = 0.3f, color = 0, blur = 0.4f, grain = 0;
        float modRate = 0.3f, modDepth = 0.2f; int modShape = WingMod::sine;
        float shimmer = 0; int shimmerSt = 12; float shimmerFade = 0.5f;
        bool freeze = false; int freezeMode = 0; bool infinite = false;
        float tiltDb = 0, lowCutHz = 20, highCutHz = 20000, width = 1, duckDb = 0, preDelayMs = 0;
        float mix = 0.4f, outputDb = 0;
    };
    /** The cloud's amplitude envelope at a position 0..1 along Time (X/Y or the drawn shape). */
    inline float envelopeAt (const Params& p, float f)
    {
        f = clamp (f, 0.0f, 1.0f);
        if (p.customEnv)
        {
            const float xx = f * 15.0f; const int i0 = std::min (14, (int) xx); const float fr = xx - (float) i0;
            return std::max (0.0f, p.env[i0] + (p.env[i0 + 1] - p.env[i0]) * fr);
        }
        const float pk = clamp (p.x, 0.0f, 1.0f), k = 0.15f + 6.0f * clamp (p.y, 0.0f, 1.0f);
        const float u = f < pk ? (pk > 1e-4f ? f / pk : 1.0f) : (pk < 0.9999f ? (1.0f - f) / (1.0f - pk) : 1.0f);
        return std::pow (clamp (u, 0.0f, 1.0f), k * 0.5f);
    }
    inline int tapsFor (float texture) { return clamp ((int) std::lround (128.0 * std::pow (2.0, 3.0 * clamp (texture, 0.0f, 1.0f))), 128, maxTaps); }

    /** Radix-2 complex FFT, in place (n a power of two). */
    inline void fft (std::complex<float>* a, int n, bool inverse)
    {
        for (int i = 1, j = 0; i < n; ++i) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (int len = 2; len <= n; len <<= 1)
        {
            const float ang = (float) (2.0 * pi / len * (inverse ? 1 : -1)); const std::complex<float> wl (std::cos (ang), std::sin (ang));
            for (int i = 0; i < n; i += len) { std::complex<float> wv (1); for (int k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * wv; a[i + k] = u + v; a[i + k + len / 2] = u - v; wv *= wl; } }
        }
        if (inverse) for (int i = 0; i < n; ++i) a[i] /= (float) n;
    }

    class Processor
    {
    public:
        Params p;
        static constexpr int fftN = 4096;
        void prepare (double sampleRate, int)
        {
            sr = sampleRate;
            size = (int) (sr * 92.0) + 1024;
            for (auto& b : buf) b.assign ((size_t) size, 0.0f);
            for (auto& g : shim) for (auto& b : g) b.assign (8192, 0.0f);
            for (auto& v : specMag) v.assign (fftN / 2 + 1, 0.0f);
            for (auto& v : specOut) v.assign (fftN * 2, 0.0f);
            specBuf.assign (fftN, {});
            static const int apLen[groups][2] { { 113, 167 }, { 131, 191 }, { 149, 223 }, { 173, 241 }, { 199, 277 }, { 211, 307 }, { 233, 331 }, { 251, 359 } };
            for (int c = 0; c < 2; ++c) for (int g = 0; g < groups; ++g) for (int k = 0; k < 2; ++k)
                ap[c][g][k].d.assign ((size_t) ((apLen[g][k] + c * 17) * std::max (1.0, sr / 48000.0)), 0.0f);
            for (int i = 0; i < 1024; ++i) { const float v = (float) std::sin (pi * i / 1023.0); sin2[i] = v * v; }
            for (auto& f : fbHp) f.set (Biquad::highpass, sr, 60.0, 0.7071);
            layoutKey = -1;
            reset();
        }
        void reset()
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            for (auto& g : shim) for (auto& b : g) std::fill (b.begin(), b.end(), 0.0f);
            w = 0; sw = 0; tCur = -1; rnd = {}; modPh = 0; modHeld = 0; modCycle = -1; duckEnv = 0; wasFrozen = false; fadeIn = 0;
            for (auto& c : ap) for (auto& g : c) for (auto& a : g) { std::fill (a.d.begin(), a.d.end(), 0.0f); a.i = 0; }
            for (auto& c : tilt) for (auto& g : c) for (auto& f : g) f.reset();
            for (auto& f : tail) for (auto& b : f) b.reset();
            for (auto& c : shimPhi) for (auto& v : c) v = 0;
            for (auto& g : gateG) g = 1;
            specHop = 0; specRead = 0; specReady = false; fbS[0] = fbS[1] = 0; fbE = 0; fbBlock[0].fill (0); fbBlock[1].fill (0); for (auto& c : modBuf) for (auto& g : c) g.fill (0); modW = 0; for (auto& f : fbHp) f.reset(); infG = 0.985f;
            for (auto& v : specOut) std::fill (v.begin(), v.end(), 0.0f);
        }
        void setTransport (double bpm, double, bool) { tBpm = bpm > 1 ? bpm : 120; }
        double timeMsTarget() const
        {
            double t = p.timeBeats > 0 ? p.timeBeats * 60000.0 / tBpm : (p.sync >= 0 ? WingMod::syncBeats (p.sync) * 60000.0 / tBpm : (double) p.timeMs);
            return clamp (t, 10.0, 90000.0);
        }
        double timeMsNow() const { return tCur < 0 ? timeMsTarget() : tCur / sr * 1000.0; }
        int numTaps() const { return nTaps; }
        /** Tap positions (0..1 along Time) and gains, channel c, for the face. */
        const float* tapPos (int c) const { return pos[c]; }
        const float* tapGain (int c) const { return gain[c]; }

        void process (float* const* ch, int nch, int n)
        {
            nch = std::min (nch, 2);
            updateLayout();
            const double target = timeMsTarget() * 0.001 * sr;
            if (tCur < 0) tCur = target;
            const double glide = 1.0 - std::exp (-1.0 / ((0.02 + 2.0 * clamp (p.warp, 0.0f, 1.0f)) * sr));
            const double pre = clamp (p.preDelayMs, 0.0f, 1000.0f) * 0.001 * sr;
            for (int c = 0; c < 2; ++c)
            {
                tail[c][0].set (Biquad::highpass, sr, std::max (20.0f, p.lowCutHz), 0.7071); tail[c][1].set (Biquad::lowpass, sr, std::min (20000.0f, p.highCutHz), 0.7071);
                tail[c][2].set (Biquad::lowShelf, sr, 600, 0.6, -0.5 * p.tiltDb); tail[c][3].set (Biquad::highShelf, sr, 2400, 0.6, 0.5 * p.tiltDb);
                for (int g = 0; g < groups; ++g)
                {
                    const float col = p.color * (float) g / (groups - 1);          // the tail tilts further the later it is
                    tilt[c][g][0].set (Biquad::lowShelf, sr, 500, 0.6, -9.0 * col); tilt[c][g][1].set (Biquad::highShelf, sr, 3000, 0.6, 9.0 * col);
                }
            }
            const float blurG = clamp (p.blur, 0.0f, 1.0f) * 0.7f;
            const double modHz = clamp (p.modRate, 0.01f, 20.0f), modAmp = clamp (p.modDepth, 0.0f, 1.0f) * 0.004 * sr;   // up to 4 ms of tap wander
            const float shimR = (float) std::pow (2.0, clamp (p.shimmerSt, -24, 24) / 12.0);
            const float mixV = clamp (p.mix, 0.0f, 1.0f), out = dbToGain (p.outputDb);
            const float duckRel = (float) (1.0 - std::exp (-1.0 / (0.25 * sr))), duckAtt = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
            if (p.freeze && ! wasFrozen) { freezeStart(); fadeIn = 0; }
            wasFrozen = p.freeze;
            const float widthV = clamp (p.width, 0.0f, 2.0f);
            float shimAmt[groups];
            for (int g = 0; g < groups; ++g) shimAmt[g] = p.shimmer > 0.001f ? clamp (p.shimmer * (1.0f - p.shimmerFade + p.shimmerFade * (float) g / (groups - 1)), 0.0f, 1.0f) : 0.0f;
            // block-wise: every tap reads a contiguous run of the buffer per block (the delay ramps linearly across the
            // block), which is what keeps 2048 taps affordable; the groups' diffusion, colour and shimmer run per sample
            for (int off = 0; off < n; off += blockLen)
            {
                const int m = std::min (blockLen, n - off);
                const int w0 = w;
                // 1. what enters the buffer: input, frozen loop or spectral freeze, plus the Infinite feedback (one block late)
                for (int i = 0; i < m; ++i)
                {
                    const float in0 = ch[0][off + i], in1 = nch > 1 ? ch[1][off + i] : in0;
                    float x0 = in0, x1 = in1;
                    if (p.freeze)
                    {
                        fadeIn = std::min (1.0f, fadeIn + (float) (1.0 / (0.03 * sr)));
                        if (p.freezeMode == 1) { const float sv = specNext(); x0 = sv; x1 = sv; }
                        else
                        {
                            const int L = std::max (64, (int) std::min (tCur, 4.0 * sr));
                            const int li = ((w - L) % size + size) % size;
                            const float l0 = buf[0][(size_t) li], l1 = buf[1][(size_t) li];
                            x0 = l0 + (1.0f - fadeIn) * (in0 - l0); x1 = l1 + (1.0f - fadeIn) * (in1 - l1);
                        }
                    }
                    if (p.infinite && ! p.freeze)
                    {
                        // the loop holds the tail at about -18 dBFS RMS however much the cloud sums up: a slow level servo
                        // (high-passed: the cloud sums low frequencies coherently, so the loop would otherwise pile up sub-bass)
                        const float f0 = fbHp[0].process (fbBlock[0][(size_t) i]), f1 = fbHp[1].process (fbBlock[1][(size_t) i]);
                        fbE += (float) (1.0 / (0.3 * sr)) * (0.5f * (f0 * f0 + f1 * f1) - fbE);
                        // a level ceiling, not a servo: the loop runs at 0.985 until the tail nears -18 dBFS RMS (the cloud's
                        // own resonances set where it settles; a servo that chased it oscillated with the loop delay)
                        infG = 0.985f * std::min (1.0f, 0.03f / std::sqrt (fbE + 1.0e-12f));
                        x0 += infG * soft (f0); x1 += infG * soft (f1);
                    }
                    buf[0][(size_t) w] = x0; buf[1][(size_t) w] = x1;
                    w = (w + 1) % size;
                }
                // 2. time glide and the mod LFO at the block's two ends
                const double t0 = tCur; double t1 = target + (t0 - target) * std::pow (1.0 - glide, (double) m);
                if (std::abs (t1 - target) < 0.02) t1 = target;
                tCur = t1;
                double mo0[groups], mo1[groups];
                const double ph0 = modPh; modPh += modHz * m / sr; if (modPh >= 1.0) { modPh -= std::floor (modPh); modHeld = rnd.next(); }
                for (int g = 0; g < groups; ++g)
                {
                    mo0[g] = modAmp * (1.0 + ModShapes::shape (p.modShape, (float) ph0 + (float) g / groups, 0.5f, p.env, 16, modHeld)) * 0.5;
                    mo1[g] = modAmp * (1.0 + ModShapes::shape (p.modShape, (float) modPh + (float) g / groups, 0.5f, p.env, 16, modHeld)) * 0.5;
                }
                if (p.grain > 0.001f) { if ((sw += (unsigned) m) >= 256) { sw = 0; for (int g = 0; g < groups; ++g) gateG[g] = (rnd.next() * 0.5f + 0.5f) < p.grain * 0.7f ? 0.0f : 1.0f; } }
                else for (int g = 0; g < groups; ++g) gateG[g] = 1;
                // 3. the taps
                for (int c = 0; c < 2; ++c)
                {
                    float (&acc)[groups][blockLen] = accBuf[c];
                    for (int g = 0; g < groups; ++g) std::fill (acc[g], acc[g] + m, 0.0f);
                    const float* b = buf[c].data();
                    for (int k = 0; k < nTaps; ++k)
                    {
                        const int g = grp[c][k]; const float gk = gain[c][k] * gateG[g];
                        if (gk == 0.0f) continue;
                        const double d0 = pre + (double) pos[c][k] * t0 + 2.0, d1 = pre + (double) pos[c][k] * t1 + 2.0;
                        double st = (double) w0 - d0; const double step = 1.0 - (d1 - d0) / m;
                        if (st < 0) st += size;
                        const double last = st + step * (m - 1);
                        float* a = acc[g];
                        if (step == 1.0 && st >= 0 && st < size - m - 2)
                        {
                            // settled time: a fixed fraction, a straight run of the buffer (vectorises)
                            const int base = (int) st; const float fr = (float) (st - base); const float* q = b + base;
                            for (int i = 0; i < m; ++i) a[i] += gk * (q[i] + fr * (q[i + 1] - q[i]));
                        }
                        else if (st >= 0 && last >= 0 && st < size - 2 && last < size - 2)
                        {
                            const float stf = (float) (st - std::floor (st)); const int base = (int) std::floor (st); const float sf = (float) step;
                            for (int i = 0; i < m; ++i)
                            {
                                const float pp = stf + sf * (float) i; const int ii = (int) pp; const float fr = pp - (float) ii;
                                const float* q = b + base + ii;
                                a[i] += gk * (q[0] + fr * (q[1] - q[0]));
                            }
                        }
                        else
                            for (int i = 0; i < m; ++i)
                            {
                                double pp = st + step * i; while (pp >= size) pp -= size; while (pp < 0) pp += size;
                                const int ii = (int) pp; const float fr = (float) (pp - ii);
                                const float x0 = b[ii], x1 = b[(ii + 1) % size];
                                a[i] += gk * (x0 + fr * (x1 - x0));
                            }
                    }
                }
                // 4. per sample: groups (diffusion, colour, shimmer), tail EQ, width, ducking, mix
                for (int i = 0; i < m; ++i)
                {
                    const float in0 = ch[0][off + i], in1 = nch > 1 ? ch[1][off + i] : in0;
                    float wet[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        float sum = 0;
                        for (int g = 0; g < groups; ++g)
                        {
                            float v = accBuf[c][g][i];
                            if (modAmp > 0.0)
                            {
                                // Mod: each group's sum passes a short wandering delay (the same as moving every tap in it)
                                auto& mb = modBuf[c][g]; mb[(size_t) (modW & 1023)] = v;
                                const double d = 1.0 + mo0[g] + (mo1[g] - mo0[g]) * (double) i / (double) m;
                                const double rp = (double) modW - d; const long long i0 = (long long) std::floor (rp); const float fr = (float) (rp - (double) i0);
                                const float a0 = mb[(size_t) (i0 & 1023)], a1 = mb[(size_t) ((i0 + 1) & 1023)];
                                v = a0 + fr * (a1 - a0);
                            }
                            v = ap[c][g][0].process (v, blurG); v = ap[c][g][1].process (v, -blurG);
                            if (p.color != 0) v = tilt[c][g][1].process (tilt[c][g][0].process (v));
                            if (shimAmt[g] > 0.001f) v += shimAmt[g] * (shift (c, g, v, shimR) - v);
                            sum += v;
                        }
                        if (p.shimmer > 0.001f && c == 1) shimW = (shimW + 1) & 8191;
                        if (c == 1) ++modW;
                        wet[c] = sum;
                    }
                    for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) wet[c] = tail[c][k].process (wet[c]);
                    const float mm = 0.5f * (wet[0] + wet[1]), sd = 0.5f * (wet[0] - wet[1]) * widthV;
                    wet[0] = mm + sd; wet[1] = mm - sd;
                    fbBlock[0][(size_t) i] = undenorm (wet[0]); fbBlock[1][(size_t) i] = undenorm (wet[1]);
                    float dg = 1;
                    if (p.duckDb > 0.01f)
                    {
                        const float a = std::max (std::abs (in0), std::abs (in1));
                        duckEnv += (a > duckEnv ? duckAtt : duckRel) * (a - duckEnv);
                        dg = dbToGain (-p.duckDb * clamp ((gainToDb (duckEnv) + 40.0f) / 30.0f, 0.0f, 1.0f));
                    }
                    ch[0][off + i] = (in0 + mixV * (wet[0] * dg - in0)) * out;
                    if (nch > 1) ch[1][off + i] = (in1 + mixV * (wet[1] * dg - in1)) * out;
                }
            }
        }

    private:
        struct AP { float z = 0; std::vector<float> d; int i = 0;
                    float process (float x, float g) { if (d.empty()) return x; const float y = d[(size_t) i]; const float v = x + g * y; d[(size_t) i] = v; i = (i + 1) % (int) d.size(); return y - g * v; } };
        double sr = 48000, tBpm = 120, tCur = -1, modPh = 0, modOff[groups] {}; std::vector<float> buf[2]; int size = 1, w = 0; unsigned sw = 0;
        float pos[2][maxTaps] {}, gain[2][maxTaps] {}; int grp[2][maxTaps] {}; int nTaps = 0; double layoutKey = -1;
        AP ap[2][groups][2]; Biquad tilt[2][groups][2], tail[2][4]; ModShapes::Rand rnd; float modHeld = 0; long long modCycle = -1; float duckEnv = 0, gateG[groups] {}, fbS[2] {}, fbE = 0;
        std::vector<float> shim[2][groups]; double shimPhi[2][groups] {}; int shimW = 0;
        bool wasFrozen = false; float fadeIn = 0;
        static constexpr int blockLen = 64;
        float accBuf[2][groups][blockLen] {}; std::array<float, blockLen> fbBlock[2] {};
        std::array<float, 1024> modBuf[2][groups] {}; long long modW = 0; Biquad fbHp[2]; float infG = 0.985f;
        std::vector<float> specMag[1], specOut[1]; std::vector<std::complex<float>> specBuf; int specHop = 0, specRead = 0; bool specReady = false;
        static float soft (float x) { return std::abs (x) < 0.7f ? x : (x > 0 ? 1.0f : -1.0f) * (0.7f + 0.3f * std::tanh ((std::abs (x) - 0.7f) / 0.3f)); }

        float read (int c, double d) const
        {
            double rp = (double) w - d; while (rp < 0) rp += size;
            const int i0 = (int) rp; const float fr = (float) (rp - i0);
            const float a = buf[c][(size_t) i0], b = buf[c][(size_t) ((i0 + 1) % size)];
            return a + (b - a) * fr;
        }
        /** Per-group pitch shifter (two rotating Hann taps over a 60 ms window). */
        float shift (int c, int g, float x, float ratio)
        {
            auto& b = shim[c][g]; const int mask = 8191;
            const int wi = (shimW + c * 7 + g) & mask;
            b[(size_t) wi] = x;
            const double W = 0.06 * sr;
            shimPhi[c][g] += (1.0 - ratio) / W; shimPhi[c][g] -= std::floor (shimPhi[c][g]);
            float y = 0;
            for (int t = 0; t < 2; ++t)
            {
                double f = shimPhi[c][g] + 0.5 * t; f -= std::floor (f);
                const double rp = (double) wi - (2.0 + f * W); const long long i0 = (long long) std::floor (rp); const float fr = (float) (rp - (double) i0);
                const float a = b[(size_t) (i0 & mask)], bb = b[(size_t) ((i0 + 1) & mask)];
                y += sin2[(size_t) (f * 1023.0)] * (a + (bb - a) * fr);
            }
            return y;
        }
        float sin2[1024] {};
        void updateLayout()
        {
            // rebuild the taps when the shape changes (cheap: at most 2048 values)
            double key = p.texture * 1000.0 + p.x * 37.0 + p.y * 91.0 + (p.customEnv ? 7.0 : 0.0) + std::round (timeMsTarget()) * 1.0e-3;
            if (p.customEnv) for (int i = 0; i < 16; ++i) key += p.env[i] * (i + 3) * 13.0;
            if (key == layoutKey) return;
            layoutKey = key;
            nTaps = tapsFor (p.texture);
            const float tex = clamp (p.texture, 0.0f, 1.0f);
            const int K = std::max (4, std::min (nTaps, (int) std::lround (4.0 * std::pow (2.0, 8.0 * tex))));      // clusters: 4 discrete echoes .. one tap each
            for (int c = 0; c < 2; ++c)
            {
                ModShapes::Rand r; r.s = 0x1234567u + (uint32_t) c * 7919u;
                std::vector<double> cluster ((size_t) K, 0.0);
                double e2 = 0;
                for (int k = 0; k < nTaps; ++k)
                {
                    const int cl = k % K;
                    float f = ((float) cl + 0.5f) / (float) K + tex * (r.next() * 0.5f) / (float) K * (K == nTaps ? 1.0f : 1.0f);
                    f = clamp (f, 0.0f, 1.0f);
                    pos[c][k] = f; gain[c][k] = envelopeAt (p, f) * (0.75f + 0.25f * r.next());
                    grp[c][k] = clamp ((int) (f * groups), 0, groups - 1);
                    cluster[(size_t) cl] += gain[c][k]; e2 += (double) gain[c][k] * gain[c][k];
                }
                double ec = 0; for (auto g : cluster) ec += g * g;
                // taps in one cluster add coherently while they sit within ~1.5 ms of each other, as noise beyond that
                const double spreadMs = tex * 0.5 * timeMsTarget() / K;
                const double coh = clamp (1.0 - spreadMs / 1.5, 0.0, 1.0);
                const double e = coh * ec + (1.0 - coh) * e2;
                const float nrm = (float) (1.0 / std::sqrt (std::max (1e-9, e)));
                for (int k = 0; k < nTaps; ++k) gain[c][k] *= nrm;
            }
        }
        // ---- spectral freeze: hold the magnitudes of the last 4096 samples, resynthesise with fresh phases
        void freezeStart()
        {
            for (int k = 0; k < fftN; ++k)
            {
                const float win = 0.5f - 0.5f * std::cos (2.0f * (float) pi * k / fftN);
                const int idx = (w - fftN + k + size) % size;
                specBuf[(size_t) k] = { 0.5f * (buf[0][(size_t) idx] + buf[1][(size_t) idx]) * win, 0.0f };
            }
            fft (specBuf.data(), fftN, false);
            for (int k = 0; k <= fftN / 2; ++k) specMag[0][(size_t) k] = std::abs (specBuf[(size_t) k]);
            std::fill (specOut[0].begin(), specOut[0].end(), 0.0f); specHop = 0; specRead = 0; specReady = true;
        }
        float specNext()
        {
            if (! specReady) return 0;
            const int hop = fftN / 4;
            if (specHop == 0)
            {
                for (int k = 0; k <= fftN / 2; ++k)
                {
                    const float ph = (float) pi * (rnd.next() + 1.0f);
                    specBuf[(size_t) k] = std::polar (specMag[0][(size_t) k], ph);
                    if (k > 0 && k < fftN / 2) specBuf[(size_t) (fftN - k)] = std::conj (specBuf[(size_t) k]);
                }
                fft (specBuf.data(), fftN, true);
                auto& o = specOut[0];
                for (int k = 0; k < fftN; ++k)
                {
                    const float win = 0.5f - 0.5f * std::cos (2.0f * (float) pi * k / fftN);
                    o[(size_t) ((specRead + k) % (2 * fftN))] += specBuf[(size_t) k].real() * win * (2.0f / 3.0f);
                }
            }
            auto& o = specOut[0];
            const float y = o[(size_t) specRead]; o[(size_t) specRead] = 0;
            specRead = (specRead + 1) % (2 * fftN);
            specHop = (specHop + 1) % hop;
            return y;
        }
    };
}

#pragma once
// Squelchmoth 1.11 Voice family, phase C engines (super prompt §3). Standard C++ only (tests/voice111_test.cpp).
//
//   Voice::KeyFinder   Moth Key (V2): key, scale and tempo from audio, with key changes over time
//   Voice::SirenComp   Siren Comp (V6): FET / Opto A / Opto B / Modern, single or dual (series), warm saturation,
//                      per-stage GR, pitch-filter duck, Assist (learn -> previewable suggestion), Punch module
//   Voice::SirenEq     Siren EQ (V5): 6 bands fixed / pitch-tracking / dynamic, HP / LP 6-24 dB/oct, air band, tilt,
//                      solo / mute, Vocal Learning that can be auditioned before it is applied
//   Voice::DeEss2      Hush De-Ess 2 (V7): separate Hard (T, K, Ch) and Soft (S, Sh, Z) detection, split-band
//                      reduction only, link, solo, assist, detection history

#include "SqEngine111Dsp.h"
#include <complex>
#include <functional>

namespace Voice
{
    using namespace Eng;

    // ---------------------------------------------------------------- small radix-2 FFT (power spectrum)
    inline void fftPower (std::vector<std::complex<float>>& a, std::vector<float>& outPow)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = -2 * pi / (double) len; const std::complex<float> wl ((float) std::cos (ang), (float) std::sin (ang));
            for (size_t i = 0; i < n; i += len) { std::complex<float> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
        }
        outPow.resize (n / 2);
        for (size_t i = 0; i < n / 2; ++i) outPow[i] = std::norm (a[i]);
    }

    //==========================================================================
    // Moth Key
    //==========================================================================
    struct KeyResult
    {
        int root = 0; bool minor = false; float confidence = 0;            // 0..1
        float bpm = 0, bpmConfidence = 0;
        struct Alt { int root; bool minor; float score; };
        std::vector<Alt> alternatives;                                    // best first (includes the winner)
        struct Segment { double start, end; int root; bool minor; };
        std::vector<Segment> segments;                                   // key changes over time
        std::array<float, 12> chroma {};
    };
    inline const char* noteName (int pc) { static const char* n[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }; return n[((pc % 12) + 12) % 12]; }

    class KeyFinder
    {
    public:
        /** Analyse a mono signal (any rate). Not real-time: call off the audio thread. */
        KeyResult analyse (const float* x, size_t n, double sr) const
        {
            KeyResult res;
            // decimate to ~11 kHz with a two-stage low-pass
            const int D = std::max (1, (int) std::lround (sr / 11025.0));
            const double fs = sr / D;
            Biquad a1, a2; a1.set (Biquad::lowpass, sr, std::min (4500.0, fs * 0.42), 0.707); a2 = a1;
            std::vector<float> y; y.reserve (n / D + 1);
            for (size_t i = 0; i < n; ++i) { const float v = a2.process (a1.process (x[i])); if (i % (size_t) D == 0) y.push_back (v); }
            if (y.size() < 8192) return res;
            // ---- chroma frames (4096-point, hop 2048)
            const int N = 4096, H = 2048;
            std::vector<std::array<float, 12>> frames; std::vector<float> frameE;
            std::vector<std::complex<float>> buf ((size_t) N); std::vector<float> pw;
            std::vector<int> binPc ((size_t) N / 2, -1); std::vector<float> binW ((size_t) N / 2, 0.0f);
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * fs / N; if (f < 60 || f > 2100) continue;
                const double midi = 69 + 12 * std::log2 (f / 440.0); const int pc = (int) std::lround (midi);
                const double dev = std::abs (midi - pc);                                // semitone fraction
                binPc[(size_t) k] = ((pc % 12) + 12) % 12; binW[(size_t) k] = (float) std::exp (-dev * dev / 0.08);   // closer to the centre = more weight
            }
            for (size_t p = 0; p + (size_t) N <= y.size(); p += H)
            {
                double e = 0;
                for (int i = 0; i < N; ++i) { const float w = 0.5f - 0.5f * (float) std::cos (2 * pi * i / (N - 1)); buf[(size_t) i] = y[p + (size_t) i] * w; e += (double) y[p + (size_t) i] * y[p + (size_t) i]; }
                fftPower (buf, pw);
                // whiten: divide by a local (1/3 octave-ish) spectral mean so drums and formants do not dominate
                std::array<float, 12> c {};
                for (int k = 2; k < N / 2 - 2; ++k)
                {
                    if (binPc[(size_t) k] < 0) continue;
                    const int w = std::max (2, k / 12); double m = 0; int cnt = 0;
                    for (int j = std::max (1, k - w); j <= std::min (N / 2 - 1, k + w); ++j) { m += pw[(size_t) j]; ++cnt; }
                    const double r = pw[(size_t) k] / (m / cnt + 1e-12);
                    if (r > 1.5 && pw[(size_t) k] >= pw[(size_t) k - 1] && pw[(size_t) k] >= pw[(size_t) k + 1])
                        c[(size_t) binPc[(size_t) k]] += (float) (std::log (r) * binW[(size_t) k]);
                }
                frames.push_back (c); frameE.push_back ((float) std::sqrt (e / N));
            }
            // ---- global key
            std::array<double, 12> h {};
            for (size_t f = 0; f < frames.size(); ++f) for (int i = 0; i < 12; ++i) h[(size_t) i] += frames[f][(size_t) i] * std::sqrt (frameE[f]);
            double hm = 0; for (double v : h) hm = std::max (hm, v);
            for (int i = 0; i < 12; ++i) res.chroma[(size_t) i] = hm > 0 ? (float) (h[(size_t) i] / hm) : 0.0f;
            auto scores = keyScores (h);
            std::vector<int> order (24); for (int i = 0; i < 24; ++i) order[(size_t) i] = i;
            std::sort (order.begin(), order.end(), [&] (int a, int b) { return scores[(size_t) a] > scores[(size_t) b]; });
            res.root = order[0] % 12; res.minor = order[0] >= 12;
            const double s1 = scores[(size_t) order[0]], s2 = scores[(size_t) order[1]];
            res.confidence = (float) clamp ((s1 - s2) / std::max (0.05, std::abs (s1)) * 4.0, 0.0, 1.0);
            for (int i = 0; i < 4; ++i) res.alternatives.push_back ({ order[(size_t) i] % 12, order[(size_t) i] >= 12, (float) scores[(size_t) order[(size_t) i]] });
            // ---- key changes: 12 s windows, 3 s hop, smoothed with a change penalty (Viterbi over 24 keys)
            const double hopSec = H / fs; const int win = std::max (1, (int) (12.0 / hopSec)), step = std::max (1, (int) (3.0 / hopSec));
            std::vector<std::array<double, 24>> ws; std::vector<double> wt;
            for (size_t s = 0; s + (size_t) win <= frames.size() || (s == 0 && ! frames.empty()); s += (size_t) step)
            {
                std::array<double, 12> hh {}; const size_t e2 = std::min (frames.size(), s + (size_t) win);
                for (size_t f = s; f < e2; ++f) for (int i = 0; i < 12; ++i) hh[(size_t) i] += frames[f][(size_t) i] * std::sqrt (frameE[f]);
                auto sc = keyScores (hh); std::array<double, 24> a {}; for (int i = 0; i < 24; ++i) a[(size_t) i] = sc[(size_t) i];
                ws.push_back (a); wt.push_back (s * hopSec);
                if (e2 == frames.size()) break;
            }
            if (! ws.empty())
            {
                const double penalty = 0.35;
                std::vector<std::array<double, 24>> V (ws.size()); std::vector<std::array<int, 24>> B (ws.size());
                V[0] = ws[0];
                for (size_t t = 1; t < ws.size(); ++t)
                    for (int k = 0; k < 24; ++k)
                    {
                        double best = -1e9; int bk = k;
                        for (int j = 0; j < 24; ++j) { const double v = V[t - 1][(size_t) j] - (j == k ? 0.0 : penalty); if (v > best) { best = v; bk = j; } }
                        V[t][(size_t) k] = best + ws[t][(size_t) k]; B[t][(size_t) k] = bk;
                    }
                int k = 0; for (int j = 1; j < 24; ++j) if (V.back()[(size_t) j] > V.back()[(size_t) k]) k = j;
                std::vector<int> path (ws.size()); path.back() = k;
                for (size_t t = ws.size() - 1; t > 0; --t) path[t - 1] = B[t][(size_t) path[t]];
                const double total = (double) y.size() / fs;
                for (size_t t = 0; t < path.size(); ++t)
                {
                    const double st = t == 0 ? 0.0 : wt[t] + 12.0 * 0.5 - 1.5, en = total;
                    if (res.segments.empty() || res.segments.back().root != path[t] % 12 || res.segments.back().minor != (path[t] >= 12))
                    {
                        if (! res.segments.empty()) res.segments.back().end = st;
                        res.segments.push_back ({ st, en, path[t] % 12, path[t] >= 12 });
                    }
                }
            }
            // ---- tempo: spectral-flux onset envelope (1024 / 256 at ~11 kHz), autocorrelation 60..200 BPM with a soft prior
            {
                // two onset envelopes: the whole spectrum and the lows (< 250 Hz: kick, bass, the beat itself). The lows count
                // double, so a hi-hat on every eighth does not pull the answer to twice the tempo.
                const int n2 = 1024, h2 = 256; std::vector<std::complex<float>> b2 ((size_t) n2); std::vector<float> p2, prev ((size_t) n2 / 2, 0.0f), prevLow ((size_t) n2 / 2, 0.0f), onAll, onLow;
                const int lowBins = std::max (2, (int) (250.0 * n2 / fs));
                for (size_t p = 0; p + (size_t) n2 <= y.size(); p += h2)
                {
                    for (int i = 0; i < n2; ++i) b2[(size_t) i] = y[p + (size_t) i] * (0.5f - 0.5f * (float) std::cos (2 * pi * i / (n2 - 1)));
                    fftPower (b2, p2);
                    // whole band: log-compressed flux (every instrument counts); lows: magnitude flux, so the kick's energy
                    // wins over the faint low end of a noisy hat
                    double flux = 0, fl = 0;
                    for (int k = 1; k < n2 / 2; ++k) { const float l = std::log1p (1000.0f * p2[(size_t) k]); flux += std::max (0.0f, l - prev[(size_t) k]); prev[(size_t) k] = l; }
                    for (int k = 1; k < lowBins; ++k) { const float m = std::sqrt (p2[(size_t) k]); fl += std::max (0.0f, m - prevLow[(size_t) k]); prevLow[(size_t) k] = m; }
                    onAll.push_back ((float) flux); onLow.push_back ((float) fl);
                }
                std::vector<float> onset (onAll.size(), 0.0f);
                for (auto* v : { &onAll, &onLow })
                {
                    double m = 0; for (float o : *v) m += o; m /= std::max<size_t> (1, v->size());
                    double sd = 0; for (auto& o : *v) { o = std::max (0.0f, o - (float) m); sd += (double) o * o; } sd = std::sqrt (sd / std::max<size_t> (1, v->size())) + 1e-9;
                    const float w = v == &onLow ? 3.0f : 1.0f; for (size_t i = 0; i < v->size(); ++i) onset[i] += w * (float) ((*v)[i] / sd);
                }
                if (onset.size() > 64)
                {
                    const double rate = fs / h2; double best = -1, bestLag = 0;
                    std::vector<double> acf (onset.size() / 2, 0.0);
                    const int lo = (int) (rate * 60.0 / 190.0), hi = std::min ((int) acf.size() - 2, (int) (rate * 60.0 / 60.0));
                    for (int L = std::max (2, lo - 1); L <= hi + 1; ++L) { double s = 0; for (size_t i = (size_t) L; i < onset.size(); ++i) s += onset[i] * onset[i - (size_t) L]; acf[(size_t) L] = s; }
                    for (int L = lo; L <= hi; ++L)
                    {
                        const double bpm = 60.0 * rate / L, prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 110.0) / 1.2, 2));
                        // reward lags whose double also correlates (beat vs half-beat)
                        const double sc = (acf[(size_t) L] + 0.5 * (2 * L < (int) acf.size() ? acf[(size_t) (2 * L)] : 0.0)) * prior;
                        if (sc > best) { best = sc; bestLag = L; }
                    }
                    if (bestLag > 2)
                    {
                        const int L = (int) bestLag; const double ym = acf[(size_t) L - 1], y0 = acf[(size_t) L], yp = acf[(size_t) L + 1];
                        const double d = 0.5 * (ym - yp) / (ym - 2 * y0 + yp - 1e-12);
                        res.bpm = (float) (60.0 * rate / (bestLag + clamp (d, -0.5, 0.5)));
                        res.bpmConfidence = (float) clamp (acf[(size_t) L] / (acf[2] + 1e-12), 0.0, 1.0);
                    }
                }
            }
            return res;
        }
        /** Correlation of a chroma histogram with the 24 major / minor key profiles (index = root + 12 * minor). */
        static std::array<double, 24> keyScores (const std::array<double, 12>& h)
        {
            static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
            static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
            std::array<double, 24> out {};
            for (int m = 0; m < 2; ++m)
                for (int r = 0; r < 12; ++r)
                {
                    const double* prof = m ? mnr : maj; double mx = 0, my = 0;
                    for (int i = 0; i < 12; ++i) { mx += h[(size_t) ((i + r) % 12)]; my += prof[i]; }
                    mx /= 12; my /= 12; double sxy = 0, sxx = 0, syy = 0;
                    for (int i = 0; i < 12; ++i) { const double a = h[(size_t) ((i + r) % 12)] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
                    out[(size_t) (r + 12 * m)] = sxx > 0 ? sxy / std::sqrt (sxx * syy) : 0.0;
                }
            return out;
        }
    };

    //==========================================================================
    // Siren Comp
    //==========================================================================
    namespace Comp
    {
        enum Model { fet, optoA, optoB, modern, numModels };
        inline const char* modelName (int m) { static const char* n[] = { "FET", "Opto A", "Opto B", "Modern" }; return n[clamp (m, 0, 3)]; }
        inline float fetRatio (int idx) { static const float r[] = { 4, 8, 12, 20, 30 }; return r[clamp (idx, 0, 4)]; }   // 30 = all buttons in
        inline const char* fetRatioName (int idx) { static const char* n[] = { "4:1", "8:1", "12:1", "20:1", "All" }; return n[clamp (idx, 0, 4)]; }

        struct StageParams { int model = optoA; float thresholdDb = -18, ratio = 4, attackMs = 10, releaseMs = 120, kneeDb = 6; int fetIdx = 0; };

        /** One compressor stage; returns the gain (linear) for a detector input and keeps its gain reduction. */
        struct Stage
        {
            double sr = 48000; float env = 0, grDb = 0, held = 0, optoSlow = 0;
            void prepare (double rate) { sr = rate; env = 0; grDb = 0; held = 0; optoSlow = 0; }
            static float curve (float levelDb, float thr, float ratio, float knee)
            {
                const float over = levelDb - thr;
                if (2 * over < -knee) return 0.0f;
                if (2 * std::abs (over) <= knee && knee > 0) { const float t = over + knee * 0.5f; return (1 - 1 / ratio) * t * t / (2 * knee); }
                return (1 - 1 / ratio) * over;
            }
            inline float process (float det, const StageParams& p)
            {
                const float a = std::abs (det);
                float target = 0;
                switch (p.model)
                {
                    case fet:
                    {
                        // fast peak detector: attack 0.02..0.8 ms, release 50..1100 ms (program dependent)
                        const float atk = clamp (p.attackMs, 0.02f, 0.8f), rel = clamp (p.releaseMs, 50.0f, 1100.0f);
                        const float ca = (float) (1.0 - std::exp (-1.0 / (atk * 0.001 * sr))), cr = (float) (1.0 - std::exp (-1.0 / (rel * 0.001 * sr)));
                        env += (a > env ? ca : cr) * (a - env);
                        const float ratio = fetRatio (p.fetIdx);
                        target = curve (gainToDb (env + 1e-9f), p.thresholdDb, ratio, p.fetIdx == 4 ? 2.0f : 1.0f);
                        if (p.fetIdx == 4) target *= 1.15f;                                   // all-buttons: the overshoot that makes it famous
                        grDb = target;
                        break;
                    }
                    case optoA:
                    case optoB:
                    {
                        // light-dependent resistor: ~10 ms attack, two-stage release (fast to 50 %, then 0.5..5 s depending on history)
                        const float ca = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
                        env += ((a * a) - env) * (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
                        const float lvl = gainToDb (std::sqrt (env) + 1e-9f);
                        const float ratio = p.model == optoA ? 3.0f : 10.0f, knee = p.model == optoA ? 10.0f : 6.0f;
                        target = curve (lvl, p.thresholdDb, ratio, knee);
                        optoSlow += (float) (1.0 - std::exp (-1.0 / (2.0 * sr))) * ((grDb > 1.0f ? 1.0f : 0.0f) - optoSlow);
                        if (target > grDb) { grDb += ca * (target - grDb); held = grDb; }
                        else
                        {
                            // the first half of the reduction recovers in ~60 ms, the rest in 0.5..5 s (longer after long, heavy GR)
                            const float tau = grDb > target + 0.5f * (held - target) ? 0.06f : 0.5f + 4.5f * optoSlow;
                            grDb += (float) (1.0 - std::exp (-1.0 / (tau * sr))) * (target - grDb);
                        }
                        return dbToGain (-grDb);
                    }
                    default:
                    {
                        const float ca = (float) (1.0 - std::exp (-1.0 / (std::max (0.05f, p.attackMs) * 0.001 * sr))), cr = (float) (1.0 - std::exp (-1.0 / (std::max (5.0f, p.releaseMs) * 0.001 * sr)));
                        const float lvl = gainToDb (a + 1e-9f);
                        target = curve (lvl, p.thresholdDb, std::max (1.0f, p.ratio), p.kneeDb);
                        grDb += (target > grDb ? ca : cr) * (target - grDb);
                        return dbToGain (-grDb);
                    }
                }
                return dbToGain (-grDb);
            }
        };

        struct Params
        {
            StageParams s1, s2; bool dual = false;
            float warm = 0, mix = 1, makeupDb = 0, outputDb = 0; bool autoMakeup = true;
            bool punch = false; float punchGainDb = 0, punchImpact = 0, punchCeilingDb = -1;
            float duckDepthDb = 0; bool keyOn = false;                         // pitch-filter duck (key = the vocal)
        };

        class Processor
        {
        public:
            void prepare (double rate, int maxBlock)
            {
                sr = rate; (void) maxBlock;
                for (auto& s : st1) s.prepare (sr); for (auto& s : st2) s.prepare (sr);
                const auto co = HalfBandDesign::design (100.0, 0.08);
                for (int c = 0; c < 2; ++c) { up[c].setCoefs (co); dn[c].setCoefs (co); warmSt[c].model = Nonlinear::glow; warmSt[c].character = 0.4f; warmSt[c].bias = 0.3f; warmSt[c].prepare (sr * 2); }
                for (int c = 0; c < 2; ++c) { punchUp[c].setCoefs (co); punchDn[c].setCoefs (co); for (auto& b : duckBq[c]) b.reset(); }
                impFast = impSlow = 0; punchGr = 0; learnClear(); f0 = 0;
            }
            Params p;
            float gr1() const { return std::max (st1[0].grDb, st1[1].grDb); }
            float gr2() const { return std::max (st2[0].grDb, st2[1].grDb); }
            float punchGrDb() const { return punchGr; }
            float outLevelDb() const { return outLevel; }
            /** Pitch of the key (vocal) for the pitch-filter duck, set per block by the host (0 = none). */
            void setKeyPitch (float hz) { f0 = hz; }

            // ---- Assist: learn the input's loudness, then suggest settings (preview before commit)
            enum Amount { minimal, controlled, aggressive };
            void learnClear() { std::fill (std::begin (hist), std::end (hist), 0); histN = 0; learnAcc = 0; learnCount = 0; peakMax = 0; }
            bool learning = false;
            Params suggest (int amount) const
            {
                Params s = p;
                auto pct = [&] (double q) { if (histN == 0) return -24.0f; long long need = (long long) (q * histN), acc = 0; for (int i = 0; i < 800; ++i) { acc += hist[i]; if (acc >= need) return -80.0f + i * 0.1f; } return 0.0f; };
                const float loud = pct (0.75), mid = pct (0.5);
                const float targetGr = amount == minimal ? 2.0f : amount == controlled ? 5.0f : 9.0f;
                if (amount == minimal) { s.dual = false; s.s1.model = optoA; s.s1.thresholdDb = loud - targetGr / (1 - 1 / 3.0f); }
                else if (amount == controlled) { s.dual = true; s.s1.model = fet; s.s1.fetIdx = 0; s.s1.attackMs = 0.3f; s.s1.releaseMs = 120; s.s1.thresholdDb = pct (0.9) - 3.0f; s.s2.model = optoA; s.s2.thresholdDb = mid - 1.0f; }
                else { s.dual = true; s.s1.model = fet; s.s1.fetIdx = 1; s.s1.attackMs = 0.1f; s.s1.releaseMs = 80; s.s1.thresholdDb = pct (0.6) - 2.0f; s.s2.model = optoB; s.s2.thresholdDb = mid - 4.0f; }
                // safe make-up: bring the average back, but never push the loudest peak above -1 dBFS
                const float peakDb = gainToDb (peakMax + 1e-9f);
                s.autoMakeup = false; s.makeupDb = std::min (targetGr * 0.9f, -1.0f - (peakDb - targetGr * 0.5f));
                return s;
            }

            void process (float* const* ch, int nch, const float* const* key, int n)
            {
                float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
                const float outG = dbToGain (p.outputDb);
                // auto make-up: half of the static GR at the threshold +10 dB point (a predictable, safe compensation)
                float mk = p.makeupDb;
                if (p.autoMakeup) mk += 0.5f * (stageStatic (p.s1) + (p.dual ? stageStatic (p.s2) : 0.0f));
                const float mkG = dbToGain (mk);
                // pitch-filter duck: six harmonics of the key's pitch, depth follows the key's level
                const bool duck = p.keyOn && key != nullptr && p.duckDepthDb > 0 && f0 > 50;
                if (duck && ++duckCount >= 1) for (int c = 0; c < 2; ++c) for (int h = 0; h < 6; ++h) duckBq[c][h].set (Biquad::peak, sr, f0 * (h + 1), 12.0, -p.duckDepthDb * keyLevel * (1.0f - 0.1f * h));
                const float kA = (float) (1.0 - std::exp (-1.0 / (0.01 * sr))), kR = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
                const float iF = (float) (1.0 - std::exp (-1.0 / (0.001 * sr))), iS = (float) (1.0 - std::exp (-1.0 / (0.040 * sr)));
                const float ceil = dbToGain (p.punchCeilingDb), pGain = dbToGain (p.punchGainDb);
                float outPk = 0;
                for (int i = 0; i < n; ++i)
                {
                    float a = L[i], b = R[i]; const float dryA = a, dryB = b;
                    if (learning)
                    {
                        const float m = std::max (std::abs (a), std::abs (b)); peakMax = std::max (peakMax, m);
                        learnAcc += 0.5 * ((double) a * a + (double) b * b); if (++learnCount >= (int) (0.05 * sr)) { const float db = gainToDb ((float) std::sqrt (learnAcc / learnCount) + 1e-9f); if (db > -70) { hist[clamp ((int) ((db + 80.0f) * 10.0f), 0, 799)]++; ++histN; } learnAcc = 0; learnCount = 0; }
                    }
                    // stage 1 and 2 (stereo-linked detection)
                    const float det = std::max (std::abs (a), std::abs (b));
                    const float g1 = st1[0].process (det, p.s1); st1[1].grDb = st1[0].grDb;
                    a *= g1; b *= g1;
                    if (p.dual) { const float d2 = std::max (std::abs (a), std::abs (b)); const float g2 = st2[0].process (d2, p.s2); st2[1].grDb = st2[0].grDb; a *= g2; b *= g2; }
                    else st2[0].grDb = st2[1].grDb = 0;
                    a *= mkG; b *= mkG;
                    // warm: 2x-oversampled tube stage (unity small-signal)
                    if (p.warm > 0.001f)
                    {
                        const float drv = dbToGain (p.warm * 18.0f);
                        float* v[2] = { &a, &b };
                        for (int c = 0; c < 2; ++c)
                        {
                            float u0, u1; up[c].up (*v[c], u0, u1); warmSt[c].drive = drv;
                            u0 = warmSt[c].process (u0) / drv; u1 = warmSt[c].process (u1) / drv; *v[c] = dn[c].down (u0, u1);
                        }
                    }
                    // pitch duck
                    if (duck)
                    {
                        const float k = 0.5f * (std::abs (key[0][i]) + std::abs (key[1][i]));
                        keyEnv += (k > keyEnv ? kA : kR) * (k - keyEnv);
                        for (int h = 0; h < 6; ++h) { a = duckBq[0][h].process (a); b = duckBq[1][h].process (b); }
                    }
                    // mix
                    a = dryA * (1 - p.mix) + a * p.mix; b = dryB * (1 - p.mix) + b * p.mix;
                    // punch: impact (transient lift) -> gain -> ceiling (2x, ADAA-style soft knee)
                    if (p.punch)
                    {
                        const float lv = std::max (std::abs (a), std::abs (b));
                        impFast += iF * (lv - impFast); impSlow += iS * (lv - impSlow);
                        const float tr = clamp (gainToDb (impFast + 1e-9f) - gainToDb (impSlow + 1e-9f), 0.0f, 12.0f);
                        const float ig = dbToGain (p.punchImpact * tr * 0.5f) * pGain;
                        float pr = 0;
                        float* v[2] = { &a, &b };
                        for (int c = 0; c < 2; ++c)
                        {
                            // the soft knee runs 2x oversampled and 1 dB under the ceiling; the filter's small overshoot is caught
                            // by a final sample clip that only ever touches that residue
                            constexpr float inner = 0.8913f;
                            float u0, u1; punchUp[c].up (*v[c] * ig / (ceil * inner), u0, u1);
                            u0 = (float) Nonlinear::softAlgD (u0); u1 = (float) Nonlinear::softAlgD (u1);
                            const float y = clamp (punchDn[c].down (u0, u1) * ceil * inner, -ceil, ceil);
                            pr = std::max (pr, std::abs (*v[c] * ig) > 1e-9f ? gainToDb (std::abs (*v[c] * ig) / std::max (1e-9f, std::abs (y))) : 0.0f);
                            *v[c] = y;
                        }
                        punchGr += (pr > punchGr ? 0.2f : 0.0005f) * (pr - punchGr);
                    }
                    L[i] = a * outG; if (nch > 1) R[i] = b * outG;
                    outPk = std::max (outPk, std::max (std::abs (L[i]), std::abs (b * outG)));
                }
                keyLevel = clamp ((gainToDb (keyEnv + 1e-9f) + 40.0f) / 30.0f, 0.0f, 1.0f);
                outLevel = gainToDb (outPk + 1e-9f);
            }
        private:
            double sr = 48000;
            Stage st1[2], st2[2]; HalfBand up[2], dn[2], punchUp[2], punchDn[2]; Nonlinear::Stage warmSt[2];
            Biquad duckBq[2][6]; float f0 = 0, keyEnv = 0, keyLevel = 0; int duckCount = 0;
            float impFast = 0, impSlow = 0, punchGr = 0, outLevel = -100;
            long long hist[800] {}; long long histN = 0; double learnAcc = 0; int learnCount = 0; float peakMax = 0;
            static float stageStatic (const StageParams& s)
            {
                const float r = s.model == fet ? fetRatio (s.fetIdx) : s.model == optoA ? 3.0f : s.model == optoB ? 10.0f : std::max (1.0f, s.ratio);
                return std::max (0.0f, (-8.0f - s.thresholdDb)) * (1 - 1 / r) * 0.6f;   // GR a typical -8 dBFS peak would get
            }
        };
    }

    //==========================================================================
    // Siren EQ
    //==========================================================================
    namespace Eq
    {
        enum BandType { bell, lowShelf, highShelf, notch };
        enum BandMode { fixed, pitchTrack, dynamic };
        struct Band { bool on = false; int type = bell, mode = fixed, harmonic = 1; float freq = 1000, gainDb = 0, q = 1, thresholdDb = -30, rangeDb = 6; bool expand = false, solo = false, mute = false; };
        struct Params
        {
            std::array<Band, 6> band {};
            float hpHz = 20, lpHz = 20000; int hpSlope = 1, lpSlope = 1;       // 0..3 = 6 / 12 / 18 / 24 dB/oct
            float airHz = 12000, airDb = 0, tiltDb = 0, outputDb = 0;
            bool preview = false;                                              // audition the Vocal Learning suggestion
            std::array<Band, 6> suggested {}; float suggestedHp = 20;
        };
        struct Learned { float lowHz = 0, highHz = 0; std::vector<Band> bands; float hpHz = 20; bool ok = false; };

        class Processor
        {
        public:
            void prepare (double rate, int)
            {
                sr = rate;
                for (int c = 0; c < 2; ++c) { for (auto& b : bq[c]) b.reset(); for (auto& b : det[c]) b.reset(); for (auto& f : hp[c]) f.reset(); for (auto& f : lp[c]) f.reset(); hp1[c].reset(); lp1[c].reset(); air[c].reset(); tl[c].reset(); th[c].reset(); }
                for (auto& r : envC) for (auto& e : r) e = 0; for (auto& g : dynG) g = 0; f0Sm = 0; counter = 0; configured = false; learnReset();
            }
            Params p;
            void setPitch (float hz) { f0 = hz; }
            float trackedHz() const { return f0Sm; }
            float bandFreq (int b) const { return curFreq[clamp (b, 0, 5)]; }
            float bandDynGain (int b) const { return dynG[clamp (b, 0, 5)]; }

            void process (float* const* ch, int nch, int n)
            {
                float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
                const auto& bands = p.preview ? p.suggested : p.band;
                bool anySolo = false; for (auto& b : bands) anySolo = anySolo || (b.on && b.solo);
                const float outG = dbToGain (p.outputDb);
                const float ea = (float) (1.0 - std::exp (-1.0 / (0.005 * sr))), er = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
                for (int i = 0; i < n; ++i)
                {
                    if ((counter++ & 31) == 0) configure (bands, anySolo);
                    float s[2] = { L[i], R[i] };
                    if (learning) learnPush (0.5f * (s[0] + s[1]));
                    for (int c = 0; c < 2; ++c)
                    {
                        float v = s[c];
                        if (p.hpHz > 20.5f || (p.preview && p.suggestedHp > 20.5f)) { if (hpSlopeNow == 0 || hpSlopeNow == 2) v = hp1[c].hp (v); for (int k = 0; k < hpStages; ++k) v = hp[c][k].process (v); }
                        if (p.lpHz < 19900.0f) { if (p.lpSlope == 0 || p.lpSlope == 2) v = lp1[c].lp (v); for (int k = 0; k < lpStages; ++k) v = lp[c][k].process (v); }
                        if (anySolo)
                        {
                            float o = 0; for (int b = 0; b < 6; ++b) if (bands[(size_t) b].on && bands[(size_t) b].solo) o += det[c][b].process (v);
                            v = o;
                        }
                        else
                        {
                            for (int b = 0; b < 6; ++b)
                            {
                                const auto& bd = bands[(size_t) b];
                                if (! bd.on || bd.mute) continue;
                                if (bd.mode == dynamic) { const float d = std::abs (det[c][b].process (v)); envC[c][b] += (d > envC[c][b] ? ea : er) * (d - envC[c][b]); }
                                v = bq[c][b].process (v);
                            }
                            if (std::abs (p.tiltDb) > 0.01f) v = th[c].process (tl[c].process (v));
                            if (std::abs (p.airDb) > 0.01f) v = air[c].process (v);
                        }
                        s[c] = v;
                    }
                    L[i] = s[0] * outG; if (nch > 1) R[i] = s[1] * outG;
                }
            }

            // ---- Vocal Learning: long-term spectrum + pitch range -> suggested bands (audition with p.preview)
            bool learning = false;
            void learnReset() { frameH.clear(); frameH.reserve (800); ltas.assign (2048, 0.0); hEnv.assign (2048, 0.0); hCnt.assign (2048, 0); lbuf.assign (4096, 0.0f); lpos = 0; lframes = 0; voicedFrames = 0; f0Lo = 1e9f; f0Hi = 0; f0Now = 0; }
            void learnPitch (float hz) { f0Now = (hz > 50 && hz < 1500) ? hz : 0.0f; if (learning && f0Now > 0) { f0Lo = std::min (f0Lo, hz); f0Hi = std::max (f0Hi, hz); } }
            Learned learnResult () const
            {
                Learned r; if (lframes < 8) return r;
                r.ok = true; r.lowHz = f0Lo < 1e8f ? f0Lo : 0; r.highHz = f0Hi;
                // Long-term spectrum, first smoothed wider than the voice's harmonic spacing (so a held note's harmonics are
                // never mistaken for resonances), then compared with a much wider (about an octave) average: resonances are
                // the bumps that stand 3 dB above it, 200 Hz .. 8 kHz.
                const int N = 4096; std::vector<double> env (N / 2, 0.0), db (N / 2), sm (N / 2);
                const bool sung = voicedFrames >= 4 && voicedFrames * 2 >= lframes;
                if (sung)
                {
                    // the harmonic-sampled envelope, gaps filled by interpolation (in dB)
                    int last = -1;
                    for (int k = 1; k < N / 2; ++k)
                        if (hCnt[(size_t) k] > 0)
                        {
                            env[(size_t) k] = 10 * std::log10 (hEnv[(size_t) k] / hCnt[(size_t) k] + 1e-20);
                            if (last < 0) for (int j = 1; j < k; ++j) env[(size_t) j] = env[(size_t) k];
                            else for (int j = last + 1; j < k; ++j) env[(size_t) j] = env[(size_t) last] + (env[(size_t) k] - env[(size_t) last]) * (j - last) / (double) (k - last);
                            last = k;
                        }
                    if (last < 0) return r;
                    for (int j = last + 1; j < N / 2; ++j) env[(size_t) j] = env[(size_t) last];
                    for (int k = 1; k < N / 2; ++k) { const int w = std::max (1, k / 16); double a = 0; int c = 0; for (int j = std::max (1, k - w); j <= std::min (N / 2 - 1, k + w); ++j) { a += env[(size_t) j]; ++c; } db[(size_t) k] = a / c; }
                }
                else
                    for (int k = 1; k < N / 2; ++k)
                    {
                        const int w = std::max (2, k / 12); double a = 0; int c = 0;
                        for (int j = std::max (1, k - w); j <= std::min (N / 2 - 1, k + w); ++j) { a += ltas[(size_t) j] / lframes + 1e-20; ++c; }
                        db[(size_t) k] = 10 * std::log10 (a / c);
                    }
                for (int k = 1; k < N / 2; ++k)
                {
                    const int w = std::max (4, k / 3); double a = 0; int c = 0;
                    for (int j = std::max (1, k - w); j <= std::min (N / 2 - 1, k + w); ++j) { a += db[(size_t) j]; ++c; }
                    sm[(size_t) k] = a / c;
                }
                struct Pk { double hz, ex, q; }; std::vector<Pk> peaks;
                for (int k = 2; k < N / 2 - 2; ++k)
                {
                    const double hz = k * sr / N; if (hz < 200 || hz > 8000) continue;
                    const double ex = db[(size_t) k] - sm[(size_t) k];
                    if (ex > 3 && db[(size_t) k] >= db[(size_t) k - 1] && db[(size_t) k] >= db[(size_t) k + 1])
                    {
                        int l = k, u = k; while (l > 1 && db[(size_t) l] > db[(size_t) k] - 3) --l; while (u < N / 2 - 1 && db[(size_t) u] > db[(size_t) k] - 3) ++u;
                        const double q = hz / std::max (1.0, (u - l) * sr / N);
                        // a resonance to cut is narrow (Q 3.5 and up); the broad bumps are the vowels' own formants
                        if (q >= 3.5 && persistence (hz) >= 0.55) peaks.push_back ({ hz, ex, clamp (q, 1.0, 12.0) });
                    }
                }
                std::sort (peaks.begin(), peaks.end(), [] (const Pk& a, const Pk& b) { return a.ex > b.ex; });
                for (size_t i = 0; i < peaks.size() && r.bands.size() < 3; ++i)
                {
                    bool tooClose = false; for (auto& b : r.bands) tooClose = tooClose || std::abs (std::log2 (b.freq / peaks[i].hz)) < 0.33;
                    if (tooClose) continue;
                    Band b; b.on = true; b.type = bell; b.freq = (float) peaks[i].hz; b.gainDb = (float) -std::min (6.0, peaks[i].ex * 0.6); b.q = (float) peaks[i].q; b.mode = fixed;
                    r.bands.push_back (b);
                }
                r.hpHz = r.lowHz > 0 ? std::max (40.0f, r.lowHz * 0.7f) : 80.0f;
                return r;
            }
        private:
            double sr = 48000; float f0 = 0, f0Sm = 0; int counter = 0; bool configured = false;
            Biquad bq[2][6], det[2][6], hp[2][2], lp[2][2], air[2], tl[2], th[2]; OnePole hp1[2], lp1[2];
            int hpStages = 1, lpStages = 1, hpSlopeNow = 1;
            float envC[2][6] {}, dynG[6] {}, curFreq[6] {};
            struct FrameH { float f0 = 0; float db[61] {}; };
            std::vector<FrameH> frameH;
            /** How often (fraction of sung frames) the harmonic nearest hz stands 3 dB above the harmonics around it: a
                fixed resonance lifts it on every note and vowel; a vowel's own formant only while that vowel is sung. */
            float persistence (double hz) const
            {
                if (frameH.empty()) return 1.0f;   // noise / instruments: nothing to tell apart
                int n = 0, hit = 0;
                for (auto& f : frameH)
                {
                    const int h = (int) std::lround (hz / f.f0); if (h < 1 || h > 58 || std::abs (h * f.f0 - hz) > 0.5 * f.f0) continue;
                    double ref = 0; int c = 0;
                    for (int j = 1; j <= 60; ++j) if (std::abs (j - h) >= 2 && std::abs (std::log2 ((double) j / h)) < 0.45 && f.db[j] != 0) { ref += f.db[j]; ++c; }
                    if (c == 0 || f.db[h] == 0) continue;
                    ++n; if (f.db[h] - ref / c > 3.0) ++hit;
                }
                return n > 0 ? (float) hit / (float) n : 0.0f;
            }
            std::vector<double> ltas, hEnv; std::vector<int> hCnt; std::vector<float> lbuf; int lpos = 0; int lframes = 0, voicedFrames = 0; float f0Lo = 1e9f, f0Hi = 0, f0Now = 0;
            void learnPush (float x)
            {
                lbuf[(size_t) lpos++] = x;
                if (lpos >= 4096)
                {
                    lpos = 0; std::vector<std::complex<float>> a (4096); std::vector<float> pw;
                    for (int i = 0; i < 4096; ++i) a[(size_t) i] = lbuf[(size_t) i] * (0.5f - 0.5f * (float) std::cos (2 * pi * i / 4095));
                    fftPower (a, pw); for (int k = 0; k < 2048; ++k) ltas[(size_t) k] += pw[(size_t) k]; ++lframes;
                    // sung frames: sample the envelope at the harmonics (wherever they fall on this note); over the notes
                    // of a take the samples cover the axis finely, and a fixed resonance lifts all of them
                    if (f0Now > 0)
                    {
                        ++voicedFrames;
                        FrameH fh; fh.f0 = f0Now;
                        const double fb = f0Now * 4096.0 / sr; const int halfW = std::max (1, (int) (fb * 0.25));
                        for (int h = 1; h * f0Now < 9000.0 && h < 120; ++h)
                        {
                            const int c = (int) std::lround (h * fb); int best = c; float bp = -1;
                            for (int k = std::max (1, c - halfW); k <= std::min (2046, c + halfW); ++k) if (pw[(size_t) k] > bp) { bp = pw[(size_t) k]; best = k; }
                            if (best < 2 || best > 2045) continue;
                            const double hp = (double) pw[(size_t) best - 1] + pw[(size_t) best] + pw[(size_t) best + 1];
                            hEnv[(size_t) best] += hp; hCnt[(size_t) best]++;
                            if (h <= 60) fh.db[h] = (float) (10 * std::log10 (hp + 1e-20));
                        }
                        if (frameH.size() < 800) frameH.push_back (fh);
                    }
                }
            }
            void configure (const std::array<Band, 6>& bands, bool anySolo)
            {
                if (f0 > 50) f0Sm = f0Sm <= 0 ? f0 : f0Sm + 0.3f * (f0 - f0Sm);
                for (int b = 0; b < 6; ++b)
                {
                    const auto& bd = bands[(size_t) b];
                    if (! bd.on) continue;
                    float fq = bd.freq;
                    if (bd.mode == pitchTrack && f0Sm > 50) fq = f0Sm * (float) std::max (1, bd.harmonic);
                    fq = clamp (fq, 20.0f, (float) (sr * 0.45));
                    curFreq[b] = fq;
                    float g = bd.gainDb;
                    if (bd.mode == dynamic)
                    {
                        const float lvl = gainToDb (std::max (envC[0][b], envC[1][b]) + 1e-9f), over = std::max (0.0f, lvl - bd.thresholdDb);
                        const float amt = std::min (std::abs (bd.rangeDb), over * 0.5f);
                        dynG[b] = bd.expand ? amt : -amt; g += dynG[b];
                    }
                    for (int c = 0; c < 2; ++c)
                    {
                        const Biquad::Type t = bd.type == lowShelf ? Biquad::lowShelf : bd.type == highShelf ? Biquad::highShelf : bd.type == notch ? Biquad::notch : Biquad::peak;
                        bq[c][b].set (t, sr, fq, bd.q, g);
                        if (bd.mode == dynamic || anySolo) det[c][b].set (Biquad::bandpass, sr, fq, std::max (0.5f, bd.q));
                    }
                }
                const float hpHz = (p.preview && p.suggestedHp > p.hpHz) ? p.suggestedHp : p.hpHz;
                hpSlopeNow = p.hpSlope;
                hpStages = p.hpSlope <= 0 ? 0 : (p.hpSlope <= 2 ? 1 : 2); lpStages = p.lpSlope <= 0 ? 0 : (p.lpSlope <= 2 ? 1 : 2);
                for (int c = 0; c < 2; ++c)
                {
                    hp1[c].set (sr, hpHz); lp1[c].set (sr, p.lpHz);
                    if (hpStages == 2) { hp[c][0].set (Biquad::highpass, sr, hpHz, 0.5412); hp[c][1].set (Biquad::highpass, sr, hpHz, 1.3066); } else hp[c][0].set (Biquad::highpass, sr, hpHz, p.hpSlope == 2 ? 1.0 : 0.7071);
                    if (lpStages == 2) { lp[c][0].set (Biquad::lowpass, sr, p.lpHz, 0.5412); lp[c][1].set (Biquad::lowpass, sr, p.lpHz, 1.3066); } else lp[c][0].set (Biquad::lowpass, sr, p.lpHz, p.lpSlope == 2 ? 1.0 : 0.7071);
                    air[c].set (Biquad::highShelf, sr, std::min ((double) p.airHz, sr * 0.45), 0.5, p.airDb);
                    tl[c].set (Biquad::lowShelf, sr, 1000, 0.5, -0.5 * p.tiltDb); th[c].set (Biquad::highShelf, sr, 1000, 0.5, 0.5 * p.tiltDb);
                }
                configured = true;
            }
        };
    }

    //==========================================================================
    // Hush De-Ess 2
    //==========================================================================
    namespace DeEss
    {
        enum Kind { none, hard, soft };
        struct Params
        {
            float splitHz = 4500, hardThresholdDb = -30, softThresholdDb = -34, hardRangeDb = 8, softRangeDb = 10;
            float sensitivity = 0.5f;                                          // how sibilant a frame must be (ratio of highs to the whole)
            bool link = true, solo = false; float outputDb = 0;
        };
        struct Event { float t; int kind; float grDb; };

        class Processor
        {
        public:
            static constexpr int historyLen = 512;
            void prepare (double rate, int)
            {
                sr = rate; blockLen = std::max (16, (int) (0.0025 * sr));
                setSplit (p.splitHz);
                for (auto& e : eHigh) e = 0; for (auto& e : eAll) e = 0; pending = false; kind = none; eventLen = 0; grDb = 0; histW = 0; for (auto& h : history) h = {}; acc = 0; accHi = 0; accCount = 0;
                learnReset();
            }
            Params p;
            void setSplit (float hz)
            {
                splitNow = hz;
                for (int c = 0; c < 2; ++c) { lo1[c].set (Biquad::lowpass, sr, hz, 0.7071); lo2[c] = lo1[c]; hi1[c].set (Biquad::highpass, sr, hz, 0.7071); hi2[c] = hi1[c]; }
                // detector: the same split (reads the high band) plus a broadband reference
            }
            int currentKind() const { return kind; }
            float gainReductionDb() const { return grDb; }
            void historyCopy (Event* out) const { for (int i = 0; i < historyLen; ++i) out[i] = history[(histW + i) % historyLen]; }
            int countHard = 0, countSoft = 0;

            void process (float* const* ch, int nch, int n)
            {
                if (std::abs (p.splitHz - splitNow) > 1.0f) setSplit (p.splitHz);
                float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
                const float outG = dbToGain (p.outputDb);
                const float atk = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr))), rel = (float) (1.0 - std::exp (-1.0 / (0.040 * sr)));
                for (int i = 0; i < n; ++i)
                {
                    float lo[2], hi[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float x = c ? R[i] : L[i];
                        lo[c] = lo2[c].process (lo1[c].process (x)); hi[c] = hi2[c].process (hi1[c].process (x));   // Linkwitz-Riley 4th order: lo + hi is flat
                        // LR4 sums to an allpass; the low band is reconstructed in phase by summing the two
                    }
                    const float x0 = L[i], x1 = R[i];
                    acc += 0.5 * ((double) x0 * x0 + (double) x1 * x1); accHi += 0.5 * ((double) hi[0] * hi[0] + (double) hi[1] * hi[1]);
                    if (++accCount >= blockLen) { classify(); acc = accHi = 0; accCount = 0; }
                    // split-band reduction only: the high band is turned down, the lows never move
                    grSm += (targetGr > grSm ? atk : rel) * (targetGr - grSm);
                    grDb = grSm;
                    const float g = dbToGain (-grSm);
                    float o[2];
                    for (int c = 0; c < 2; ++c) o[c] = p.solo ? hi[c] * (1.0f - g) : lo[c] + hi[c] * g;
                    L[i] = o[0] * outG; if (nch > 1) R[i] = o[1] * outG;
                }
            }

            // ---- Assist: learn the sibilant level, set the thresholds so the loudest fifth gets ~6 dB
            bool learning = false;
            void learnReset() { hardLv.clear(); softLv.clear(); hardLv.reserve (4096); softLv.reserve (4096); }
            Params suggest() const
            {
                Params s = p;
                auto pct = [] (std::vector<float> v, double q, float def) { if (v.size() < 4) return def; std::sort (v.begin(), v.end()); return v[(size_t) (q * (v.size() - 1))]; };
                s.hardThresholdDb = pct (hardLv, 0.8, p.hardThresholdDb) - 8.0f;
                s.softThresholdDb = pct (softLv, 0.8, p.softThresholdDb) - 8.0f;
                return s;
            }
        private:
            double sr = 48000; int blockLen = 120;
            Biquad lo1[2], lo2[2], hi1[2], hi2[2]; float splitNow = 4500;
            double acc = 0, accHi = 0; int accCount = 0; float eHigh[2] {}, eAll[2] {};
            float hiDbNow = -100, startDb = -100, peakDb = -100; bool pending = false; int kind = none, eventLen = 0; float targetGr = 0, grSm = 0, grDb = 0;
            Event history[historyLen] {}; int histW = 0; double tNow = 0; int histTick = 0;
            std::vector<float> hardLv, softLv;
            void learnPush (float db, int k) { if (k == hard && hardLv.size() < 4096) hardLv.push_back (db); else if (k == soft && softLv.size() < 4096) softLv.push_back (db); }
            /** Every 2.5 ms: is this frame sibilant, and is it a hard (burst) or soft (sustained) consonant? */
            void classify()
            {
                const float allDb = gainToDb ((float) std::sqrt (acc / accCount) + 1e-9f), hiDb = gainToDb ((float) std::sqrt (accHi / accCount) + 1e-9f);
                hiDbNow = hiDb;
                const float ratioDb = hiDb - allDb;                                   // 0 dB = all the energy is above the split
                const float need = -5.5f - 9.0f * clamp (p.sensitivity, 0.0f, 1.0f);   // sensitivity 0: highs within 5.5 dB of the whole; 1: within 14.5 dB
                const bool shape = ratioDb > need && hiDb > -70.0f;
                const bool sib = shape && (learning || hiDb > std::min (p.hardThresholdDb, p.softThresholdDb) - 6.0f);
                if (! sib) { kind = none; eventLen = 0; pending = false; }
                else
                {
                    if (kind == none && ! pending) { pending = true; startDb = peakDb = hiDb; eventLen = 0; }
                    ++eventLen; peakDb = std::max (peakDb, hiDb);
                    // a plosive / affricate (T, K, Ch) arrives at full level at once; a fricative (S, Sh, Z) swells in.
                    // Decided 7.5 ms into the event: hard if its first frame was already within 4 dB of the peak so far.
                    if (pending && eventLen >= 3)
                    {
                        pending = false; kind = (peakDb - startDb) < 4.0f ? hard : soft;
                        if (kind == hard) ++countHard; else ++countSoft;
                    }
                    if (kind == hard && eventLen * 2.5f > 60.0f) kind = soft;          // a burst that keeps going is a sustained fricative
                }
                const int k = pending ? soft : kind;
                float gr = 0;
                if (k == hard) gr = clamp ((hiDb - p.hardThresholdDb) * 0.8f, 0.0f, p.hardRangeDb);
                else if (k == soft && sib) gr = clamp ((hiDb - p.softThresholdDb) * 0.8f, 0.0f, p.softRangeDb);
                targetGr = gr;
                if (learning && ! pending && kind != none) learnPush (hiDb, kind);
                tNow += blockLen / sr;
                if (++histTick >= 8) { history[histW] = { (float) tNow, kind, gr }; histW = (histW + 1) % historyLen; histTick = 0; }
            }
        };
    }
}

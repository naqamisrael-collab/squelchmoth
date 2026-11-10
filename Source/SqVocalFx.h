#pragma once
// Moth Tune FX - creative vocal tuning (original design and code; the EFX-class companion to Moth Tune).
//
//  * The Moth Tune core (Live or Studio engine) with Retune, Humanize, Formant, Throat, Transpose and the key.
//  * Motion: an EDITABLE step pattern (1-16 steps of scale steps or semitones, rests, tempo-synced or free rate,
//    glide, Auto / Momentary / Toggle / Always triggers, latch) that moves the tuned note - not a fixed list.
//  * A six-slot rack you can re-order, each slot one of 16 modules driven by an X/Y pad plus its own mix:
//    Mutate (pitch-tracked ring mod), Duet (doubler), Vocode (16-band vocoder on a pitch-tracked carrier), Tube,
//    Breath, Reverb, Echo, Chorus, Compress, Low-pass, High-pass, Tilt EQ, Octaver, Bit-crush, Talkbox, Stutter,
//    and (version 2) EQ, Phaser, Flanger, Auto-Pan and Fuzz - 21 modules.
//  * Version 2: every slot has its own trigger (Always, Auto while singing, Momentary while a MIDI note / HOLD is down,
//    Toggle on each MIDI note) with a click-free fade, and Motion has a style: Hard, Soft (glided steps, rests fade)
//    or Legato (long glides, rests hold the note, the pattern runs on across phrases).

#include "SqVocal.h"
#include "SqEngine111Dsp.h"

namespace SqVocalFx
{
    using SqVocal::Svf;

    inline const juce::StringArray& moduleNames()
    {
        static const juce::StringArray n { "EMPTY", "MUTATE", "DUET", "VOCODE", "TUBE", "BREATH", "REVERB", "ECHO", "CHORUS", "COMPRESS",
                                           "LOW-PASS", "HIGH-PASS", "TILT EQ", "OCTAVER", "BIT-CRUSH", "TALKBOX", "STUTTER",
                                           "EQ", "PHASER", "FLANGER", "AUTO-PAN", "FUZZ" };
        return n;
    }
    /** What the X and Y axes do for each module (shown on the pads). */
    inline std::pair<const char*, const char*> axisNames (int m)
    {
        switch (m)
        {
            case 1:  return { "CARRIER", "SPREAD" };
            case 2:  return { "DETUNE", "DELAY" };
            case 3:  return { "FORMANT", "CHORD" };
            case 4:  return { "DRIVE", "TONE" };
            case 5:  return { "AMOUNT", "COLOUR" };
            case 6:  return { "SIZE", "DAMP" };
            case 7:  return { "TIME", "FEEDBACK" };
            case 8:  return { "RATE", "DEPTH" };
            case 9:  return { "THRESHOLD", "RATIO" };
            case 10: case 11: return { "CUTOFF", "RESONANCE" };
            case 12: return { "TILT", "AIR" };
            case 13: return { "SUB", "UP OCTAVE" };
            case 14: return { "BITS", "RATE" };
            case 15: return { "VOWEL", "RESONANCE" };
            case 16: return { "DIVISION", "DEPTH" };
            case 17: return { "FREQUENCY", "GAIN" };
            case 18: return { "RATE", "DEPTH" };
            case 19: return { "RATE", "FEEDBACK" };
            case 20: return { "RATE", "DEPTH" };
            case 21: return { "DRIVE", "TONE" };
            default: return { "X", "Y" };
        }
    }

    /** One rack slot. Holds the state every module needs (only the chosen module runs). */
    struct Slot
    {
        int type = 0;
        double sr = 44100;
        Svf f[2][3];
        SqVocal::Ring dl[2]; juce::int64 dw = 0;
        juce::Reverb verb;
        float apz[2][6] {};
        float env = 0, gr = 1, ph = 0, ph2 = 0, lfo = 0, holdL = 0, holdR = 0, noiseLp = 0, subPh = 0, subSign = 1, lastX = 0, rnd = 0;
        int holdCount = 0;
        static constexpr int bands = 16;
        Svf an[bands], sy[2][bands]; float bandEnv[bands] {};
        Eng::Biquad eqB[2], eqH[2];
        juce::Random random;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& d : dl) { d.prepare ((int) (sr * 2.1)); d.clear(); }
            dw = 0;
            juce::Reverb::Parameters p; verb.setParameters (p); verb.setSampleRate (sr); verb.reset();
            reset();
        }
        void reset()
        {
            for (auto& a : f) for (auto& b : a) b.reset();
            for (auto& b : an) b.reset();
            for (auto& c : sy) for (auto& b : c) b.reset();
            for (auto& e : bandEnv) e = 0;
            for (auto& c : apz) for (auto& z : c) z = 0;
            env = 0; gr = 1; ph = ph2 = lfo = 0; holdL = holdR = 0; holdCount = 0; noiseLp = 0; subPh = 0;
            for (auto& d : dl) d.clear();
            verb.reset();
        }

        /** x, y in 0..1; f0 = tuned pitch in Hz (0 when unvoiced); bpm for synced modules. */
        void process (float* L, float* R, int n, float x, float y, float mix, float f0, double bpm)
        {
            if (type == 0 || mix <= 0.0001f) return;
            const float tw = juce::MathConstants<float>::twoPi;
            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i], inR = R[i];
                float oL = inL, oR = inR;
                switch (type)
                {
                    case 1:   // MUTATE: ring modulation on a carrier locked to the sung pitch
                    {
                        const float ratio = std::pow (2.0f, juce::jmap (x, -2.0f, 2.0f));
                        const float fc = (f0 > 0 ? f0 : 220.0f) * ratio;
                        ph += fc / (float) sr; ph -= std::floor (ph);
                        ph2 += fc * (1.0f + y * 0.03f) / (float) sr; ph2 -= std::floor (ph2);
                        oL = inL * std::sin (tw * ph) * 1.4f; oR = inR * std::sin (tw * ph2) * 1.4f;
                        break;
                    }
                    case 2:   // DUET: two modulated delays, panned wide
                    {
                        dl[0].at (dw) = inL; dl[1].at (dw) = inR;
                        lfo += 0.31f / (float) sr; lfo -= std::floor (lfo);
                        const float base = (0.006f + y * 0.034f) * (float) sr;
                        const float depth = (0.0002f + x * 0.0022f) * (float) sr;
                        const float a = dl[0].cubic ((double) dw - base - depth * (1.0f + std::sin (tw * lfo)));
                        const float b = dl[1].cubic ((double) dw - base * 1.37f - depth * (1.0f + std::sin (tw * (lfo + 0.37f))));
                        ++dw;
                        oL = inL * 0.75f + a * 0.85f; oR = inR * 0.75f + b * 0.85f;
                        break;
                    }
                    case 3:   // VOCODE: 16-band vocoder, carrier = saw chord on the tuned pitch
                    {
                        const float m = 0.5f * (inL + inR);
                        const float fc = f0 > 0 ? f0 : 0.0f;
                        float car = 0;
                        if (fc > 0)
                        {
                            ph += fc / (float) sr; ph -= std::floor (ph);
                            ph2 += fc * 1.4983f / (float) sr; ph2 -= std::floor (ph2);
                            subPh += fc * 2.0f / (float) sr; subPh -= std::floor (subPh);
                            car = (2.0f * ph - 1.0f) + y * 0.7f * (2.0f * ph2 - 1.0f) + y * 0.5f * (2.0f * subPh - 1.0f);
                        }
                        car += (random.nextFloat() * 2.0f - 1.0f) * 0.04f;
                        const float shift = std::pow (2.0f, juce::jmap (x, -1.0f, 1.0f));
                        if (i == 0) setupVocoder (shift);
                        float out = 0;
                        for (int k = 0; k < bands; ++k)
                        {
                            const float bp = an[k].bandpassNorm (m);
                            bandEnv[k] += (std::abs (bp) - bandEnv[k]) * (std::abs (bp) > bandEnv[k] ? 0.01f : 0.002f);
                            out += sy[0][k].bandpassNorm (car) * bandEnv[k];
                        }
                        oL = oR = out * 6.0f;
                        break;
                    }
                    case 4:   // TUBE: asymmetric triode-style drive with a tone control
                    {
                        const float g = 1.0f + x * x * 24.0f;
                        auto sh = [g] (float v) { const float b = 0.12f; return (std::tanh (g * v + b) - std::tanh (b)) / std::tanh (g * 0.5f + b) * 0.5f; };
                        const float cut = 1500.0f * std::pow (12.0f, y);
                        f[0][0].set (sr, cut); f[1][0].set (sr, cut);
                        const float makeup = 1.0f / (1.0f + x * x * 6.0f);
                        oL = f[0][0].lp (sh (inL)) * makeup; oR = f[1][0].lp (sh (inR)) * makeup;
                        break;
                    }
                    case 5:   // BREATH: shaped noise that follows the voice
                    {
                        const float m = std::abs (0.5f * (inL + inR));
                        env += (m - env) * (m > env ? 0.02f : 0.0015f);
                        const float c = 1500.0f * std::pow (6.0f, y);
                        f[0][0].set (sr, c, 0.9f); f[1][0].set (sr, c * 1.07f, 0.9f);
                        const float nL = f[0][0].bandpassNorm (random.nextFloat() * 2.0f - 1.0f), nR = f[1][0].bandpassNorm (random.nextFloat() * 2.0f - 1.0f);
                        oL = inL + nL * env * x * 3.0f; oR = inR + nR * env * x * 3.0f;
                        break;
                    }
                    case 6:   // REVERB
                    {
                        if (i == 0) { juce::Reverb::Parameters p; p.roomSize = 0.3f + 0.69f * x; p.damping = y; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; verb.setParameters (p); }
                        float a = inL, b = inR; verb.processStereo (&a, &b, 1);
                        oL = a * 0.35f; oR = b * 0.35f;
                        break;
                    }
                    case 7:   // ECHO: tempo-synced, filtered feedback
                    {
                        static const float divs[] = { 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                        const float beats = divs[juce::jlimit (0, 6, (int) (x * 6.99f))];
                        const float d = (float) juce::jlimit (8.0, sr * 2.0, beats * 60.0 / juce::jmax (30.0, bpm) * sr);
                        const float fb = y * 0.88f;
                        f[0][0].set (sr, 5500.0); f[1][0].set (sr, 5500.0);
                        const float a = dl[0].cubic ((double) dw - d), b = dl[1].cubic ((double) dw - d);
                        dl[0].at (dw) = inL + f[0][0].lp (b) * fb;   // ping-pong
                        dl[1].at (dw) = inR + f[1][0].lp (a) * fb;
                        ++dw;
                        oL = inL + a; oR = inR + b;
                        break;
                    }
                    case 8:   // CHORUS
                    {
                        dl[0].at (dw) = inL; dl[1].at (dw) = inR;
                        lfo += (0.1f + x * 4.9f) / (float) sr; lfo -= std::floor (lfo);
                        const float dep = (0.0008f + y * 0.009f) * (float) sr, base = 0.012f * (float) sr;
                        oL = 0.6f * inL + 0.7f * dl[0].cubic ((double) dw - base - dep * (0.5f + 0.5f * std::sin (tw * lfo)));
                        oR = 0.6f * inR + 0.7f * dl[1].cubic ((double) dw - base - dep * (0.5f + 0.5f * std::sin (tw * (lfo + 0.25f))));
                        ++dw;
                        break;
                    }
                    case 9:   // COMPRESS: RMS compressor with automatic make-up
                    {
                        const float m = 0.5f * (inL * inL + inR * inR);
                        env += (m - env) * 0.0015f;
                        const float lvl = 10.0f * std::log10 (env + 1e-12f);
                        const float th = -40.0f + 40.0f * x, ratio = 1.0f + y * 19.0f;
                        const float over = juce::jmax (0.0f, lvl - th);
                        const float target = juce::Decibels::decibelsToGain (-over * (1.0f - 1.0f / ratio) + (-th) * (1.0f - 1.0f / ratio) * 0.3f);
                        gr += (target - gr) * (target < gr ? 0.01f : 0.0005f);
                        oL = inL * gr; oR = inR * gr;
                        break;
                    }
                    case 10: case 11:   // LOW-PASS / HIGH-PASS
                    {
                        const float cut = 40.0f * std::pow (450.0f, x), q = 0.6f + y * 7.0f;
                        f[0][0].set (sr, cut, q); f[1][0].set (sr, cut, q);
                        oL = type == 10 ? f[0][0].lp (inL) : f[0][0].hpf (inL);
                        oR = type == 10 ? f[1][0].lp (inR) : f[1][0].hpf (inR);
                        break;
                    }
                    case 12:   // TILT EQ + AIR
                    {
                        const float tilt = (x - 0.5f) * 24.0f, air = y * 12.0f;
                        f[0][0].set (sr, 900.0); f[1][0].set (sr, 900.0); f[0][1].set (sr, 9000.0); f[1][1].set (sr, 9000.0);
                        const float gLo = juce::Decibels::decibelsToGain (-tilt * 0.5f), gHi = juce::Decibels::decibelsToGain (tilt * 0.5f), gAir = juce::Decibels::decibelsToGain (air) - 1.0f;
                        auto proc = [&] (int c, float v)
                        {
                            float lo, bp, hp; f[c][0].tick (v, lo, bp, hp);
                            float lo2, bp2, hp2; f[c][1].tick (v, lo2, bp2, hp2);
                            return lo * gLo + (bp + hp) * gHi + hp2 * gAir;
                        };
                        oL = proc (0, inL); oR = proc (1, inR);
                        break;
                    }
                    case 13:   // OCTAVER: tracked sub-octave sine + rectified octave up
                    {
                        const float m = 0.5f * (inL + inR);
                        env += (std::abs (m) - env) * (std::abs (m) > env ? 0.01f : 0.001f);
                        float sub = 0;
                        if (f0 > 0) { subPh += 0.5f * f0 / (float) sr; subPh -= std::floor (subPh); sub = std::sin (tw * subPh) * env * 2.2f; }
                        f[0][0].set (sr, 60.0); f[1][0].set (sr, 60.0);
                        const float up = f[0][0].hpf (std::abs (inL)) * 1.5f, upR = f[1][0].hpf (std::abs (inR)) * 1.5f;
                        oL = inL + sub * x * 0.9f + up * y * 0.6f; oR = inR + sub * x * 0.9f + upR * y * 0.6f;
                        break;
                    }
                    case 14:   // BIT-CRUSH
                    {
                        const float bits = 16.0f - x * 12.0f;
                        const int hold = 1 + (int) (y * y * 40.0f);
                        if (++holdCount >= hold) { holdCount = 0; holdL = inL; holdR = inR; }
                        const float q = std::pow (2.0f, bits - 1.0f);
                        oL = std::round (holdL * q) / q; oR = std::round (holdR * q) / q;
                        break;
                    }
                    case 15:   // TALKBOX: three-formant vowel filter morphing A-E-I-O-U
                    {
                        static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1900, 2950 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                        const float pos = x * 3.999f; const int a = (int) pos; const float t = pos - (float) a;
                        const float q = 2.0f + y * 8.0f;
                        if (i == 0) for (int k = 0; k < 3; ++k) { const float fr = F[a][k] + (F[a + 1][k] - F[a][k]) * t; f[0][k].set (sr, fr, q); f[1][k].set (sr, fr, q); }
                        float l = 0, r = 0;
                        for (int k = 0; k < 3; ++k) { const float w = k == 0 ? 1.0f : k == 1 ? 0.7f : 0.4f; l += f[0][k].bandpassNorm (inL) * w; r += f[1][k].bandpassNorm (inR) * w; }
                        // keep the level of the voice whatever vowel the filter is on
                        env += ((inL * inL + inR * inR) - env) * 0.0005f;
                        noiseLp += ((l * l + r * r) - noiseLp) * 0.0005f;
                        const float ag = noiseLp > 1e-10f ? juce::jlimit (0.25f, 6.0f, std::sqrt (env / noiseLp)) : 1.0f;
                        gr += (ag - gr) * 0.001f;
                        oL = l * gr; oR = r * gr;
                        break;
                    }
                    case 16:   // STUTTER: tempo-synced gate
                    {
                        static const float divs[] = { 1.0f, 0.5f, 0.25f, 0.125f };
                        const float beats = divs[juce::jlimit (0, 3, (int) (x * 3.99f))];
                        ph += (float) (bpm / 60.0 / sr / beats); ph -= std::floor (ph);
                        const float gate = ph < 0.5f ? 1.0f : 1.0f - y;
                        env += (gate - env) * 0.01f;
                        oL = inL * env; oR = inR * env;
                        break;
                    }
                    case 17:   // EQ: a sweepable bell (100 Hz - 10 kHz, +-15 dB) over a gentle 80 Hz low cut
                    {
                        if (i == 0)
                        {
                            const double fq = 100.0 * std::pow (100.0, (double) x), gdb = (y - 0.5) * 30.0;
                            for (int c = 0; c < 2; ++c) { eqB[c].set (Eng::Biquad::peak, sr, fq, 1.1, gdb); eqH[c].set (Eng::Biquad::highpass, sr, 80.0, 0.6); }
                        }
                        oL = eqB[0].process (eqH[0].process (inL)); oR = eqB[1].process (eqH[1].process (inR));
                        break;
                    }
                    case 18:   // PHASER: six all-pass stages swept by an LFO, with feedback
                    {
                        lfo += (0.05f + x * x * 6.0f) / (float) sr; lfo -= std::floor (lfo);
                        float outs[2]; const float in[2] = { inL, inR };
                        for (int c = 0; c < 2; ++c)
                        {
                            const float l = 0.5f + 0.5f * std::sin (tw * (lfo + 0.25f * (float) c));
                            const float fc = 200.0f * std::pow (20.0f, l * (0.3f + 0.7f * y));
                            const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, (float) sr * 0.45f) / (float) sr), a = (t - 1.0f) / (t + 1.0f);
                            float v = in[c] + apz[c][5] * 0.55f * y;
                            for (int k = 0; k < 5; ++k) { const float yk = a * v + apz[c][k]; apz[c][k] = v - a * yk; v = yk; }
                            apz[c][5] = v;
                            outs[c] = 0.5f * (in[c] + v);
                        }
                        oL = outs[0] * 1.3f; oR = outs[1] * 1.3f;
                        break;
                    }
                    case 19:   // FLANGER: a short modulated delay with feedback
                    {
                        lfo += (0.05f + x * x * 3.0f) / (float) sr; lfo -= std::floor (lfo);
                        const float fb = -0.2f + y * 0.95f;
                        const float dA = (0.0008f + 0.0042f * (0.5f + 0.5f * std::sin (tw * lfo))) * (float) sr;
                        const float dB = (0.0008f + 0.0042f * (0.5f + 0.5f * std::sin (tw * (lfo + 0.25f)))) * (float) sr;
                        const float a = dl[0].cubic ((double) dw - dA), b = dl[1].cubic ((double) dw - dB);
                        dl[0].at (dw) = inL + a * fb; dl[1].at (dw) = inR + b * fb;
                        ++dw;
                        oL = 0.7f * (inL + a); oR = 0.7f * (inR + b);
                        break;
                    }
                    case 20:   // AUTO-PAN: tempo-synced (or free) panning, equal power
                    {
                        static const float divs[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
                        const float beats = divs[juce::jlimit (0, 5, (int) (x * 5.99f))];
                        ph += (float) (bpm / 60.0 / sr / beats); ph -= std::floor (ph);
                        const float pan = y * std::sin (tw * ph);
                        const float m = 0.5f * (inL + inR), sd = 0.5f * (inL - inR);
                        const float gl = std::cos ((pan + 1.0f) * 0.785398f) * 1.41421f, grr = std::sin ((pan + 1.0f) * 0.785398f) * 1.41421f;
                        oL = (m + sd) * gl; oR = (m - sd) * grr;
                        break;
                    }
                    case 21:   // FUZZ: hard, rectified clipping with a tone control; its level follows the input
                    {
                        const float g = 2.0f + x * x * 60.0f;
                        auto fz = [g] (float v) { const float a = g * v; const float c = juce::jlimit (-1.0f, 1.0f, a + 0.2f * std::abs (a)); return c * 0.6f; };
                        const float cut = 1200.0f * std::pow (8.0f, y);
                        f[0][0].set (sr, cut); f[1][0].set (sr, cut);
                        const float l = f[0][0].lp (fz (inL)), r = f[1][0].lp (fz (inR));
                        env += ((inL * inL + inR * inR) - env) * 0.0008f;
                        noiseLp += ((l * l + r * r) - noiseLp) * 0.0008f;
                        const float ag = noiseLp > 1e-10f ? juce::jlimit (0.05f, 2.0f, std::sqrt (env / noiseLp)) : 1.0f;
                        gr += (ag - gr) * 0.002f;
                        oL = l * gr; oR = r * gr;
                        break;
                    }
                    default: break;
                }
                L[i] = inL + (oL - inL) * mix;
                R[i] = inR + (oR - inR) * mix;
            }
        }

    private:
        float lastShift = -1;
        void setupVocoder (float shift)
        {
            if (std::abs (shift - lastShift) < 0.002f) return;
            lastShift = shift;
            for (int k = 0; k < bands; ++k)
            {
                const double fc = 120.0 * std::pow (8000.0 / 120.0, (double) k / (bands - 1));
                an[k].set (sr, fc, 6.0f);
                sy[0][k].set (sr, juce::jlimit (40.0, sr * 0.45, fc * shift), 6.0f);
            }
        }
    };
}

//==============================================================================
class SqTuneFxPlugin : public te::Plugin,
                       public SqDsp::BankOwner,
                       private juce::Timer,
                       private juce::AsyncUpdater
{
public:
    enum P { retune, humanize, formant, throat, transpose, motionRate, motionGlide, mix, output,
             s1x, s1y, s1mix, s2x, s2y, s2mix, s3x, s3y, s3mix, s4x, s4y, s4mix, s5x, s5y, s5mix, s6x, s6y, s6mix, numParams };
    static constexpr int numSlots = 6, maxSteps = 16;

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> specs = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "retune",      "Retune Speed",  0.0f, 400.0f, 0.0f, 0.35f, " ms", 0 },
                { "humanize",    "Humanize",      0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
                { "formant",     "Formant",       -12.0f, 12.0f, 0.0f, 1.0f, " st", 1 },
                { "throat",      "Throat",        50.0f, 150.0f, 100.0f, 1.0f, " %", 0 },
                { "transpose",   "Transpose",     -24.0f, 24.0f, 0.0f, 1.0f, " st", 0 },
                { "motionRate",  "Motion Rate",   0.25f, 20.0f, 4.0f, 0.5f, " Hz", 2 },
                { "motionGlide", "Motion Glide",  0.0f, 300.0f, 0.0f, 0.5f, " ms", 0 },
                { "mix",         "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "output",      "Output",        -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
            auto addS = [] (const std::string& s) { names.push_back (s); return names.back().c_str(); };
            for (int i = 1; i <= numSlots; ++i)
            {
                const auto n = std::to_string (i);
                v.push_back ({ addS ("s" + n + "x"),   addS ("Slot " + n + " X"),   0.0f, 1.0f, 0.5f, 1.0f, "", 2 });
                v.push_back ({ addS ("s" + n + "y"),   addS ("Slot " + n + " Y"),   0.0f, 1.0f, 0.5f, 1.0f, "", 2 });
                v.push_back ({ addS ("s" + n + "mix"), addS ("Slot " + n + " Mix"), 0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
            }
            return v;
        }();
        return specs;
    }

    SqTuneFxPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        engine.referTo (state, "engine", um, 0);
        voiceType.referTo (state, "voiceType", um, 1);
        keyRoot.referTo (state, "keyRoot", um, 0);
        scaleIndex.referTo (state, "scale", um, 0);
        autoKey.referTo (state, "autoKey", um, false);
        preserve.referTo (state, "formantPreserve", um, true);
        motionMode.referTo (state, "motionMode", um, 0);
        motionSync.referTo (state, "motionSync", um, 4);
        motionLen.referTo (state, "motionLen", um, 8);
        motionSteps.referTo (state, "motionSteps", um, "0 2 4 2 0 2 4 7 0 0 0 0 0 0 0 0");
        motionDiatonic.referTo (state, "motionDiatonic", um, true);
        motionLatch.referTo (state, "motionLatch", um, false);
        motionStyle.referTo (state, "motionStyle", um, 0);
        for (int i = 0; i < numSlots; ++i)
        {
            slotTrig[(size_t) i].referTo (state, juce::Identifier ("slotTrig" + juce::String (i + 1)), um, 0);
            slotType[(size_t) i].referTo (state, juce::Identifier ("slotType" + juce::String (i + 1)), um, 0);
            slotOn[(size_t) i].referTo (state, juce::Identifier ("slotOn" + juce::String (i + 1)), um, true);
        }
        startTimerHz (10);
    }
    ~SqTuneFxPlugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Moth Tune FX 2"); }
    static constexpr const char* xmlTypeName = "sqmTuneFx";
    juce::String getName() const override               { return "Moth Tune FX 2"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "TuneFX"; }
    juce::String getSelectableDescription() override    { return "Moth Tune FX 2 creative vocal effects"; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return true; }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 4.0; }
    bool producesAudioWhenNoAudioInput() override       { return true; }   // reverb / echo tails
    double getLatencySeconds() override                 { return (double) SqVocal::TuneProcessor::latencyFor (engine.get(), voiceType.get(), sampleRate) / juce::jmax (1.0, sampleRate); }

    static juce::StringArray motionModeNames()  { return { "OFF", "AUTO", "MOMENTARY", "TOGGLE", "ALWAYS" }; }
    static juce::StringArray motionSyncNames()  { return { "FREE", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
    static juce::StringArray motionStyleNames() { return { "HARD", "SOFT", "LEGATO" }; }
    static juce::StringArray slotTrigNames()    { return { "ALWAYS", "AUTO", "MOMENTARY", "TOGGLE" }; }
    static double syncBeats (int i)              { const double b[] = { 0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 }; return b[juce::jlimit (0, 6, i)]; }

    struct MotionPattern { const char* name; const char* steps; int len; int sync; bool diatonic; };
    static const std::vector<MotionPattern>& factoryPatterns()
    {
        static const std::vector<MotionPattern> p {
            { "Up Arp",         "0 2 4 7",                      4, 4, true },
            { "Down Arp",       "7 4 2 0",                      4, 4, true },
            { "Up & Down",      "0 2 4 7 4 2",                  6, 4, true },
            { "Trill",          "0 1",                          2, 6, true },
            { "Octave Bounce",  "0 7",                          2, 4, true },
            { "Stutter Fifths", "0 0 4 0 0 4 0 4",              8, 6, true },
            { "Triplet Run",    "0 2 4 0 2 4 7 4 2",            9, 5, true },
            { "Trap Flip",      "0 0 -1 0 2 0 -1 -3",           8, 6, true },
            { "Yodel",          "0 5 0 5 3 5 3 0",              8, 4, true },
            { "Chromatic Rise", "0 1 2 3 4 5 6 7",              8, 4, false },
            { "Robot Steps",    "0 x 0 x 7 x 5 x",              8, 4, true },
            { "Phone Ring",     "0 4 0 4 0 4 x x",              8, 6, true },
            { "Pentatonic Run", "0 1 2 3 4 3 2 1",              8, 4, true },
            { "Minor Drop",     "0 -1 -2 -3",                   4, 2, true },
            { "Fifth Pulse",    "0 4",                          2, 2, true },
            { "Siren",          "0 2 4 7 9 7 4 2",              8, 6, true },
            { "Gate Steps",     "0 x 0 x 0 x 0 0",              8, 6, true },
            { "Sparkle",        "7 0 9 0 11 0 9 0",             8, 6, true },
            { "Half-Step Wobble", "0 1 0 -1",                   4, 5, false },
            { "Wide Leap",      "0 7 -7 0",                     4, 2, true },
            { "Bounce Ball",    "7 x 4 x 2 1 0 x",              8, 4, true },
            { "Glitch Run",     "0 0 0 4 x 7 7 x 0 x 4 x 2 x 0 x", 16, 6, true },
            { "Slow Climb",     "0 1 2 3",                      4, 1, true },
            { "Call Response",  "0 2 4 x -3 -1 0 x",            8, 4, true } };
        return p;
    }

    SqDsp::ParamBank bank;
    juce::CachedValue<int> engine, voiceType, keyRoot, scaleIndex, motionMode, motionSync, motionLen;
    juce::CachedValue<juce::String> motionSteps;
    juce::CachedValue<bool> autoKey, preserve, motionDiatonic, motionLatch;
    juce::CachedValue<int> motionStyle;
    std::array<juce::CachedValue<int>, numSlots> slotType, slotTrig;
    std::array<juce::CachedValue<bool>, numSlots> slotOn;
    std::atomic<bool> momentaryHeld { false };       // the editor's HOLD button / MIDI notes
    std::atomic<int> currentStep { -1 };
    std::atomic<float> liveNote { -1 };
    std::array<std::atomic<float>, numSlots> slotActivity {};

    SqDsp::ParamBank& getBank() override   { return bank; }
    void resetProps (juce::UndoManager* um) override
    {
        engine.setValue (0, um); motionMode.setValue (0, um); motionSync.setValue (4, um); motionLen.setValue (8, um);
        motionSteps.setValue ("0 2 4 2 0 2 4 7 0 0 0 0 0 0 0 0", um); motionDiatonic.setValue (true, um); motionLatch.setValue (false, um);
        preserve.setValue (true, um);
        motionStyle.setValue (0, um);
        for (int i = 0; i < numSlots; ++i) { slotType[(size_t) i].setValue (0, um); slotOn[(size_t) i].setValue (true, um); slotTrig[(size_t) i].setValue (0, um); }
    }

    /** Steps as numbers; rests are NaN. */
    std::array<float, maxSteps> parsedSteps() const
    {
        std::array<float, maxSteps> out; out.fill (0.0f);
        auto toks = juce::StringArray::fromTokens (motionSteps.get(), " ,", "");
        for (int i = 0; i < maxSteps && i < toks.size(); ++i)
            out[(size_t) i] = toks[i].trim().equalsIgnoreCase ("x") || toks[i].trim() == "-" ? std::numeric_limits<float>::quiet_NaN() : toks[i].getFloatValue();
        return out;
    }
    void setStep (int i, float v, bool rest, juce::UndoManager* um)
    {
        auto st = parsedSteps();
        st[(size_t) juce::jlimit (0, maxSteps - 1, i)] = rest ? std::numeric_limits<float>::quiet_NaN() : v;
        juce::StringArray s;
        for (auto x : st) s.add (std::isnan (x) ? "x" : juce::String (juce::roundToInt (x)));
        motionSteps.setValue (s.joinIntoString (" "), um);
    }
    /** Swap two rack slots (module, on/off, X, Y and mix). */
    void swapSlots (int a, int b, juce::UndoManager* um)
    {
        if (a == b || ! juce::isPositiveAndBelow (a, numSlots) || ! juce::isPositiveAndBelow (b, numSlots)) return;
        const int ta = slotType[(size_t) a].get(); const bool oa = slotOn[(size_t) a].get(); const int tra = slotTrig[(size_t) a].get();
        slotTrig[(size_t) a].setValue (slotTrig[(size_t) b].get(), um); slotTrig[(size_t) b].setValue (tra, um);
        slotType[(size_t) a].setValue (slotType[(size_t) b].get(), um); slotOn[(size_t) a].setValue (slotOn[(size_t) b].get(), um);
        slotType[(size_t) b].setValue (ta, um); slotOn[(size_t) b].setValue (oa, um);
        for (int k = 0; k < 3; ++k)
        {
            auto pa = bank.params[(size_t) (s1x + a * 3 + k)], pb = bank.params[(size_t) (s1x + b * 3 + k)];
            const float va = pa->getCurrentValue();
            pa->setParameter (pb->getCurrentValue(), juce::sendNotification); pb->setParameter (va, juce::sendNotification);
        }
    }
    int allowedMask() const
    {
        const int sc = scaleIndex.get();
        return sc >= (int) SqScales::scales().size() ? 0xfff : SqScales::maskFor (keyRoot.get(), sc);
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        proc.prepare (sr, engine.get(), voiceType.get());
        proc.onFrame = [this] (const SqVocal::FrameResult& r, const std::array<bool, 128>&) { liveNote = r.voiced ? r.outNote : -1.0f; lastVoiced = r.voiced; };
        for (auto& s : slots) s.prepare (sr);
        heldIn.fill (false); stepClock = 0; toggled = false; wasVoiced = false; restGain = 1; lastStepValue = 0; slotToggled.fill (false);
        for (int i = 0; i < numSlots; ++i) slotGate[(size_t) i] = slotTrig[(size_t) i].get() == 0 ? 1.0f : 0.0f;
    }
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
    }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& buf = *fc.destBuffer;
        const int nch = juce::jmin (2, buf.getNumChannels());
        if (nch <= 0) return;
        bool midiTrigger = false;
        if (fc.bufferForMidiMessages != nullptr)
            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn()) { heldIn[(size_t) m.getNoteNumber()] = true; if (motionMode.get() == 3) toggled = ! toggled; stepClock = 0; for (auto& t : slotToggled) t = ! t; }
                else if (m.isNoteOff()) heldIn[(size_t) m.getNoteNumber()] = false;
                else if (m.isAllNotesOff()) heldIn.fill (false);
            }
        for (auto h : heldIn) midiTrigger = midiTrigger || h;

        SqVocal::TuneSettings s;
        s.retuneMs = bank[retune]; s.humanize = bank[humanize] / 100.0f; s.vibKeep = 0; s.classic = true;
        s.formantSt = bank[formant]; s.throat = bank[throat] / 100.0f; s.transpose = std::round (bank[transpose]); s.preserve = preserve.get();
        s.allowedMask = allowedMask(); s.keyRoot = keyRoot.get();
        s.motionDiatonic = motionDiatonic.get(); s.motionGlideMs = bank[motionGlide];
        const int style = juce::jlimit (0, 2, motionStyle.get());
        if (style == 1) s.motionGlideMs = juce::jmax (s.motionGlideMs, 40.0f);
        if (style == 2) s.motionGlideMs = juce::jmax (s.motionGlideMs, 110.0f);
        SqVocal::TuneProcessor::Context ctx;
        ctx.playing = fc.isPlaying; ctx.editStart = fc.editTime.getStart().inSeconds();
        ctx.mix = 1.0f; ctx.outGain = 1.0f;

        // motion clock
        const int mode = motionMode.get();
        const auto steps = parsedSteps();
        const int len = juce::jlimit (1, maxSteps, motionLen.get());
        const double bpmNow = bpm.load();
        const double stepSec = motionSync.get() == 0 ? 1.0 / juce::jmax (0.05f, bank[motionRate]) : syncBeats (motionSync.get()) * 60.0 / juce::jmax (20.0, bpmNow);
        const bool held = midiTrigger || momentaryHeld.load();
        bool running = false;
        switch (mode)
        {
            case 1: running = lastVoiced || (motionLatch.get() && latchedRun); break;
            case 2: running = held; break;
            case 3: running = toggled != momentaryHeld.load(); break;
            case 4: running = true; break;
            default: break;
        }
        if (mode == 1)
        {
            if (lastVoiced && ! wasVoiced && style != 2) stepClock = 0;   // each new phrase starts the pattern again (Legato runs on)
            if (lastVoiced) latchedRun = true;
            wasVoiced = lastVoiced;
        }

        float* io[2] = { buf.getWritePointer (0, fc.bufferStartSample), buf.getWritePointer (nch > 1 ? 1 : 0, fc.bufferStartSample) };
        const int num = fc.bufferNumSamples;
        int done = 0;
        while (done < num)
        {
            int chunk = num - done;
            int stepIdx = -1;
            if (running && mode != 0)
            {
                const double stepSamples = stepSec * sr;
                const double posInStep = std::fmod (stepClock, stepSamples);
                chunk = juce::jlimit (1, num - done, (int) std::ceil (stepSamples - posInStep));
                stepIdx = (int) std::floor (stepClock / stepSamples) % len;
                const float v = steps[(size_t) stepIdx];
                if (std::isnan (v) && style == 2) s.motionSteps = lastStepValue;      // Legato: a rest holds the note
                else { s.motionSteps = std::isnan (v) ? 0.0f : v; lastStepValue = s.motionSteps; }
                restNow = std::isnan (v) && style != 2;
                stepClock += chunk;
            }
            else { s.motionSteps = 0; restNow = false; }
            currentStep = stepIdx;
            float* sub[2] = { io[0] + done, io[1] + done };
            proc.process (sub, nch, chunk, s, ctx);
            // rests: Hard cuts, Soft fades (about 12 ms), Legato never rests
            const float restTarget = restNow ? 0.0f : 1.0f;
            const float rk = style == 0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (0.012 * sr)));
            for (int i = 0; i < chunk; ++i)
            {
                restGain += rk * (restTarget - restGain);
                for (int c = 0; c < nch; ++c) sub[c][i] *= restGain;
            }
            done += chunk;
            ctx.editStart += chunk / sr;
        }
        // rack
        if (nch == 1) { tmpR.resize ((size_t) num); std::copy (io[0], io[0] + num, tmpR.begin()); io[1] = tmpR.data(); }
        const float f0 = proc.latestHz() > 0 && liveNote.load() > 0 ? SqVocal::noteToHz (liveNote.load()) : 0.0f;
        dryL.assign (io[0], io[0] + num); dryR.assign (io[1], io[1] + num);
        for (int i = 0; i < numSlots; ++i)
        {
            auto& sl = slots[(size_t) i];
            const int t = slotOn[(size_t) i].get() ? slotType[(size_t) i].get() : 0;
            if (t != sl.type) { sl.type = t; sl.reset(); }
            // the slot's trigger: Always, Auto (while singing), Momentary (a MIDI note or HOLD down), Toggle (each MIDI note)
            bool open = true;
            switch (juce::jlimit (0, 3, slotTrig[(size_t) i].get()))
            {
                case 1: open = lastVoiced; break;
                case 2: open = held; break;
                case 3: open = slotToggled[(size_t) i] != momentaryHeld.load(); break;
                default: break;
            }
            const float want = open ? 1.0f : 0.0f, gk = (float) (1.0 - std::exp (-(double) num / (0.008 * sr)));
            slotGate[(size_t) i] += gk * (want - slotGate[(size_t) i]);
            const float sm = bank[(size_t) (s1mix + i * 3)] / 100.0f * slotGate[(size_t) i];
            if (sm <= 0.0001f && t != 0 && ! open) { slotActivity[(size_t) i] = 0.0f; continue; }
            sl.process (io[0], io[1], num, bank[(size_t) (s1x + i * 3)], bank[(size_t) (s1y + i * 3)], sm, f0, bpmNow);
            slotActivity[(size_t) i] = t != 0 ? juce::FloatVectorOperations::findMaximum (io[0], num) : 0.0f;
        }
        const float mixW = bank[mix] / 100.0f, outG = juce::Decibels::decibelsToGain (bank[output]);
        for (int i = 0; i < num; ++i)
        {
            io[0][i] = (dryL[(size_t) i] * (1.0f - mixW) + io[0][i] * mixW) * outG;
            if (nch > 1) io[1][i] = (dryR[(size_t) i] * (1.0f - mixW) + io[1][i] * mixW) * outG;
            if (! std::isfinite (io[0][i])) io[0][i] = 0;
            if (nch > 1 && ! std::isfinite (io[1][i])) io[1][i] = 0;
        }
    }

private:
    double sr = 44100;
    SqVocal::TuneProcessor proc;
    std::array<SqVocalFx::Slot, numSlots> slots;
    std::array<bool, 128> heldIn {};
    std::vector<float> tmpR, dryL, dryR;
    double stepClock = 0;
    bool toggled = false, wasVoiced = false, lastVoiced = false, latchedRun = false, restNow = false;
    float lastStepValue = 0, restGain = 1;
    std::array<float, numSlots> slotGate { 1, 1, 1, 1, 1, 1 };
    std::array<bool, numSlots> slotToggled {};
    std::atomic<double> bpm { 120.0 };

    void timerCallback() override
    {
        bpm = edit.tempoSequence.getTempo (0) != nullptr ? edit.tempoSequence.getTempo (0)->getBpm() : 120.0;
        if (autoKey.get() && edit.state.hasProperty ("sqmKeyRoot"))
        {
            const int r = (int) edit.state["sqmKeyRoot"], sc = (int) edit.state.getProperty ("sqmKeyScale", 1);
            if (keyRoot.get() != r) keyRoot.setValue (r, nullptr);
            if (scaleIndex.get() != sc) scaleIndex.setValue (sc, nullptr);
        }
    }
    void handleAsyncUpdate() override   { edit.restartPlayback(); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && (id == juce::Identifier ("engine") || id == juce::Identifier ("voiceType"))) triggerAsyncUpdate();
        if (v == state && id == juce::Identifier ("motionPattern") && juce::MessageManager::existsAndIsCurrentThread())
        {
            const int k = (int) state["motionPattern"];
            if (k > 0) juce::MessageManager::callAsync ([ref = juce::WeakReference<te::Selectable> (this), k] { if (auto p = dynamic_cast<SqTuneFxPlugin*> (ref.get())) p->applyPattern (k, p->getUndoManager()); });
        }
    }
public:
    /** Load factory motion pattern k (1-based) into the step editor. */
    void applyPattern (int k, juce::UndoManager* um)
    {
        auto& fp = factoryPatterns();
        if (k <= 0 || k > (int) fp.size()) return;
        auto& pt = fp[(size_t) k - 1];
        auto toks = juce::StringArray::fromTokens (pt.steps, " ", "");
        while (toks.size() < maxSteps) toks.add ("0");
        motionSteps.setValue (toks.joinIntoString (" "), um); motionLen.setValue (pt.len, um);
        motionSync.setValue (pt.sync, um); motionDiatonic.setValue (pt.diatonic, um);
        if (motionMode.get() == 0) motionMode.setValue (1, um);
        state.removeProperty ("motionPattern", nullptr);
    }
};

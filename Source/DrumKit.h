#pragma once
// Squelchmoth sound kits: twenty-seven drum kits (12 sounds each) synthesised at first use with a proper
// drum-synthesis engine - multi-layer kicks with pitch envelopes and transient clicks, resonator snares
// with wire noise, 808-style six-oscillator metallic hats, multi-burst claps, FM cymbals, 808 sub tones -
// rendered as 24-bit WAVs into the Kits folder, peak-normalised, then loaded into the Sampler for the beat maker.
//
//   Mothwing 808   - the classic drum machine: deep booming kick, snappy 808 snare, metallic hats, cowbell
//   Hive 909       - club / techno: punchy kick, cracking snare, sizzling hats, big clap
//   Chrome Trap    - modern hip-hop: distorted knock kick, tight rimshots, crisp hats, gliding 808 sub
//   Velvet Lo-Fi   - dusty, filtered, tape-warm kit with vinyl noise for chill / boom-bap
//   Dust Acoustic  - a real-kit impression: resonant kick body, buzzy snare wires, washy ride and crash

#include "Session.h"
#include "KitInfo.h"

namespace DrumKit
{
    //==========================================================================
    // Tiny DSP toolkit for the renderer
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double sr, double f, double q)  { set (sr, f, q, 0); }
        void highpass (double sr, double f, double q) { set (sr, f, q, 1); }
        void bandpass (double sr, double f, double q) { set (sr, f, q, 2); }
        void set (double sr, double f, double q, int type)
        {
            f = juce::jlimit (20.0, sr * 0.45, f);
            double w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * juce::jmax (0.1, q));
            double a0 = 1 + al;
            if (type == 0)      { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
            else if (type == 1) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
            else                { b0 = al; b1 = 0; b2 = -al; }
            a1 = -2 * c; a2 = 1 - al;
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        }
        float process (float x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    struct Resonator   // 2-pole resonant body (for drum shells)
    {
        double a1 = 0, a2 = 0, y1 = 0, y2 = 0, gain = 1;
        void set (double sr, double f, double decaySecs)
        {
            double r = std::exp (-1.0 / (decaySecs * sr));
            a1 = -2.0 * r * std::cos (juce::MathConstants<double>::twoPi * f / sr);
            a2 = r * r;
            gain = (1.0 - r);
        }
        float process (float x) { double y = gain * x - a1 * y1 - a2 * y2; y2 = y1; y1 = y; return (float) y; }
    };

    inline float noise (juce::Random& r)                      { return r.nextFloat() * 2.0f - 1.0f; }
    inline float env (double t, double decay)                 { return t < 0 ? 0.0f : (float) std::exp (-t / decay); }
    inline float softclip (float x, float drive)              { return std::tanh (x * drive) / std::tanh (drive); }
    inline float sine (double& phase, double f, double sr)    { phase += f / sr; if (phase > 1) phase -= 1; return (float) std::sin (phase * juce::MathConstants<double>::twoPi); }
    inline float square (double& phase, double f, double sr)  { phase += f / sr; if (phase > 1) phase -= 1; return phase < 0.5 ? 1.0f : -1.0f; }

    /** 808-style metallic hat source: six detuned square waves summed and band-passed. */
    struct Metallic
    {
        double ph[6] = { 0, 0, 0, 0, 0, 0 };
        const double freqs[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        Biquad bp, hp;
        void prepare (double sr, double centre, double hpf) { bp.bandpass (sr, centre, 1.2); hp.highpass (sr, hpf, 0.7); }
        float process (double sr, double ratio)
        {
            float s = 0;
            for (int i = 0; i < 6; ++i) s += square (ph[i], freqs[i] * ratio, sr);
            return hp.process (bp.process (s / 6.0f));
        }
    };

    //==========================================================================
    /** Every number that gives a kit its character. The first five kits keep their original 1.0 values. */
    enum class Perc { cowbell, block, shaker, conga, logDrum, squeak, clave, phonkBell };
    struct Style
    {
        bool shellKick = false, kickExtraClip = false, lofi = false, acoustic = false;
        double kickBase = 46, kickSweep = 120, kickSweepRate = 26, kickDecay = 0.55; float kickDrive = 1.6f, clickAmt = 0.5f;
        double snareTone = 176, snareQ = 0.09, snareBp = 2200, wireDecay = 0.13; float wireGain = 0.75f, snareDrive = 1.4f;
        double rimFreq = 980;
        double clapBp = 1100, clapTail = 0.11;
        double hatBp = 7600, hatHp = 6000, hatRatio = 1.0, hatOpenDecay = 0.42, hatClosedDecay = 0.04; float hatNoise = 0.22f;
        double tomLow = 95, tomHigh = 160, tomDecay = 0.42, tomQ = 0.16; float tomRes = 0.4f;
        double rideDecay = 0.7, crashDecay = 1.1;
        Perc perc = Perc::cowbell;
        double subBase = 48, subDecay = 0.75, subGlide = 0.9; float subDrive = 1.2f, subClip = 1.3f;
        float room = 0.0f;   // small-room reverb send (Rock Room, Boom Bap)
    };

    inline Style styleFor (int kit)
    {
        Style s;
        const bool is808 = kit == 0, is909 = kit == 1, trap = kit == 2, lofi = kit == 3, acoustic = kit == 4;
        auto legacy = [&]
        {
            s.lofi = lofi; s.acoustic = acoustic; s.shellKick = acoustic; s.kickExtraClip = trap;
            s.kickDrive = is808 ? 1.6f : is909 ? 2.6f : trap ? 3.4f : lofi ? 2.0f : 1.3f;
            s.kickBase  = is808 ? 46.0 : is909 ? 52.0 : trap ? 44.0 : lofi ? 50.0 : 58.0;
            s.kickDecay = is808 ? 0.55 : is909 ? 0.28 : trap ? 0.42 : lofi ? 0.3 : 0.24;
            s.kickSweep = is909 ? 180.0 : 120.0; s.kickSweepRate = is909 ? 42.0 : 26.0; s.clickAmt = is909 ? 0.9f : 0.5f;
            s.snareTone = is808 ? 176.0 : is909 ? 190.0 : trap ? 210.0 : lofi ? 168.0 : 196.0;
            s.snareQ = acoustic ? 0.12 : 0.09; s.snareBp = acoustic ? 2800.0 : is909 ? 3600.0 : 2200.0;
            s.wireDecay = is909 ? 0.16 : acoustic ? 0.22 : 0.13; s.wireGain = is808 ? 0.75f : 1.0f; s.snareDrive = is909 ? 1.9f : 1.4f;
            s.rimFreq = trap ? 1750.0 : 980.0;
            s.clapBp = is909 ? 1500.0 : 1100.0; s.clapTail = is909 ? 0.17 : 0.11;
            s.hatBp = is909 ? 9000.0 : 7600.0; s.hatHp = is909 ? 7000.0 : 6000.0;
            s.hatRatio = is808 ? 1.0 : is909 ? 1.07 : trap ? 1.15 : lofi ? 0.93 : 1.0;
            s.hatNoise = is909 ? 0.5f : lofi ? 0.35f : 0.22f;
            s.hatOpenDecay = is808 ? 0.42 : 0.32; s.hatClosedDecay = trap ? 0.028 : 0.04;
            s.tomLow = is808 ? 95.0 : 105.0; s.tomHigh = is808 ? 160.0 : 175.0; s.tomDecay = is808 ? 0.42 : 0.26;
            s.tomQ = acoustic ? 0.22 : 0.16; s.tomRes = acoustic ? 1.5f : 0.4f;
            s.rideDecay = acoustic ? 1.1 : 0.7; s.crashDecay = acoustic ? 1.5 : 1.1;
            s.perc = (is808 || is909) ? Perc::cowbell : trap ? Perc::block : Perc::shaker;
            s.subBase = trap ? 55.0 : 48.0; s.subDecay = trap ? 0.9 : 0.75; s.subDrive = trap ? 1.8f : 1.2f; s.subClip = trap ? 2.4f : 1.3f;
        };
        legacy();
        switch (kit)
        {
            case 5: // Neon Trap
                s.kickExtraClip = true; s.kickBase = 48; s.kickDecay = 0.5; s.kickDrive = 3.8f; s.clickAmt = 0.7f;
                s.snareTone = 232; s.snareBp = 3800; s.wireDecay = 0.12; s.wireGain = 1.1f; s.snareDrive = 2.0f; s.rimFreq = 1850;
                s.clapBp = 1600; s.clapTail = 0.13; s.hatRatio = 1.22; s.hatClosedDecay = 0.022; s.hatNoise = 0.3f; s.hatBp = 9000; s.hatHp = 7200;
                s.perc = Perc::block; s.subBase = 49.0; s.subDecay = 1.3; s.subDrive = 2.2f; s.subClip = 2.6f; s.subGlide = 0.7;
                break;
            case 6: // Drill Smoke
                s.kickExtraClip = true; s.kickBase = 50; s.kickDecay = 0.32; s.kickDrive = 3.0f; s.kickSweepRate = 34; s.clickAmt = 0.8f;
                s.snareTone = 250; s.snareBp = 4200; s.wireDecay = 0.11; s.wireGain = 1.2f; s.snareDrive = 2.2f; s.rimFreq = 2100;
                s.clapBp = 1800; s.clapTail = 0.1; s.hatRatio = 1.28; s.hatClosedDecay = 0.02; s.hatNoise = 0.32f; s.hatBp = 9500; s.hatHp = 7600;
                s.perc = Perc::block; s.subBase = 46.0; s.subDecay = 1.6; s.subDrive = 2.4f; s.subClip = 2.8f; s.subGlide = 1.25;
                break;
            case 7: // Afro Sun
                s.kickBase = 55; s.kickDecay = 0.3; s.kickDrive = 1.6f; s.kickSweep = 140; s.kickSweepRate = 34; s.clickAmt = 0.6f;
                s.snareTone = 200; s.snareBp = 3000; s.wireDecay = 0.12; s.wireGain = 0.9f; s.rimFreq = 1400; s.clapBp = 1300; s.clapTail = 0.09;
                s.hatNoise = 0.6f; s.hatClosedDecay = 0.035; s.hatRatio = 1.05; s.hatBp = 8000; s.tomLow = 180; s.tomHigh = 260; s.tomDecay = 0.2;
                s.perc = Perc::conga; s.subBase = 50; s.subDecay = 0.8; s.subDrive = 1.3f; s.subClip = 1.4f;
                break;
            case 8: // Amapiano Log
                s.kickBase = 52; s.kickDecay = 0.35; s.kickDrive = 1.8f; s.kickSweep = 150; s.kickSweepRate = 32;
                s.snareTone = 205; s.clapBp = 1500; s.clapTail = 0.2; s.hatNoise = 0.6f; s.hatClosedDecay = 0.03; s.hatOpenDecay = 0.26; s.hatRatio = 1.08; s.hatBp = 8500; s.hatHp = 6800;
                s.perc = Perc::logDrum; s.subBase = 55; s.subDecay = 0.5; s.subDrive = 1.9f; s.subClip = 2.2f; s.subGlide = 1.6;
                break;
            case 9: // Dembow Heat
                s.kickBase = 50; s.kickDecay = 0.4; s.kickDrive = 2.2f; s.kickSweep = 150; s.kickSweepRate = 30; s.clickAmt = 0.7f;
                s.snareTone = 220; s.snareBp = 3400; s.wireDecay = 0.12; s.wireGain = 1.1f; s.snareDrive = 1.9f; s.rimFreq = 1600;
                s.hatBp = 9000; s.hatHp = 7000; s.hatRatio = 1.05; s.hatNoise = 0.45f; s.perc = Perc::block; s.subBase = 50; s.subDecay = 0.9;
                s.tomLow = 210; s.tomHigh = 320; s.tomDecay = 0.18;
                break;
            case 10: // Jersey Bounce
                s.kickBase = 54; s.kickDecay = 0.25; s.kickDrive = 2.8f; s.kickSweep = 190; s.kickSweepRate = 40; s.clickAmt = 0.9f;
                s.snareTone = 210; s.snareBp = 3600; s.clapBp = 1500; s.clapTail = 0.19; s.hatBp = 9000; s.hatHp = 7000; s.hatRatio = 1.1; s.hatNoise = 0.45f;
                s.perc = Perc::squeak; s.subBase = 52; s.subDecay = 0.7; s.subDrive = 1.8f;
                break;
            case 11: // Phonk Cowbell
                s.kickExtraClip = true; s.kickBase = 46; s.kickDecay = 0.45; s.kickDrive = 4.0f;
                s.snareTone = 190; s.snareDrive = 2.2f; s.clapBp = 1200; s.clapTail = 0.12; s.hatRatio = 0.95; s.hatNoise = 0.35f; s.hatClosedDecay = 0.03;
                s.perc = Perc::phonkBell; s.subBase = 46; s.subDecay = 1.0; s.subDrive = 3.0f; s.subClip = 3.2f;
                break;
            case 12: // Boom Bap Crates
                s.lofi = true; s.shellKick = true; s.kickBase = 56; s.kickDecay = 0.28; s.kickDrive = 2.4f; s.clickAmt = 0.6f;
                s.snareTone = 185; s.snareBp = 2600; s.wireDecay = 0.18; s.wireGain = 1.0f; s.snareDrive = 1.8f; s.snareQ = 0.11;
                s.hatNoise = 0.4f; s.hatRatio = 0.95; s.perc = Perc::shaker; s.room = 0.08f;
                break;
            case 13: // House Silk
                s.kickBase = 50; s.kickDecay = 0.34; s.kickDrive = 2.0f; s.kickSweep = 150; s.kickSweepRate = 36; s.clickAmt = 0.6f;
                s.snareTone = 195; s.snareBp = 3600; s.wireDecay = 0.15; s.clapBp = 1400; s.clapTail = 0.2;
                s.hatBp = 9500; s.hatHp = 7400; s.hatRatio = 1.07; s.hatNoise = 0.5f; s.hatOpenDecay = 0.28; s.perc = Perc::shaker;
                break;
            case 14: // Techno Steel
                s.kickBase = 48; s.kickDecay = 0.4; s.kickDrive = 3.4f; s.kickSweep = 200; s.kickSweepRate = 36; s.clickAmt = 1.0f;
                s.snareTone = 200; s.snareBp = 4000; s.rimFreq = 1750; s.clapBp = 1700; s.clapTail = 0.15;
                s.hatBp = 10000; s.hatHp = 7800; s.hatRatio = 1.12; s.hatNoise = 0.45f; s.perc = Perc::clave; s.rideDecay = 0.9;
                break;
            case 15: // R&B Velvet
                s.kickBase = 50; s.kickDecay = 0.5; s.kickDrive = 1.3f; s.clickAmt = 0.3f;
                s.snareTone = 190; s.snareBp = 2600; s.wireDecay = 0.15; s.wireGain = 0.8f; s.rimFreq = 1100;
                s.clapBp = 2000; s.clapTail = 0.06; s.hatNoise = 0.3f; s.hatRatio = 0.97; s.hatClosedDecay = 0.035;
                s.perc = Perc::shaker; s.subBase = 49; s.subDecay = 1.1; s.subDrive = 1.4f; s.subClip = 1.5f;
                break;
            case 16: // Rock Room
                s.acoustic = true; s.shellKick = true; s.kickBase = 60; s.kickDecay = 0.3; s.kickDrive = 1.6f; s.clickAmt = 0.7f;
                s.snareTone = 200; s.snareQ = 0.14; s.snareBp = 2800; s.wireDecay = 0.25; s.wireGain = 1.1f; s.snareDrive = 1.6f;
                s.tomLow = 90; s.tomHigh = 150; s.tomDecay = 0.34; s.tomQ = 0.26; s.tomRes = 1.7f; s.rideDecay = 1.3; s.crashDecay = 1.8;
                s.perc = Perc::shaker; s.room = 0.2f;
                break;
            case 17: // Memphis Tape
                s.lofi = true; s.kickExtraClip = true; s.kickBase = 46; s.kickDecay = 0.5; s.kickDrive = 3.6f; s.clickAmt = 0.4f;
                s.snareTone = 180; s.snareBp = 2000; s.wireDecay = 0.14; s.wireGain = 0.9f; s.snareDrive = 2.4f; s.rimFreq = 1200;
                s.clapBp = 1100; s.clapTail = 0.12; s.hatRatio = 0.92; s.hatNoise = 0.5f; s.hatBp = 6500; s.hatHp = 5000; s.hatClosedDecay = 0.035;
                s.perc = Perc::phonkBell; s.subBase = 45; s.subDecay = 1.1; s.subDrive = 3.2f; s.subClip = 3.4f;
                break;
            case 18: // Plugg Pillow
                s.kickBase = 48; s.kickDecay = 0.36; s.kickDrive = 1.4f; s.kickSweep = 100; s.kickSweepRate = 24; s.clickAmt = 0.3f;
                s.snareTone = 200; s.snareBp = 2400; s.wireDecay = 0.1; s.wireGain = 0.7f; s.snareDrive = 1.2f; s.rimFreq = 1500;
                s.clapBp = 1300; s.clapTail = 0.08; s.hatRatio = 1.02; s.hatNoise = 0.28f; s.hatClosedDecay = 0.025; s.hatOpenDecay = 0.22; s.hatBp = 8000;
                s.perc = Perc::block; s.subBase = 50; s.subDecay = 0.55; s.subDrive = 1.3f; s.subClip = 1.4f; s.subGlide = 0.6;
                break;
            case 19: // Rage Red
                s.kickExtraClip = true; s.kickBase = 50; s.kickDecay = 0.42; s.kickDrive = 4.4f; s.kickSweep = 200; s.kickSweepRate = 40; s.clickAmt = 0.9f;
                s.snareTone = 260; s.snareBp = 4400; s.wireDecay = 0.1; s.wireGain = 1.3f; s.snareDrive = 2.6f; s.rimFreq = 2200;
                s.clapBp = 1900; s.clapTail = 0.1; s.hatRatio = 1.3; s.hatNoise = 0.3f; s.hatBp = 10000; s.hatHp = 8000; s.hatClosedDecay = 0.02;
                s.perc = Perc::block; s.subBase = 48; s.subDecay = 1.5; s.subDrive = 3.0f; s.subClip = 3.4f; s.subGlide = 0.8;
                break;
            case 20: // Hyperpop Pink
                s.kickExtraClip = true; s.kickBase = 54; s.kickDecay = 0.25; s.kickDrive = 3.6f; s.kickSweep = 220; s.kickSweepRate = 44; s.clickAmt = 1.0f;
                s.snareTone = 300; s.snareBp = 5000; s.wireDecay = 0.09; s.wireGain = 1.2f; s.snareDrive = 2.4f; s.rimFreq = 2400;
                s.clapBp = 2200; s.clapTail = 0.09; s.hatRatio = 1.35; s.hatNoise = 0.35f; s.hatBp = 11000; s.hatHp = 8500; s.hatClosedDecay = 0.018;
                s.perc = Perc::squeak; s.subBase = 52; s.subDecay = 0.6; s.subDrive = 3.2f; s.subClip = 3.6f;
                break;
            case 21: // Garage Swing
                s.kickBase = 54; s.kickDecay = 0.26; s.kickDrive = 2.4f; s.kickSweep = 170; s.kickSweepRate = 38; s.clickAmt = 0.8f;
                s.snareTone = 220; s.snareBp = 3800; s.wireDecay = 0.11; s.wireGain = 1.1f; s.snareDrive = 2.0f; s.rimFreq = 1800;
                s.clapBp = 1500; s.clapTail = 0.11; s.hatRatio = 1.1; s.hatNoise = 0.5f; s.hatBp = 9500; s.hatHp = 7500; s.hatClosedDecay = 0.025; s.hatOpenDecay = 0.24;
                s.perc = Perc::shaker; s.subBase = 50; s.subDecay = 0.5; s.subDrive = 1.6f;
                break;
            case 22: // Grime Concrete
                s.kickExtraClip = true; s.kickBase = 52; s.kickDecay = 0.3; s.kickDrive = 3.2f; s.kickSweep = 120; s.kickSweepRate = 30; s.clickAmt = 0.6f;
                s.snareTone = 240; s.snareBp = 4200; s.wireDecay = 0.08; s.wireGain = 1.0f; s.snareDrive = 2.4f; s.rimFreq = 2000;
                s.clapBp = 1700; s.clapTail = 0.07; s.hatRatio = 1.25; s.hatNoise = 0.25f; s.hatBp = 9000; s.hatHp = 7400; s.hatClosedDecay = 0.02;
                s.perc = Perc::clave; s.subBase = 44; s.subDecay = 1.2; s.subDrive = 2.0f; s.subClip = 2.4f; s.subGlide = 0.5;
                break;
            case 23: // Baile Funk
                s.kickBase = 52; s.kickDecay = 0.3; s.kickDrive = 2.6f; s.kickSweep = 160; s.kickSweepRate = 36; s.clickAmt = 0.8f;
                s.snareTone = 230; s.snareBp = 3600; s.wireDecay = 0.09; s.wireGain = 1.0f; s.snareDrive = 2.0f; s.rimFreq = 1700;
                s.clapBp = 1700; s.clapTail = 0.09; s.hatRatio = 1.08; s.hatNoise = 0.4f; s.hatBp = 9000; s.hatHp = 7000;
                s.tomLow = 150; s.tomHigh = 230; s.tomDecay = 0.22; s.tomQ = 0.14; s.tomRes = 0.6f; s.perc = Perc::conga; s.subBase = 50; s.subDecay = 0.6; s.subDrive = 1.8f;
                break;
            case 24: // Bossa Brush
                s.acoustic = true; s.shellKick = true; s.kickBase = 62; s.kickDecay = 0.24; s.kickDrive = 1.2f; s.clickAmt = 0.3f;
                s.snareTone = 196; s.snareQ = 0.1; s.snareBp = 2400; s.wireDecay = 0.3; s.wireGain = 1.3f; s.snareDrive = 1.1f; s.rimFreq = 1050;
                s.clapBp = 1200; s.clapTail = 0.08; s.hatNoise = 0.55f; s.hatRatio = 0.98; s.hatClosedDecay = 0.04; s.hatOpenDecay = 0.3; s.hatBp = 7000; s.hatHp = 5500;
                s.tomLow = 110; s.tomHigh = 180; s.tomDecay = 0.3; s.tomQ = 0.2; s.tomRes = 1.2f; s.rideDecay = 1.2; s.crashDecay = 1.3; s.perc = Perc::shaker; s.room = 0.06f;
                break;
            case 25: // Gospel Live
                s.acoustic = true; s.shellKick = true; s.kickBase = 56; s.kickDecay = 0.34; s.kickDrive = 1.8f; s.kickSweep = 130; s.clickAmt = 0.7f;
                s.snareTone = 205; s.snareQ = 0.14; s.snareBp = 3000; s.wireDecay = 0.24; s.wireGain = 1.2f; s.snareDrive = 1.8f; s.rimFreq = 1150;
                s.clapBp = 1300; s.clapTail = 0.14; s.hatNoise = 0.45f; s.hatRatio = 1.02; s.hatClosedDecay = 0.045; s.hatOpenDecay = 0.4; s.hatBp = 8000; s.hatHp = 6200;
                s.tomLow = 88; s.tomHigh = 145; s.tomDecay = 0.4; s.tomQ = 0.26; s.tomRes = 1.8f; s.rideDecay = 1.5; s.crashDecay = 2.0; s.perc = Perc::shaker; s.room = 0.18f;
                break;
            case 26: // Neo-Soul Dust
                s.lofi = true; s.kickBase = 50; s.kickDecay = 0.4; s.kickDrive = 1.5f; s.kickSweep = 110; s.kickSweepRate = 26; s.clickAmt = 0.35f;
                s.snareTone = 186; s.snareQ = 0.11; s.snareBp = 2500; s.wireDecay = 0.16; s.wireGain = 0.9f; s.snareDrive = 1.5f; s.rimFreq = 1050;
                s.clapBp = 1200; s.clapTail = 0.1; s.hatNoise = 0.38f; s.hatRatio = 0.96; s.hatClosedDecay = 0.04; s.hatOpenDecay = 0.3; s.hatBp = 7200; s.hatHp = 5600;
                s.perc = Perc::shaker; s.subBase = 48; s.subDecay = 1.0; s.subDrive = 1.4f; s.subClip = 1.5f; s.room = 0.05f;
                break;
            default: break;
        }
        return s;
    }

    /** Small-room ambience (four damped combs + two allpasses) for the room kits. */
    struct SmallRoom
    {
        std::vector<float> c[4], ap[2]; int ci[4] {}, ai[2] {}; float lp[4] {};
        void prepare (double sr)
        {
            const int cl[] = { 1116, 1188, 1277, 1356 }, al[] = { 225, 341 };
            for (int k = 0; k < 4; ++k) c[k].assign ((size_t) (cl[k] * sr / 44100.0 * 0.6), 0.0f);
            for (int k = 0; k < 2; ++k) ap[k].assign ((size_t) (al[k] * sr / 44100.0), 0.0f);
        }
        float process (float x)
        {
            float out = 0;
            for (int k = 0; k < 4; ++k)
            {
                auto& b = c[k]; auto& i = ci[k];
                float y = b[(size_t) i];
                lp[k] = y * 0.6f + lp[k] * 0.4f;
                b[(size_t) i] = x + lp[k] * 0.72f;
                i = (i + 1) % (int) b.size();
                out += y;
            }
            out *= 0.25f;
            for (int k = 0; k < 2; ++k)
            {
                auto& b = ap[k]; auto& i = ai[k];
                float y = b[(size_t) i];
                b[(size_t) i] = out + y * 0.5f;
                out = y - out * 0.5f;
                i = (i + 1) % (int) b.size();
            }
            return out;
        }
    };

    inline std::vector<float> synthesise (int kit, int index, double sr)
    {
        juce::Random r (1234 + index * 31 + kit * 977);
        const Style st = styleFor (kit);
        double length = 1.6;
        if (index == 11) length = kit < 5 ? 2.6 : juce::jmax (2.6, st.subDecay * 3.2);                  // sub 808 rings out
        if (index == 8 || index == 9) length = kit < 5 ? 2.4 : juce::jmax (2.4, st.crashDecay * 2.0);   // cymbals
        if (index == 4) length = 0.4;
        auto n = (int) (sr * length);
        std::vector<float> out ((size_t) n, 0.0f);

        double ph = 0, ph2 = 0, ph3 = 0;
        Biquad lp, hp, bp, bp2, lpBody;
        Resonator res1, res2;
        Metallic metal;
        SmallRoom room;
        if (st.room > 0) room.prepare (sr);

        lp.lowpass (sr, st.lofi ? 4800.0 : 16000.0, 0.7);
        hp.highpass (sr, 30.0, 0.7);

        switch (index)
        {
            case 0: // KICK: pitch-swept sine body + click transient + saturation (+ resonant shell for acoustic kits)
                res1.set (sr, st.kickBase * 1.9, 0.08); res2.set (sr, st.kickBase * 3.1, 0.04);
                break;
            case 1: // SNARE: two-partial shell + noise through band/high-pass, snappy
                res1.set (sr, st.snareTone, st.snareQ); res2.set (sr, st.snareTone * 1.83, 0.05);
                bp.bandpass (sr, st.snareBp, 0.8);
                hp.highpass (sr, 900.0, 0.7);
                break;
            case 2: // RIM / SIDESTICK: short resonant click
                res1.set (sr, st.rimFreq, 0.012); res2.set (sr, 2600.0, 0.006);
                break;
            case 3: // CLAP: 3-4 noise bursts + tail, band-passed
                bp.bandpass (sr, st.clapBp, 0.9); hp.highpass (sr, 500.0, 0.7);
                break;
            case 4: case 5: // HATS
                metal.prepare (sr, st.hatBp, st.hatHp);
                hp.highpass (sr, 7000.0, 0.7);
                break;
            case 6: case 7: // TOMS
                res1.set (sr, index == 6 ? 110.0 : 170.0, st.tomQ);
                break;
            case 8: // RIDE: inharmonic partials + metallic noise
                hp.highpass (sr, 2500.0, 0.6); bp.bandpass (sr, 6500.0, 0.5);
                break;
            case 9: // CRASH: dense metallic noise burst with long decay
                hp.highpass (sr, 1800.0, 0.6); bp.bandpass (sr, 5000.0, 0.4);
                break;
            case 10: // PERC
                bp.bandpass (sr, st.perc == Perc::squeak ? 2400.0 : 6000.0, 0.8);
                if (st.perc == Perc::conga) res1.set (sr, 330.0, 0.14);
                if (st.perc == Perc::logDrum) { res1.set (sr, 220.0, 0.09); lpBody.lowpass (sr, 1800.0, 0.7); }
                break;
            case 11: // SUB 808: long pitched sine with glide and saturation
                if (st.perc == Perc::logDrum) lpBody.lowpass (sr, 1400.0, 0.7);
                break;
            default: break;
        }

        for (int i = 0; i < n; ++i)
        {
            double t = i / sr;
            float s = 0;
            switch (index)
            {
                case 0:
                {
                    double f = st.kickBase + st.kickSweep * std::exp (-t * st.kickSweepRate);
                    float body = sine (ph, f, sr) * env (t, st.kickDecay);
                    if (st.shellKick) body = body * 0.7f + (res1.process (body) + res2.process (body)) * 0.5f;
                    float click = (t < 0.006 ? (float) (1.0 - t / 0.006) : 0.0f) * (noise (r) * 0.6f + sine (ph2, 1800.0, sr) * 0.5f) * st.clickAmt;
                    s = softclip (body * 1.1f + click, st.kickDrive);
                    if (st.kickExtraClip) s = softclip (s, 2.2f) * 0.95f;
                    break;
                }
                case 1:
                {
                    float exc = (t < 0.003 ? 1.0f : 0.0f) * noise (r);
                    float shell = (res1.process (exc) * 1.2f + res2.process (exc) * 0.6f) * env (t, 0.18);
                    float wires = hp.process (bp.process (noise (r))) * env (t, st.wireDecay) * st.wireGain;
                    s = softclip (shell * 2.4f + wires * 0.9f + sine (ph, st.snareTone, sr) * env (t, 0.06) * 0.35f, st.snareDrive) * 0.9f;
                    break;
                }
                case 2:
                {
                    float exc = (t < 0.001 ? 1.0f : 0.0f) * noise (r);
                    s = softclip ((res1.process (exc) * 3.0f + res2.process (exc) * 1.2f) * env (t, 0.05) + noise (r) * env (t, 0.004) * 0.4f, 1.8f) * 0.9f;
                    break;
                }
                case 3:
                {
                    float e = 0;
                    for (double d : { 0.0, 0.010, 0.021, 0.031 }) if (t >= d) e += env (t - d, 0.009) * 0.7f;
                    if (t > 0.035) e += env (t - 0.035, st.clapTail) * 0.9f;
                    s = softclip (hp.process (bp.process (noise (r))) * e * 2.0f, 1.5f) * 0.85f;
                    break;
                }
                case 4:
                case 5:
                {
                    bool open = index == 5;
                    float m = metal.process (sr, st.hatRatio);
                    float nz = hp.process (noise (r)) * st.hatNoise;
                    s = (m * 1.4f + nz) * env (t, open ? st.hatOpenDecay : st.hatClosedDecay);
                    if (st.acoustic) s = s * 0.6f + hp.process (noise (r)) * env (t, open ? 0.3 : 0.05) * 0.5f;
                    s = softclip (s, 1.3f) * 0.6f;
                    break;
                }
                case 6:
                case 7:
                {
                    double base = index == 6 ? st.tomLow : st.tomHigh;
                    double f = base + base * 0.8 * std::exp (-t * 16.0);
                    float tone = sine (ph, f, sr) * env (t, st.tomDecay);
                    float exc = (t < 0.002 ? 1.0f : 0.0f) * noise (r);
                    s = softclip (tone * 1.1f + res1.process (exc) * st.tomRes + noise (r) * 0.08f * env (t, 0.02), 1.4f) * 0.85f;
                    break;
                }
                case 8:
                {
                    const double fs[] = { 423.0, 619.0, 811.0, 1223.0, 1567.0, 2311.0, 3123.0 };
                    float part = 0;
                    for (int k = 0; k < 7; ++k) part += (float) std::sin (t * fs[k] * (1.0 + 0.0007 * k) * juce::MathConstants<double>::twoPi) * (0.14f - 0.012f * k);
                    float wash = bp.process (noise (r)) * 0.6f;
                    float ping = (float) std::sin (t * 5200.0 * juce::MathConstants<double>::twoPi) * env (t, 0.05) * 0.3f;
                    s = hp.process (part * env (t, st.rideDecay) + wash * env (t, 1.4) + ping) * 0.55f;
                    break;
                }
                case 9:
                {
                    float m = metal.process (sr, 1.37);
                    s = hp.process (bp.process (noise (r)) * 1.3f + m * 0.6f) * env (t, st.crashDecay) * 0.6f;
                    s = softclip (s, 1.2f);
                    break;
                }
                case 10:
                {
                    switch (st.perc)
                    {
                        case Perc::cowbell:   s = softclip ((square (ph, 540.0, sr) + square (ph2, 800.0, sr)) * 0.5f, 1.3f) * env (t, 0.22) * 0.7f; break;
                        case Perc::phonkBell: s = softclip ((square (ph, 587.3, sr) + square (ph2, 880.0, sr)) * 0.5f, 3.0f) * env (t, 0.34) * 0.65f; break;
                        case Perc::block:     s = (sine (ph, 1200.0 + 600.0 * std::exp (-t * 60.0), sr) * env (t, 0.09) + noise (r) * env (t, 0.003) * 0.3f) * 0.8f; break;
                        case Perc::shaker:    s = bp.process (noise (r)) * (env (t, 0.045) + 0.4f * env (t - 0.06, 0.03)) * 0.9f; break;
                        case Perc::clave:     s = (sine (ph, 2500.0, sr) * env (t, 0.03) + sine (ph2, 5000.0, sr) * env (t, 0.012) * 0.3f) * 0.9f; break;
                        case Perc::conga:
                        {
                            double f = 330.0 + 120.0 * std::exp (-t * 40.0);
                            float exc = (t < 0.0015 ? 1.0f : 0.0f) * noise (r);
                            s = softclip (sine (ph, f, sr) * env (t, 0.16) + res1.process (exc) * 0.8f + noise (r) * env (t, 0.006) * 0.25f, 1.3f) * 0.85f;
                            break;
                        }
                        case Perc::logDrum:
                        {
                            // the amapiano log drum: a bouncing pitched body with a woody knock and a little drive
                            double f = 110.0 * (1.0 + 0.6 * std::exp (-t * 18.0));
                            float body = sine (ph, f, sr) + 0.35f * sine (ph2, f * 2.01, sr) * env (t, 0.12);
                            float knock = res1.process ((t < 0.002 ? 1.0f : 0.0f) * noise (r)) * 1.4f;
                            s = softclip (lpBody.process (body * env (t, 0.32) + knock), 2.2f) * 0.9f;
                            break;
                        }
                        case Perc::squeak:
                        {
                            // the jersey "bed squeak": a quick upward chirp with a squeaky band
                            double f = 1100.0 + 900.0 * juce::jmin (1.0, t / 0.09);
                            s = (sine (ph, f, sr) * 0.7f + bp.process (noise (r)) * 0.4f) * (t < 0.11 ? (float) std::sin (juce::jmin (1.0, t / 0.11) * juce::MathConstants<double>::pi) : 0.0f) * 0.85f;
                            break;
                        }
                    }
                    break;
                }
                case 11:
                {
                    double f = st.subBase * (1.0 + st.subGlide * std::exp (-t * 14.0));
                    float tone = sine (ph, f, sr);
                    if (st.perc == Perc::logDrum)
                    {
                        tone = tone + 0.3f * sine (ph2, f * 2.0, sr) * env (t, 0.1);
                        s = softclip (lpBody.process (tone) * st.subDrive, st.subClip) * env (t, st.subDecay) * 0.95f;
                    }
                    else
                        s = softclip (tone * st.subDrive, st.subClip) * env (t, st.subDecay) * 0.95f;
                    s += sine (ph3, f * 2.0, sr) * env (t, 0.25) * 0.12f;
                    break;
                }
                default: break;
            }
            if (st.lofi)
            {
                s = lp.process (s);
                if (index != 2 && index != 10) s += noise (r) * 0.004f * env (t, 0.5) * (index == 0 || index == 11 ? 1.0f : 0.6f);   // vinyl dust under the long sounds
                s = softclip (s, 1.6f);
                s = std::round (s * 2048.0f) / 2048.0f;                 // 12-bit grit
            }
            if (st.room > 0 && index != 11) s += room.process (s) * st.room;
            out[(size_t) i] = juce::jlimit (-1.0f, 1.0f, s);
        }

        // DC block + peak-normalise to -1 dBFS + fade the tail to silence
        float dc = 0; for (auto v : out) dc += v; dc /= (float) n;
        float peak = 0.0001f;
        for (auto& v : out) { v -= dc; peak = juce::jmax (peak, std::abs (v)); }
        float g = juce::Decibels::decibelsToGain (-1.0f) / peak;
        for (auto& v : out) v *= g;
        for (int i = juce::jmax (0, n - 3000); i < n; ++i) out[(size_t) i] *= (float) (n - i) / 3000.0f;
        return out;
    }

    //==========================================================================
    inline juce::File getKitDir (Session& session, int kit)
    {
        return session.sessionsDir.getChildFile ("Kits").getChildFile (kits[juce::jlimit (0, numKits - 1, kit)].name);
    }

    inline juce::Array<juce::File> ensureKit (Session& session, int kit)
    {
        auto dir = getKitDir (session, kit);
        dir.createDirectory();
        juce::Array<juce::File> files;
        const double sr = 44100.0;

        for (int i = 0; i < numSounds; ++i)
        {
            auto f = dir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " " + sounds[i].name + ".wav");
            if (! f.existsAsFile() || f.getSize() < 1000)
            {
                auto data = synthesise (kit, i, sr);
                juce::WavAudioFormat wav;
                f.deleteFile();
                if (auto stream = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
                    if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), sr, 1, 24, {}, 0)))
                    {
                        const float* chans[] = { data.data() };
                        writer->writeFromFloatArrays (chans, 1, (int) data.size());
                    }
            }
            files.add (f);
        }
        return files;
    }

    inline juce::Array<juce::File> ensureKit (Session& session)   { return ensureKit (session, 0); }

    /** Pattern 0 repeats for the whole clip (and keeps repeating when the clip is resized). */
    inline void fillWithPattern (te::StepClip& clip)
    {
        clip.repeatSequence = true;
        clip.removeAllVariations();
        clip.insertVariation (0, -1);
    }

    /** Loads a kit into a track's Sampler (replacing its sounds) and renames the step clip channels. */
    inline bool loadKit (Session& session, te::AudioTrack& t, int kit)
    {
        if (! Edition::isKitAllowed (kit)) { session.lastError = juce::String (kits[juce::jlimit (0, numKits - 1, kit)].name) + " is locked. " + Edition::kitLockReason (kit) + " - " + Edition::storeUrl (kit >= 17 ? "BEAT" : "CORE"); return false; }
        te::SamplerPlugin* sampler = nullptr;
        for (auto p : t.pluginList.getPlugins()) if ((sampler = dynamic_cast<te::SamplerPlugin*> (p)) != nullptr) break;
        if (sampler == nullptr) return false;
        auto files = ensureKit (session, kit);
        while (sampler->getNumSounds() > 0) sampler->removeSound (0);
        for (int i = 0; i < numSounds; ++i)
        {
            sampler->addSound (files[i].getFullPathName(), sounds[i].name, 0.0, 0.0, gainsDb[i]);
            sampler->setSoundParams (sampler->getNumSounds() - 1, sounds[i].note, sounds[i].note, sounds[i].note);
            sampler->setSoundOpenEnded (sampler->getNumSounds() - 1, true);
        }
        t.state.setProperty ("sqmKit", kits[kit].name, &t.edit.getUndoManager());
        for (auto c : t.getClips())
            if (auto sc = dynamic_cast<te::StepClip*> (c))
            {
                auto chans = sc->getChannels();
                for (int i = 0; i < juce::jmin (numSounds, chans.size()); ++i) { chans[i]->noteNumber = sounds[i].note; chans[i]->name = sounds[i].name; }
            }
        return true;
    }

    /** Makes a drum track: Sampler with the kit + a step clip. Returns the track. */
    inline te::AudioTrack* createDrumTrack (Session& session, const juce::String& name, int bars, int kit = 0)
    {
        auto& edit = *session.edit;
        auto t = session.addTrack ("instrument", name.isNotEmpty() ? name : "Drums", te::SamplerPlugin::xmlTypeName);
        if (t == nullptr) return nullptr;
        t->setColour (juce::Colour (0xffffb23a));
        t->state.setProperty ("sqmKind", "instrument", nullptr);
        t->state.setProperty ("sqmFormat", "stereo", nullptr);

        te::SamplerPlugin* sampler = nullptr;
        for (auto p : t->pluginList.getPlugins()) if (auto sp = dynamic_cast<te::SamplerPlugin*> (p)) { sampler = sp; break; }
        if (sampler == nullptr)
        {
            sampler = dynamic_cast<te::SamplerPlugin*> (edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}).get());
            if (sampler == nullptr) return t;
            t->pluginList.insertPlugin (*sampler, 0, nullptr);
        }

        auto start = session.barToTime (1);
        auto end = session.barToTime (1 + juce::jmax (1, bars));
        if (auto clip = dynamic_cast<te::StepClip*> (t->insertNewClip (te::TrackItem::Type::step, "Beat", { start, end }, nullptr)))
        {
            // make sure the clip has 12 channels
            while (clip->getChannels().size() < numSounds) clip->insertNewChannel (clip->getChannels().size());
            auto p = clip->getPattern (0);
            p.setNumNotes (16);
            p.setNoteLength (te::BeatDuration::fromBeats (0.25));
            fillWithPattern (*clip);
        }
        loadKit (session, *t, juce::jlimit (0, numKits - 1, kit));
        session.assignMidiInput (*t);
        return t;
    }

    inline te::StepClip* findStepClip (te::AudioTrack& t)
    {
        for (auto c : t.getClips())
            if (auto s = dynamic_cast<te::StepClip*> (c))
                return s;
        return nullptr;
    }

    inline int channelIndexFor (te::StepClip& clip, const juce::String& nameOrIndex)
    {
        auto chans = clip.getChannels();
        if (nameOrIndex.containsOnly ("0123456789") && nameOrIndex.isNotEmpty())
            return juce::jlimit (0, chans.size() - 1, nameOrIndex.getIntValue() - 1);
        for (int i = 0; i < chans.size(); ++i)
            if (chans[i]->name.get().equalsIgnoreCase (nameOrIndex)) return i;
        for (int i = 0; i < chans.size(); ++i)
            if (chans[i]->name.get().containsIgnoreCase (nameOrIndex) || nameOrIndex.containsIgnoreCase (chans[i]->name.get().upToFirstOccurrenceOf (" ", false, false))) return i;
        static const char* aliases[] = { "kick,bd,bass drum,kik", "snare,sd,sn", "rim,rimshot,sidestick,side stick,stick", "clap,cp,hand clap",
                                         "closed,chh,hh,hat,hihat,hi-hat,closed hihat", "open,ohh,open hihat", "low tom,lt,tom1,tom", "high tom,ht,tom2,hi tom",
                                         "ride,rd,cymbal", "crash,cr,cymbal2", "perc,cowbell,shaker,block,percussion", "sub,808,bass,sub 808,808 bass" };
        for (int i = 0; i < numSounds && i < chans.size(); ++i)
            for (auto a : juce::StringArray::fromTokens (aliases[i], ",", ""))
                if (nameOrIndex.equalsIgnoreCase (a)) return i;
        return -1;
    }
}

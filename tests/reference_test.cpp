// Reference Listener DSP: measured on beats with a known answer. Build: scripts/build_reference_test.sh
#include "SqReferenceDsp.h"
#include <cstdio>
#include <random>
#include <cstdlib>

static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }

struct Song { std::vector<float> L, R; double sr = 44100; };
static double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

/** A 32-bar beat: intro (pad + hats), hook (everything), verse (drums + 808 + quieter pad), hook again.
    i - VI - III - VII in A minor (Am F C G), kick x.....x...x....., snare on 2 and 4, 8th hats with a 1/32 roll every 4th bar,
    an 808 on the chord root with a glide in hook bar 3, a lead in the hooks. swing = position of the off-16th in its 8th. */
static Song makeSong (double bpm, double swing, bool rolls, bool hats16 = false, bool pump = false)
{
    Song s; const double sr = s.sr; const double beat = 60.0 / bpm, bar = 4 * beat, step = beat / 4;
    const int bars = 32; const int n = (int) (sr * (bars * bar + 1.0));
    s.L.assign ((size_t) n, 0.0f); s.R.assign ((size_t) n, 0.0f);
    std::mt19937 rng (7); std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    auto add = [&] (double t, const std::vector<float>& v, float panL = 1, float panR = 1) { const int a = (int) (t * sr); for (size_t i = 0; i < v.size() && a + (int) i < n; ++i) { s.L[(size_t) a + i] += v[i] * panL; s.R[(size_t) a + i] += v[i] * panR; } };
    auto stepTime = [&] (int b, int st) { double t = b * bar + st * step; if (st % 2 == 1) t = b * bar + (st - 1) * step + swing * 2 * step; return t; };
    std::vector<float> kick ((size_t) (sr * 0.35)), snare ((size_t) (sr * 0.18)), hat ((size_t) (sr * 0.03));
    { double ph = 0; for (size_t i = 0; i < kick.size(); ++i) { const double t = i / sr; const double f = 50 + 110 * std::exp (-t * 30); ph += 2 * M_PI * f / sr; kick[i] = (float) (0.9 * std::sin (ph) * std::exp (-t * 9)); } }
    { juce::IIRFilter bp; bp.setCoefficients (juce::IIRCoefficients::makeBandPass (sr, 2500, 0.8)); for (size_t i = 0; i < snare.size(); ++i) { const double t = i / sr; snare[i] = (float) (uni (rng) * 0.7 * std::exp (-t * 22) + 0.35 * std::sin (2 * M_PI * 200 * t) * std::exp (-t * 30)); } bp.processSamples (snare.data(), (int) snare.size()); for (auto& v : snare) v *= 2.2f; }
    { juce::IIRFilter hp; hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 8000)); for (size_t i = 0; i < hat.size(); ++i) hat[i] = (float) (uni (rng) * 0.45 * std::exp (-(double) i / sr * 120)); hp.processSamples (hat.data(), (int) hat.size()); }
    const int roots[4] = { 57, 53, 48, 55 };             // A3 F3 C3 G3
    const int thirds[4] = { 3, 4, 4, 4 };                // Am F C G
    const int lead[8] = { 76, 74, 72, 74, 76, 79, 76, 72 };
    for (int b = 0; b < bars; ++b)
    {
        const int sec = b / 8; const bool hook = sec == 1 || sec == 3, verse = sec == 2, intro = sec == 0;
        const int ci = b % 4;
        // pad: root, third, fifth, octave (a few harmonics), quieter in the verse
        const float padAmp = intro ? 0.05f : verse ? 0.035f : 0.06f;
        std::vector<float> pad ((size_t) (sr * bar));
        for (int tone : { 0, thirds[ci], 7, 12 })
            for (int h = 1; h <= 4; ++h) { const double f = mtof (roots[ci] + tone) * h; for (size_t i = 0; i < pad.size(); ++i) pad[i] += (float) (padAmp / h * std::sin (2 * M_PI * f * i / sr)); }
        for (size_t i = 0; i < pad.size(); ++i) { const double env = juce::jmin (1.0, i / (sr * 0.03)) * juce::jmin (1.0, (pad.size() - i) / (sr * 0.03)); pad[i] *= (float) env; }
        if (pump) for (size_t i = 0; i < pad.size(); ++i) { const double tb = std::fmod (i / sr, beat); pad[i] *= (float) (1.0 - 0.75 * std::exp (-tb / 0.09)); }
        add (b * bar, pad, 0.8f, 1.0f);
        // hats everywhere (8ths, swung 16ths in rolls)
        for (int st = 0; st < 16; st += hats16 ? 1 : 2) add (stepTime (b, st), hat, 1.0f, 0.7f);
        if (rolls && b % 4 == 3 && ! intro) for (int k = 0; k < 8; ++k) add (b * bar + 3 * beat + k * beat / 8, hat, 1.0f, 0.7f);
        if (intro) continue;
        for (int st : { 0, 6, 10 }) add (stepTime (b, st), kick);
        for (int st : { 4, 12 }) add (stepTime (b, st), snare);
        // 808: root an octave below the pad's octave (A1 etc.), held for the bar; glide up 5 st in hook bar 3
        std::vector<float> sub ((size_t) (sr * bar * 0.95)); double ph = 0;
        const bool glide = hook && b % 8 == 2;
        for (size_t i = 0; i < sub.size(); ++i)
        {
            const double t = i / sr; double m = roots[ci] - 24;
            if (glide && t > bar * 0.5) m += juce::jmin (5.0, (t - bar * 0.5) / 0.08 * 5.0);
            ph += 2 * M_PI * mtof (m) / sr; sub[i] = (float) (0.55 * std::tanh (1.5 * std::sin (ph)) * std::exp (-t * 0.6) * juce::jmin (1.0, t / 0.005));
        }
        add (b * bar, sub);
        if (hook)
            for (int k = 0; k < 2; ++k)
            {
                std::vector<float> nt ((size_t) (sr * beat * 1.8)); const double f = mtof (lead[(b % 4) * 2 + k]);
                for (size_t i = 0; i < nt.size(); ++i) { const double t = i / sr; nt[i] = (float) (0.08 * (std::sin (2 * M_PI * f * t) + 0.3 * std::sin (4 * M_PI * f * t)) * std::exp (-t * 1.5)); }
                add (b * bar + k * 2 * beat, nt, 1.0f, 0.6f);
            }
    }
    return s;
}

/** The self-test's beat (SelfTest.h 1.15 section): 90 BPM, D minor, broadband-noise hats on 8ths, 808 re-struck each bar. */
static Song makeBeat90()
{
    Song s; const double sr = 44100, bpm = 90, beat = 60.0 / bpm, bar = 4 * beat; const int bars = 24; s.sr = sr;
    s.L.assign ((size_t) (sr * (bars * bar + 1.0)), 0.0f); s.R.assign (s.L.size(), 0.0f);
    const double tp = 2 * M_PI; juce::Random rnd (7);
    auto hit = [&] (double t0, int kind)
    {
        const size_t a = (size_t) (t0 * sr); const size_t n = (size_t) (sr * (kind == 0 ? 0.35 : kind == 1 ? 0.2 : 0.05)); double ph = 0;
        for (size_t i = 0; i < n && a + i < s.L.size(); ++i)
        {
            const double t = i / sr; float v = 0;
            if (kind == 0) { ph += tp * (50 + 120 * std::exp (-t * 40)) / sr; v = (float) (0.8 * std::sin (ph) * std::exp (-t * 9)); }
            else if (kind == 1) v = (float) ((rnd.nextFloat() * 2 - 1) * 0.45 * std::exp (-t * 22) + 0.3 * std::sin (tp * 190 * t) * std::exp (-t * 30));
            else v = (rnd.nextFloat() * 2 - 1) * 0.18f * (float) std::exp (-t * 90);
            s.L[a + i] += v; s.R[a + i] += v;
        }
    };
    const int roots[4] = { 62, 58, 53, 60 }; const int thirds[4] = { 3, 4, 4, 4 };
    for (int b = 0; b < bars; ++b)
    {
        const int ci = b % 4; const bool intro = b < 8; const double t0 = b * bar;
        const size_t a = (size_t) (t0 * sr), n = (size_t) (bar * sr);
        for (int tone : { 0, thirds[ci], 7 })
            for (size_t i = 0; i < n && a + i < s.L.size(); ++i) { const double t = i / sr; const float v = (float) (0.045 * std::sin (tp * mtof (roots[ci] + tone) * t) * juce::jmin (1.0, t * 30)); s.L[a + i] += v; s.R[a + i] += v * 0.9f; }
        for (int st = 0; st < 16; st += 2) hit (t0 + st * bar / 16, 2);
        if (intro) continue;
        for (int st : { 0, 6, 8 }) hit (t0 + st * bar / 16, 0);
        for (int st : { 4, 12 }) hit (t0 + st * bar / 16, 1);
        double ph = 0;
        for (size_t i = 0; i < (size_t) (n * 0.9) && a + i < s.L.size(); ++i) { const double t = i / sr; ph += tp * mtof (roots[ci] - 24) / sr; const float v = (float) (0.45 * std::tanh (1.4 * std::sin (ph)) * std::exp (-t * 0.7) * juce::jmin (1.0, t * 300)); s.L[a + i] += v; s.R[a + i] += v; }
    }
    for (auto& v : s.L) v *= 0.7f; for (auto& v : s.R) v *= 0.7f;
    return s;
}

int main()
{
    std::printf ("== Reference Listener ==\n");
    {
        auto s = makeSong (140.0, 0.5, true);
        auto r = SqRef::analyse (s.L.data(), s.R.data(), (int) s.L.size(), s.sr);
        check (r.ok, "analyses a 32-bar 140 BPM beat (" + juce::String (r.seconds, 1) + " s)");
        check (std::abs (r.bpm - 140.0) < 0.5, "tempo 140 BPM within 0.5 (" + juce::String (r.bpm, 2) + ")");
        const double bar = 240.0 / 140.0;
        double off = 1e9; for (auto t : r.bars) off = std::min (off, std::abs (std::fmod (t + 100 * bar, bar)) < bar / 2 ? std::fmod (t + 100 * bar, bar) : bar - std::fmod (t + 100 * bar, bar));
        check (off < 0.04, "downbeats on the bar lines (" + juce::String (off * 1000, 1) + " ms off)");
        check (r.keyName == "A minor", "key A minor (" + r.keyName + ")");
        // chords in the hook bars 8-15: expected roots A F C G
        int good = 0, tot = 0; const int exp[4] = { 9, 5, 0, 7 };
        for (auto& c : r.chords)
        {
            const int barIdx = (int) std::floor (c.startSec / bar + 0.05);
            if (barIdx < 8 || barIdx >= 16) continue;
            ++tot; if (c.root == exp[barIdx % 4]) ++good;
            if (std::getenv ("REFDBG")) std::printf ("     bar %d (%.2f s): %s\n", barIdx, c.startSec, c.name.toRawUTF8());
        }
        check (tot > 0 && good >= tot * 3 / 4, "chord roots A F C G in the hook (" + juce::String (good) + "/" + juce::String (tot) + ")");
        juce::StringArray rom; for (auto& c : r.chords) if (rom.size() < 4) rom.add (c.roman);
        std::printf ("     chords: %s\n", rom.joinIntoString (" ").toRawUTF8());
        // drum grid in the hook: kick steps 0,6,10; snare 4,12
        int kOk = 0, kTot = 0, sOk = 0, sTot = 0;
        const int firstBar = (int) std::lround (r.bars.front() / bar);
        for (int b = 8; b < 16; ++b)
        {
            const int idx = b - firstBar; if (idx < 0 || idx >= (int) r.drums.size()) continue;
            auto& d = r.drums[(size_t) idx];
            for (int st = 0; st < 16; ++st)
            {
                const bool kExp = st == 0 || st == 6 || st == 10, sExp = st == 4 || st == 12;
                kTot++; if ((d.kick[(size_t) st] > 0) == kExp) kOk++;
                sTot++; if ((d.snare[(size_t) st] > 0) == sExp) sOk++;
            }
        }
        check (kOk >= kTot * 9 / 10, "kick grid x.....x...x..... (" + juce::String (kOk) + "/" + juce::String (kTot) + " steps right; e.g. " + SqRef::typicalRow (r.drums, 8 - firstBar, 16 - firstBar, 0) + ")");
        check (sOk >= sTot * 9 / 10, "snare on 2 and 4 (" + juce::String (sOk) + "/" + juce::String (sTot) + "; e.g. " + SqRef::typicalRow (r.drums, 8 - firstBar, 16 - firstBar, 1) + ")");
        int rolls = 0; for (auto& d : r.drums) rolls += d.hatRolls;
        check (rolls >= 4, "1/32 hat rolls found (" + juce::String (rolls) + " beats with rolls; 6 written)");
        juce::String secs; for (auto& sc : r.sections) secs << sc.letter << sc.bars << " ";
        std::printf ("     sections: %s\n", secs.toRawUTF8());
        bool bOk = r.sections.size() >= 4;
        if (bOk) for (int want : { 8, 16, 24 }) { bool f = false; for (auto& sc : r.sections) if (std::abs (sc.startBar + firstBar - want) <= 1) f = true; bOk = bOk && f; }
        check (bOk, "section boundaries at bars 8, 16 and 24");
        char hookA = 0, hookB = 0; for (auto& sc : r.sections) { if (std::abs (sc.startBar + firstBar - 8) <= 1) hookA = sc.letter; if (std::abs (sc.startBar + firstBar - 24) <= 1) hookB = sc.letter; }
        check (hookA != 0 && hookA == hookB, "the second hook is recognised as a repeat of the first (letter " + juce::String::charToString (hookA) + ")");
        juce::String names; for (auto& sc : r.sections) names << sc.name << " ";
        check (r.sections.size() >= 2 && r.sections.front().name == "Intro" && names.contains ("Hook"), "named: " + names.trim());
        int introEl = 0, hookEl = 0; for (auto& sc : r.sections) { if (sc.name == "Intro") introEl = sc.elements; if (sc.name == "Hook") hookEl = juce::jmax (hookEl, sc.elements); }
        check (hookEl > introEl, "more elements in the hook (" + juce::String (hookEl) + ") than the intro (" + juce::String (introEl) + ")");
        check (r.subGlides >= 1, "808 glide found (" + juce::String (r.subGlides) + ")");
        float nearRoot = 0; int cnt = 0; for (auto& sn : r.sub) { ++cnt; const int pc = ((int) std::lround (sn.midi) % 12 + 12) % 12; if (pc == 9 || pc == 5 || pc == 0 || pc == 7) nearRoot += 1; }
        check (cnt > 0 && nearRoot >= 0.8f * cnt, "808 notes on the chord roots (" + juce::String ((int) nearRoot) + "/" + juce::String (cnt) + ")");
        check (r.bandDb[0] > r.bandDb[4], "mix profile: sub band above air (" + juce::String (r.bandDb[0], 1) + " vs " + juce::String (r.bandDb[4], 1) + " dB)");
        check (r.widthBands[0] < 0.05f && r.widthBands[2] > r.widthBands[0], "width: low end mono, top wider");
    }
    {
        auto s = makeSong (92.0, 0.62, false, true);
        auto r = SqRef::analyse (s.L.data(), s.R.data(), (int) s.L.size(), s.sr);
        check (std::abs (r.bpm - 92.0) < 0.5, "tempo 92 BPM within 0.5 (" + juce::String (r.bpm, 2) + ")");
        check (std::abs (r.swing - 0.62) < 0.05, "swing 62 % measured (" + juce::String (r.swing * 100, 1) + " %)");
        int falseRolls = 0; for (auto& d : r.drums) falseRolls += d.hatRolls;
        check (falseRolls == 0, "swung straight 16th hats are not read as rolls (" + juce::String (falseRolls) + ")");
    }
    {
        auto s = makeSong (124.0, 0.5, false);
        // 48 kHz version
        juce::AudioBuffer<float> b (2, (int) s.L.size());
        auto r = SqRef::analyse (s.L.data(), s.R.data(), (int) s.L.size(), s.sr);
        check (std::abs (r.bpm - 124.0) < 0.5, "tempo 124 BPM within 0.5 (" + juce::String (r.bpm, 2) + ")");
        check (r.pumpDb < 2.0f, "no sidechain: pump under 2 dB (" + juce::String (r.pumpDb, 1) + " dB)");
    }
    {
        auto s = makeSong (124.0, 0.5, false, false, true);
        auto r = SqRef::analyse (s.L.data(), s.R.data(), (int) s.L.size(), s.sr);
        check (r.pumpDb > 4.0f, "pads ducked on every beat: pump over 4 dB (" + juce::String (r.pumpDb, 1) + " dB)");
    }
    {
        auto s = makeBeat90();
        auto r = SqRef::analyse (s.L.data(), s.R.data(), (int) s.L.size(), s.sr);
        const auto sn = SqRef::typicalRow (r.drums, 8, 24, 1), k = SqRef::typicalRow (r.drums, 8, 24, 0), h = SqRef::typicalRow (r.drums, 8, 24, 2);
        std::printf ("     beat90: %.2f BPM %s; kick %s snare %s hat %s; glides %d pump %.1f\n", r.bpm, r.keyName.toRawUTF8(), k.toRawUTF8(), sn.toRawUTF8(), h.toRawUTF8(), r.subGlides, r.pumpDb);
        check (sn == "....x.......x...", "broadband noise hats on 8ths stay out of the snare row (" + sn + ")");
        check (h == "x.x.x.x.x.x.x.x.", "and are read as 8th hats (" + h + ")");
        check (r.subGlides == 0, "808 re-struck on each chord: no glides (" + juce::String (r.subGlides) + ")");
        check (r.pumpDb < 2.0f, "no sidechain in beat90 (" + juce::String (r.pumpDb, 1) + " dB)");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

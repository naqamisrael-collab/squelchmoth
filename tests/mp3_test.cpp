// Measures Squelchmoth's own MP3 encoder (Source/SqMp3Encoder.h) against JUCE's MP3 decoder.
// Build + run: scripts/build_mp3_test.sh
#include "SqMp3Encoder.h"
#include <cstdio>

static juce::AudioBuffer<float> decode (const juce::File& f, double& rate)
{
    juce::MP3AudioFormat fmt;
    juce::AudioBuffer<float> b;
    if (auto r = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (f.createInputStream().release(), true)))
    {
        rate = r->sampleRate; b.setSize ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    }
    return b;
}

struct Result { double gainDb, corr, snrDb, kbps; int delay, mismatches, frames; };

static Result run (const juce::AudioBuffer<float>& src, double sr, SqMp3::Settings st, const char* name)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (juce::String (name) + ".mp3");
    f.deleteFile();
    Result res {};
    {
        auto os = f.createOutputStream();
        SqMp3::Encoder enc (sr, src.getNumChannels(), st);
        SqMp3::Tags tags; tags.title = "Test"; tags.artist = "Jamar Stewart"; tags.isrc = "USQLM2600001";
        enc.begin (*os, tags);
        for (int p = 0; p < src.getNumSamples(); p += 1000)
        {
            const int n = juce::jmin (1000, src.getNumSamples() - p);
            const float* ch[2] = { src.getReadPointer (0, p), src.getReadPointer (juce::jmin (1, src.getNumChannels() - 1), p) };
            enc.write (*os, ch, n);
        }
        enc.finish (*os);
        res.mismatches = enc.bitMismatches; res.frames = enc.framesOut(); res.kbps = enc.averageKbps();
    }
    double rate = 0; auto d = decode (f, rate);
    if (d.getNumSamples() == 0) { std::printf ("%s: decode failed\n", name); return res; }
    // delay by cross-correlation over the first 0..3000 lags
    const int N = juce::jmin (src.getNumSamples(), d.getNumSamples()) - 4000;
    int bestL = 0; double bestC = -1e30;
    for (int L = 0; L < 3000; ++L)
    {
        double c = 0; for (int i = 4000; i < juce::jmin (N, 4000 + 20000); i += 3) c += (double) src.getSample (0, i) * d.getSample (0, i + L);
        if (c > bestC) { bestC = c; bestL = L; }
    }
    double xy = 0, xx = 0, yy = 0, e = 0;
    for (int i = 4000; i < N - bestL; ++i)
        for (int c = 0; c < src.getNumChannels(); ++c)
        {
            const double x = src.getSample (c, i), y = d.getSample (juce::jmin (c, d.getNumChannels() - 1), i + bestL);
            xy += x * y; xx += x * x; yy += y * y; e += (x - y) * (x - y);
        }
    res.delay = bestL; res.corr = xy / std::sqrt (xx * yy + 1e-30); res.gainDb = 10.0 * std::log10 (yy / (xx + 1e-30)); res.snrDb = 10.0 * std::log10 (xx / (e + 1e-30));
    std::printf ("%-16s frames %4d  kbps %6.1f  delay %4d  gain %+6.2f dB  corr %.5f  snr %6.1f dB  mismatches %d  size %lld\n", name, res.frames, res.kbps, res.delay, res.gainDb, res.corr, res.snrDb, res.mismatches, (long long) f.getSize());
    return res;
}

int main()
{
    const double sr = 44100.0; const int len = (int) (sr * 6.0);
    juce::AudioBuffer<float> sine (2, len), music (2, len);
    juce::Random rnd (7);
    for (int i = 0; i < len; ++i)
    {
        const double t = i / sr;
        const float s = (float) (0.4 * std::sin (2 * juce::MathConstants<double>::pi * 441.0 * t));
        sine.setSample (0, i, s); sine.setSample (1, i, s);
        // music-like: chord + decaying noise bursts (drums) + a stereo difference
        const double env = std::exp (-std::fmod (t, 0.5) * 12.0);
        double m = 0.12 * std::sin (2 * juce::MathConstants<double>::pi * 220.0 * t) + 0.09 * std::sin (2 * juce::MathConstants<double>::pi * 277.2 * t) + 0.07 * std::sin (2 * juce::MathConstants<double>::pi * 329.6 * t)
                 + 0.05 * std::sin (2 * juce::MathConstants<double>::pi * 1760.0 * t) + env * 0.25 * (rnd.nextFloat() * 2 - 1);
        music.setSample (0, i, (float) (m + 0.03 * std::sin (2 * juce::MathConstants<double>::pi * 3000.0 * t)));
        music.setSample (1, i, (float) (m - 0.03 * std::sin (2 * juce::MathConstants<double>::pi * 3000.0 * t)));
    }
    SqMp3::Settings s320; s320.bitrate = 320;
    SqMp3::Settings s128; s128.bitrate = 128;
    SqMp3::Settings v0; v0.vbr = true; v0.vbrQuality = 0;
    SqMp3::Settings v4; v4.vbr = true; v4.vbrQuality = 4;
    run (sine, sr, s320, "sine320");
    run (music, sr, s320, "music320");
    run (music, sr, s128, "music128");
    run (music, sr, v0, "musicV0");
    run (music, sr, v4, "musicV4");
    juce::AudioBuffer<float> chord (2, len);
    for (int i = 0; i < len; ++i) { const double tt = i / sr; const float v = (float) (0.15 * std::sin (2 * juce::MathConstants<double>::pi * 220.0 * tt) + 0.1 * std::sin (2 * juce::MathConstants<double>::pi * 330.0 * tt) + 0.05 * std::sin (2 * juce::MathConstants<double>::pi * 1320.0 * tt)); chord.setSample (0, i, v); chord.setSample (1, i, v); }
    run (chord, sr, v0, "chordV0"); run (chord, sr, v4, "chordV4"); run (chord, sr, s128, "chord128");
    return 0;
}

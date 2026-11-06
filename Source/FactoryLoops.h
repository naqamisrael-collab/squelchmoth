#pragma once
// Renders the Mandible factory loops: two bars of a Beat Lab genre groove played by one of the 27 kits, mixed straight
// from the kit WAVs (no engine round trip), with the genre's swing, accents, ghosts, rolls and the open-hat / 808 chokes.
#include "BeatLab.h"
#include "SqFactory.h"

namespace FactoryLoops
{
    inline juce::File render (const juce::String& genreName, int kit, int variant)
    {
        const auto& gs = BeatLab::genres(); int gi = -1;
        for (int i = 0; i < (int) gs.size(); ++i) if (genreName.equalsIgnoreCase (gs[(size_t) i].name)) gi = i;
        if (gi < 0) gi = BeatLab::genreIndex (genreName);
        if (gi < 0 || kit < 0 || kit >= DrumKit::numKits) return {};
        const auto& g = gs[(size_t) gi];
        const int bpm = (int) std::lround ((g.bpmLo + g.bpmHi) * 0.5);
        auto out = SqFactory::root().getChildFile ("Loops").getChildFile (juce::File::createLegalFileName (juce::String (g.name) + " - " + DrumKit::kits[kit].name + " - " + juce::String (variant + 1) + " - " + juce::String (bpm) + " BPM.wav"));
        if (out.existsAsFile() && out.getSize() > 1000) return out;
        out.getParentDirectory().createDirectory();

        auto files = SqFactory::kitFiles (kit); if (files.size() < DrumKit::numSounds) return {};
        std::array<juce::AudioBuffer<float>, DrumKit::numSounds> snd; double sr = 44100;
        for (int i = 0; i < DrumKit::numSounds; ++i) { double r = 44100; snd[(size_t) i] = SqFactory::readFile (files[i], r); sr = r; }
        const double stepLen = 60.0 / bpm / 4.0 * sr; const int steps = 32; const int total = (int) std::lround (stepLen * steps);
        juce::AudioBuffer<float> mix (2, total); mix.clear();
        juce::Random rnd (4321 + gi * 97 + kit * 13 + variant * 7);

        // per channel: a list of (start sample, gain); chokes cut the previous hit of the same voice group
        struct Hit { int at; float gain; int ch; };
        std::vector<Hit> hits;
        for (auto& row : g.rows)
        {
            if (row.options.empty()) continue;
            for (int bar = 0; bar < 2; ++bar)
            {
                // bar 1 plays the variant's option, bar 2 moves the busy rows (hats, percussion) on by one for movement
                const bool busy = row.channel == BeatLab::hat || row.channel == BeatLab::perc || row.channel == BeatLab::rim;
                const auto* pat = row.options[(size_t) ((variant + (bar == 1 && busy ? 1 : 0)) % (int) row.options.size())];
                const int len = (int) std::strlen (pat);
                for (int s = 0; s < 16 && s < len; ++s)
                {
                    const char c = pat[s]; if (c == '.') continue;
                    const int step = bar * 16 + s;
                    double t = step * stepLen; if (s % 2 == 1) t += g.swing * stepLen * 0.5;
                    float vel = c == 'X' ? 1.0f : c == 'g' ? 0.35f : 0.8f;
                    if (c == '?') { if (rnd.nextFloat() < 0.5f) continue; vel = 0.7f; }
                    int rolls = (c >= '2' && c <= '8') ? c - '0' : 1;
                    for (int k = 0; k < rolls; ++k) hits.push_back ({ (int) std::lround (t + k * stepLen / rolls), rolls > 1 ? 0.55f + 0.1f * (float) k / (float) rolls : vel, row.channel });
                }
            }
        }
        std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.at < b.at; });
        auto chokeGroup = [] (int ch) { return ch == BeatLab::hat || ch == BeatLab::openHat ? 1 : ch == BeatLab::sub ? 2 : 0; };
        for (size_t h = 0; h < hits.size(); ++h)
        {
            const auto& hit = hits[h]; const auto& b = snd[(size_t) hit.ch]; if (b.getNumSamples() == 0) continue;
            int length = b.getNumSamples();
            if (const int grp = chokeGroup (hit.ch); grp != 0)
                for (size_t n = h + 1; n < hits.size(); ++n)
                    if (chokeGroup (hits[n].ch) == grp && (hit.ch == BeatLab::openHat || hit.ch == BeatLab::sub)) { length = juce::jmin (length, hits[n].at - hit.at + (int) (0.006 * sr)); break; }
            const float gain = hit.gain * juce::Decibels::decibelsToGain (DrumKit::gainsDb[hit.ch]);
            const float pan = hit.ch == BeatLab::hat || hit.ch == BeatLab::openHat ? 0.15f : hit.ch == BeatLab::perc ? -0.2f : hit.ch == BeatLab::highTom ? 0.25f : hit.ch == BeatLab::lowTom ? -0.25f : 0.0f;
            const float gl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * 1.4142f, gr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * 1.4142f;
            const int fade = (int) (0.006 * sr);
            auto* src = b.getReadPointer (0);
            for (int i = 0; i < length; ++i)
            {
                float x = src[i] * gain;
                if (length < b.getNumSamples() && i > length - fade) x *= (float) (length - i) / (float) fade;
                const int at = (hit.at + i) % total;   // tails wrap into the start so the loop is seamless
                mix.addSample (0, at, x * gl); mix.addSample (1, at, x * gr);
            }
        }
        const float peak = mix.getMagnitude (0, total);
        if (peak > 0.0f) mix.applyGain (juce::Decibels::decibelsToGain (-3.0f) / peak);   // -3 dBFS: the same headroom as the instrument presets
        juce::WavAudioFormat wav; out.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), sr, 2, 24, {}, 0)))
                writer->writeFromAudioSampleBuffer (mix, 0, total);
        return out.existsAsFile() ? out : juce::File();
    }

    /** Points the factory hooks at the app's kits and this renderer. */
    inline void install()
    {
        SqFactory::loopHook() = [] (const juce::String& g, int k, int v) { return render (g, k, v); };
    }
}

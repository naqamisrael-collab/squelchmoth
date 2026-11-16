// Included inside namespace Skins (PluginSkins.h): faces for the 1.11 Voice family, first wave.

    //==============================================================================
    /** Moth Key: the key big, confidence, the alternatives, the chroma wheel of what was heard, the key changes over
        time and the tempo. */
    struct MothKeyViz : public Viz, private juce::Timer
    {
        MothKeyViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mk (dynamic_cast<SqMothKeyPlugin*> (&p)) { startTimerHz (8); }
        SqMothKeyPlugin* mk;
        void timerCallback() override { repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (mk == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto left = r.removeFromLeft (300.0f); r.removeFromLeft (8.0f);
            auto wheel = r.removeFromLeft (juce::jmin (r.getHeight(), 230.0f)); r.removeFromLeft (8.0f);
            ColourBits::panel (g, left, acc, true);
            auto q = left.reduced (14.0f, 10.0f);
            const bool have = mk->hasResult(); const auto res = mk->lastResult();
            ColourBits::caption (g, q.removeFromTop (14.0f), mk->isAnalysing() ? "ANALYSING..." : "KEY");
            ColourBits::big (g, q.removeFromTop (46.0f), have ? MothKeyAnalysis::keyText (res.root, res.minor) : juce::String ("-"), juce::Colours::white, 34.0f);
            if (have)
            {
                // confidence bar
                auto cb = q.removeFromTop (12.0f); g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cb, 3.0f);
                g.setColour (acc); g.fillRoundedRectangle (cb.withWidth (cb.getWidth() * juce::jlimit (0.02f, 1.0f, res.confidence)), 3.0f);
                ColourBits::line (g, q.removeFromTop (16.0f), "confidence " + juce::String (juce::roundToInt (res.confidence * 100)) + " %");
                q.removeFromTop (4.0f);
                ColourBits::big (g, q.removeFromTop (28.0f), res.bpm > 0 && res.bpmConfidence >= 0.15f ? juce::String (res.bpm, 1) + " BPM" : juce::String ("tempo unclear"), acc.brighter (0.4f), 20.0f);
                q.removeFromTop (4.0f);
                ColourBits::caption (g, q.removeFromTop (14.0f), "ALTERNATIVES");
                for (size_t i = 1; i < res.alternatives.size(); ++i) ColourBits::line (g, q.removeFromTop (15.0f), MothKeyAnalysis::keyText (res.alternatives[i].root, res.alternatives[i].minor) + "   " + juce::String (res.alternatives[i].score, 2));
            }
            else ColourBits::line (g, q.removeFromTop (16.0f), "play the song through Moth Key, then Analyse");
            ColourBits::line (g, left.reduced (14.0f, 8.0f).removeFromBottom (16.0f), "heard " + juce::String (mk->heardSeconds(), 0) + " s   " + mk->status());
            // chroma wheel: the 12 pitch classes heard, the key's scale lit
            ColourBits::panel (g, wheel, acc);
            {
                auto wc = wheel.reduced (18.0f); const float cx = wc.getCentreX(), cy = wc.getCentreY(), rad = juce::jmin (wc.getWidth(), wc.getHeight()) * 0.5f;
                const int mask = have ? SqScales::maskFor (res.root, res.minor ? 2 : 1) : 0;
                for (int k = 0; k < 12; ++k)
                {
                    const int pc = (k * 7) % 12;   // circle of fifths order
                    const float a0 = juce::MathConstants<float>::twoPi * (float) k / 12.0f - juce::MathConstants<float>::halfPi;
                    const float v = have ? res.chroma[(size_t) pc] : 0.0f;
                    juce::Path seg; seg.addPieSegment (cx - rad, cy - rad, rad * 2, rad * 2, a0 + 0.04f + juce::MathConstants<float>::halfPi, a0 + juce::MathConstants<float>::twoPi / 12.0f - 0.04f + juce::MathConstants<float>::halfPi, 0.42f + 0.0f * v);
                    const bool inKey = (mask >> pc) & 1;
                    g.setColour ((inKey ? acc : juce::Colours::white).withAlpha (0.10f + 0.75f * v)); g.fillPath (seg);
                    const float am = a0 + juce::MathConstants<float>::pi / 12.0f;
                    g.setColour (pc == res.root && have ? juce::Colours::white : juce::Colours::white.withAlpha (inKey ? 0.85f : 0.4f)); g.setFont (Theme::font (10.0f, pc == res.root));
                    g.drawText (Voice::noteName (pc), juce::Rectangle<float> (cx + std::cos (am) * rad * 0.71f - 14, cy + std::sin (am) * rad * 0.71f - 7, 28, 14), juce::Justification::centred);
                }
                ColourBits::caption (g, wheel.reduced (10.0f, 6.0f).removeFromTop (12.0f), "HEARD");
            }
            // key changes over time
            ColourBits::panel (g, r, acc);
            auto tl = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, tl.removeFromTop (14.0f), "KEY CHANGES OVER TIME   (written to the session key track)");
            if (have && ! res.segments.empty())
            {
                const double t0 = res.segments.front().start, t1 = juce::jmax (t0 + 1.0, res.segments.back().end);
                auto bar = tl.removeFromTop (34.0f);
                for (size_t i = 0; i < res.segments.size(); ++i)
                {
                    const auto& s = res.segments[i];
                    const float x0 = bar.getX() + bar.getWidth() * (float) ((s.start - t0) / (t1 - t0)), x1 = bar.getX() + bar.getWidth() * (float) ((s.end - t0) / (t1 - t0));
                    const auto c = juce::Colour::fromHSV ((float) ((s.root * 7) % 12) / 12.0f, 0.65f, s.minor ? 0.7f : 0.95f, 1.0f);
                    g.setColour (c.withAlpha (0.8f)); g.fillRoundedRectangle (juce::Rectangle<float> (x0 + 1, bar.getY(), juce::jmax (2.0f, x1 - x0 - 2), bar.getHeight()), 4.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.85f)); g.setFont (Theme::font (10.0f, true));
                    g.drawText (MothKeyAnalysis::keyText (s.root, s.minor), juce::Rectangle<float> (x0 + 4, bar.getY(), juce::jmax (10.0f, x1 - x0 - 8), bar.getHeight()), juce::Justification::centredLeft, true);
                }
                tl.removeFromTop (6.0f);
                const double off = juce::jmax (0.0, mk->lastOffset());
                for (auto& s : res.segments) ColourBits::line (g, tl.removeFromTop (15.0f), juce::String (s.start + off, 1) + " s   " + MothKeyAnalysis::keyText (s.root, s.minor));
            }
            else ColourBits::line (g, tl.removeFromTop (16.0f), "no key changes found yet");
        }
    };

    //==============================================================================
    /** Siren Comp: GR per stage (and Punch) as bars with history, the output meter, and the Assist suggestion. */
    struct SirenCompViz : public Viz, private juce::Timer
    {
        SirenCompViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sc (dynamic_cast<SqSirenCompPlugin*> (&p)) { startTimerHz (30); }
        SqSirenCompPlugin* sc; static constexpr int N = 180; float h1[N] {}, h2[N] {}, hp[N] {}; int w = 0;
        void timerCallback() override { if (sc == nullptr) return; h1[w] = sc->gr1.load(); h2[w] = sc->gr2.load(); hp[w] = sc->punchGr.load(); w = (w + 1) % N; repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (sc == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (250.0f); r.removeFromRight (8.0f);
            auto meters = r.removeFromRight (150.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "GAIN REDUCTION PER STAGE   (last 6 s, 0 to -24 dB)");
            auto trace = [&] (const float* hv, juce::Colour c, bool fill)
            {
                juce::Path pth;
                for (int i = 0; i < N; ++i) { const float v = hv[(w + i) % N]; const float x = plot.getX() + plot.getWidth() * (float) i / (N - 1), y = plot.getY() + juce::jlimit (0.0f, 1.0f, v / 24.0f) * plot.getHeight(); if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y); }
                if (fill) { juce::Path f (pth); f.lineTo (plot.getRight(), plot.getY()); f.lineTo (plot.getX(), plot.getY()); f.closeSubPath(); g.setColour (c.withAlpha (0.16f)); g.fillPath (f); }
                glowStroke (g, pth, c, 1.8f);
            };
            for (int db : { 6, 12, 18 }) { const float y = plot.getY() + db / 24.0f * plot.getHeight(); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight()); g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (Theme::font (8.5f)); g.drawText ("-" + juce::String (db), (int) plot.getX() + 2, (int) y - 11, 30, 10, juce::Justification::left); }
            trace (h1, acc, true); if (sc->dual.get()) trace (h2, juce::Colour (0xff3ad1ff), false); if (sc->punch.get()) trace (hp, juce::Colour (0xffffd23a), false);
            // vertical meters: stage 1, stage 2, punch, output
            ColourBits::panel (g, meters, acc, true);
            auto mr = meters.reduced (10.0f); ColourBits::caption (g, mr.removeFromTop (14.0f), "METERS");
            auto lbls = mr.removeFromBottom (12.0f);
            const float vals[4] { sc->gr1.load(), sc->dual.get() ? sc->gr2.load() : 0.0f, sc->punch.get() ? sc->punchGr.load() : 0.0f, juce::jlimit (0.0f, 60.0f, -sc->outDb.load()) };
            const char* nm[4] { "GR1", "GR2", "PCH", "OUT" };
            const float bw = mr.getWidth() / 4.0f;
            for (int i = 0; i < 4; ++i)
            {
                auto b = juce::Rectangle<float> (mr.getX() + bw * i + 4, mr.getY(), bw - 8, mr.getHeight());
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 3.0f);
                const float frac = i < 3 ? juce::jlimit (0.0f, 1.0f, vals[i] / 24.0f) : 1.0f - juce::jlimit (0.0f, 1.0f, vals[i] / 60.0f);
                const auto col = i == 3 ? (vals[i] < 1.0f ? juce::Colour (0xffff3a5a) : juce::Colour (0xff3affb0)) : i == 0 ? acc : i == 1 ? juce::Colour (0xff3ad1ff) : juce::Colour (0xffffd23a);
                if (i < 3) { g.setColour (col.withAlpha (0.85f)); g.fillRoundedRectangle (b.withHeight (b.getHeight() * frac), 3.0f); }
                else { g.setColour (col.withAlpha (0.85f)); g.fillRoundedRectangle (b.withTrimmedTop (b.getHeight() * (1.0f - frac)), 3.0f); }
                g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (8.5f, true)); g.drawText (nm[i], juce::Rectangle<float> (b.getX() - 4, lbls.getY(), b.getWidth() + 8, 12), juce::Justification::centred);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), sc->dual.get() ? "DUAL (SERIES)" : "SINGLE");
            ColourBits::big (g, q.removeFromTop (32.0f), juce::String (-(sc->gr1.load() + sc->gr2.load()), 1) + " dB", juce::Colours::white, 24.0f);
            const auto p = sc->knobParams();
            auto stage = [&] (const Voice::Comp::StageParams& s) { return juce::String (Voice::Comp::modelName (s.model)) + (s.model == Voice::Comp::fet ? juce::String (" ") + Voice::Comp::fetRatioName (s.fetIdx) : juce::String()) + ", thr " + juce::String (s.thresholdDb, 1); };
            ColourBits::line (g, q.removeFromTop (15.0f), "1: " + stage (p.s1));
            if (p.dual) ColourBits::line (g, q.removeFromTop (15.0f), "2: " + stage (p.s2));
            if (p.warm > 0.01f) ColourBits::line (g, q.removeFromTop (15.0f), "warm " + juce::String (juce::roundToInt (p.warm * 100)) + " %");
            if (p.punch) ColourBits::line (g, q.removeFromTop (15.0f), "punch +" + juce::String (p.punchGainDb, 1) + " dB, ceiling " + juce::String (p.punchCeilingDb, 1));
            if (p.keyOn) ColourBits::line (g, q.removeFromTop (15.0f), "pitch duck at " + (sc->keyHz.load() > 0 ? ColourBits::hz (sc->keyHz.load()) : juce::String ("-")) + " harmonics");
            q.removeFromTop (4.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), sc->learning.load() ? "ASSIST: LISTENING..." : sc->preview.get() ? "ASSIST: PREVIEWING SUGGESTION" : sc->hasSuggestion() ? "ASSIST: SUGGESTION READY" : "ASSIST", acc.brighter (0.5f));
            if (sc->hasSuggestion())
            {
                const auto s = sc->suggestionCopy();
                ColourBits::line (g, q.removeFromTop (15.0f), SqSirenCompPlugin::amountNames()[juce::jlimit (0, 2, sc->amount.get())].toLowerCase() + ": " + stage (s.s1));
                if (s.dual) ColourBits::line (g, q.removeFromTop (15.0f), "   then " + stage (s.s2));
                ColourBits::line (g, q.removeFromTop (15.0f), "safe make-up " + juce::String (s.makeupDb, 1) + " dB");
            }
        }
    };

    //==============================================================================
    /** Siren EQ: spectrum of the output, the EQ curve with every band's handle, the singer's pitch and its harmonics
        drawn on the spectrum, pitch-tracking bands following them, dynamic bands showing their live gain. */
    struct SirenEqViz : public Viz, private juce::Timer
    {
        SirenEqViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), eq (dynamic_cast<SqSirenEqPlugin*> (&p)) { startTimerHz (24); sm.fill (-100.0f); }
        SqSirenEqPlugin* eq; std::array<float, 512> sm {}; std::vector<std::complex<float>> buf; std::vector<float> pw; Voice::Eq::Params pp;
        void timerCallback() override
        {
            if (eq == nullptr) return;
            pp = eq->knobParams();
            const int n = 2048; buf.resize ((size_t) n);
            const auto w = eq->specW.load();
            for (int i = 0; i < n; ++i) buf[(size_t) i] = eq->spec[(size_t) ((w - n + i) & (SqSirenEqPlugin::specLen - 1))] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1)));
            Voice::fftPower (buf, pw);
            const double sr = eq->rate();
            for (int k = 0; k < 512; ++k)
            {
                const double f = 20.0 * std::pow (1000.0, k / 511.0); const int bin = juce::jlimit (1, n / 2 - 1, (int) (f * n / sr));
                const float db = 10.0f * std::log10 (pw[(size_t) bin] / (float) (n * n) * 4.0f + 1e-14f) + 6.0f;
                sm[(size_t) k] += (db > sm[(size_t) k] ? 0.5f : 0.15f) * (db - sm[(size_t) k]);
            }
            repaint();
        }
        float responseDb (float hz) const
        {
            const double sr = eq->rate(); double db = 0;
            const auto& p = pp; const auto& bands = p.preview ? p.suggested : p.band;
            for (int b = 0; b < 6; ++b)
            {
                const auto& bd = bands[(size_t) b]; if (! bd.on || bd.mute) continue;
                Eng::Biquad q; const float f = p.preview ? bd.freq : eq->bandHz[(size_t) b].load() > 0 ? eq->bandHz[(size_t) b].load() : bd.freq;
                const auto t = bd.type == 1 ? Eng::Biquad::lowShelf : bd.type == 2 ? Eng::Biquad::highShelf : bd.type == 3 ? Eng::Biquad::notch : Eng::Biquad::peak;
                q.set (t, sr, f, bd.q, bd.gainDb + (bd.mode == Voice::Eq::dynamic && ! p.preview ? eq->bandDyn[(size_t) b].load() : 0.0f)); db += q.magnitudeDb (hz, sr);
            }
            const float hp = (p.preview && p.suggestedHp > p.hpHz) ? p.suggestedHp : p.hpHz;
            if (hp > 20.5f) { const int order = p.hpSlope + 1; db += -10.0 * std::log10 (1.0 + std::pow (hp / hz, 2.0 * order)); }
            if (p.lpHz < 19900.0f) { const int order = p.lpSlope + 1; db += -10.0 * std::log10 (1.0 + std::pow (hz / p.lpHz, 2.0 * order)); }
            if (std::abs (p.airDb) > 0.01f) { Eng::Biquad a; a.set (Eng::Biquad::highShelf, sr, juce::jmin ((double) p.airHz, sr * 0.45), 0.5, p.airDb); db += a.magnitudeDb (hz, sr); }
            if (std::abs (p.tiltDb) > 0.01f) { Eng::Biquad l, hh; l.set (Eng::Biquad::lowShelf, sr, 1000, 0.5, -0.5 * p.tiltDb); hh.set (Eng::Biquad::highShelf, sr, 1000, 0.5, 0.5 * p.tiltDb); db += l.magnitudeDb (hz, sr) + hh.magnitudeDb (hz, sr); }
            return (float) db + p.outputDb;
        }
        void paint (juce::Graphics& g) override
        {
            if (eq == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 10.0f);
            ColourBits::freqGrid (g, plot, 18.0f);
            // spectrum (output), -90..0 dBFS
            {
                juce::Path sp; for (int k = 0; k < 512; ++k) { const float x = plot.getX() + plot.getWidth() * k / 511.0f, y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (sm[(size_t) k] + 90.0f) / 90.0f) * plot.getHeight(); if (k == 0) sp.startNewSubPath (x, y); else sp.lineTo (x, y); }
                juce::Path f (sp); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath();
                g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillPath (f); g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (sp, juce::PathStrokeType (1.0f));
            }
            // the singer's pitch and its harmonics
            const float f0 = eq->pitchHz.load();
            if (f0 > 50)
                for (int hN = 1; hN <= 12; ++hN)
                {
                    const float hz = f0 * hN; if (hz > 20000) break;
                    const float x = Viz::xForHz (hz, plot);
                    g.setColour (juce::Colour (0xff3ad1ff).withAlpha (hN == 1 ? 0.75f : 0.28f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
                    if (hN == 1) { g.setFont (Theme::font (9.0f, true)); g.drawText ("f0 " + ColourBits::hz (f0), (int) x + 3, (int) plot.getY() + 2, 70, 11, juce::Justification::left); }
                }
            // EQ curve
            auto curve = ColourBits::curve (plot, 18.0f, [this] (float hz) { return responseDb (hz); });
            glowStroke (g, curve, pp.preview ? juce::Colour (0xffffd23a) : acc, 2.2f);
            // band handles
            const auto& bands = pp.band;
            for (int b = 0; b < 6; ++b)
            {
                if (! bands[(size_t) b].on) continue;
                const float hz = eq->bandHz[(size_t) b].load() > 0 ? eq->bandHz[(size_t) b].load() : bands[(size_t) b].freq;
                const float x = Viz::xForHz (hz, plot), y = plot.getCentreY() - juce::jlimit (-18.0f, 18.0f, responseDb (hz)) / 18.0f * plot.getHeight() * 0.5f;
                const auto col = bands[(size_t) b].mode == Voice::Eq::pitchTrack ? juce::Colour (0xff3ad1ff) : bands[(size_t) b].mode == Voice::Eq::dynamic ? juce::Colour (0xffffd23a) : acc;
                g.setColour (col); g.fillEllipse (x - 6, y - 6, 12, 12); g.setColour (juce::Colours::black); g.setFont (Theme::font (8.5f, true)); g.drawText (juce::String (b + 1), juce::Rectangle<float> (x - 6, y - 6, 12, 12), juce::Justification::centred);
                if (bands[(size_t) b].mode == Voice::Eq::dynamic && std::abs (eq->bandDyn[(size_t) b].load()) > 0.1f)
                { g.setColour (col.withAlpha (0.8f)); g.drawLine (x, y, x, y - eq->bandDyn[(size_t) b].load() / 18.0f * plot.getHeight() * 0.5f, 2.0f); }
            }
            if (pp.preview) { g.setColour (juce::Colour (0xffffd23a)); g.setFont (Theme::font (10.0f, true)); g.drawText ("PREVIEWING VOCAL LEARNING", plot.removeFromTop (14.0f), juce::Justification::centredRight); }
            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "SINGER");
            ColourBits::big (g, q.removeFromTop (30.0f), f0 > 50 ? ColourBits::hz (f0) : juce::String ("-"), juce::Colours::white, 22.0f);
            int nOn = 0; for (int b = 0; b < 6; ++b) if (bands[(size_t) b].on) ++nOn;
            ColourBits::line (g, q.removeFromTop (15.0f), juce::String (nOn) + " band" + (nOn == 1 ? "" : "s") + " on");
            for (int b = 0; b < 6; ++b)
                if (bands[(size_t) b].on)
                {
                    const auto& bd = bands[(size_t) b];
                    juce::String t; t << b + 1 << ": " << (bd.mode == Voice::Eq::pitchTrack ? "H" + juce::String (bd.harmonic) + " " : juce::String()) << ColourBits::hz (eq->bandHz[(size_t) b].load() > 0 ? eq->bandHz[(size_t) b].load() : bd.freq) << " " << juce::String (bd.gainDb, 1) << " dB";
                    if (bd.mode == Voice::Eq::dynamic) t << " (" << juce::String (eq->bandDyn[(size_t) b].load(), 1) << ")";
                    ColourBits::line (g, q.removeFromTop (14.0f), t);
                }
            q.removeFromTop (4.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), eq->learning.load() ? "VOCAL LEARNING: LISTENING..." : eq->hasSuggestion() ? "VOCAL LEARNING: SUGGESTION" : "VOCAL LEARNING", acc.brighter (0.5f));
            if (eq->hasSuggestion())
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (9.5f));
                g.drawFittedText (eq->suggestionText.get(), q.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 5);
            }
        }
    };

    //==============================================================================
    /** Hush De-Ess 2: the detection history (hard bursts and soft fricatives in different colours, with the reduction),
        the reduction meter and what is being detected now. */
    struct HushViz : public Viz, private juce::Timer
    {
        HushViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), hu (dynamic_cast<SqHushDeEss2Plugin*> (&p)) { startTimerHz (30); }
        SqHushDeEss2Plugin* hu; Voice::DeEss::Event hist[Voice::DeEss::Processor::historyLen] {};
        void timerCallback() override { if (hu != nullptr) hu->proc.historyCopy (hist); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (hu == nullptr) return;
            const auto acc = neon (accent);
            const juce::Colour hardC (0xffff5a3a), softC (0xff3ad1ff);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "DETECTIONS   (last 10 s)   HARD = T K Ch    SOFT = S Sh Z");
            const int N = Voice::DeEss::Processor::historyLen;
            const float bw = plot.getWidth() / (float) N;
            for (int i = 0; i < N; ++i)
            {
                const auto& e = hist[i]; if (e.kind == Voice::DeEss::none) continue;
                const float x = plot.getX() + bw * i, hgt = juce::jlimit (0.04f, 1.0f, e.grDb / 18.0f) * plot.getHeight();
                g.setColour ((e.kind == Voice::DeEss::hard ? hardC : softC).withAlpha (0.85f)); g.fillRect (x, plot.getBottom() - hgt, juce::jmax (1.0f, bw), hgt);
            }
            for (int db : { 6, 12 }) { const float y = plot.getBottom() - db / 18.0f * plot.getHeight(); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight()); g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (Theme::font (8.5f)); g.drawText ("-" + juce::String (db) + " dB", (int) plot.getX() + 2, (int) y - 11, 40, 10, juce::Justification::left); }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            const int k = hu->kindNow.load();
            ColourBits::caption (g, q.removeFromTop (14.0f), "NOW");
            ColourBits::big (g, q.removeFromTop (30.0f), k == Voice::DeEss::hard ? "HARD" : k == Voice::DeEss::soft ? "SOFT" : "-", k == Voice::DeEss::hard ? hardC : k == Voice::DeEss::soft ? softC : juce::Colours::white.withAlpha (0.4f), 24.0f);
            ColourBits::big (g, q.removeFromTop (26.0f), juce::String (-hu->grDb.load(), 1) + " dB", juce::Colours::white, 18.0f);
            const auto p = hu->knobParams();
            ColourBits::line (g, q.removeFromTop (15.0f), "split " + ColourBits::hz (p.splitHz) + " (highs only)");
            ColourBits::line (g, q.removeFromTop (15.0f), "hard " + juce::String (p.hardThresholdDb, 1) + " dB / " + juce::String (p.hardRangeDb, 0) + " dB max");
            ColourBits::line (g, q.removeFromTop (15.0f), "soft " + juce::String (p.softThresholdDb, 1) + " dB / " + juce::String (p.softRangeDb, 0) + " dB max");
            ColourBits::line (g, q.removeFromTop (15.0f), juce::String (hu->proc.countHard) + " hard, " + juce::String (hu->proc.countSoft) + " soft so far");
            if (p.solo) ColourBits::line (g, q.removeFromTop (15.0f), "SOLO: hearing what is removed");
            if (p.link) ColourBits::line (g, q.removeFromTop (15.0f), "LINK: one threshold moves both");
            q.removeFromTop (4.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), hu->learning.load() ? "ASSIST: LISTENING..." : hu->preview.get() ? "ASSIST: PREVIEWING" : hu->hasSuggestion() ? "ASSIST: READY" : "ASSIST", acc.brighter (0.5f));
            if (hu->hasSuggestion()) ColourBits::line (g, q.removeFromTop (15.0f), "hard " + juce::String (hu->suggestedHard(), 1) + ", soft " + juce::String (hu->suggestedSoft(), 1) + " dB");
        }
    };

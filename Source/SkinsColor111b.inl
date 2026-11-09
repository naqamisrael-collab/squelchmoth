// Included inside namespace Skins (PluginSkins.h): faces for the 1.11 Color & Space second wave, part 1
// (Vellum EQ, Tremor, Orbit Pan, Mirror Shift / Lite, Shutter Delay / Lite).

    //==============================================================================
    /** Vellum EQ: the live output spectrum under the EQ curve, the three fixed steps marked, and what each band is doing. */
    struct VellumViz : public Viz, private juce::Timer
    {
        VellumViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ve (dynamic_cast<SqVellumPlugin*> (&p))
        { curveProc.prepare (48000, 64); startTimerHz (24); sm.fill (-100.0f); }
        SqVellumPlugin* ve; Vellum::Processor curveProc; std::array<float, 512> sm {}; std::vector<std::complex<float>> buf; std::vector<float> pw; Vellum::Params kp;
        void timerCallback() override
        {
            if (ve == nullptr) return;
            kp = ve->knobParams(); kp.drive = 0; kp.oversampleLog2 = 0;
            curveProc.p = kp; { std::array<float, 16> a {}, b {}; float* ch[2] = { a.data(), b.data() }; curveProc.process (ch, 2, 16); }
            const int n = 2048; buf.resize ((size_t) n);
            const int w = ve->proc.ringPos.load();
            for (int i = 0; i < n; ++i) buf[(size_t) i] = ve->proc.ring[(size_t) ((w + i) & 2047)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1)));
            Voice::fftPower (buf, pw);
            const double sr = juce::jmax (8000.0, ve->proc.rate());
            for (int k = 0; k < 512; ++k)
            {
                const double f = 20.0 * std::pow (1000.0, k / 511.0); const int bin = juce::jlimit (1, n / 2 - 1, (int) (f * n / sr));
                const float db = 10.0f * std::log10 (pw[(size_t) bin] / (float) (n * n) * 4.0f + 1e-14f) + 6.0f;
                sm[(size_t) k] += (db > sm[(size_t) k] ? 0.5f : 0.15f) * (db - sm[(size_t) k]);
            }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (ve == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "BROADCAST CURVE  +  OUTPUT SPECTRUM");
            ColourBits::freqGrid (g, plot, 18.0f);
            {
                juce::Path sp; for (int k = 0; k < 512; ++k) { const float x = plot.getX() + plot.getWidth() * k / 511.0f, y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (sm[(size_t) k] + 90.0f) / 90.0f) * plot.getHeight(); if (k == 0) sp.startNewSubPath (x, y); else sp.lineTo (x, y); }
                juce::Path f (sp); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath();
                g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillPath (f); g.setColour (juce::Colours::white.withAlpha (0.22f)); g.strokePath (sp, juce::PathStrokeType (1.0f));
            }
            // the steps on the front panel
            auto mark = [&] (float hz, bool active, const juce::String& t)
            {
                const float x = Viz::xForHz (hz, plot);
                g.setColour (acc.withAlpha (active ? 0.75f : 0.18f)); g.fillEllipse (x - (active ? 3.5f : 2.0f), plot.getBottom() - 18.0f, active ? 7.0f : 4.0f, active ? 7.0f : 4.0f);
                if (active) { g.setFont (Theme::font (8.5f, true)); g.drawText (t, (int) x - 20, (int) plot.getBottom() - 30, 40, 11, juce::Justification::centred); }
            };
            for (int i = 0; i < 6; ++i) { mark (Vellum::lowFreqs()[i], i == kp.lowIdx, "LOW"); mark (Vellum::midFreqs()[i], i == kp.midIdx, "MID"); mark (Vellum::highFreqs()[i], i == kp.highIdx, "HIGH"); }
            auto pth = ColourBits::curve (plot, 18.0f, [this] (float hz) { return (float) curveProc.magnitudeDb (hz) + kp.outputDb; });
            ColourBits::fillUnder (g, pth, plot, acc); glowStroke (g, pth, acc, 2.2f);
            if (kp.mid2On) { const float x = Viz::xForHz (kp.mid2Hz, plot); g.setColour (juce::Colour (0xffffe08a).withAlpha (0.6f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom()); }

            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "SECTIONS");
            auto row = [&] (const juce::String& t) { ColourBits::line (g, q.removeFromTop (17.0f), t); };
            row ("LOW  " + ColourBits::hz (Vellum::lowFreqs()[kp.lowIdx]) + "  " + juce::String (curveProc.lowDbUsed(), 1) + " dB  " + (curveProc.lowDbUsed() < 0 ? "shelf" : "bell"));
            row ("MID  " + ColourBits::hz (Vellum::midFreqs()[kp.midIdx]) + "  " + juce::String (curveProc.midDbUsed(), 1) + " dB  Q " + juce::String (Vellum::midQ (curveProc.midDbUsed()), 2));
            if (kp.mid2On) row ("MID 2  " + ColourBits::hz (kp.mid2Hz) + "  " + juce::String (curveProc.mid2DbUsed(), 1) + " dB");
            row ("HIGH  " + ColourBits::hz (Vellum::highFreqs()[kp.highIdx]) + "  " + juce::String (curveProc.highDbUsed(), 1) + " dB  " + (curveProc.highDbUsed() < 0 ? "low-pass" : "bell"));
            q.removeFromTop (6.0f);
            row (juce::String (Vellum::targetName (kp.target)) + (kp.stepped ? "  /  STEPPED" : "  /  continuous"));
            row ("DRIVE " + juce::String (juce::roundToInt (ve->bank[SqVellumPlugin::drive])) + " %  (auto gain, " + juce::String (1 << juce::jlimit (0, 3, ve->oversample.get())) + "x)");
            ColourBits::caption (g, q.removeFromBottom (14.0f), "OUT  " + juce::String (Eng::gainToDb (ve->levelOut.load()), 1) + " dBFS peak");
        }
    };

    //==============================================================================
    /** Tremor: two cycles of the gain the tremolo applies (or the 16 rhythm steps with their depth), the playhead, the
        live gain meter and the dynamics state. */
    struct TremorViz : public Viz, private juce::Timer
    {
        TremorViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), tr (dynamic_cast<SqTremorPlugin*> (&p)) { startTimerHz (30); }
        SqTremorPlugin* tr; float gainSm = 1; std::array<float, 120> hist {}; int hp = 0;
        void timerCallback() override { if (tr == nullptr) return; gainSm = tr->proc.currentGain(); hist[(size_t) hp] = gainSm; hp = (hp + 1) % (int) hist.size(); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (tr == nullptr) return;
            const auto acc = neon (accent); const auto kp = tr->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f); r.removeFromRight (8.0f);
            auto meter = r.removeFromRight (46.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), kp.mode == Tremor::rhythm ? "RHYTHM  (bar height = step depth)" : "GAIN OVER TWO CYCLES");
            plot.removeFromTop (4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * k / 4.0f), plot.getX(), plot.getRight());
            auto gainAt = [&] (float pos, float stepDepth)
            {
                long long idx = 0; const float lp = ModShapes::feel (ModShapes::swing (pos, kp.groove, idx), kp.feel);
                const float hv = (float) std::sin ((double) idx * 12.9898) ;      // a stand-in for the random shapes
                const float m = 0.5f * (1.0f + ModShapes::shape (kp.shape, lp, kp.pulseWidth, kp.custom, kp.customPoints, hv));
                return 1.0f - kp.depth * stepDepth * (1.0f - m);
            };
            if (kp.mode == Tremor::rhythm)
            {
                const int ns = juce::jlimit (1, 16, kp.numSteps); const float bw = plot.getWidth() / (float) ns;
                for (int s = 0; s < ns; ++s)
                {
                    auto cell = juce::Rectangle<float> (plot.getX() + s * bw + 2, plot.getY(), bw - 4, plot.getHeight());
                    const float d = juce::jlimit (0.0f, 1.0f, kp.steps[s]);
                    g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (cell, 3.0f);
                    auto bar = cell.withTrimmedTop (cell.getHeight() * (1.0f - d));
                    const bool now = tr->proc.currentStep() == s;
                    g.setGradientFill (juce::ColourGradient (acc.withAlpha (now ? 1.0f : 0.7f), bar.getX(), bar.getY(), acc.withAlpha (0.15f), bar.getX(), bar.getBottom(), false));
                    g.fillRoundedRectangle (bar, 3.0f);
                    if (now) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (cell, 3.0f, 1.5f); }
                }
            }
            else
            {
                juce::Path p; const int N = (int) plot.getWidth();
                for (int i = 0; i <= N; ++i) { const float pos = 2.0f * i / (float) N, gv = gainAt (pos, 1.0f); const float x = plot.getX() + i, y = plot.getBottom() - gv * plot.getHeight(); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
                juce::Path f (p); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath();
                g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.35f), plot.getX(), plot.getY(), acc.withAlpha (0.02f), plot.getX(), plot.getBottom(), false)); g.fillPath (f);
                glowStroke (g, p, acc, 2.2f);
                if (kp.harmonic)
                {
                    juce::Path q2; for (int i = 0; i <= N; ++i) { long long idx = 0; const float lp = ModShapes::feel (ModShapes::swing (2.0f * i / (float) N, kp.groove, idx), kp.feel);
                        const float m = 0.5f * (1.0f + ModShapes::shape (kp.shape, lp, kp.pulseWidth, kp.custom, kp.customPoints, 0)); const float gv = 1.0f - kp.depth * m;
                        const float x = plot.getX() + i, y = plot.getBottom() - gv * plot.getHeight(); if (i == 0) q2.startNewSubPath (x, y); else q2.lineTo (x, y); }
                    g.setColour (juce::Colour (0xffffc23a).withAlpha (0.8f)); g.strokePath (q2, juce::PathStrokeType (1.6f));
                }
            }
            // live meter
            ColourBits::panel (g, meter, acc);
            auto mm = meter.reduced (12.0f, 14.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (mm, 3.0f);
            g.setColour (acc); g.fillRoundedRectangle (mm.withTrimmedTop (mm.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, gainSm))), 3.0f);

            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "TREMOR");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (tr->proc.cycleHz(), 2) + " Hz", juce::Colours::white, 20.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Tremor::modeName (kp.mode)) + "  /  " + Eng::WingMod::shapeName (kp.shape)
                                                            + (kp.mode == Tremor::rhythm || kp.sync ? "  /  " + juce::String (Eng::WingMod::syncName (kp.mode == Tremor::rhythm ? kp.stepDivision : kp.syncIndex)) : juce::String()));
            ColourBits::line (g, q.removeFromTop (16.0f), "Depth " + juce::String (juce::roundToInt (kp.depth * 100)) + " %   Gain now " + juce::String (Eng::gainToDb (gainSm), 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Dynamics: ") + Tremor::dynName (kp.dyn) + (kp.dyn ? "  (" + juce::String (tr->proc.envelopeDb(), 0) + " dB)" : juce::String()));
            if (kp.harmonic) ColourBits::line (g, q.removeFromTop (16.0f), "HARMONIC split at " + ColourBits::hz (kp.xoverHz));
            if (kp.analog > 0) ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Analog: ") + Eng::AnalogStyle::voicingName (kp.analog - 1));
            if (kp.stereoDeg > 0.5f) ColourBits::line (g, q.removeFromTop (16.0f), "Stereo phase " + juce::String (juce::roundToInt (kp.stereoDeg)) + " deg");
        }
    };

    //==============================================================================
    /** Orbit Pan: the listener seen from above, the source orbiting at its live angle with a fading trail, the width arc,
        and (Circle) the front / back depth. */
    struct OrbitViz : public Viz, private juce::Timer
    {
        OrbitViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ob (dynamic_cast<SqOrbitPanPlugin*> (&p)) { startTimerHz (40); }
        SqOrbitPanPlugin* ob; std::array<juce::Point<float>, 48> trail {}; int tp = 0; float pos = 0, back = 0;
        void timerCallback() override
        {
            if (ob == nullptr) return;
            pos = ob->proc.position(); back = ob->proc.backness();
            const float az = pos * 0.5f * juce::MathConstants<float>::pi;
            const float rad = ob->mode.get() == Orbit::circle ? 1.0f - 0.35f * back : 1.0f;
            const float y = ob->mode.get() == Orbit::circle && back > 0.5f ? 1.0f : -1.0f;
            trail[(size_t) tp] = { std::sin (az) * rad, y * std::abs (std::cos (az)) * rad }; tp = (tp + 1) % (int) trail.size();
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (ob == nullptr) return;
            const auto acc = neon (accent); const auto kp = ob->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto stage = r.reduced (14.0f);
            ColourBits::caption (g, stage.removeFromTop (14.0f), "ORBIT  (seen from above, front is up)");
            const float R = juce::jmin (stage.getWidth() * 0.45f, stage.getHeight() * 0.48f);
            const juce::Point<float> c (stage.getCentreX(), stage.getCentreY() + 4);
            // rings and the speakers
            for (int k = 1; k <= 3; ++k) { g.setColour (acc.withAlpha (0.08f * k)); g.drawEllipse (c.x - R * k / 3.0f, c.y - R * k / 3.0f, R * 2 * k / 3.0f, R * 2 * k / 3.0f, 1.0f); }
            auto spk = [&] (float deg) { const float a = juce::degreesToRadians (deg); g.setColour (juce::Colours::white.withAlpha (0.5f)); g.fillRoundedRectangle (c.x + std::sin (a) * R - 7, c.y - std::cos (a) * R - 7, 14, 14, 3); };
            spk (-30); spk (30);
            // width arc
            {
                const float a0 = juce::degreesToRadians (kp.offsetDeg - kp.widthDeg * 0.5f), a1 = juce::degreesToRadians (kp.offsetDeg + kp.widthDeg * 0.5f);
                juce::Path arc; arc.addCentredArc (c.x, c.y, R * 0.92f, R * 0.92f, 0, a0, a1, true);
                g.setColour (acc.withAlpha (0.4f)); g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            // head
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillEllipse (c.x - 18, c.y - 22, 36, 44);
            g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawEllipse (c.x - 18, c.y - 22, 36, 44, 1.2f);
            g.fillEllipse (c.x - 22, c.y - 6, 5, 12); g.fillEllipse (c.x + 17, c.y - 6, 5, 12);
            // trail and the source
            for (int i = 0; i < (int) trail.size(); ++i)
            {
                const int k = (tp + i) % (int) trail.size(); const float a = (float) i / trail.size();
                const auto pt = trail[(size_t) k];
                g.setColour (acc.withAlpha (0.05f + 0.4f * a)); g.fillEllipse (c.x + pt.x * R * 0.8f - 3, c.y + pt.y * R * 0.8f - 3, 6, 6);
            }
            const auto pt = trail[(size_t) ((tp + (int) trail.size() - 1) % (int) trail.size())];
            const juce::Point<float> sp (c.x + pt.x * R * 0.8f, c.y + pt.y * R * 0.8f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, sp.x, sp.y, acc.withAlpha (0.0f), sp.x + 22, sp.y, true)); g.fillEllipse (sp.x - 22, sp.y - 22, 44, 44);
            g.setColour (juce::Colours::white); g.fillEllipse (sp.x - 6, sp.y - 6, 12, 12);

            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "POSITION");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (juce::roundToInt (pos * 90.0f)) + " deg", juce::Colours::white, 20.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Orbit::modeName (kp.mode)) + (kp.binaural ? "  /  BINAURAL" : "  /  law " + juce::String (Orbit::lawName (kp.law))));
            ColourBits::line (g, q.removeFromTop (16.0f), "Offset " + juce::String (juce::roundToInt (kp.offsetDeg)) + "   Width " + juce::String (juce::roundToInt (kp.widthDeg)) + " deg");
            if (kp.audioTrigger) ColourBits::line (g, q.removeFromTop (16.0f), "Trigger every " + juce::String (kp.divider) + " hit(s), " + juce::String (ob->proc.onsets()) + " heard");
            if (kp.mode == Orbit::circle) ColourBits::line (g, q.removeFromTop (16.0f), "Behind " + juce::String (juce::roundToInt (back * 100)) + " %   Haas " + juce::String (kp.haasMs, 2) + " ms");
            ColourBits::line (g, q.removeFromTop (16.0f), "Mix " + juce::String (juce::roundToInt (kp.mix * 100)) + " %");
        }
    };

    //==============================================================================
    /** Mirror Shift / Lite: the two micro-pitch voices fanned out from the centre (cents and ms), the correlation meter
        and the mono-sum loss - what the width will cost in mono. */
    struct MirrorViz : public Viz, private juce::Timer
    {
        MirrorViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), full (dynamic_cast<SqMirrorShiftPlugin*> (&p)), lite (dynamic_cast<SqMirrorShiftLitePlugin*> (&p)) { startTimerHz (24); }
        SqMirrorShiftPlugin* full; SqMirrorShiftLitePlugin* lite; float corr = 1, loss = 0, duck = 0, t = 0;
        Mirror::Processor* proc() { return full != nullptr ? &full->proc : (lite != nullptr ? &lite->proc : nullptr); }
        void timerCallback() override
        {
            if (auto* p = proc()) { corr += 0.25f * (p->correlation() - corr); loss += 0.25f * (p->monoLossDb() - loss); duck = p->duckGainDb(); }
            t += 0.04f; repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto* pr = proc(); if (pr == nullptr) return;
            const auto acc = neon (accent);
            const auto kp = full != nullptr ? full->knobParams() : lite->knobParams();
            const auto& st = Mirror::style (kp.style);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String ("VOICES  -  ") + st.name);
            auto meterR = plot.removeFromBottom (34.0f);
            // fanned voices: each one a wavy ribbon leaving the centre to its side, spread by detune, length by delay
            const float cx = plot.getCentreX(), cy = plot.getCentreY();
            const float spread = 0.15f + 0.85f * (st.maxCents > 0 ? kp.detune : 0.6f), len = plot.getWidth() * (0.18f + 0.3f * kp.delay);
            for (int side = -1; side <= 1; side += 2)
                for (int v = 0; v < st.voices; ++v)
                {
                    juce::Path p; const float ang = side * (0.35f + 0.5f * spread) * (v == 0 ? 1.0f : 0.6f);
                    for (int i = 0; i <= 60; ++i)
                    {
                        const float u = i / 60.0f, wob = std::sin (u * 14.0f + t * (3.0f + v) + side) * 6.0f * (1.0f - u) * (0.3f + st.wander);
                        const float x = cx + side * u * len, y = cy - std::tan (ang * 0.6f) * u * len * 0.35f * side + wob;
                        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    }
                    glowStroke (g, p, v == 0 ? acc : acc.brighter (0.5f).withAlpha (0.7f), v == 0 ? 2.4f : 1.4f);
                }
            g.setColour (juce::Colours::white); g.fillEllipse (cx - 5, cy - 5, 10, 10);
            g.setFont (Theme::font (9.0f, true)); g.setColour (juce::Colours::white.withAlpha (0.7f));
            const double cents = st.maxCents * kp.detune, ms = st.maxDelayMs * kp.delay;
            g.drawText ("L  " + (st.maxCents > 0 ? "+" + juce::String (cents, 1) + " ct" : juce::String ("wow")) + "  " + juce::String (ms, 1) + " ms", plot.withWidth (plot.getWidth() * 0.5f).toNearestInt(), juce::Justification::topLeft);
            g.drawText ("R  " + (st.maxCents > 0 ? "-" + juce::String (cents, 1) + " ct" : juce::String ("flutter")) + "  " + juce::String (ms * 1.3, 1) + " ms", plot.toNearestInt(), juce::Justification::topRight);
            // correlation meter
            auto bar = meterR.reduced (0, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (bar, 4.0f);
            const float xc = bar.getX() + bar.getWidth() * 0.5f * (1.0f + juce::jlimit (-1.0f, 1.0f, corr));
            const auto mc = corr < 0 ? juce::Colour (0xffff4a4a) : corr < 0.3f ? juce::Colour (0xffffc23a) : acc;
            g.setColour (mc); g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (xc, bar.getCentreX()), bar.getY(), std::abs (xc - bar.getCentreX()) + 2, bar.getHeight()), 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (8.5f));
            g.drawText ("-1", bar.toNearestInt(), juce::Justification::centredLeft); g.drawText ("+1", bar.toNearestInt(), juce::Justification::centredRight); g.drawText ("0", bar.toNearestInt(), juce::Justification::centred);

            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "MONO COMPATIBILITY");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (loss, 1) + " dB", loss < -2.5f ? juce::Colour (0xffffc23a) : juce::Colours::white, 20.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "mono-sum loss  /  correlation " + juce::String (corr, 2));
            if (full != nullptr) ColourBits::line (g, q.removeFromTop (16.0f), kp.focusHz > 21 ? "Focus above " + ColourBits::hz (kp.focusHz) : juce::String ("Focus: full range"));
            if (full != nullptr && kp.duckDb > 0) ColourBits::line (g, q.removeFromTop (16.0f), "Duck " + juce::String (duck, 1) + " dB" + (kp.keyOn ? " (key)" : ""));
            ColourBits::line (g, q.removeFromTop (16.0f), "Mix " + juce::String (juce::roundToInt (kp.mix * 100)) + " %");
        }
    };

    //==============================================================================
    /** Shutter Delay / Lite: the echo train of the two lines as the algorithm routes them (bar height = level after the
        feedback), the converter's clock and bandwidth, and a stepped "sample" glyph that coarsens with Multiply. */
    struct ShutterViz : public Viz, private juce::Timer
    {
        ShutterViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sh (dynamic_cast<SqShutterBase*> (&p)) { startTimerHz (20); }
        SqShutterBase* sh; float flash = 0;
        void timerCallback() override { if (sh == nullptr) return; if (! sh->edit.getTransport().isPlaying()) sh->refreshSettings(); flash = juce::jmax (flash * 0.85f, sh->levelIn.load()); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (sh == nullptr) return;
            const auto acc = neon (accent); const auto& p = sh->proc.p;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            auto glyph = r.removeFromLeft (juce::jmin (180.0f, r.getWidth() * 0.25f)); r.removeFromLeft (8.0f);
            // stepped sample glyph
            ColourBits::panel (g, glyph, acc);
            {
                auto gr = glyph.reduced (12.0f); ColourBits::caption (g, gr.removeFromTop (14.0f), "CLOCK");
                const int steps = 32 / Shutter::multiplyFactor (p.multiply); juce::Path pth;
                for (int i = 0; i <= steps; ++i)
                {
                    const float x0 = gr.getX() + gr.getWidth() * i / (float) steps, x1 = gr.getX() + gr.getWidth() * (i + 1) / (float) steps;
                    const float v = std::sin (juce::MathConstants<float>::twoPi * (i + 0.5f) / steps * 1.5f);
                    const float y = gr.getCentreY() - v * gr.getHeight() * 0.38f;
                    if (i == 0) pth.startNewSubPath (x0, y); else pth.lineTo (x0, y);
                    if (i < steps) pth.lineTo (juce::jmin (x1, gr.getRight()), y);
                }
                glowStroke (g, pth, acc, 1.8f);
            }
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String ("ECHOES  -  ") + Shutter::algoName (p.algo) + (p.freeze ? "   (FROZEN)" : ""));
            const double ta = sh->proc.totalMs (0), tb = sh->proc.totalMs (1);
            const double span = juce::jmax (500.0, (p.algo == Shutter::classic ? ta : juce::jmax (ta, tb)) * 6.5);
            g.setColour (juce::Colours::white.withAlpha (0.1f)); g.drawHorizontalLine ((int) plot.getBottom() - 1, plot.getX(), plot.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillRect (plot.getX(), plot.getBottom() - plot.getHeight() * juce::jlimit (0.2f, 1.0f, flash * 2.0f), 3.0f, plot.getHeight() * juce::jlimit (0.2f, 1.0f, flash * 2.0f));
            auto tap = [&] (double ms, float level, bool lineB)
            {
                if (ms > span || level < 0.02f) return;
                const float x = plot.getX() + (float) (ms / span) * plot.getWidth(), h = plot.getHeight() * juce::jlimit (0.0f, 1.0f, level);
                const auto col = lineB ? juce::Colour (0xffffc23a) : acc;
                g.setGradientFill (juce::ColourGradient (col, x, plot.getBottom() - h, col.withAlpha (0.1f), x, plot.getBottom(), false));
                g.fillRoundedRectangle (x - 3, plot.getBottom() - h, 6, h, 2);
            };
            const float fa = p.freeze ? 1.0f : juce::jlimit (0.0f, 1.0f, p.feedbackA), fb = p.freeze ? 1.0f : juce::jlimit (0.0f, 1.0f, p.feedbackB);
            switch (p.algo)
            {
                case Shutter::parallel: case Shutter::reverb:
                    for (int k = 1; k < 24; ++k) { tap (ta * k, std::pow (fa, (float) (k - 1)), false); tap (tb * k, std::pow (fb, (float) (k - 1)), true); } break;
                case Shutter::series: case Shutter::pingPong:
                    for (int k = 0; k < 24; ++k) { tap (ta + k * (ta + tb), std::pow (p.algo == Shutter::series ? fb : fa, (float) k), false); tap (ta + tb + k * (ta + tb), std::pow (p.algo == Shutter::series ? fb : fa, (float) k), true); } break;
                case Shutter::crissCross:
                    for (int k = 1; k < 24; ++k) { tap (k % 2 ? ta * k : tb * k, std::pow (0.5f * (fa + fb), (float) (k - 1)), k % 2 == 0); } break;
                default: for (int k = 1; k < 30; ++k) tap (ta * k, std::pow (fa, (float) (k - 1)), false); break;
            }
            g.setFont (Theme::font (8.5f)); g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText (juce::String (span / 1000.0, 2) + " s", plot.toNearestInt(), juce::Justification::bottomRight);

            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "CONVERTER");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (Shutter::multiplyFactor (p.multiply)) + "x  " + juce::String (Shutter::bandwidthHz (p.multiply) / 1000.0, 1) + " kHz", juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "clock " + juce::String (sh->proc.lineRate() / 1000.0, 1) + " kHz   " + Shutter::bitsName (p.bits));
            ColourBits::line (g, q.removeFromTop (16.0f), "A " + juce::String (ta, 0) + " ms" + (p.algo != Shutter::classic ? "   B " + juce::String (tb, 0) + " ms" : juce::String()));
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Time change: ") + (p.smooth ? "Smooth" : "Glitch"));
            if (p.lfoDepth > 0) ColourBits::line (g, q.removeFromTop (16.0f), "LFO " + juce::String (p.lfoHz, p.lfoHz < 10 ? 2 : 0) + " Hz  " + juce::String (juce::roundToInt (p.lfoDepth * 100)) + " %");
        }
    };

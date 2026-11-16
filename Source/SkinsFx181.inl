// Included inside namespace Skins (PluginSkins.h): faces for the 1.8.1 effects (SqFx181.h).
// Time Weaver (two 32-step envelopes with factory slots), Larva Filter (eight stage curves + 16-step sequencer), Apex Triad (three-band envelope view).

    //==========================================================================
    /** 32 bars bound to "Prefix N" parameters; drag to draw. When a factory slot is active the bars show the slot (read-only) and a click offers to copy it to the user steps. */
    class BarEditor32 : public juce::Component
    {
    public:
        BarEditor32 (HandleMap& h, juce::Colour acc, juce::String pattern, bool bipolarBeats) : handles (h), accent (acc), namePattern (std::move (pattern)), beats (bipolarBeats) {}
        std::function<float (int)> factoryValue;      // when set, the bars show these values instead of the handles
        std::function<void()> onCopyRequest, onChange;
        std::function<int()> currentStep;
        float maxValue = 1.0f;                        // natural max (8 beats for time, 1 for volume)
        Handle* handleFor (int i) const   { auto it = handles.find (namePattern.replace ("%d", juce::String (i + 1))); return it != handles.end() ? it->second : nullptr; }
        float valueAt (int i) const
        {
            if (factoryValue) return factoryValue (i) / maxValue;
            auto* hd = handleFor (i); return hd != nullptr ? hd->get() : 0.0f;
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat(); const float bw = r.getWidth() / 32.0f; const int play = currentStep ? currentStep() : -1;
            for (int i = 0; i < 32; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + bw * (float) i, r.getY(), bw, r.getHeight()).reduced (1.0f, 0.0f);
                g.setColour (juce::Colours::black.withAlpha (i % 4 == 0 ? 0.45f : 0.3f)); g.fillRect (cell);
                if (i == 16) { g.setColour (Theme::text().withAlpha (0.25f)); g.drawVerticalLine ((int) cell.getX(), r.getY(), r.getBottom()); }
                const float v = juce::jlimit (0.0f, 1.0f, valueAt (i));
                auto bar = cell.withTop (cell.getBottom() - juce::jmax (2.0f, cell.getHeight() * v));
                g.setColour ((factoryValue ? accent.withAlpha (0.55f) : accent).withAlpha (i == play ? 1.0f : 0.75f)); g.fillRoundedRectangle (bar, 1.5f);
                if (i == play) { g.setColour (Theme::text().withAlpha (0.15f)); g.fillRect (cell); }
            }
            if (factoryValue) { g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText ("factory slot - click to copy to USER and edit", getLocalBounds().reduced (4, 2), juce::Justification::topRight); }
        }
        void apply (juce::Point<float> p)
        {
            const int i = juce::jlimit (0, 31, (int) (p.x / ((float) getWidth() / 32.0f)));
            if (auto* hd = handleFor (i)) hd->set (juce::jlimit (0.0f, 1.0f, 1.0f - p.y / (float) getHeight()));
        }
        void mouseDown (const juce::MouseEvent& e) override   { if (factoryValue) { if (onCopyRequest) onCopyRequest(); return; } apply (e.position); repaint(); }
        void mouseDrag (const juce::MouseEvent& e) override   { if (factoryValue) return; apply (e.position); repaint(); }
        void mouseUp (const juce::MouseEvent&) override       { if (onChange) onChange(); }
    private:
        HandleMap& handles; juce::Colour accent; juce::String namePattern; bool beats;
    };

    //==========================================================================
    struct TimeWeaverViz : public Viz
    {
        TimeWeaverViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c)
        {
            tw = dynamic_cast<SqTimeWeaverPlugin*> (&plugin);
            setInterceptsMouseClicks (false, true);
            timeBars = std::make_unique<BarEditor32> (h, c, "Time Step %d", true); timeBars->maxValue = 8.0f;
            volBars = std::make_unique<BarEditor32> (h, Theme::amber(), "Volume Step %d", false);
            for (auto* be : { timeBars.get(), volBars.get() }) { be->currentStep = [this] { return tw != nullptr ? tw->currentStep.load() : -1; }; be->onChange = [this] { repaint(); }; addAndMakeVisible (*be); }
            timeBars->onCopyRequest = [this] { if (tw != nullptr) { tw->copySlotToUser (true, tw->liveTimeSlot.load()); if (onChange) onChange(); } };
            volBars->onCopyRequest = [this] { if (tw != nullptr) { tw->copySlotToUser (false, tw->liveVolSlot.load()); if (onChange) onChange(); } };
        }
        std::function<void()> onChange;
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            auto top = r.removeFromTop (r.getHeight() / 2).reduced (0, 3), bot = r.reduced (0, 3);
            timeBars->setBounds (top.reduced (8, 14).withTrimmedTop (4)); volBars->setBounds (bot.reduced (8, 14).withTrimmedTop (4));
        }
        void paint (juce::Graphics& g) override
        {
            if (tw == nullptr) return;
            const int ts = tw->liveTimeSlot.load(), vs = tw->liveVolSlot.load();
            timeBars->factoryValue = ts > 0 ? std::function<float (int)> ([ts] (int i) { return SqTimeWeaverPlugin::timeSlots()[(size_t) juce::jlimit (0, SqTimeWeaverPlugin::numSlots - 1, ts - 1)].values[(size_t) i]; }) : nullptr;
            volBars->factoryValue = vs > 0 ? std::function<float (int)> ([vs] (int i) { return SqTimeWeaverPlugin::volumeSlots()[(size_t) juce::jlimit (0, SqTimeWeaverPlugin::numSlots - 1, vs - 1)].values[(size_t) i]; }) : nullptr;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto top = r.removeFromTop (r.getHeight() * 0.5f).reduced (0.0f, 3.0f), bot = r.reduced (0.0f, 3.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t, const juce::String& right)
            {
                g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
                g.drawText (right, a.toNearestInt().reduced (6, 3), juce::Justification::topRight);
            };
            box (top, "TIME  " + SqTimeWeaverPlugin::timeSlotNames()[juce::jlimit (0, SqTimeWeaverPlugin::numSlots, ts)] + "   (bars = how far back it reads, 0 = live, two bars of 1/16 steps)", "delay " + juce::String (tw->liveDelayBeats.load(), 2) + " beats   MIDI C1.. picks a time slot");
            box (bot, "VOLUME  " + SqTimeWeaverPlugin::volumeSlotNames()[juce::jlimit (0, SqTimeWeaverPlugin::numSlots, vs)], "gain " + juce::String (juce::roundToInt (tw->liveGain.load() * 100)) + " %   MIDI C4.. picks a volume slot");
        }
        SqTimeWeaverPlugin* tw = nullptr; std::unique_ptr<BarEditor32> timeBars, volBars;
    };

    //==========================================================================
    struct LarvaFilterViz : public Viz
    {
        LarvaFilterViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c)
        {
            lf = dynamic_cast<SqLarvaFilterPlugin*> (&plugin);
            setInterceptsMouseClicks (false, true);
            grid = std::make_unique<StepGrid> (h, c, std::vector<StepGrid::Lane> { { "Seq", "Step %d", true } }, [this] { return lf != nullptr && lf->bank[SqLarvaFilterPlugin::seqOn] > 0.5f ? lf->currentStep.load() : -1; });
            grid->onChange = [this] { repaint(); };
            addAndMakeVisible (*grid);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            grid->setBounds (r.removeFromRight (r.getWidth() * 0.4f).reduced (8, 14).withTrimmedTop (4));
        }
        static float stageDb (int type, float cut, float res, float hz)
        {
            const float w = hz / juce::jmax (20.0f, cut), q = 0.5f + res * res * 19.5f;
            const float lp2 = -10.0f * std::log10 (std::pow (1.0f - w * w, 2.0f) + std::pow (w / q, 2.0f));
            const float hp2 = lp2 + 40.0f * std::log10 (juce::jmax (1.0e-3f, w));
            switch (type)
            {
                case 0: return lp2;
                case 1: return lp2 + (w < 1.0f ? 0.0f : -40.0f * std::log10 (w));
                case 2: return hp2;
                case 3: return hp2 + (w > 1.0f ? 0.0f : 40.0f * std::log10 (juce::jmax (1.0e-3f, w)));
                case 4: return -10.0f * std::log10 (1.0f + q * q * std::pow (w - 1.0f / w, 2.0f));
                case 5: return 10.0f * std::log10 (std::pow (w - 1.0f / w, 2.0f) / (std::pow (w - 1.0f / w, 2.0f) + 1.0f / (q * q)) + 1.0e-4f);
                case 6: return 20.0f * std::log10 (1.0f + (0.5f + res * 3.0f) / std::sqrt (1.0f + q * q * std::pow (w - 1.0f / w, 2.0f)));
                default: return 0.0f;
            }
        }
        void paint (juce::Graphics& g) override
        {
            if (lf == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto seqArea = r.removeFromRight (r.getWidth() * 0.4f); r.removeFromRight (8.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t) { g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f); g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true)); g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft); };
            const int route = juce::jlimit (0, 2, (int) std::lround (lf->bank[SqLarvaFilterPlugin::routing]));
            box (seqArea, juce::String ("SEQUENCER  ") + (lf->bank[SqLarvaFilterPlugin::seqOn] > 0.5f ? "ON  " : "off  ") + SqLarvaFilterPlugin::rateNames()[juce::jlimit (0, 5, (int) std::lround (lf->bank[SqLarvaFilterPlugin::seqRate]))]);
            box (r, "STAGES  " + SqLarvaFilterPlugin::routingNames()[route] + "   live curves move with the sequencer, LFOs and envelope");
            auto in = r.reduced (8.0f, 16.0f); drawFreqGrid (g, in, 24.0f);
            juce::Graphics::ScopedSaveState save (g); g.reduceClipRegion (in.toNearestInt());
            static const juce::uint32 cols[8] = { 0xffb8ff3a, 0xff3affb0, 0xff3ad6ff, 0xffffb23a, 0xffff3ad1, 0xffb14dff, 0xffff5a3a, 0xffffd23a };
            juce::Path sum; bool anyOn = false;
            for (int i = 0; i <= (int) in.getWidth(); ++i)
            {
                const float hz = hzForX (in.getX() + (float) i, in); float total = 0.0f; int cnt = 0; float par = 0.0f;
                for (int s = 0; s < SqLarvaFilterPlugin::stages; ++s)
                {
                    if (lf->stageParam (s, SqLarvaFilterPlugin::sOn) < 0.5f) continue;
                    const float cut = lf->liveCutoff[(size_t) s].load() > 0.0f ? lf->liveCutoff[(size_t) s].load() : lf->stageParam (s, SqLarvaFilterPlugin::sCut);
                    const float db = stageDb ((int) std::lround (lf->stageParam (s, SqLarvaFilterPlugin::sType)), cut, lf->stageParam (s, SqLarvaFilterPlugin::sRes), hz) * lf->stageParam (s, SqLarvaFilterPlugin::sMix);
                    if (route == 0) total += db; else { par += std::pow (10.0f, db / 20.0f); ++cnt; }
                    anyOn = true;
                }
                if (route != 0 && cnt > 0) total = 20.0f * std::log10 (par / (float) cnt);
                const float y = in.getCentreY() - juce::jlimit (-36.0f, 24.0f, total) / 36.0f * in.getHeight() * 0.5f;
                if (i == 0) sum.startNewSubPath (in.getX(), y); else sum.lineTo (in.getX() + (float) i, y);
            }
            // individual stages, thin
            for (int s = 0; s < SqLarvaFilterPlugin::stages; ++s)
            {
                if (lf->stageParam (s, SqLarvaFilterPlugin::sOn) < 0.5f) continue;
                const float cut = lf->liveCutoff[(size_t) s].load() > 0.0f ? lf->liveCutoff[(size_t) s].load() : lf->stageParam (s, SqLarvaFilterPlugin::sCut);
                juce::Path p;
                for (int i = 0; i <= (int) in.getWidth(); i += 2)
                {
                    const float hz = hzForX (in.getX() + (float) i, in);
                    const float db = stageDb ((int) std::lround (lf->stageParam (s, SqLarvaFilterPlugin::sType)), cut, lf->stageParam (s, SqLarvaFilterPlugin::sRes), hz);
                    const float y = in.getCentreY() - juce::jlimit (-36.0f, 24.0f, db) / 36.0f * in.getHeight() * 0.5f;
                    if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
                }
                g.setColour (juce::Colour (cols[s]).withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (1.2f));
                const float x = xForHz (cut, in); g.setColour (juce::Colour (cols[s])); g.fillEllipse (x - 3.0f, in.getY() + 4.0f + 8.0f * (float) s, 6.0f, 6.0f);
                g.setFont (Theme::font (7.5f)); g.drawText (juce::String (s + 1), (int) x + 5, (int) (in.getY() + 1.0f + 8.0f * (float) s), 14, 12, juce::Justification::centredLeft);
            }
            if (anyOn) { g.setColour (accent.withAlpha (0.15f)); juce::Path f (sum); f.lineTo (in.getRight(), in.getBottom()); f.lineTo (in.getX(), in.getBottom()); f.closeSubPath(); g.fillPath (f); g.setColour (accent); g.strokePath (sum, juce::PathStrokeType (2.2f)); }
            else { g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f)); g.drawText ("switch a stage on", in.toNearestInt(), juce::Justification::centred); }
        }
        SqLarvaFilterPlugin* lf = nullptr; std::unique_ptr<StepGrid> grid;
    };

    //==========================================================================
    struct TriadViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto* mx = dynamic_cast<SqApexTriadPlugin*> (&plugin); if (mx == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            static const char* names[3] = { "LOW", "MID", "HIGH" };
            static const juce::uint32 cols[3] = { 0xffff6a3a, 0xffffb23a, 0xffffe03a };
            const float laneH = r.getHeight() / 3.0f;
            const int w = mx->historyWrite.load();
            for (int k = 0; k < 3; ++k)
            {
                auto lane = juce::Rectangle<float> (r.getX(), r.getY() + laneH * (float) k, r.getWidth(), laneH).reduced (0.0f, 2.0f);
                g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (lane, 4.0f);
                const float thr = mx->bandParam (k, SqApexTriadPlugin::bThr);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawText (juce::String (names[k]) + "   " + (k == 0 ? "< " + juce::String (juce::roundToInt (mx->bank[SqApexTriadPlugin::xLow])) + " Hz" : k == 2 ? "> " + juce::String (juce::roundToInt (mx->bank[SqApexTriadPlugin::xHigh])) + " Hz" : juce::String (juce::roundToInt (mx->bank[SqApexTriadPlugin::xLow])) + " - " + juce::String (juce::roundToInt (mx->bank[SqApexTriadPlugin::xHigh])) + " Hz"), lane.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
                g.drawText ("GR " + juce::String (mx->bandGrDb[(size_t) k].load(), 1) + " dB   in " + juce::String (mx->bandInDb[(size_t) k].load(), 1) + " dB", lane.toNearestInt().reduced (6, 3), juce::Justification::topRight);
                auto in = lane.reduced (8.0f, 14.0f).withTrimmedTop (2.0f);
                auto yFor = [&] (float db) { return in.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * in.getHeight(); };
                // threshold line
                g.setColour (Theme::text().withAlpha (0.25f)); g.drawHorizontalLine ((int) yFor (thr), in.getX(), in.getRight());
                juce::Path level, gr; const int points = juce::jmin (SqApexBase::historySize, (int) in.getWidth());
                for (int i = 0; i < points; ++i)
                {
                    const int idx = (w - points + i) & (SqApexBase::historySize - 1);
                    const float x = in.getX() + in.getWidth() * (float) i / (float) (points - 1);
                    const float lv = mx->bandIn[(size_t) k][(size_t) idx], gd = mx->bandGr[(size_t) k][(size_t) idx];
                    if (i == 0) { level.startNewSubPath (x, yFor (lv)); gr.startNewSubPath (x, in.getY() - gd / 24.0f * in.getHeight()); }
                    else { level.lineTo (x, yFor (lv)); gr.lineTo (x, in.getY() - gd / 24.0f * in.getHeight()); }
                }
                juce::Path f (level); f.lineTo (in.getRight(), in.getBottom()); f.lineTo (in.getX(), in.getBottom()); f.closeSubPath();
                g.setColour (juce::Colour (cols[k]).withAlpha (0.25f)); g.fillPath (f);
                g.setColour (juce::Colour (cols[k])); g.strokePath (level, juce::PathStrokeType (1.4f));
                g.setColour (Theme::text().withAlpha (0.85f)); g.strokePath (gr, juce::PathStrokeType (1.2f));
            }
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
            g.drawText ("ceiling " + juce::String (mx->bank[SqApexTriadPlugin::ceiling], 1) + " dB   limiter GR " + juce::String (mx->limiterGrDb.load(), 1) + " dB", r.toNearestInt().reduced (8, 2), juce::Justification::bottomRight);
        }
    };

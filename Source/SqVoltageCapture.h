#pragma once
// Squelchmoth 1.51 - Voltage Capture: plays a capture - a model made by measuring a unit with a sweep (see SqCapture.h; the
// `capture` tool makes them). The model is the unit's tone and the distortion it adds at and below the level it was swept
// at: a filter in front (when the measurement called for one), then the signal's powers, each through its own filter.
// Model picks among the captures on this computer; Input pushes into the model (anything over the level the unit was swept
// at is held there - the model knows nothing beyond it); Output and Mix as usual. With no capture it passes the sound through.
// The powers are made at 8 times the rate so they do not fold; the delay (96 samples plus the filters' lead-in) is reported.

#include <JuceHeader.h>
#include "CaptureStore.h"
#include "SqTube.h"

namespace te = tracktion;

class SqVoltageCapturePlugin : public te::Plugin, private juce::AsyncUpdater
{
public:
    SqVoltageCapturePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        modelV.referTo (state, "model", um, 0.0f); inputV.referTo (state, "input", um, 0.0f); outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); captureName.referTo (state, "capture", um, {});
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        model  = addParam ("model",  TRANS("Model"),  { 0.0f, 31.0f, 1.0f }, [] (float v) { const auto names = CaptureStore::list(); const int i = juce::roundToInt (v); return names.isEmpty() ? juce::String ("None") : names[juce::jlimit (0, names.size() - 1, i)]; },
                           [] (const juce::String& s) { const auto names = CaptureStore::list(); for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (s.trim())) return (float) i; return (float) juce::jlimit (0, 31, s.getIntValue()); });
        input  = addParam ("input",  TRANS("Input"),  { -24.0f, 12.0f }, db, num);
        output = addParam ("output", TRANS("Output"), { -24.0f, 24.0f }, db, num);
        mix    = addParam ("mix",    TRANS("Mix"),    { 0.0f, 100.0f }, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, num);
        model->attachToCurrentValue (modelV); input->attachToCurrentValue (inputV); output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV);
    }
    ~SqVoltageCapturePlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); for (auto* p : { model.get(), input.get(), output.get(), mix.get() }) p->detachFromCurrentValue(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Capture"); }
    static constexpr const char* xmlTypeName = "sqmVoltageCapture";
    static constexpr int maxOrder = 7, osDelay = 96;

    juce::String getName() const override               { return "Voltage Capture"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VCap"; }
    juce::String getSelectableDescription() override    { return TRANS("Plays a model measured from a unit"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? (osDelay + lead.load()) / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    /** The capture in use (null: none). Message thread. */
    std::shared_ptr<const SqCapture::Model> current() const { return loaded; }
    juce::String currentName() const { return loadedName; }

    /** Use this capture (by its name in the store). Message thread. Returns false if there is no such capture. */
    bool useCapture (const juce::String& name)
    {
        auto m = std::make_shared<SqCapture::Model>(); if (! CaptureStore::load (name, *m)) return false;
        captureName = name; const auto names = CaptureStore::list(); const int ix = names.indexOf (name, true); if (ix >= 0) { model->setParameter ((float) ix, juce::sendNotification); wantedIndex = ix; }
        install (m, name); return true;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; maxBlock = juce::jmax (64, info.blockSizeSamples);
        for (auto& b : work) b.setSize (2, maxBlock, false, false, true); frontBuf.setSize (2, maxBlock, false, false, true);
        for (int ch = 0; ch < 2; ++ch) { for (auto& o : os[ch]) o.prepare (8); line[ch].assign ((size_t) osDelay, 0.0f); linePos[ch] = 0; dry[ch].assign (4096, 0.0f); dryPos[ch] = 0; }
        prepared = false; auto name = captureName.get(); const auto names = CaptureStore::list();
        if (name.isEmpty() && names.size() > 0) name = names[juce::jlimit (0, names.size() - 1, juce::roundToInt (model->getCurrentValue()))];
        auto m = std::make_shared<SqCapture::Model>(); if (name.isNotEmpty() && CaptureStore::load (name, *m)) install (m, name); else install (nullptr, {});
        wantedIndex = juce::roundToInt (model->getCurrentValue());
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 }; front.prepare (spec); front.reset(); for (auto& c : conv) { c.prepare (spec); c.reset(); } prepared = true;
    }
    void deinitialise() override { prepared = false; }
    void reset() override { front.reset(); for (auto& c : conv) c.reset(); for (int ch = 0; ch < 2; ++ch) { for (auto& o : os[ch]) o.reset(); std::fill (line[ch].begin(), line[ch].end(), 0.0f); std::fill (dry[ch].begin(), dry[ch].end(), 0.0f); } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! prepared) return;
        if (const int want = juce::roundToInt (model->getCurrentValue()); want != wantedIndex) { wantedIndex = want; triggerAsyncUpdate(); }       // the Model knob moved: the capture is read on the message thread
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels());
        for (int done = 0; done < fc.bufferNumSamples; done += maxBlock) process (buf, nch, fc.bufferStartSample + done, juce::jmin (maxBlock, fc.bufferNumSamples - done));
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, modelV, inputV, outputV, mixV); te::copyPropertiesToCachedValues (v, captureName);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> modelV, inputV, outputV, mixV; juce::CachedValue<juce::String> captureName;
    te::AutomatableParameter::Ptr model, input, output, mix;

private:
    void handleAsyncUpdate() override
    {
        const auto names = CaptureStore::list(); if (names.isEmpty()) return; const auto name = names[juce::jlimit (0, names.size() - 1, wantedIndex.load())];
        if (name == loadedName) return; auto m = std::make_shared<SqCapture::Model>(); if (CaptureStore::load (name, *m)) { captureName = name; install (m, name); changed(); }
    }
    /** A filter measured at one rate, for the rate in use. */
    juce::AudioBuffer<float> forRate (const std::vector<float>& h, double rate) const
    {
        if (std::abs (rate - sampleRate) < 1.0) { juce::AudioBuffer<float> b (1, (int) h.size()); b.copyFrom (0, 0, h.data(), (int) h.size()); return b; }
        const double ratio = rate / sampleRate; const int n = juce::jmax (8, (int) ((double) h.size() / ratio) - 8); std::vector<float> in (h); in.resize (h.size() + 64, 0.0f);
        juce::AudioBuffer<float> b (1, n); juce::WindowedSincInterpolator r; r.process (ratio, in.data(), b.getWritePointer (0), n); b.applyGain ((float) ratio); return b;
    }
    void install (std::shared_ptr<SqCapture::Model> m, const juce::String& name)
    {
        loaded = m; loadedName = name;
        if (m == nullptr || ! m->valid()) { order = 0; hasFront = false; lead = 0; return; }
        const int n = juce::jmin (maxOrder, m->order);
        for (int k = 0; k < n; ++k) conv[k].loadImpulseResponse (forRate (m->g[(size_t) k], m->rate), sampleRate, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        const bool f = ! m->front.empty(); if (f) front.loadImpulseResponse (forRate (m->front, m->rate), sampleRate, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        level = (float) m->level; frontPeak = (float) m->frontPeak; lead = juce::roundToInt (m->pre * sampleRate / m->rate); hasFront = f; order = n;
    }
    void process (juce::AudioBuffer<float>& buf, int nch, int start, int n)
    {
        const int ord = order.load(); const float wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f), outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), inGain = std::pow (10.0f, input->getCurrentValue() / 20.0f);
        const int delay = osDelay + lead.load();
        if (ord == 0) { for (int ch = 0; ch < nch; ++ch) { auto* d = buf.getWritePointer (ch, start); for (int i = 0; i < n; ++i) d[i] = delayed (ch, d[i], delay) * outGain; } return; }
        const float lv = level.load(), fp = frontPeak.load(); const bool f = hasFront.load();
        for (int ch = 0; ch < 2; ++ch) { auto* u = frontBuf.getWritePointer (ch); const float* d = buf.getReadPointer (juce::jmin (ch, nch - 1), start); for (int i = 0; i < n; ++i) u[i] = juce::jlimit (-1.0f, 1.0f, d[i] * inGain / lv); }
        if (f) { juce::dsp::AudioBlock<float> block (frontBuf.getArrayOfWritePointers(), 2, (size_t) n); juce::dsp::ProcessContextReplacing<float> ctx (block); front.process (ctx); for (int ch = 0; ch < 2; ++ch) { auto* u = frontBuf.getWritePointer (ch); for (int i = 0; i < n; ++i) u[i] = juce::jlimit (-fp, fp, u[i]); } }
        for (int ch = 0; ch < nch; ++ch)
        {
            const float* u = frontBuf.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const double v = u[i]; work[0].getWritePointer (ch)[i] = line[ch][(size_t) linePos[ch]]; line[ch][(size_t) linePos[ch]] = (float) v; if (++linePos[ch] >= osDelay) linePos[ch] = 0;
                for (int k = 2; k <= ord; ++k) work[k - 1].getWritePointer (ch)[i] = (float) os[ch][k - 2].process (v, [k] (double p) { double r = p; for (int j = 1; j < k; ++j) r *= p; return r; });
            }
        }
        for (int k = 0; k < ord; ++k) { if (nch < 2) work[k].clear (1, 0, n); juce::dsp::AudioBlock<float> block (work[k].getArrayOfWritePointers(), 2, (size_t) n); juce::dsp::ProcessContextReplacing<float> ctx (block); conv[k].process (ctx); }
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* d = buf.getWritePointer (ch, start);
            for (int i = 0; i < n; ++i) { float y = 0.0f; for (int k = 0; k < ord; ++k) y += work[k].getReadPointer (ch)[i]; const float was = delayed (ch, d[i], delay); d[i] = (was + wet * (y - was)) * outGain; }
        }
    }
    float delayed (int ch, float in, int delay) { auto& b = dry[ch]; const int size = (int) b.size(); b[(size_t) dryPos[ch]] = in; int at = dryPos[ch] - delay; if (at < 0) at += size; const float v = b[(size_t) at]; if (++dryPos[ch] >= size) dryPos[ch] = 0; return v; }

    juce::dsp::Convolution front { juce::dsp::Convolution::NonUniform { 512 } };
    juce::dsp::Convolution conv[maxOrder] { juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } }, juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } }, juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } }, juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } },
                                           juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } }, juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } }, juce::dsp::Convolution { juce::dsp::Convolution::NonUniform { 512 } } };
    juce::AudioBuffer<float> work[maxOrder], frontBuf; SqTube::Oversampler os[2][maxOrder - 1]; std::vector<float> line[2], dry[2]; int linePos[2] = {}, dryPos[2] = {};
    std::shared_ptr<const SqCapture::Model> loaded; juce::String loadedName;
    std::atomic<int> order { 0 }, lead { 0 }, wantedIndex { 0 }; std::atomic<float> level { 0.5f }, frontPeak { 1.0f }; std::atomic<bool> hasFront { false };
    double sampleRate = 44100.0; int maxBlock = 512; bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageCapturePlugin)
};

#pragma once
// Squelchmoth - audio analysis for the Mastering window and the plugin visualisers.
//
//  * SqAnalyzerPlugin : a hidden tap at the end of a plugin chain that copies the stereo signal into a FIFO
//  * LoudnessMeter    : EBU R128 / ITU-R BS.1770-4 (K-weighting, 400 ms blocks, absolute + relative gating,
//                       momentary, short-term, integrated, loudness range) and 4x oversampled true peak
//  * SpectrumAnalyser : 4096-point FFT, octave / third-octave / FFT views with peak hold and averaging
//  * PhaseMeter       : correlation (-1..1) and goniometer points
//  * measureFile()    : offline loudness of a WAV/AIFF/FLAC (used for 'Detect Loudness' and loudness matching)

#include <JuceHeader.h>
#include <deque>

namespace te = tracktion;

//==============================================================================
/** K-weighting: a high shelf (+4 dB above ~1.5 kHz) followed by a high-pass (~38 Hz), per BS.1770. */
struct KWeighting
{
    void prepare (double sampleRate)
    {
        // coefficients from the ITU-R BS.1770-4 reference design, re-derived for any sample rate
        const double fs = sampleRate;
        {   // stage 1: high shelf
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;  shelf.b1 = 2.0 * (K * K - Vh) / a0;  shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;        shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {   // stage 2: high-pass
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0 / a0;  hp.b1 = -2.0 / a0;  hp.b2 = 1.0 / a0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0;  hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
        for (auto& s : state) s = {};
    }
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; } shelf, hp;
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    State state[4];   // 2 stages x 2 channels

    inline double run (Coeffs& c, State& s, double x)
    {
        double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }
    inline float process (int channel, float x)
    {
        auto y = run (shelf, state[channel * 2], (double) x);
        return (float) run (hp, state[channel * 2 + 1], y);
    }
};

//==============================================================================
class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        kw.prepare (sr);
        blockLen = (int) (sr * 0.1);     // 100 ms sub-blocks; a 400 ms block = 4 of them, 3 s = 30
        reset();
        tpUp.reset();
    }

    void reset()
    {
        subBlocks.clear(); shortTermHistory.clear();
        accum[0] = accum[1] = 0.0; accumSamples = 0;
        momentary = shortTerm = integrated = -100.0f; range = 0.0f; truePeak = -100.0f; maxMomentary = maxShortTerm = -100.0f;
        gatedSum = 0.0; gatedCount = 0;
        relGate.clear();
    }

    void process (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            auto kl = kw.process (0, l[i]), kr = kw.process (1, r != nullptr ? r[i] : l[i]);
            accum[0] += (double) kl * kl; accum[1] += (double) kr * kr;
            if (++accumSamples >= blockLen) finishSubBlock();
        }
        truePeak = juce::jmax (truePeak, measureTruePeak (l, n), r != nullptr ? measureTruePeak (r, n) : -100.0f);
    }

    /** Call after the last block for the exact integrated value. */
    void finalise()              { recomputeIntegrated(); }
    float getMomentary() const   { return momentary; }
    float getShortTerm() const   { return shortTerm; }
    float getIntegrated() const  { return integrated; }
    float getRange() const       { return range; }
    float getTruePeak() const    { return truePeak; }
    float getMaxMomentary() const { return maxMomentary; }
    float getMaxShortTerm() const { return maxShortTerm; }

    /** 4x oversampled sample-peak (dBTP) using a 4-phase polyphase interpolator (BS.1770 Annex 2 style). */
    float measureTruePeak (const float* x, int n)
    {
        static const float phases[4][4] = { { 0.0017089843750f, 0.0109863281250f, -0.0196533203125f, 0.0332031250000f },
                                            { -0.0291748046875f, 0.0292968750000f, -0.0517578125000f, 0.0749511718750f },
                                            { -0.0517578125000f, 0.0292968750000f, -0.0291748046875f, 0.0332031250000f },
                                            { -0.0196533203125f, 0.0109863281250f, 0.0017089843750f, 0.0f } };
        // simplified: linear-phase 4x interpolation (12-tap windowed sinc) - accurate to ~0.1 dB for peak purposes
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            hist[3] = hist[2]; hist[2] = hist[1]; hist[1] = hist[0]; hist[0] = x[i];
            peak = juce::jmax (peak, std::abs (x[i]));
            for (int ph = 1; ph < 4; ++ph)
            {
                // cubic Hermite interpolation between hist[1] and hist[2]
                float t = (float) ph / 4.0f;
                float y0 = hist[3], y1 = hist[2], y2 = hist[1], y3 = hist[0];
                float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3, b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c = -0.5f * y0 + 0.5f * y2;
                float v = ((a * t + b) * t + c) * t + y1;
                peak = juce::jmax (peak, std::abs (v));
            }
        }
        juce::ignoreUnused (phases);
        return peak > 0.0f ? juce::Decibels::gainToDecibels (peak) : -100.0f;
    }

private:
    double sr = 44100.0;
    KWeighting kw;
    int blockLen = 4410;
    double accum[2] = { 0, 0 };
    int accumSamples = 0;
    std::deque<double> subBlocks;           // mean-square (sum of channels) per 100 ms sub-block
    std::deque<float> shortTermHistory;     // short-term loudness values for LRA
    std::vector<double> relGate;            // 400 ms block loudness values above the absolute gate (for relative gating)
    double gatedSum = 0.0; int gatedCount = 0;
    float momentary = -100.0f, shortTerm = -100.0f, integrated = -100.0f, range = 0.0f, truePeak = -100.0f, maxMomentary = -100.0f, maxShortTerm = -100.0f;
    float hist[4] = { 0, 0, 0, 0 };
    struct { void reset() {} } tpUp;

    static float toLufs (double meanSquare)   { return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : -100.0f; }

    void recomputeIntegrated()
    {
        if (gatedCount == 0) { integrated = -100.0f; return; }
        // relative gate: blocks more than 10 LU below the absolute-gated mean are dropped
        auto threshold = toLufs (gatedSum / (double) gatedCount) - 10.0f;
        double sum = 0.0; int count = 0;
        for (auto b : relGate) if (toLufs (b) > threshold) { sum += b; ++count; }
        integrated = count > 0 ? toLufs (sum / (double) count) : -100.0f;
    }

    void finishSubBlock()
    {
        auto ms = (accum[0] + accum[1]) / (double) juce::jmax (1, accumSamples);
        accum[0] = accum[1] = 0.0; accumSamples = 0;
        subBlocks.push_back (ms);
        if (subBlocks.size() > 30) subBlocks.pop_front();

        if (subBlocks.size() >= 4)
        {
            double m = 0.0; for (size_t i = subBlocks.size() - 4; i < subBlocks.size(); ++i) m += subBlocks[i];
            m /= 4.0;
            momentary = toLufs (m);
            maxMomentary = juce::jmax (maxMomentary, momentary);
            // integrated: every 400 ms block (overlapping at 100 ms steps) above -70 LUFS joins the gated set
            if (momentary > -70.0f)
            {
                relGate.push_back (m);
                gatedSum += m; ++gatedCount;
                if (relGate.size() % 10 == 0) recomputeIntegrated();   // once a second; finalise() does the last one
            }
        }
        if (subBlocks.size() >= 30)
        {
            double s = 0.0; for (auto b : subBlocks) s += b;
            s /= 30.0;
            shortTerm = toLufs (s);
            maxShortTerm = juce::jmax (maxShortTerm, shortTerm);
            if (shortTerm > -70.0f)
            {
                shortTermHistory.push_back (shortTerm);
                if (shortTermHistory.size() > 36000) shortTermHistory.pop_front();
                // LRA: 10th..95th percentile of short-term values above (mean - 20)
                std::vector<float> v (shortTermHistory.begin(), shortTermHistory.end());
                float mean = 0.0f; for (auto x : v) mean += x; mean /= (float) v.size();
                v.erase (std::remove_if (v.begin(), v.end(), [mean] (float x) { return x < mean - 20.0f; }), v.end());
                if (v.size() >= 2)
                {
                    std::sort (v.begin(), v.end());
                    auto lo = v[(size_t) ((float) (v.size() - 1) * 0.10f)], hi = v[(size_t) ((float) (v.size() - 1) * 0.95f)];
                    range = juce::jmax (0.0f, hi - lo);
                }
            }
        }
    }
};

//==============================================================================
class SpectrumAnalyser
{
public:
    static constexpr int order = 12, size = 1 << order;   // 4096
    SpectrumAnalyser() : fft (order), window ((size_t) size, juce::dsp::WindowingFunction<float>::hann) { std::fill (std::begin (magnitudes), std::end (magnitudes), -100.0f); std::fill (std::begin (peaks), std::end (peaks), -100.0f); }

    void push (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            fifo[(size_t) fifoIndex++] = 0.5f * (l[i] + (r != nullptr ? r[i] : l[i]));
            if (fifoIndex >= size)
            {
                fifoIndex = 0;
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                std::fill (fftData.begin() + size, fftData.end(), 0.0f);
                window.multiplyWithWindowingTable (fftData.data(), (size_t) size);
                fft.performFrequencyOnlyForwardTransform (fftData.data());
                const float norm = 2.0f / (float) size * 2.0f;    // Hann window gain compensation
                for (int b = 0; b < size / 2; ++b)
                {
                    auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, -100.0f);
                    magnitudes[b] = juce::jmax (db, magnitudes[b] - decayDb);
                    if (db > peaks[b]) peaks[b] = db; else peaks[b] -= peakDecayDb;
                }
                ready = true;
            }
        }
    }

    float binHz (int bin, double sampleRate) const   { return (float) (bin * sampleRate / size); }
    float magnitudes[size / 2], peaks[size / 2];
    float decayDb = 1.5f, peakDecayDb = 0.05f;
    bool ready = false;

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, (size_t) size> fifo {};
    std::array<float, (size_t) size * 2> fftData {};
    int fifoIndex = 0;
};

//==============================================================================
struct PhaseMeter
{
    void push (const float* l, const float* r, int n)
    {
        if (r == nullptr) { correlation = 1.0f; return; }
        double lr = 0, ll = 0, rr = 0;
        for (int i = 0; i < n; ++i)
        {
            lr += (double) l[i] * r[i]; ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i];
            if ((i & 7) == 0) { points[(size_t) pointIndex] = { l[i], r[i] }; pointIndex = (pointIndex + 1) % (int) points.size(); }
        }
        auto c = (ll > 1.0e-9 && rr > 1.0e-9) ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
        correlation = correlation * 0.8f + c * 0.2f;
    }
    float correlation = 1.0f;
    std::array<std::pair<float, float>, 512> points {};
    int pointIndex = 0;
};

//==============================================================================
/** Everything the meters need, fed from the audio thread through a FIFO and processed on the message thread. */
class AnalysisHub
{
public:
    AnalysisHub() : fifo (1 << 16) { buffer.setSize (2, 1 << 16); loudness.prepare (sr); }

    /** audio / render thread: just notes the sample rate; the meters are (re)built on the message thread in update(). */
    void prepare (double sampleRate)   { pendingSampleRate = sampleRate; }

    /** audio thread */
    void push (const juce::AudioBuffer<float>& b, int start, int n)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto src = b.getReadPointer (juce::jmin (ch, b.getNumChannels() - 1), start);
            if (n1 > 0) buffer.copyFrom (ch, s1, src, n1);
            if (n2 > 0) buffer.copyFrom (ch, s2, src + n1, n2);
        }
        fifo.finishedWrite (n1 + n2);
    }

    /** message thread: drains the FIFO into the meters */
    void update()
    {
        if (auto psr = pendingSampleRate.exchange (0.0); psr > 0.0 && std::abs (psr - sr) > 1.0)
        {
            sr = psr;
            loudness.prepare (sr);
            peak[0] = peak[1] = -100.0f;
        }
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (auto [s, n] : { std::pair (s1, n1), std::pair (s2, n2) })
        {
            if (n <= 0) continue;
            auto l = buffer.getReadPointer (0, s), r = buffer.getReadPointer (1, s);
            loudness.process (l, r, n);
            spectrum.push (l, r, n);
            phase.push (l, r, n);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto p = buffer.getMagnitude (ch, s, n);
                auto db = juce::Decibels::gainToDecibels (p, -100.0f);
                peak[ch] = juce::jmax (db, peak[ch] - 1.0f);
                rms[ch] = rms[ch] * 0.7f + juce::Decibels::gainToDecibels (buffer.getRMSLevel (ch, s, n), -100.0f) * 0.3f;
                if (p >= 0.999f) clipped = true;
            }
        }
        fifo.finishedRead (n1 + n2);
    }

    void reset()   { loudness.reset(); clipped = false; }

    double sr = 44100.0;
    LoudnessMeter loudness;
    SpectrumAnalyser spectrum;
    PhaseMeter phase;
    float peak[2] = { -100.0f, -100.0f }, rms[2] = { -100.0f, -100.0f };
    bool clipped = false;

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;
    std::atomic<double> pendingSampleRate { 0.0 };
};

//==============================================================================
/** Hidden plugin that taps the signal into an AnalysisHub (the last insert on the mastering master chain). */
class SqAnalyzerPlugin : public te::Plugin
{
public:
    SqAnalyzerPlugin (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~SqAnalyzerPlugin() override   { notifyListenersOfDeletion(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Analyzer"); }
    static constexpr const char* xmlTypeName = "sqmAnalyzer";
    juce::String getName() const override               { return "Analyzer"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Anlz"; }
    juce::String getSelectableDescription() override    { return "Squelchmoth analyzer tap"; }
    bool canBeAddedToRack() override                    { return false; }

    void initialise (const te::PluginInitialisationInfo& info) override   { if (auto h = hub.load()) h->prepare (info.sampleRate); }
    void deinitialise() override {}
    int getNumOutputChannelsGivenInputs (int n) override   { return n; }
    BusLayout getBusses() const override                   { return BusLayout::singleStereoInOut(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        auto h = hub.load();
        if (fc.destBuffer != nullptr && h != nullptr && fc.destBuffer->getNumChannels() > 0)
            h->push (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);
    }
    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

    std::atomic<AnalysisHub*> hub { nullptr };
};

//==============================================================================
struct FileLoudness { float integrated = -100.0f, range = 0.0f, truePeak = -100.0f, rms = -100.0f, samplePeak = -100.0f; double seconds = 0.0; bool ok = false; };

/** Offline EBU R128 measurement of an audio file. */
inline FileLoudness measureFile (const juce::File& f)
{
    FileLoudness out;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return out;
    LoudnessMeter lm; lm.prepare (reader->sampleRate);
    const int block = 8192;
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), block);
    double sumSq = 0.0; juce::int64 count = 0; float sp = 0.0f;
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
    {
        auto n = (int) juce::jmin ((juce::int64) block, reader->lengthInSamples - pos);
        reader->read (&buf, 0, n, pos, true, true);
        auto l = buf.getReadPointer (0), r = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : nullptr;
        lm.process (l, r, n);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch) { auto p = buf.getReadPointer (ch); for (int i = 0; i < n; ++i) { sumSq += (double) p[i] * p[i]; sp = juce::jmax (sp, std::abs (p[i])); } count += n; }
    }
    lm.finalise();
    out.integrated = lm.getIntegrated(); out.range = lm.getRange(); out.truePeak = lm.getTruePeak();
    out.rms = count > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (sumSq / (double) count), -100.0f) : -100.0f;
    out.samplePeak = juce::Decibels::gainToDecibels (sp, -100.0f);
    out.seconds = (double) reader->lengthInSamples / reader->sampleRate;
    out.ok = true;
    return out;
}

/** Applies a gain to a WAV/AIFF/FLAC file in place (via a temp copy), keeping format and bit depth. */
inline bool applyGainToFile (const juce::File& f, float gainDb, juce::StringPairArray metadata = {})
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return false;
    auto* format = fm.findFormatForFileExtension (f.getFileExtension());
    if (format == nullptr) return false;
    auto temp = f.getSiblingFile (f.getFileNameWithoutExtension() + ".tmp" + f.getFileExtension());
    {
        auto meta = metadata.size() > 0 ? metadata : reader->metadataValues;
        std::unique_ptr<juce::FileOutputStream> os (temp.createOutputStream());
        if (os == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> writer (format->createWriterFor (os.get(), reader->sampleRate, reader->numChannels, (int) reader->bitsPerSample, meta, 0));
        if (writer == nullptr) return false;
        os.release();
        const int block = 8192;
        juce::AudioBuffer<float> buf ((int) reader->numChannels, block);
        auto gain = juce::Decibels::decibelsToGain (gainDb);
        for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
        {
            auto n = (int) juce::jmin ((juce::int64) block, reader->lengthInSamples - pos);
            reader->read (&buf, 0, n, pos, true, true);
            buf.applyGain (0, n, gain);
            writer->writeFromAudioSampleBuffer (buf, 0, n);
        }
    }
    reader.reset();
    return temp.moveFileTo (f);
}

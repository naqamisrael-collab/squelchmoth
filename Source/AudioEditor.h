#pragma once
// Squelchmoth 1.8.1 - Cut Lab, the audio editor window: open a WAV or a clip's audio, record from the
// inputs, select and edit (cut / copy / paste / trim / silence / fade / normalise / reverse / gain), process (denoise,
// convolution, blur, time stretch, pitch shift), loop points and regions, save, send to the timeline or drag the file out.
#include "Session.h"
#include "Theme.h"

namespace AudioEditor
{
    struct Doc
    {
        juce::AudioBuffer<float> buf; double sr = 48000.0; juce::File file; bool dirty = false;
        int loopStart = -1, loopEnd = -1;
        struct Region { int start, end; juce::String name; };
        std::vector<Region> regions;
        int length() const   { return buf.getNumSamples(); }
    };

    /** Offline processes. Every one works on a [start, end) range of the buffer in place (or returns a new buffer where the length changes). */
    namespace Ops
    {
        inline void normalise (Doc& d, int a, int b, float peakDb = -0.3f)
        {
            float mx = 0.0f; for (int c = 0; c < d.buf.getNumChannels(); ++c) mx = juce::jmax (mx, d.buf.getMagnitude (c, a, b - a));
            if (mx > 1.0e-6f) d.buf.applyGain (a, b - a, juce::Decibels::decibelsToGain (peakDb) / mx);
        }
        inline void gain (Doc& d, int a, int b, float db)   { d.buf.applyGain (a, b - a, juce::Decibels::decibelsToGain (db)); }
        inline void silence (Doc& d, int a, int b)         { d.buf.clear (a, b - a); }
        inline void fade (Doc& d, int a, int b, bool in)   { for (int c = 0; c < d.buf.getNumChannels(); ++c) d.buf.applyGainRamp (c, a, b - a, in ? 0.0f : 1.0f, in ? 1.0f : 0.0f); }
        inline void reverse (Doc& d, int a, int b)         { for (int c = 0; c < d.buf.getNumChannels(); ++c) d.buf.reverse (c, a, b - a); }
        inline void removeDc (Doc& d, int a, int b)        { for (int c = 0; c < d.buf.getNumChannels(); ++c) { auto* p = d.buf.getWritePointer (c); double m = 0; for (int i = a; i < b; ++i) m += p[i]; m /= juce::jmax (1, b - a); for (int i = a; i < b; ++i) p[i] -= (float) m; } }
        inline juce::AudioBuffer<float> cut (Doc& d, int a, int b)
        {
            juce::AudioBuffer<float> out (d.buf.getNumChannels(), b - a);
            for (int c = 0; c < d.buf.getNumChannels(); ++c) out.copyFrom (c, 0, d.buf, c, a, b - a);
            juce::AudioBuffer<float> rest (d.buf.getNumChannels(), d.length() - (b - a));
            for (int c = 0; c < d.buf.getNumChannels(); ++c) { rest.copyFrom (c, 0, d.buf, c, 0, a); rest.copyFrom (c, a, d.buf, c, b, d.length() - b); }
            d.buf = std::move (rest);
            return out;
        }
        inline void paste (Doc& d, int at, const juce::AudioBuffer<float>& clip)
        {
            if (clip.getNumSamples() == 0) return;
            juce::AudioBuffer<float> out (d.buf.getNumChannels(), d.length() + clip.getNumSamples());
            for (int c = 0; c < d.buf.getNumChannels(); ++c)
            {
                out.copyFrom (c, 0, d.buf, c, 0, at);
                out.copyFrom (c, at, clip, juce::jmin (c, clip.getNumChannels() - 1), 0, clip.getNumSamples());
                out.copyFrom (c, at + clip.getNumSamples(), d.buf, c, at, d.length() - at);
            }
            d.buf = std::move (out);
        }
        inline void trim (Doc& d, int a, int b)
        {
            juce::AudioBuffer<float> out (d.buf.getNumChannels(), b - a);
            for (int c = 0; c < d.buf.getNumChannels(); ++c) out.copyFrom (c, 0, d.buf, c, a, b - a);
            d.buf = std::move (out);
        }
        /** Spectral gate denoise: a noise profile from the quietest frames (or the selection when 'profileFromRange'), then
            every bin below profile * threshold is attenuated by 'reduction' dB. 2048-point frames, 75% overlap, Hann. */
        inline void denoise (Doc& d, int a, int b, float thresholdDb = 6.0f, float reductionDb = -18.0f, int profA = -1, int profB = -1)
        {
            const int N = 2048, hop = N / 4, bins = N / 2 + 1; juce::dsp::FFT fft (11);
            std::vector<float> win ((size_t) N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
            for (int c = 0; c < d.buf.getNumChannels(); ++c)
            {
                auto* p = d.buf.getWritePointer (c);
                // noise profile
                std::vector<float> profile ((size_t) bins, 0.0f);
                {
                    const int pa = profA >= 0 ? profA : a, pb = profB >= 0 ? profB : b;
                    std::vector<std::vector<float>> frames; std::vector<float> energies;
                    std::vector<float> fd ((size_t) N * 2);
                    for (int s = pa; s + N <= pb; s += hop)
                    {
                        std::fill (fd.begin(), fd.end(), 0.0f); float e = 0.0f;
                        for (int i = 0; i < N; ++i) { fd[(size_t) i] = p[s + i] * win[(size_t) i]; e += p[s + i] * p[s + i]; }
                        fft.performFrequencyOnlyForwardTransform (fd.data());
                        frames.emplace_back (fd.begin(), fd.begin() + bins); energies.push_back (e);
                    }
                    if (frames.empty()) continue;
                    std::vector<size_t> order (frames.size()); std::iota (order.begin(), order.end(), 0);
                    std::sort (order.begin(), order.end(), [&] (size_t x, size_t y) { return energies[x] < energies[y]; });
                    const size_t use = profA >= 0 ? frames.size() : juce::jmax ((size_t) 1, frames.size() / 5);   // the quietest fifth of the file is the noise
                    for (size_t k = 0; k < use; ++k) for (int i = 0; i < bins; ++i) profile[(size_t) i] += frames[order[k]][(size_t) i];
                    for (auto& v : profile) v /= (float) use;
                }
                // gate
                std::vector<float> out ((size_t) (b - a), 0.0f), norm ((size_t) (b - a), 0.0f);
                std::vector<float> fd ((size_t) N * 2);
                const float thr = juce::Decibels::decibelsToGain (thresholdDb), red = juce::Decibels::decibelsToGain (reductionDb);
                for (int s = a; s < b; s += hop)
                {
                    std::fill (fd.begin(), fd.end(), 0.0f);
                    for (int i = 0; i < N && s + i < b; ++i) fd[(size_t) i] = p[s + i] * win[(size_t) i];
                    fft.performRealOnlyForwardTransform (fd.data());
                    for (int i = 0; i < bins; ++i)
                    {
                        float& re = fd[(size_t) (2 * i)]; float& im = fd[(size_t) (2 * i + 1)];
                        const float mag = std::sqrt (re * re + im * im);
                        const float g = mag < profile[(size_t) i] * thr ? red : juce::jlimit (red, 1.0f, 1.0f - (profile[(size_t) i] * thr) / juce::jmax (1.0e-9f, mag) * (1.0f - red));
                        re *= g; im *= g;
                    }
                    fft.performRealOnlyInverseTransform (fd.data());
                    for (int i = 0; i < N && s + i < b; ++i) { out[(size_t) (s + i - a)] += fd[(size_t) i] * win[(size_t) i]; norm[(size_t) (s + i - a)] += win[(size_t) i] * win[(size_t) i]; }
                }
                for (int i = a; i < b; ++i) p[i] = norm[(size_t) (i - a)] > 1.0e-6f ? out[(size_t) (i - a)] / norm[(size_t) (i - a)] : p[i];
            }
        }
        /** Blur: granular smear - the range is rebuilt from short windowed grains scattered in time (amount = spread in seconds). */
        inline void blur (Doc& d, int a, int b, float amountSec = 0.08f, float mix = 1.0f)
        {
            const int grain = juce::jmax (64, (int) (d.sr * 0.03)), hop = grain / 4, spread = (int) (amountSec * d.sr);
            juce::Random rnd (7);
            for (int c = 0; c < d.buf.getNumChannels(); ++c)
            {
                auto* p = d.buf.getWritePointer (c); std::vector<float> out ((size_t) (b - a), 0.0f), norm ((size_t) (b - a), 0.0f);
                for (int s = a; s < b; s += hop)
                {
                    const int src = juce::jlimit (a, juce::jmax (a, b - grain), s + rnd.nextInt (juce::jmax (1, spread * 2)) - spread);
                    for (int i = 0; i < grain && s + i < b; ++i)
                    {
                        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) grain);
                        out[(size_t) (s + i - a)] += p[juce::jmin (b - 1, src + i)] * w; norm[(size_t) (s + i - a)] += w;
                    }
                }
                for (int i = a; i < b; ++i) { const float wet = norm[(size_t) (i - a)] > 1.0e-6f ? out[(size_t) (i - a)] / norm[(size_t) (i - a)] : p[i]; p[i] = p[i] + (wet - p[i]) * mix; }
            }
        }
        /** Convolution with an impulse response file (FFT, whole-range, normalised so the dry peak is kept). Returns false if the IR can't be read. */
        inline bool convolve (Doc& d, int a, int b, const juce::File& irFile, float mix = 1.0f)
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (irFile)); if (rd == nullptr || rd->lengthInSamples <= 0) return false;
            juce::AudioBuffer<float> ir ((int) rd->numChannels, (int) juce::jmin ((juce::int64) d.sr * 10, rd->lengthInSamples)); rd->read (&ir, 0, ir.getNumSamples(), 0, true, true);
            const int n = b - a, m = ir.getNumSamples(); const int order = (int) std::ceil (std::log2 ((double) (n + m))); juce::dsp::FFT fft (order); const int L = 1 << order;
            std::vector<float> x ((size_t) L * 2), h ((size_t) L * 2);
            for (int c = 0; c < d.buf.getNumChannels(); ++c)
            {
                auto* p = d.buf.getWritePointer (c);
                const float before = d.buf.getMagnitude (c, a, n);
                std::fill (x.begin(), x.end(), 0.0f); std::fill (h.begin(), h.end(), 0.0f);
                for (int i = 0; i < n; ++i) x[(size_t) i] = p[a + i];
                for (int i = 0; i < m; ++i) h[(size_t) i] = ir.getSample (juce::jmin (c, ir.getNumChannels() - 1), i);
                fft.performRealOnlyForwardTransform (x.data()); fft.performRealOnlyForwardTransform (h.data());
                for (int i = 0; i <= L / 2; ++i) { const float xr = x[(size_t) (2 * i)], xi = x[(size_t) (2 * i + 1)], hr = h[(size_t) (2 * i)], hi = h[(size_t) (2 * i + 1)]; x[(size_t) (2 * i)] = xr * hr - xi * hi; x[(size_t) (2 * i + 1)] = xr * hi + xi * hr; }
                fft.performRealOnlyInverseTransform (x.data());
                float after = 0.0f; for (int i = 0; i < n; ++i) after = juce::jmax (after, std::abs (x[(size_t) i]));
                const float g = after > 1.0e-6f ? before / after : 1.0f;
                for (int i = 0; i < n; ++i) p[a + i] = p[a + i] + (x[(size_t) i] * g - p[a + i]) * mix;
            }
            return true;
        }
        /** Time stretch / pitch shift through the engine's stretcher (offline). ratio 2 = twice as long. */
        inline bool stretch (te::Engine& engine, Doc& d, int a, int b, float ratio, float semitones)
        {
            auto mode = te::TimeStretcher::checkModeIsAvailable (te::TimeStretcher::defaultMode);
            if (! te::TimeStretcher::canProcessFor (mode)) return false;
            const int nch = d.buf.getNumChannels(), n = b - a, block = 1024;
            te::TimeStretcher ts; ts.initialise (d.sr, block, nch, mode, {}, false);
            ts.setSpeedAndPitch (ratio, semitones);
            juce::AudioBuffer<float> in (nch, n); for (int c = 0; c < nch; ++c) in.copyFrom (c, 0, d.buf, c, a, n);
            juce::AudioBuffer<float> out (nch, (int) (n * ratio) + block * 4); int written = 0, readPos = 0;
            juce::AudioBuffer<float> tmp (nch, block);
            while (readPos < n)
            {
                const int need = juce::jmin (ts.getFramesNeeded(), n - readPos); if (need <= 0) break;
                const float* ins[8]; for (int c = 0; c < nch; ++c) ins[c] = in.getReadPointer (c, readPos);
                float* outs[8]; for (int c = 0; c < nch; ++c) outs[c] = tmp.getWritePointer (c);
                const int got = ts.processData (ins, need, outs);
                readPos += need;
                if (got > 0 && written + got <= out.getNumSamples()) { for (int c = 0; c < nch; ++c) out.copyFrom (c, written, tmp, c, 0, got); written += got; }
            }
            for (int k = 0; k < 8; ++k) { float* outs[8]; for (int c = 0; c < nch; ++c) outs[c] = tmp.getWritePointer (c); const int got = ts.flush (outs); if (got <= 0) break; if (written + got <= out.getNumSamples()) { for (int c = 0; c < nch; ++c) out.copyFrom (c, written, tmp, c, 0, got); written += got; } }
            juce::AudioBuffer<float> res (nch, written); for (int c = 0; c < nch; ++c) res.copyFrom (c, 0, out, c, 0, written);
            juce::AudioBuffer<float> rest (nch, d.length() - n + written);
            for (int c = 0; c < nch; ++c) { rest.copyFrom (c, 0, d.buf, c, 0, a); rest.copyFrom (c, a, res, c, 0, written); rest.copyFrom (c, a + written, d.buf, c, b, d.length() - b); }
            d.buf = std::move (rest);
            return true;
        }
        inline bool save (const Doc& d, const juce::File& f, int bits = 24)
        {
            juce::WavAudioFormat wav; juce::StringPairArray meta;
            if (d.loopEnd > d.loopStart && d.loopStart >= 0) { meta.set ("NumSampleLoops", "1"); meta.set ("Loop0Start", juce::String (d.loopStart)); meta.set ("Loop0End", juce::String (d.loopEnd)); meta.set ("Loop0Type", "0"); meta.set ("Loop0Identifier", "0"); meta.set ("Loop0Fraction", "0"); meta.set ("Loop0PlayCount", "0"); meta.set ("MidiUnityNote", "60"); meta.set ("MidiPitchFraction", "0"); meta.set ("Manufacturer", "0"); meta.set ("Product", "0"); meta.set ("SamplePeriod", juce::String ((int) (1.0e9 / d.sr))); meta.set ("SmpteFormat", "0"); meta.set ("SmpteOffset", "0"); }
            if (! d.regions.empty()) { meta.set ("NumCuePoints", juce::String ((int) d.regions.size())); for (size_t i = 0; i < d.regions.size(); ++i) { auto k = "Cue" + juce::String ((int) i); meta.set (k + "Identifier", juce::String ((int) i + 1)); meta.set (k + "Order", juce::String ((int) i)); meta.set (k + "ChunkID", "data"); meta.set (k + "ChunkStart", "0"); meta.set (k + "BlockStart", "0"); meta.set (k + "Offset", juce::String (d.regions[i].start)); } }
            f.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), d.sr, (unsigned) d.buf.getNumChannels(), bits, meta, 0)))
                    return writer->writeFromAudioSampleBuffer (d.buf, 0, d.length());
            return false;
        }
        inline bool load (Doc& d, const juce::File& f)
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f)); if (rd == nullptr || rd->lengthInSamples <= 0) return false;
            d.buf.setSize ((int) juce::jmax (1u, rd->numChannels), (int) rd->lengthInSamples); rd->read (&d.buf, 0, (int) rd->lengthInSamples, 0, true, true);
            d.sr = rd->sampleRate; d.file = f; d.dirty = false; d.regions.clear(); d.loopStart = d.loopEnd = -1;
            if (rd->metadataValues.containsKey ("NumSampleLoops") && rd->metadataValues["NumSampleLoops"].getIntValue() > 0) { d.loopStart = rd->metadataValues["Loop0Start"].getIntValue(); d.loopEnd = rd->metadataValues["Loop0End"].getIntValue(); }
            const int cues = rd->metadataValues["NumCuePoints"].getIntValue();
            for (int i = 0; i < cues; ++i) { const int off = rd->metadataValues["Cue" + juce::String (i) + "Offset"].getIntValue(); d.regions.push_back ({ off, off, "Cue " + juce::String (i + 1) }); }
            return true;
        }
    }

    //==========================================================================
    class Window : public juce::DocumentWindow, private juce::Timer, private juce::AudioIODeviceCallback
    {
    public:
        Window (Session& s, const juce::File& fileToOpen = {})
            : DocumentWindow ("Cut Lab", Theme::bg(), DocumentWindow::allButtons), session (s), content (*this)
        {
            setUsingNativeTitleBar (true); setContentNonOwned (&content, false); setResizable (true, true);
            centreWithSize (1280, 720); setVisible (true);
            if (fileToOpen.existsAsFile()) open (fileToOpen);
            startTimerHz (30);
        }
        ~Window() override   { stopRecording(); stopPlayback(); }
        std::function<void()> onClose;
        std::function<te::AudioTrack*()> targetTrack;   // the Edit window's selected track (set by the app)
        void closeButtonPressed() override   { if (onClose) onClose(); }

        Doc doc; juce::AudioBuffer<float> clipboard; int selA = 0, selB = 0; double viewStart = 0, viewLen = 0; int cursor = 0;
        std::vector<Doc> undo; juce::String status;

        bool open (const juce::File& f)
        {
            Doc d; if (! Ops::load (d, f)) { status = "Could not read " + f.getFileName(); return false; }
            doc = std::move (d); selA = selB = 0; viewStart = 0; viewLen = doc.length(); cursor = 0; undo.clear();
            setName ("Cut Lab - " + f.getFileName()); status = f.getFileName() + "  " + juce::String (doc.length() / doc.sr, 2) + " s  " + juce::String (doc.buf.getNumChannels()) + " ch  " + juce::String ((int) doc.sr) + " Hz";
            content.repaint(); return true;
        }
        void snapshot()   { undo.push_back (doc); if (undo.size() > 20) undo.erase (undo.begin()); doc.dirty = true; }
        void undoLast()   { if (! undo.empty()) { doc = undo.back(); undo.pop_back(); clampSel(); content.repaint(); } }
        void clampSel()   { selA = juce::jlimit (0, doc.length(), selA); selB = juce::jlimit (0, doc.length(), selB); if (selB < selA) std::swap (selA, selB); viewLen = juce::jmin ((double) doc.length(), juce::jmax (viewLen, 64.0)); viewStart = juce::jlimit (0.0, juce::jmax (0.0, (double) doc.length() - viewLen), viewStart); }
        /** The range an operation works on: the selection, or everything. */
        void range (int& a, int& b) const   { if (selB > selA) { a = selA; b = selB; } else { a = 0; b = doc.length(); } }
        juce::File saveTarget()
        {
            if (doc.file.existsAsFile() && doc.file.hasFileExtension ("wav")) return doc.file;
            auto dir = session.sessionsDir.getChildFile ("Edited"); dir.createDirectory();
            return dir.getNonexistentChildFile (doc.file.existsAsFile() ? doc.file.getFileNameWithoutExtension() : "Recording", ".wav");
        }
        bool save (bool as = false)
        {
            if (doc.length() == 0) return false;
            auto f = as ? session.sessionsDir.getChildFile ("Edited").getNonexistentChildFile (doc.file.existsAsFile() ? doc.file.getFileNameWithoutExtension() + " edit" : "Recording", ".wav") : saveTarget();
            f.getParentDirectory().createDirectory();
            if (! Ops::save (doc, f)) { status = "Save failed"; return false; }
            doc.file = f; doc.dirty = false; status = "Saved " + f.getFullPathName(); setName ("Cut Lab - " + f.getFileName()); return true;
        }
        /** Saves and places the file on the selected track (or a new one) at the edit cursor. */
        void sendToTimeline()
        {
            if (! save()) return;
            auto& edit = *session.edit; te::AudioTrack* t = nullptr;
            if (targetTrack) t = targetTrack();
            if (t == nullptr) t = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, te::getAllTracks (edit).getLast()), nullptr).get();
            if (t == nullptr) return;
            juce::ignoreUnused (juce::AudioFormatManager());
            const double len = doc.length() / doc.sr; auto pos = edit.getTransport().getPosition();
            if (auto c = t->insertWaveClip (doc.file.getFileNameWithoutExtension(), doc.file, { { pos, te::TimeDuration::fromSeconds (len) }, {} }, false)) status = "Placed on " + t->getName() + " at " + juce::String (pos.inSeconds(), 2) + " s";
        }

        //==========================================================================
        // playback (through the device directly, so the editor works without touching the session's transport)
        void play (bool loopSel = false)
        {
            stopPlayback(); if (doc.length() == 0) return;
            int a, b; range (a, b); playPos = loopSel ? a : (cursor >= a && cursor < b ? cursor : a); playEnd = b; playStart = a; playLoop = loopSel; playing = true;
            session.getEngine().getDeviceManager().deviceManager.addAudioCallback (this);
        }
        void stopPlayback()   { if (playing || recording) session.getEngine().getDeviceManager().deviceManager.removeAudioCallback (this); playing = false; }
        void startRecording()
        {
            stopPlayback(); recBuf.setSize (2, (int) (session.getEngine().getDeviceManager().getSampleRate() * 600)); recLen = 0; recording = true;
            session.getEngine().getDeviceManager().deviceManager.addAudioCallback (this);
            status = "Recording... press Record again to stop";
        }
        void stopRecording()
        {
            if (! recording) return;
            session.getEngine().getDeviceManager().deviceManager.removeAudioCallback (this); recording = false;
            if (recLen > 64)
            {
                snapshot();
                doc.buf.setSize (2, recLen); for (int c = 0; c < 2; ++c) doc.buf.copyFrom (c, 0, recBuf, c, 0, recLen);
                doc.sr = session.getEngine().getDeviceManager().getSampleRate(); doc.file = juce::File(); doc.regions.clear(); doc.loopStart = doc.loopEnd = -1;
                selA = selB = 0; viewStart = 0; viewLen = doc.length(); status = "Recorded " + juce::String (recLen / doc.sr, 2) + " s"; setName ("Cut Lab - recording");
            }
            else status = "Nothing recorded (no input signal?)";
            content.repaint();
        }
        void audioDeviceIOCallbackWithContext (const float* const* in, int nIn, float* const* out, int nOut, int n, const juce::AudioIODeviceCallbackContext&) override
        {
            if (recording)
            {
                for (int i = 0; i < n && recLen + i < recBuf.getNumSamples(); ++i)
                    for (int c = 0; c < 2; ++c) recBuf.setSample (c, recLen + i, nIn > 0 ? in[juce::jmin (c, nIn - 1)][i] : 0.0f);
                recLen = juce::jmin (recBuf.getNumSamples(), recLen + n);
                { float pk = 0.0f; for (int c = 0; c < juce::jmin (2, nIn); ++c) for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (in[c][i])); meterDb = juce::Decibels::gainToDecibels (pk, -100.0f); }
                for (int c = 0; c < nOut; ++c) if (out[c]) juce::FloatVectorOperations::clear (out[c], n);
                return;
            }
            if (! playing) { meterDb = -100.0f; return; }
            const double ratio = doc.sr / devRate; float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                if (playPos >= playEnd) { if (playLoop) playPos = playStart; else { playing = false; for (int c = 0; c < nOut; ++c) if (out[c]) juce::FloatVectorOperations::clear (out[c] + i, n - i); return; } }
                const int i0 = (int) playPos; const float fr = (float) (playPos - i0); const int i1 = juce::jmin (doc.length() - 1, i0 + 1);
                for (int c = 0; c < nOut; ++c) if (out[c]) { const int sc = juce::jmin (c, doc.buf.getNumChannels() - 1); out[c][i] = (doc.buf.getSample (sc, i0) + (doc.buf.getSample (sc, i1) - doc.buf.getSample (sc, i0)) * fr) * 0.9f; pk = juce::jmax (pk, std::abs (out[c][i])); }
                playPos += ratio;
            }
            meterDb = juce::Decibels::gainToDecibels (pk, -100.0f);
        }
        void audioDeviceAboutToStart (juce::AudioIODevice* d) override   { devRate = d != nullptr ? d->getCurrentSampleRate() : 48000.0; }
        void audioDeviceStopped() override {}
        void timerCallback() override   { if (playing || recording || meterDb.load() > -99.0f) content.repaint(); if (! playing && ! recording) meterDb = -100.0f; recBtnState(); }
        void recBtnState()   { content.recBtn.setToggleState (recording, juce::dontSendNotification); }

        //==========================================================================
        struct Content : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropContainer
        {
            Window& w; juce::TextButton openBtn, saveBtn, saveAsBtn, recBtn, playBtn, loopBtn, stopBtn, sendBtn, undoBtn;
            juce::TextButton cutBtn, copyBtn, pasteBtn, trimBtn, silBtn, fadeInBtn, fadeOutBtn, normBtn, revBtn, gainBtn, dcBtn, denoiseBtn, blurBtn, convBtn, stretchBtn, pitchBtn, loopPtsBtn, regionBtn, zoomInBtn, zoomOutBtn, zoomSelBtn;
            juce::Rectangle<int> waveArea; bool dragging = false; int dragAnchor = 0;
            juce::Image art; juce::Colour accent { 0xffb8ff3a };
            struct Group { juce::String title; juce::Rectangle<int> bounds; std::vector<juce::TextButton*> items; };
            std::vector<Group> groups; juce::Rectangle<int> overviewArea, rulerArea, meterArea;
            explicit Content (Window& win) : w (win)
            {
                int size = 0;
                if (auto data = BinaryData::getNamedResource ("pa_cutlab_jpg", size)) art = juce::ImageFileFormat::loadFrom (data, (size_t) size);
                auto mk = [this] (juce::TextButton& b, const char* t, std::function<void()> f, const char* tip = nullptr) { b.setButtonText (t); b.onClick = std::move (f); if (tip) b.setTooltip (tip); addAndMakeVisible (b); };
                mk (openBtn, "OPEN...", [this] { chooseFile(); });
                mk (saveBtn, "SAVE", [this] { w.save (false); repaint(); }); mk (saveAsBtn, "SAVE AS", [this] { w.save (true); repaint(); });
                mk (recBtn, "RECORD", [this] { if (w.recording) w.stopRecording(); else w.startRecording(); repaint(); }, "Records the input of the audio device into the editor (press again to stop)");
                mk (playBtn, "PLAY", [this] { w.play (false); }); mk (loopBtn, "LOOP SEL", [this] { w.play (true); }); mk (stopBtn, "STOP", [this] { w.stopPlayback(); repaint(); });
                mk (sendBtn, "SEND TO TIMELINE", [this] { w.sendToTimeline(); repaint(); }, "Saves and places the audio on the selected track at the play cursor (or drag the waveform onto the timeline)");
                mk (undoBtn, "UNDO", [this] { w.undoLast(); });
                mk (cutBtn, "CUT", [this] { op ([this] { int a, b; w.range (a, b); w.clipboard = Ops::cut (w.doc, a, b); w.selB = w.selA = a; }); });
                mk (copyBtn, "COPY", [this] { int a, b; w.range (a, b); w.clipboard.setSize (w.doc.buf.getNumChannels(), b - a); for (int c = 0; c < w.doc.buf.getNumChannels(); ++c) w.clipboard.copyFrom (c, 0, w.doc.buf, c, a, b - a); w.status = "Copied " + juce::String ((b - a) / w.doc.sr, 3) + " s"; repaint(); });
                mk (pasteBtn, "PASTE", [this] { op ([this] { Ops::paste (w.doc, w.selA, w.clipboard); w.selB = w.selA + w.clipboard.getNumSamples(); }); });
                mk (trimBtn, "TRIM", [this] { op ([this] { int a, b; w.range (a, b); Ops::trim (w.doc, a, b); w.selA = 0; w.selB = w.doc.length(); w.viewStart = 0; w.viewLen = w.doc.length(); }); });
                mk (silBtn, "SILENCE", [this] { op ([this] { int a, b; w.range (a, b); Ops::silence (w.doc, a, b); }); });
                mk (fadeInBtn, "FADE IN", [this] { op ([this] { int a, b; w.range (a, b); Ops::fade (w.doc, a, b, true); }); }); mk (fadeOutBtn, "FADE OUT", [this] { op ([this] { int a, b; w.range (a, b); Ops::fade (w.doc, a, b, false); }); });
                mk (normBtn, "NORMALISE", [this] { op ([this] { int a, b; w.range (a, b); Ops::normalise (w.doc, a, b); }); });
                mk (revBtn, "REVERSE", [this] { op ([this] { int a, b; w.range (a, b); Ops::reverse (w.doc, a, b); }); });
                mk (gainBtn, "GAIN...", [this] { askNumber ("Gain", "dB (negative to lower)", -6.0, [this] (double v) { op ([this, v] { int a, b; w.range (a, b); Ops::gain (w.doc, a, b, (float) v); }); }); });
                mk (dcBtn, "DC OFFSET", [this] { op ([this] { int a, b; w.range (a, b); Ops::removeDc (w.doc, a, b); }); });
                mk (denoiseBtn, "DENOISE...", [this] { askNumber ("Denoise", "reduction dB (e.g. -18); the quietest fifth of the file is the noise profile, or select the noise first and the selection becomes the profile", -18.0, [this] (double v) { op ([this, v] { const bool selIsProfile = w.selB > w.selA && (w.selB - w.selA) < w.doc.length() / 2; if (selIsProfile) Ops::denoise (w.doc, 0, w.doc.length(), 6.0f, (float) v, w.selA, w.selB); else { int a, b; w.range (a, b); Ops::denoise (w.doc, a, b, 6.0f, (float) v); } }); }); });
                mk (blurBtn, "BLUR...", [this] { askNumber ("Blur", "spread in seconds (0.02 - 0.5)", 0.08, [this] (double v) { op ([this, v] { int a, b; w.range (a, b); Ops::blur (w.doc, a, b, (float) juce::jlimit (0.005, 1.0, v)); }); }); });
                mk (convBtn, "CONVOLVE...", [this] { chooseIr(); });
                mk (stretchBtn, "TIME...", [this] { askNumber ("Time stretch", "new length as a percentage (200 = twice as long, 50 = half)", 100.0, [this] (double v) { op ([this, v] { int a, b; w.range (a, b); if (! Ops::stretch (w.session.getEngine(), w.doc, a, b, (float) juce::jlimit (0.1, 10.0, v / 100.0), 0.0f)) w.status = "No time stretcher available"; w.selB = w.selA; }); }); });
                mk (pitchBtn, "PITCH...", [this] { askNumber ("Pitch shift", "semitones (-24 .. 24), length kept", 0.0, [this] (double v) { op ([this, v] { int a, b; w.range (a, b); if (! Ops::stretch (w.session.getEngine(), w.doc, a, b, 1.0f, (float) juce::jlimit (-24.0, 24.0, v))) w.status = "No time stretcher available"; }); }); });
                mk (loopPtsBtn, "LOOP = SEL", [this] { if (w.selB > w.selA) { w.doc.loopStart = w.selA; w.doc.loopEnd = w.selB; w.doc.dirty = true; w.status = "Loop points set (saved into the WAV's smpl chunk)"; } else { w.doc.loopStart = w.doc.loopEnd = -1; w.status = "Loop points cleared"; } repaint(); });
                mk (regionBtn, "+ REGION", [this] { if (w.selB > w.selA) { w.doc.regions.push_back ({ w.selA, w.selB, "Region " + juce::String ((int) w.doc.regions.size() + 1) }); w.doc.dirty = true; repaint(); } });
                mk (zoomInBtn, "+", [this] { zoom (0.5); }); mk (zoomOutBtn, "-", [this] { zoom (2.0); }); mk (zoomSelBtn, "ZOOM SEL", [this] { if (w.selB > w.selA) { w.viewStart = w.selA; w.viewLen = w.selB - w.selA; w.clampSel(); repaint(); } });
                groups = { { "FILE", {}, { &openBtn, &saveBtn, &saveAsBtn, &sendBtn } }, { "TRANSPORT", {}, { &recBtn, &playBtn, &loopBtn, &stopBtn } }, { "EDIT", {}, { &undoBtn, &cutBtn, &copyBtn, &pasteBtn, &trimBtn, &silBtn, &gainBtn, &normBtn, &fadeInBtn, &fadeOutBtn, &revBtn, &dcBtn } },
                           { "PROCESS", {}, { &denoiseBtn, &blurBtn, &convBtn, &stretchBtn, &pitchBtn } }, { "MARKERS + VIEW", {}, { &loopPtsBtn, &regionBtn, &zoomSelBtn, &zoomInBtn, &zoomOutBtn } } };
                sendBtn.setButtonText ("TO TIMELINE"); loopBtn.setButtonText ("LOOP"); stretchBtn.setButtonText ("TIME..."); pitchBtn.setButtonText ("PITCH..."); loopPtsBtn.setButtonText ("LOOP PTS"); regionBtn.setButtonText ("REGION"); zoomSelBtn.setButtonText ("ZOOM SEL"); dcBtn.setButtonText ("DC");
                recBtn.setColour (juce::TextButton::buttonOnColourId, Theme::red());
                setWantsKeyboardFocus (true);
            }
            void op (std::function<void()> f)   { if (w.doc.length() == 0) return; w.snapshot(); f(); w.clampSel(); repaint(); }
            void zoom (double factor)   { const double c = w.viewStart + w.viewLen * 0.5; w.viewLen = juce::jlimit (64.0, (double) juce::jmax (64, w.doc.length()), w.viewLen * factor); w.viewStart = c - w.viewLen * 0.5; w.clampSel(); repaint(); }
            void askNumber (const juce::String& title, const juce::String& prompt, double def, std::function<void (double)> then)
            {
                auto aw = std::make_shared<juce::AlertWindow> (title, prompt, juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("v", juce::String (def)); aw->addButton ("OK", 1); aw->addButton ("Cancel", 0);
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, then] (int r) { if (r == 1) then (aw->getTextEditorContents ("v").getDoubleValue()); }), false);
            }
            void chooseFile()
            {
                auto fc = std::make_shared<juce::FileChooser> ("Open audio", w.session.sessionsDir, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
                fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) w.open (c.getResult()); });
            }
            void chooseIr()
            {
                auto fc = std::make_shared<juce::FileChooser> ("Impulse response", w.session.sessionsDir, "*.wav;*.aif;*.aiff;*.flac");
                fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c) { auto f = c.getResult(); if (f.existsAsFile()) op ([this, f] { int a, b; w.range (a, b); if (! Ops::convolve (w.doc, a, b, f)) w.status = "Could not read the impulse response"; }); });
            }
            bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true; return false; }
            void filesDropped (const juce::StringArray& files, int, int) override   { for (auto& f : files) if (juce::File (f).existsAsFile()) { w.open (juce::File (f)); return; } }
            static constexpr int headerH = 62;
            void resized() override
            {
                auto r = getLocalBounds(); r.removeFromTop (headerH); r = r.reduced (10, 8);
                // button groups along the top: every button as wide as its label, groups flow into a second row when the
                // window is too narrow for one (no label is ever cut short)
                const juce::Font bf (juce::FontOptions (12.0f, juce::Font::bold));
                auto btnW = [&] (juce::Button* bt) { return juce::jmax (40, juce::GlyphArrangement::getStringWidthInt (bf, bt->getButtonText()) + 22); };
                auto groupW = [&] (const Group& gp) { int wsum = 12; for (auto* bt : gp.items) wsum += btnW (bt); return wsum; };
                const int gap = 8, rowH = 54;
                std::vector<std::vector<Group*>> rows (1); int used = 0;
                for (auto& gp : groups)
                {
                    const int gw = groupW (gp);
                    if (! rows.back().empty() && used + gap + gw > r.getWidth()) { rows.emplace_back(); used = 0; }
                    used += (rows.back().empty() ? 0 : gap) + gw; rows.back().push_back (&gp);
                }
                for (auto& row : rows)
                {
                    auto bar = r.removeFromTop (rowH); r.removeFromTop (6);
                    int natural = 0; for (auto* gp : row) natural += groupW (*gp);
                    const float stretch = (float) (bar.getWidth() - gap * ((int) row.size() - 1)) / (float) juce::jmax (1, natural);   // share the spare width
                    int x = bar.getX();
                    for (auto* gp : row)
                    {
                        const int wdt = (int) ((float) groupW (*gp) * stretch);
                        gp->bounds = juce::Rectangle<int> (x, bar.getY(), wdt, bar.getHeight()); x += wdt + gap;
                        auto inner = gp->bounds.reduced (6, 4).withTrimmedTop (14);
                        int natInner = 0; for (auto* bt : gp->items) natInner += btnW (bt);
                        const float k = (float) inner.getWidth() / (float) juce::jmax (1, natInner);
                        for (auto* bt : gp->items) bt->setBounds (inner.removeFromLeft ((int) ((float) btnW (bt) * k)).reduced (2, 2));
                    }
                }
                r.removeFromTop (2);
                r.removeFromBottom (20);   // status strip
                meterArea = r.removeFromRight (22).reduced (0, 4); r.removeFromRight (6);
                overviewArea = r.removeFromBottom (44).reduced (0, 2); r.removeFromBottom (6);
                rulerArea = r.removeFromTop (18);
                waveArea = r;
            }
            float xFor (double s) const   { return waveArea.getX() + (float) ((s - w.viewStart) / juce::jmax (1.0, w.viewLen) * waveArea.getWidth()); }
            double sampleFor (float x) const   { return w.viewStart + (double) (x - waveArea.getX()) / waveArea.getWidth() * w.viewLen; }
            void paint (juce::Graphics& g) override
            {
                auto full = getLocalBounds();
                g.fillAll (Theme::bg());
                if (art.isValid())
                {
                    const float scale = juce::jmax ((float) getWidth() / (float) art.getWidth(), (float) getHeight() / (float) art.getHeight());
                    const float aw = art.getWidth() * scale, ah = art.getHeight() * scale;
                    g.drawImage (art, juce::Rectangle<float> ((getWidth() - aw) * 0.5f, (getHeight() - ah) * 0.5f, aw, ah));
                }
                g.setGradientFill (juce::ColourGradient (Theme::bg().withAlpha (0.12f), 0, 0, Theme::bg().withAlpha (0.70f), 0, (float) getHeight(), false)); g.fillRect (full);
                g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.14f), 0, 0, accent.withAlpha (0.0f), (float) getWidth() * 0.6f, (float) getHeight() * 0.7f, true)); g.fillRect (full);
                // header band, like every Squelchmoth face
                auto header = full.removeFromTop (headerH);
                g.setColour (Theme::bg().withAlpha (0.55f)); g.fillRect (header);
                g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.30f), 0, 0, accent.withAlpha (0.0f), (float) getWidth() * 0.55f, 0, false)); g.fillRect (header);
                g.setColour (accent); g.fillRect (header.removeFromLeft (5));
                g.setColour (Theme::text()); g.setFont (Theme::font (20.0f, true)); g.drawText ("CUT LAB", 16, 7, getWidth() - 360, 26, juce::Justification::centredLeft);
                g.setColour (Theme::dim()); g.setFont (Theme::font (10.5f));
                g.drawFittedText ("The audio editor: record, cut, trim, fade, normalise, reverse, denoise, blur, convolve, time, pitch, loop points, regions - then send it to the timeline or drag it out", 16, 32, getWidth() - 360, 14, juce::Justification::centredLeft, 1);
                g.setColour (accent.withAlpha (0.95f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("SQUELCHMOTH SIGNATURE  |  Audio Editor", 16, 46, getWidth() - 360, 11, juce::Justification::centredLeft);
                auto& d = w.doc; const int len = d.length();
                { // file badge on the right of the header
                    g.setColour (Theme::text()); g.setFont (Theme::font (11.0f, true));
                    g.drawText (d.file.existsAsFile() ? d.file.getFileName() : (len > 0 ? "recording (unsaved)" : "no audio"), getWidth() - 340, 10, 320, 18, juce::Justification::centredRight);
                    g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f));
                    g.drawText (len > 0 ? juce::String (len / d.sr, 3) + " s   " + juce::String (d.buf.getNumChannels()) + " ch   " + juce::String ((int) d.sr) + " Hz   " + juce::String (d.regions.size()) + " regions   " + (d.loopEnd > d.loopStart ? "loop set" : "no loop") : "open a file, drop one here, or record", getWidth() - 340, 30, 320, 14, juce::Justification::centredRight);
                }
                // groups
                for (auto& gp : groups)
                {
                    auto b = gp.bounds.toFloat();
                    g.setColour (Theme::panel().withAlpha (0.55f)); g.fillRoundedRectangle (b, 6.0f);
                    g.setColour (Theme::line().withAlpha (0.5f)); g.drawRoundedRectangle (b, 6.0f, 1.0f);
                    g.setColour (accent); g.setFont (Theme::font (8.5f, true)); g.drawText (gp.title, gp.bounds.reduced (8, 3), juce::Justification::topLeft);
                }
                // waveform panel
                auto panel = rulerArea.getUnion (waveArea).getUnion (overviewArea).expanded (6, 6).toFloat();
                g.setColour (Theme::panel().withAlpha (0.55f)); g.fillRoundedRectangle (panel, 6.0f); g.setColour (Theme::line().withAlpha (0.5f)); g.drawRoundedRectangle (panel, 6.0f, 1.0f);
                auto a = waveArea.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (a, 3.0f);
                g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (overviewArea.toFloat(), 3.0f);
                if (len > 0)
                {
                    const int nch = d.buf.getNumChannels(); const float chH = a.getHeight() / (float) nch;
                    // dB grid
                    g.setColour (Theme::text().withAlpha (0.05f));
                    for (int c = 0; c < nch; ++c) for (float db : { -6.0f, -12.0f, -24.0f }) { const float amp = juce::Decibels::decibelsToGain (db); const float mid = a.getY() + chH * ((float) c + 0.5f), half = chH * 0.46f; g.drawHorizontalLine ((int) (mid - amp * half), a.getX(), a.getRight()); g.drawHorizontalLine ((int) (mid + amp * half), a.getX(), a.getRight()); }
                    if (w.selB > w.selA) { g.setColour (accent.withAlpha (0.14f)); g.fillRect (juce::Rectangle<float> (xFor (w.selA), a.getY(), xFor (w.selB) - xFor (w.selA), a.getHeight())); g.setColour (accent.withAlpha (0.6f)); g.drawVerticalLine ((int) xFor (w.selA), a.getY(), a.getBottom()); g.drawVerticalLine ((int) xFor (w.selB), a.getY(), a.getBottom()); }
                    if (d.loopEnd > d.loopStart) { g.setColour (Theme::amber().withAlpha (0.10f)); g.fillRect (juce::Rectangle<float> (xFor (d.loopStart), a.getY(), xFor (d.loopEnd) - xFor (d.loopStart), a.getHeight())); }
                    for (auto& rg : d.regions) { g.setColour (Theme::mint().withAlpha (0.12f)); g.fillRect (juce::Rectangle<float> (xFor (rg.start), a.getY(), juce::jmax (2.0f, xFor (rg.end) - xFor (rg.start)), 14.0f)); g.setColour (Theme::mint()); g.setFont (Theme::font (8.0f, true)); g.drawText (rg.name, (int) xFor (rg.start) + 4, (int) a.getY(), 120, 14, juce::Justification::centredLeft); }
                    for (int c = 0; c < nch; ++c)
                    {
                        const float mid = a.getY() + chH * ((float) c + 0.5f), half = chH * 0.46f; juce::Path p;
                        const int px = juce::jmax (1, (int) a.getWidth());
                        for (int x = 0; x < px; ++x)
                        {
                            const int s0 = (int) (w.viewStart + w.viewLen * x / px), s1 = juce::jmax (s0 + 1, (int) (w.viewStart + w.viewLen * (x + 1) / px));
                            if (s0 >= len) break;
                            auto rg = d.buf.findMinMax (c, s0, juce::jmin (len, s1) - s0);
                            p.addRectangle (a.getX() + (float) x, mid - rg.getEnd() * half, 1.0f, juce::jmax (1.0f, (rg.getEnd() - rg.getStart()) * half));
                        }
                        g.setGradientFill (juce::ColourGradient (accent, 0, mid - half, accent.darker (0.6f), 0, mid, true)); g.fillPath (p);
                        g.setColour (Theme::text().withAlpha (0.12f)); g.drawHorizontalLine ((int) mid, a.getX(), a.getRight());
                        g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f)); g.drawText (nch == 1 ? "M" : (c == 0 ? "L" : "R"), (int) a.getX() + 4, (int) (mid - half), 12, 10, juce::Justification::centredLeft);
                    }
                    // ruler
                    g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
                    const double secs = w.viewLen / d.sr; const double step = secs > 60 ? 10 : secs > 20 ? 5 : secs > 5 ? 1 : secs > 1 ? 0.25 : secs > 0.2 ? 0.05 : 0.01;
                    for (double t = std::floor (w.viewStart / d.sr / step) * step; t < (w.viewStart + w.viewLen) / d.sr; t += step) { const float x = xFor (t * d.sr); if (x < a.getX()) continue; g.setColour (Theme::dim()); g.drawVerticalLine ((int) x, (float) rulerArea.getBottom() - 6.0f, (float) rulerArea.getBottom()); g.drawText (juce::String (t, step < 0.1 ? 2 : (step < 1 ? 2 : 1)) + " s", (int) x + 3, rulerArea.getY(), 60, rulerArea.getHeight() - 4, juce::Justification::centredLeft); g.setColour (Theme::text().withAlpha (0.05f)); g.drawVerticalLine ((int) x, a.getY(), a.getBottom()); }
                    // cursor / playhead
                    if (w.playing) { g.setColour (Theme::text()); g.drawVerticalLine ((int) xFor (w.playPos), a.getY(), a.getBottom()); }
                    else { g.setColour (Theme::text().withAlpha (0.7f)); g.drawVerticalLine ((int) xFor (w.cursor), a.getY(), a.getBottom()); }
                    if (d.loopEnd > d.loopStart) { g.setColour (Theme::amber()); g.drawVerticalLine ((int) xFor (d.loopStart), a.getY(), a.getBottom()); g.drawVerticalLine ((int) xFor (d.loopEnd), a.getY(), a.getBottom()); g.setFont (Theme::font (7.5f, true)); g.drawText ("LOOP", (int) xFor (d.loopStart) + 3, (int) a.getBottom() - 12, 40, 10, juce::Justification::centredLeft); }
                    // overview: the whole file, the view window and the selection
                    {
                        auto o = overviewArea.toFloat().reduced (2.0f); juce::Path p; const int px = juce::jmax (1, (int) o.getWidth());
                        for (int x = 0; x < px; ++x) { const int s0 = (int) ((juce::int64) len * x / px), s1 = juce::jmax (s0 + 1, (int) ((juce::int64) len * (x + 1) / px)); float mn = 0, mx = 0; for (int c = 0; c < nch; ++c) { auto rg = d.buf.findMinMax (c, s0, juce::jmin (len, s1) - s0); mn = juce::jmin (mn, rg.getStart()); mx = juce::jmax (mx, rg.getEnd()); } p.addRectangle (o.getX() + (float) x, o.getCentreY() - mx * o.getHeight() * 0.45f, 1.0f, juce::jmax (1.0f, (mx - mn) * o.getHeight() * 0.45f)); }
                        g.setColour (accent.withAlpha (0.45f)); g.fillPath (p);
                        auto view = juce::Rectangle<float> (o.getX() + o.getWidth() * (float) (w.viewStart / len), o.getY(), juce::jmax (3.0f, o.getWidth() * (float) (w.viewLen / len)), o.getHeight());
                        g.setColour (Theme::text().withAlpha (0.12f)); g.fillRoundedRectangle (view, 2.0f); g.setColour (Theme::text().withAlpha (0.5f)); g.drawRoundedRectangle (view, 2.0f, 1.0f);
                        if (w.selB > w.selA) { g.setColour (accent.withAlpha (0.5f)); g.fillRect (o.getX() + o.getWidth() * (float) w.selA / (float) len, o.getBottom() - 3.0f, juce::jmax (2.0f, o.getWidth() * (float) (w.selB - w.selA) / (float) len), 3.0f); }
                    }
                }
                else { g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f)); g.drawFittedText ("Open a file, drop one here, press RECORD, or right-click an audio clip in the Edit window and choose Open in Cut Lab", waveArea.reduced (40), juce::Justification::centred, 3); }
                // meter
                {
                    auto m = meterArea.toFloat(); g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (m, 3.0f);
                    const float lvl = juce::jlimit (0.0f, 1.0f, (w.meterDb.load() + 60.0f) / 60.0f);
                    if (lvl > 0.0f) { auto bar = m.reduced (3.0f).withTop (m.getBottom() - 3.0f - (m.getHeight() - 6.0f) * lvl); g.setGradientFill (juce::ColourGradient (Theme::red(), 0, m.getY(), accent, 0, m.getBottom(), false)); g.fillRoundedRectangle (bar, 2.0f); }
                    for (float db : { -6.0f, -12.0f, -24.0f, -48.0f }) { const float y = m.getBottom() - 3.0f - (m.getHeight() - 6.0f) * (db + 60.0f) / 60.0f; g.setColour (Theme::text().withAlpha (0.25f)); g.drawHorizontalLine ((int) y, m.getX() + 2.0f, m.getRight() - 2.0f); }
                }
                if (w.recording) { g.setColour (Theme::red()); g.fillEllipse ((float) waveArea.getRight() - 24.0f, (float) waveArea.getY() + 8.0f, 12.0f, 12.0f); g.setFont (Theme::font (9.0f, true)); g.drawText ("REC " + juce::String (w.recLen / juce::jmax (1.0, w.devRate), 1) + " s", waveArea.getRight() - 120, waveArea.getY() + 6, 90, 16, juce::Justification::centredRight); }
                // status strip
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
                const juce::String selText = w.selB > w.selA ? "   selection " + juce::String (w.selA / juce::jmax (1.0, d.sr), 3) + " - " + juce::String (w.selB / juce::jmax (1.0, d.sr), 3) + " s  (" + juce::String ((w.selB - w.selA) / juce::jmax (1.0, d.sr), 3) + " s)" : juce::String ("   no selection = the whole file");
                g.drawText (w.status + selText + (d.dirty ? "   (unsaved - SAVE writes a WAV in the Edited folder)" : ""), getLocalBounds().reduced (12, 6).removeFromBottom (14), juce::Justification::centredLeft);
                g.drawText ("space play   cmd-X/C/V   cmd-Z undo   cmd-S save   cmd-A all   wheel scroll   cmd-wheel zoom   drag the waveform down onto a track to place it", getLocalBounds().reduced (12, 6).removeFromBottom (14), juce::Justification::centredRight);
            }
            void mouseDown (const juce::MouseEvent& e) override
            {
                if (! waveArea.contains (e.getPosition()) || w.doc.length() == 0) return;
                if (e.mods.isPopupMenu()) { showMenu (e); return; }
                dragAnchor = juce::jlimit (0, w.doc.length(), (int) sampleFor (e.position.x)); w.cursor = dragAnchor; w.selA = w.selB = dragAnchor; dragging = true; repaint();
            }
            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (! dragging) return;
                if (e.mods.isAltDown() || (e.getDistanceFromDragStartY() > 40 && w.doc.file.existsAsFile() && ! w.doc.dirty))
                {
                    // drag the file out onto the timeline (or anywhere that takes files)
                    dragging = false; juce::StringArray files { w.doc.file.getFullPathName() }; performExternalDragDropOfFiles (files, false, this); return;
                }
                const int s = juce::jlimit (0, w.doc.length(), (int) sampleFor (e.position.x)); w.selA = juce::jmin (dragAnchor, s); w.selB = juce::jmax (dragAnchor, s); repaint();
            }
            void mouseUp (const juce::MouseEvent&) override   { dragging = false; }
            void mouseDoubleClick (const juce::MouseEvent& e) override   { if (waveArea.contains (e.getPosition())) { w.selA = 0; w.selB = w.doc.length(); repaint(); } }
            void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wh) override
            {
                if (! waveArea.contains (e.getPosition()) || w.doc.length() == 0) return;
                if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { const double at = sampleFor (e.position.x); const double f = wh.deltaY > 0 ? 0.8 : 1.25; w.viewLen = juce::jlimit (64.0, (double) w.doc.length(), w.viewLen * f); w.viewStart = at - (e.position.x - waveArea.getX()) / waveArea.getWidth() * w.viewLen; }
                else w.viewStart -= wh.deltaY * w.viewLen * 0.2;
                w.clampSel(); repaint();
            }
            bool keyPressed (const juce::KeyPress& k) override
            {
                if (k == juce::KeyPress::spaceKey) { if (w.playing) w.stopPlayback(); else w.play (false); repaint(); return true; }
                if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { cutBtn.triggerClick(); return true; }
                if (k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown())
                {
                    const auto ch = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
                    if (ch == 'x') { cutBtn.triggerClick(); return true; } if (ch == 'c') { copyBtn.triggerClick(); return true; } if (ch == 'v') { pasteBtn.triggerClick(); return true; }
                    if (ch == 'z') { w.undoLast(); return true; } if (ch == 's') { w.save (false); repaint(); return true; } if (ch == 'a') { w.selA = 0; w.selB = w.doc.length(); repaint(); return true; }
                    if (ch == 'r') { recBtn.triggerClick(); return true; } if (ch == 'n') { normBtn.triggerClick(); return true; } if (ch == 't') { trimBtn.triggerClick(); return true; }
                }
                return false;
            }
            void showMenu (const juce::MouseEvent& e)
            {
                juce::PopupMenu m;
                m.addItem (1, "Set loop points from selection"); m.addItem (2, "Add region from selection"); m.addItem (3, "Clear regions"); m.addSeparator();
                m.addItem (4, "Select all"); m.addItem (5, "Zoom to selection"); m.addItem (6, "Zoom out fully");
                const int at = (int) sampleFor (e.position.x);
                m.showMenuAsync ({}, [this, at] (int r)
                {
                    if (r == 1) loopPtsBtn.triggerClick(); else if (r == 2) regionBtn.triggerClick(); else if (r == 3) { w.doc.regions.clear(); w.doc.dirty = true; }
                    else if (r == 4) { w.selA = 0; w.selB = w.doc.length(); } else if (r == 5) zoomSelBtn.triggerClick(); else if (r == 6) { w.viewStart = 0; w.viewLen = w.doc.length(); }
                    juce::ignoreUnused (at); repaint();
                });
            }
        };

        Session& session; Content content;
        std::atomic<bool> playing { false }, recording { false }; std::atomic<float> meterDb { -100.0f }; double playPos = 0; int playEnd = 0, playStart = 0; bool playLoop = false; double devRate = 48000.0;
        juce::AudioBuffer<float> recBuf; int recLen = 0;
    };
}

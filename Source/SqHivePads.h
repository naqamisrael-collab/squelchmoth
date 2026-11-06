#pragma once
// Squelchmoth 1.8 - Hive Pads: the pad sampler.
//
//   64 pads in four banks of 16 (A-D) - each pad answers to a MIDI note (GM map by default, 36 upwards),
//   up to 8 layers per pad: velocity split, round-robin, random or crossfade; per pad: tune, fine, gain, pan,
//   attack / hold / decay, low-pass cutoff / resonance, reverse, choke group (1-16), one-shot, output pair;
//   the 17 Squelchmoth kits load in one click (pads 1-12), any WAV / AIF / FLAC / MP3 drops on a pad,
//   "slice to pads" cuts a loop at its transients across the pads, the sound is anti-click (short fades) and
//   sample-rate converted on the fly. State lives in the plugin's ValueTree (PAD / LAYER children), so it is
//   undoable and saved with the session; the global knobs (level, tune, attack, decay, cutoff, velocity) are
//   automatable and apply on top of the per-pad settings.

#include "SqInstruments.h"
#include "KitInfo.h"
#include "SqSamplers2.h"
#include "Edition.h"

class SqHivePadsPlugin : public te::Plugin,
                         public SqDsp::BankOwner
{
public:
    enum P { level, tune, attack, decay, cutoff, resonance, velSens, numParams };
    static constexpr int numPads = 64, padsPerBank = 16, maxLayers = 8, maxVoices = 48;

    static constexpr const char* xmlTypeName = "sqmHivePads";
    static const char* getPluginName()   { return NEEDS_TRANS("Hive Pads"); }
    juce::String getName() const override { return "Hive Pads"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Pads"; }
    juce::String getSelectableDescription() override { return "Hive Pads pad sampler"; }

    SqHivePadsPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, {
            { "level", "Level", -24, 12, 0, 1, " dB", 1 }, { "tune", "Tune", -24, 24, 0, 1, " st", 1 }, { "attack", "Attack", 0, 1, 0, 0.4f, " s", 3 }, { "decay", "Decay", 0.05f, 10, 10, 0.4f, " s", 2 },
            { "cutoff", "Cutoff", 40, 20000, 20000, 0.3f, " Hz", 0 }, { "resonance", "Resonance", 0, 1, 0, 1, "", 2 }, { "velSens", "Velocity", 0, 1, 0.8f, 1, "", 2 } });
        auto um = getUndoManager();
        currentBank.referTo (state, "padBank", um, 0);
        selectedPad.referTo (state, "selectedPad", um, 0);
        formats.registerBasicFormats();
        pads = state.getOrCreateChildWithName ("PADS", nullptr);
        state.addListener (&padListener);
        reloadAll();
    }
    ~SqHivePadsPlugin() override   { state.removeListener (&padListener); notifyListenersOfDeletion(); bank.detach(); }

    SqDsp::ParamBank& getBank() override   { return bank; }

    // te::Plugin
    bool isSynth() override                             { return true; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 4.0; }
    bool canBeAddedToClip() override                    { return false; }
    bool canBeAddedToRack() override                    { return true; }
    void initialise (const te::PluginInitialisationInfo& info) override   { sr = info.sampleRate; for (auto& v : voices) v.active = false; }
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (auto& cv : bank.values) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
        auto um = getUndoManager();
        if (auto src = v.getChildWithName ("PADS"); src.isValid())
        {
            auto old = state.getChildWithName ("PADS"); if (old.isValid()) state.removeChild (old, um);
            state.addChild (src.createCopy(), -1, um);
            pads = state.getChildWithName ("PADS");
        }
        reloadAll();
    }

    //==========================================================================
    // Pad model (ValueTree): PADS > PAD(index, note, name, tune, fine, gain, pan, attack, hold, decay, cutoff, res, reverse, choke, oneShot, layerMode, colour) > LAYER(file, root, gain, pan, velLo, velHi)
    juce::ValueTree padState (int i, bool create = true)
    {
        for (auto c : pads) if ((int) c.getProperty ("index") == i) return c;
        if (! create) return {};
        juce::ValueTree p ("PAD");
        p.setProperty ("index", i, nullptr); p.setProperty ("note", 36 + i, nullptr); p.setProperty ("name", "Pad " + juce::String (i + 1), nullptr);
        p.setProperty ("tune", 0, nullptr); p.setProperty ("fine", 0, nullptr); p.setProperty ("gain", 0.0, nullptr); p.setProperty ("pan", 0.0, nullptr);
        p.setProperty ("attack", 0.0, nullptr); p.setProperty ("hold", 0.0, nullptr); p.setProperty ("decay", 10.0, nullptr); p.setProperty ("cutoff", 20000.0, nullptr); p.setProperty ("res", 0.0, nullptr);
        p.setProperty ("reverse", false, nullptr); p.setProperty ("choke", 0, nullptr); p.setProperty ("oneShot", true, nullptr); p.setProperty ("layerMode", 0, nullptr);
        pads.addChild (p, -1, getUndoManager());
        return p;
    }
    static juce::StringArray layerModeNames()   { return { "VELOCITY", "ROUND ROBIN", "RANDOM", "CROSSFADE" }; }

    juce::String padName (int i)   { auto p = padState (i, false); return p.isValid() ? p.getProperty ("name").toString() : "Pad " + juce::String (i + 1); }
    int padNote (int i)            { auto p = padState (i, false); return p.isValid() ? (int) p.getProperty ("note") : 36 + i; }
    int numLayers (int i)          { auto p = padState (i, false); return p.isValid() ? p.getNumChildren() : 0; }
    bool padHasSound (int i)       { return numLayers (i) > 0; }

    /** Adds a layer (a file) to a pad; returns the layer index. */
    int addLayer (int pad, const juce::File& f, int root = 60, float gainDb = 0.0f, int velLo = 1, int velHi = 127)
    {
        auto p = padState (pad);
        if (p.getNumChildren() >= maxLayers) return -1;
        juce::ValueTree l ("LAYER");
        l.setProperty ("file", f.getFullPathName(), nullptr); l.setProperty ("root", root, nullptr); l.setProperty ("gain", gainDb, nullptr); l.setProperty ("pan", 0.0, nullptr);
        l.setProperty ("velLo", velLo, nullptr); l.setProperty ("velHi", velHi, nullptr);
        p.addChild (l, -1, getUndoManager());
        if (p.getNumChildren() == 1 && p.getProperty ("name").toString().startsWith ("Pad ")) p.setProperty ("name", f.getFileNameWithoutExtension(), getUndoManager());
        return p.getNumChildren() - 1;
    }
    void clearPad (int pad)   { auto p = padState (pad, false); if (p.isValid()) { while (p.getNumChildren() > 0) p.removeChild (0, getUndoManager()); } }
    void setPadProperty (int pad, const juce::Identifier& id, const juce::var& v)   { padState (pad).setProperty (id, v, getUndoManager()); }
    juce::var getPadProperty (int pad, const juce::Identifier& id, const juce::var& def = {})   { auto p = padState (pad, false); return p.isValid() && p.hasProperty (id) ? p.getProperty (id) : def; }

    /** Loads one of the Squelchmoth kits onto pads 1-12 of the current bank (replacing them). */
    void loadKitFiles (const juce::Array<juce::File>& files, const juce::StringArray& names, const juce::Array<int>& notes, const juce::Array<float>& gains, int firstPad = 0)
    {
        for (int i = 0; i < files.size() && firstPad + i < numPads; ++i)
        {
            const int pad = firstPad + i;
            clearPad (pad);
            auto p = padState (pad);
            p.setProperty ("name", names[i], getUndoManager());
            p.setProperty ("note", notes[i], getUndoManager());
            p.setProperty ("choke", names[i].containsIgnoreCase ("hat") ? 1 : 0, getUndoManager());   // closed + open hats choke each other
            addLayer (pad, files[i], 60, gains[i]);
        }
    }

    /** Kit files come from the app (the kits live in the Sessions folder): set by MainComponent. */
    static inline std::function<juce::Array<juce::File> (int kit)> kitFiles;
    void loadKitByIndex (int kit)
    {
        if (! kitFiles) return;
        kit = juce::jlimit (0, DrumKit::numKits - 1, kit);
        if (! Edition::isKitAllowed (kit)) return;   // locked kits (Demo: the first five; kits 18-27: Beat Lab Expansion)
        auto files = kitFiles (kit); juce::StringArray names; juce::Array<int> notes; juce::Array<float> gains;
        for (int i = 0; i < DrumKit::numSounds; ++i) { names.add (DrumKit::sounds[i].name); notes.add (DrumKit::sounds[i].note); gains.add (DrumKit::gainsDb[i]); }
        loadKitFiles (files, names, notes, gains, currentBank.get() * 16);
        state.setProperty ("sqmPadsKit", DrumKit::kits[kit].name, nullptr);
    }

    /** Slices a file at its transients onto consecutive pads (up to 16). */
    int sliceToPads (const juce::File& f, int firstPad, int maxSlices = 16)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr) return 0;
        juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buf, 0, (int) reader->lengthInSamples, 0, true, true);
        const double fsr = reader->sampleRate;
        // transient detection: the spectral-flux onset detector Mandible uses
        std::vector<int> cuts = SqSmp::onsets (buf, fsr, 0.5f, maxSlices);
        while ((int) cuts.size() > maxSlices) cuts.pop_back();
        cuts.push_back (buf.getNumSamples());
        auto dir = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " slices"); dir.createDirectory();
        juce::WavAudioFormat wav; int made = 0;
        for (size_t i = 0; i + 1 < cuts.size() && firstPad + (int) i < numPads; ++i)
        {
            auto out = dir.getChildFile ("slice " + juce::String (i + 1).paddedLeft ('0', 2) + ".wav");
            out.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), fsr, (unsigned) buf.getNumChannels(), 24, {}, 0)))
                    writer->writeFromAudioSampleBuffer (buf, cuts[i], cuts[i + 1] - cuts[i]);
            const int pad = firstPad + (int) i;
            clearPad (pad); padState (pad).setProperty ("name", "Slice " + juce::String (i + 1), getUndoManager());
            addLayer (pad, out); ++made;
        }
        return made;
    }

    //==========================================================================
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals nd;
        auto& out = *fc.destBuffer;
        const int start = fc.bufferStartSample, num = fc.bufferNumSamples;
        for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, start, num);
        if (reloadPending.exchange (false)) swapInLoaded();
        if (pendingTrigger.load() != 0) handlePendingTrigger (num);
        if (const int wg = watchCutGroup.load(); wg > 0)
        {
            const int gen = cutGeneration (wg).load();
            if (gen != lastCutGen) { lastCutGen = gen; for (auto& v : voices) if (v.active) v.choked = true; }
        }

        // MIDI -> voices
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) for (auto& v : voices) v.release = true;
            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn()) trigger (m.getNoteNumber(), m.getFloatVelocity(), juce::jlimit (0, num - 1, juce::roundToInt (m.getTimeStamp() * sr)));
                else if (m.isNoteOff()) { for (auto& v : voices) if (v.active && v.note == m.getNoteNumber() && ! v.oneShot) v.release = true; }
                else if (m.isAllNotesOff() || m.isAllSoundOff()) { for (auto& v : voices) if (v.active) v.choked = true; }   // the Rack Channel's cut groups land here
            }
        }
        bool any = false; for (auto& v : voices) any = any || v.active;
        if (! any) { lastLevel = 0.0f; return; }

        const float gLevel = juce::Decibels::decibelsToGain (bank[level]);
        const float gTune = bank[tune]; const float gAttack = bank[attack]; const float gDecay = bank[decay]; const float gCut = bank[cutoff]; const float gRes = bank[resonance];
        auto* L = out.getWritePointer (0, start); auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;
        float peak = 0;
        for (auto& v : voices)
        {
            if (! v.active || v.sample == nullptr) continue;
            auto& smp = *v.sample;
            const double ratio = smp.sampleRate / sr * std::pow (2.0, (v.tune + gTune) / 12.0);
            const float att = juce::jmax (v.attack, gAttack), dec = juce::jmin (v.decay, gDecay);
            const float cut = juce::jmin (v.cutoff, gCut), res = juce::jmax (v.res, gRes);
            const bool filt = cut < 19000.0f;
            if (filt) v.svfL.set (sr, cut, 0.5f + res * 8.0f), v.svfR.set (sr, cut, 0.5f + res * 8.0f);
            const float attInc = att > 0.0005f ? (float) (1.0 / (att * sr)) : 1.0f;
            const float decCoef = std::exp (-1.0f / (float) (juce::jmax (0.02f, dec) * sr) * 4.6f);
            const int n = smp.buffer.getNumSamples();
            for (int i = v.startOffset; i < num; ++i)
            {
                if (v.pos < 0 || v.pos >= n - 1) { v.active = false; break; }
                const int i0 = (int) v.pos; const float frac = (float) (v.pos - i0);
                const int i1 = juce::jmin (n - 1, i0 + 1);
                float l = smp.buffer.getSample (0, i0) * (1.0f - frac) + smp.buffer.getSample (0, i1) * frac;
                float r = smp.buffer.getNumChannels() > 1 ? smp.buffer.getSample (1, i0) * (1.0f - frac) + smp.buffer.getSample (1, i1) * frac : l;
                // envelope: attack -> hold -> decay (one-shots run the whole sample unless decay cuts them)
                if (v.env < 1.0f && v.phase == 0) { v.env = juce::jmin (1.0f, v.env + attInc); if (v.env >= 1.0f) v.phase = 1; }
                else if (v.phase == 1) { v.holdLeft -= 1; if (v.holdLeft <= 0) v.phase = 2; }
                else if (v.phase == 2) { v.env *= decCoef; if (v.env < 0.0005f) { v.active = false; break; } }
                if (v.release) { v.rel *= 0.9992f; if (v.rel < 0.001f) { v.active = false; break; } }
                if (v.choked) { v.chokeGain -= 0.002f; if (v.chokeGain <= 0.0f) { v.active = false; break; } }
                const float g = v.gain * v.env * v.rel * v.chokeGain * gLevel;
                // anti-click at the very start / end of the sample
                const float edge = juce::jmin (1.0f, (float) juce::jmin (i0, n - 1 - i0) / 64.0f);
                l *= g * edge; r *= g * edge;
                if (filt) { l = v.svfL.lowpass (l); r = v.svfR.lowpass (r); }
                L[i] += l * v.gl; if (R) R[i] += r * v.gr;
                peak = juce::jmax (peak, std::abs (l));
                v.pos += v.reverse ? -ratio : ratio;
            }
            v.startOffset = 0;
        }
        lastLevel = peak;
        if (R == nullptr) {}
        for (int i = 0; i < numPads; ++i) padFlash[(size_t) i] = juce::jmax (0.0f, padFlash[(size_t) i] - (float) num / (float) sr * 4.0f);
    }

    /** Cross-channel cut groups without MIDI: the Rack Channel on the same track tells us which group to watch;
        a generation bump on that group (see SqRackChannelPlugin::cutGeneration) chokes every voice at the next block. */
    std::atomic<int> watchCutGroup { 0 };
    static std::atomic<int>& cutGeneration (int group)   { static std::array<std::atomic<int>, 100> gens {}; return gens[(size_t) juce::jlimit (0, 99, group)]; }

    /** Audition from the UI / Claude. */
    void triggerPad (int pad, float vel = 1.0f)   { pendingTrigger.store ((pad & 0xff) | ((int) (vel * 127.0f) << 8) | 0x10000); }

    //==========================================================================
    // for the UI / MCP
    SqDsp::ParamBank bank;
    juce::CachedValue<int> currentBank, selectedPad;
    std::array<float, numPads> padFlash {};
    float lastLevel = 0.0f;
    juce::ValueTree pads;
    juce::AudioFormatManager formats;
    struct Sample { juce::AudioBuffer<float> buffer; double sampleRate = 44100.0; juce::String path; };
    std::shared_ptr<Sample> sampleFor (int pad, int layer)
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        auto it = loaded.find (pad * 16 + layer); return it != loaded.end() ? it->second : nullptr;
    }

    int padForNote (int note)   { for (int i = 0; i < numPads; ++i) { auto p = padState (i, false); if (p.isValid() && (int) p.getProperty ("note") == note && p.getNumChildren() > 0) return i; } return -1; }

private:
    //==========================================================================
    struct Voice
    {
        bool active = false, release = false, oneShot = true, reverse = false, choked = false;
        int note = 0, pad = 0, phase = 0, startOffset = 0, choke = 0, holdLeft = 0;
        double pos = 0; float gain = 1, gl = 0.7f, gr = 0.7f, env = 0, rel = 1, chokeGain = 1, tune = 0, attack = 0, decay = 10, cutoff = 20000, res = 0;
        std::shared_ptr<Sample> sample; SqDsp::SVF svfL, svfR; juce::int64 age = 0;
    };
    std::array<Voice, maxVoices> voices;
    juce::int64 ageClock = 0;
    double sr = 48000.0;
    std::map<int, std::shared_ptr<Sample>> loaded, loading;
    juce::SpinLock sampleLock;
    std::atomic<bool> reloadPending { false };
    std::atomic<int> pendingTrigger { 0 };
    int lastCutGen = 0;
    std::array<int, numPads> rrCounter {};

    void trigger (int note, float vel, int offset)
    {
        const int pad = padForNote (note);
        if (pad < 0) return;
        auto p = padState (pad, false);
        const int nl = p.getNumChildren(); if (nl == 0) return;
        const int mode = (int) p.getProperty ("layerMode", 0);
        const int v127 = juce::jlimit (1, 127, juce::roundToInt (vel * 127.0f));
        // choose the layer(s)
        juce::Array<std::pair<int, float>> play;   // layer, gain mult
        if (mode == 0) { for (int l = 0; l < nl; ++l) { auto L = p.getChild (l); if (v127 >= (int) L.getProperty ("velLo", 1) && v127 <= (int) L.getProperty ("velHi", 127)) play.add ({ l, 1.0f }); } if (play.isEmpty()) play.add ({ nl - 1, 1.0f }); }
        else if (mode == 1) { play.add ({ rrCounter[(size_t) pad] % nl, 1.0f }); rrCounter[(size_t) pad]++; }
        else if (mode == 2) { play.add ({ juce::Random::getSystemRandom().nextInt (nl), 1.0f }); }
        else { const float pos = vel * (float) (nl - 1); const int a = (int) pos; const float f = pos - a; play.add ({ a, 1.0f - f }); if (a + 1 < nl && f > 0.01f) play.add ({ a + 1, f }); }

        const int choke = (int) p.getProperty ("choke", 0);
        if (choke > 0) for (auto& v : voices) if (v.active && v.choke == choke) v.choked = true;
        if ((bool) p.getProperty ("cutSelf", false)) for (auto& v : voices) if (v.active && v.pad == pad) v.choked = true;

        for (auto& [layer, mult] : play)
        {
            auto smp = sampleFor (pad, layer); if (smp == nullptr) continue;
            auto L = p.getChild (layer);
            Voice* v = nullptr; for (auto& vv : voices) if (! vv.active) { v = &vv; break; }
            if (v == nullptr) { v = &voices[0]; for (auto& vv : voices) if (vv.age < v->age) v = &vv; }
            v->active = true; v->release = false; v->choked = false; v->chokeGain = 1; v->rel = 1; v->env = 0; v->phase = 0; v->age = ageClock++;
            v->note = note; v->pad = pad; v->choke = choke; v->sample = smp; v->startOffset = offset;
            v->oneShot = (bool) p.getProperty ("oneShot", true); v->reverse = (bool) p.getProperty ("reverse", false);
            v->pos = v->reverse ? smp->buffer.getNumSamples() - 2 : 0;
            const float sens = bank[velSens];
            v->gain = juce::Decibels::decibelsToGain ((float) (double) p.getProperty ("gain", 0.0) + (float) (double) L.getProperty ("gain", 0.0)) * (1.0f - sens + sens * vel) * mult;
            const float pan = juce::jlimit (-1.0f, 1.0f, (float) (double) p.getProperty ("pan", 0.0) + (float) (double) L.getProperty ("pan", 0.0));
            v->gl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi); v->gr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            v->tune = (float) (double) p.getProperty ("tune", 0.0) + (float) (double) p.getProperty ("fine", 0.0) / 100.0f + (float) (60 - (int) L.getProperty ("root", 60));
            v->attack = (float) (double) p.getProperty ("attack", 0.0); v->holdLeft = (int) ((double) p.getProperty ("hold", 0.0) * sr); v->decay = (float) (double) p.getProperty ("decay", 10.0);
            v->cutoff = (float) (double) p.getProperty ("cutoff", 20000.0); v->res = (float) (double) p.getProperty ("res", 0.0);
            v->svfL.reset(); v->svfR.reset();
            if (v->attack <= 0.0005f) { v->env = 1.0f; v->phase = 1; }
        }
        padFlash[(size_t) pad] = 1.0f;
    }

    // ValueTree listener: reload samples when layers change (off the audio thread)
    struct PadListener : public juce::ValueTree::Listener
    {
        SqHivePadsPlugin& o; explicit PadListener (SqHivePadsPlugin& p) : o (p) {}
        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id, bool) { juce::ignoreUnused (v, id); }
        void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override   { if (parent.getType() == juce::Identifier ("PAD") || parent.getType() == juce::Identifier ("PADS")) o.scheduleReload(); }
        void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override   { if (parent.getType() == juce::Identifier ("PAD") || parent.getType() == juce::Identifier ("PADS")) o.scheduleReload(); }
        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
        {
            if (v.getType() == juce::Identifier ("LAYER") && id == juce::Identifier ("file")) o.scheduleReload();
            else if (v == o.state && id == juce::Identifier ("sqmPadsKitIndex")) o.loadKitByIndex ((int) v.getProperty (id));   // presets load kits this way
        }
    } padListener { *this };
    /** Reloads straight away on the message thread (already-loaded files are reused, so a 12-pad kit load costs 12 reads),
        so a render that follows a kit load immediately - Claude's "add kit, bounce" - hears the kit. Off the message thread it is queued. */
    void scheduleReload()
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) { reloadAll(); return; }
        juce::MessageManager::callAsync ([sp = juce::WeakReference<SqHivePadsPlugin> (this)] { if (sp != nullptr) sp->reloadAll(); });
    }

    void reloadAll()
    {
        std::map<int, std::shared_ptr<Sample>> next;
        for (int i = 0; i < numPads; ++i)
        {
            auto p = padState (i, false); if (! p.isValid()) continue;
            for (int l = 0; l < p.getNumChildren(); ++l)
            {
                const auto path = p.getChild (l).getProperty ("file").toString();
                const int key = i * 16 + l;
                if (auto it = loaded.find (key); it != loaded.end() && it->second->path == path) { next[key] = it->second; continue; }
                juce::File f (path);
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
                if (reader == nullptr) continue;
                auto s = std::make_shared<Sample>();
                s->path = path; s->sampleRate = reader->sampleRate;
                s->buffer.setSize ((int) juce::jmin (2u, reader->numChannels), (int) juce::jmin ((juce::int64) 48000 * 30, reader->lengthInSamples));
                reader->read (&s->buffer, 0, s->buffer.getNumSamples(), 0, true, reader->numChannels > 1);
                next[key] = s;
            }
        }
        { const juce::SpinLock::ScopedLockType sl (sampleLock); loading = std::move (next); }
        reloadPending = true;
    }
    void swapInLoaded()   { const juce::SpinLock::ScopedTryLockType sl (sampleLock); if (sl.isLocked()) { loaded = loading; } }

    // audition requests from the message thread
    void handlePendingTrigger (int num)
    {
        const int t = pendingTrigger.exchange (0);
        if (t & 0x10000) trigger (padNote (t & 0xff), ((t >> 8) & 0xff) / 127.0f, 0);
        juce::ignoreUnused (num);
    }
public:
    void processPendingTrigger()   { if (pendingTrigger.load() != 0) handlePendingTrigger (0); }
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (SqHivePadsPlugin)
};

#pragma once
// Squelchmoth 1.8.1 samplers:
//  Chrysalis 2   - the keyed multi-zone sampler: key / velocity zones, round-robin groups, loops with crossfade, ADSR,
//                  filter, SFZ and SF2 import
//  Chrysalis SF  - the same engine as its own channel type that opens straight into an SF2 / SFZ bank
//  Mandible      - two-deck slicer: transient or grid slicing, slices on keys, per-slice pitch / reverse / level /
//                  filter / decay, stretch to tempo, dump to piano roll, slice to rack channels
//  Mandible Lite - one deck, slice resolution, slices on keys, dump to piano roll
#include "SqSynthCore.h"
#include "KitInfo.h"

namespace SqSmp
{
    struct Sample { juce::AudioBuffer<float> buffer; double sampleRate = 44100.0; juce::String path; int loopStart = -1, loopEnd = -1; int rootNote = -1; };

    /** Loads audio files once per path (shared between zones and plug-in instances). */
    class SampleCache
    {
    public:
        static SampleCache& get()   { static SampleCache c; return c; }
        SampleCache()   { formats.registerBasicFormats(); }
        std::shared_ptr<Sample> load (const juce::File& f)
        {
            const juce::ScopedLock sl (lock);
            auto key = f.getFullPathName();
            if (auto it = cache.find (key); it != cache.end()) if (auto sp = it->second.lock()) return sp;
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > 48000 * 60 * 10) return nullptr;
            auto s = std::make_shared<Sample>();
            s->buffer.setSize ((int) juce::jmax (1u, reader->numChannels), (int) reader->lengthInSamples);
            reader->read (&s->buffer, 0, (int) reader->lengthInSamples, 0, true, true);
            s->sampleRate = reader->sampleRate; s->path = key;
            // WAV smpl chunk loop points and root note, when the file has them
            if (reader->metadataValues.containsKey ("NumSampleLoops") && reader->metadataValues["NumSampleLoops"].getIntValue() > 0)
            {
                s->loopStart = reader->metadataValues["Loop0Start"].getIntValue(); s->loopEnd = reader->metadataValues["Loop0End"].getIntValue();
            }
            if (reader->metadataValues.containsKey ("MidiUnityNote")) s->rootNote = reader->metadataValues["MidiUnityNote"].getIntValue();
            cache[key] = s;
            return s;
        }
        juce::AudioFormatManager formats;
    private:
        juce::CriticalSection lock; std::map<juce::String, std::weak_ptr<Sample>> cache;
    };

    /** 4-point, 3rd-order Hermite read between i0 and i0 + 1 (clamped at the ends): far cleaner than linear when a
        sample plays at another rate or pitch. */
    inline float hermite (const juce::AudioBuffer<float>& b, int ch, int i0, float fr, int len)
    {
        auto* p = b.getReadPointer (ch);
        const float xm1 = p[juce::jmax (0, i0 - 1)], x0 = p[i0], x1 = p[juce::jmin (len - 1, i0 + 1)], x2 = p[juce::jmin (len - 1, i0 + 2)];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * fr + c2) * fr + c1) * fr + x0;
    }

    /** Spectral-flux onset detector shared by Mandible and Hive Pads: the log-magnitude rise summed over the spectrum
        (1024-point frames, 256 hop), peaks over a local mean plus a sensitivity margin. Each cut lands one hop before the
        frame centre, just ahead of the hit. Returns the slice starts (always beginning with 0). */
    inline std::vector<int> onsets (const juce::AudioBuffer<float>& b, double sampleRate, float sensitivity, int maxCuts = 64)
    {
        constexpr int order = 10, N = 1 << order, H = 256;
        const int len = b.getNumSamples(), nch = juce::jmax (1, b.getNumChannels());
        std::vector<int> cuts { 0 };
        juce::dsp::FFT fft (order); std::vector<float> win ((size_t) N), buf ((size_t) N * 2), prev ((size_t) N / 2 + 1, 0.0f);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) N);
        const int frames = juce::jmax (0, (len - N) / H); std::vector<float> flux ((size_t) frames, 0.0f);
        for (int t = 0; t < frames; ++t)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N; ++i) { float x = 0; for (int c = 0; c < b.getNumChannels(); ++c) x += b.getSample (c, t * H + i); buf[(size_t) i] = x / (float) nch * win[(size_t) i]; }
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            float f = 0;
            for (int k = 0; k <= N / 2; ++k) { const float m = std::log1p (10.0f * buf[(size_t) k]); if (t > 0) f += juce::jmax (0.0f, m - prev[(size_t) k]); prev[(size_t) k] = m; }
            flux[(size_t) t] = f;
        }
        float mx = 0; for (auto v : flux) mx = juce::jmax (mx, v);
        if (mx > 0) for (auto& v : flux) v /= mx;
        const float delta = 0.02f + 0.18f * (1.0f - sensitivity);
        const int minGap = (int) (sampleRate * (0.03 + 0.07 * (1.0 - sensitivity)));
        for (int t = 2; t + 2 < frames && (int) cuts.size() < maxCuts; ++t)
        {
            float loc = 0; const int lo = juce::jmax (0, t - 8); for (int k = lo; k < t + 3; ++k) loc += flux[(size_t) k]; loc /= (float) (t + 3 - lo);
            if (flux[(size_t) t] >= flux[(size_t) t - 1] && flux[(size_t) t] > flux[(size_t) t + 1] && flux[(size_t) t] > loc + delta)
            {
                const int at = juce::jmax (0, t * H + N / 2 - H);
                if (at - cuts.back() > minGap) cuts.push_back (at);
            }
        }
        return cuts;
    }

    //==========================================================================
    /** SFZ reader: <group> / <region> opcodes that matter for playback. Paths resolve against the .sfz folder and default_path. */
    struct SfzRegion
    {
        juce::String sample; int loKey = 0, hiKey = 127, pitchKeycenter = 60, loVel = 1, hiVel = 127, seqLength = 1, seqPosition = 1;
        int tune = 0, transpose = 0; float volume = 0.0f, pan = 0.0f; int loopMode = -1, loopStart = -1, loopEnd = -1; int offset = 0;
        float ampegAttack = -1, ampegDecay = -1, ampegSustain = -1, ampegRelease = -1; float cutoff = 0.0f;
    };
    inline int noteValue (const juce::String& v);
    inline juce::Array<SfzRegion> readSfz (const juce::File& f)
    {
        juce::Array<SfzRegion> out;
        auto text = f.loadFileAsString();
        // strip comments
        juce::StringArray lines; lines.addLines (text);
        juce::String joined; for (auto& l : lines) { auto t = l.upToFirstOccurrenceOf ("//", false, false); joined << t << " "; }
        juce::String defaultPath;
        SfzRegion globalR, masterR, groupR; bool inRegion = false; SfzRegion cur; int scope = 0;   // 0 control/global 1 master 2 group 3 region
        auto apply = [&] (SfzRegion& r, const juce::String& key, const juce::String& val)
        {
            auto k = key.toLowerCase();
            if (k == "sample") r.sample = val.replaceCharacter ('\\', '/');
            else if (k == "lokey") r.loKey = noteValue (val); else if (k == "hikey") r.hiKey = noteValue (val);
            else if (k == "key") { r.loKey = r.hiKey = r.pitchKeycenter = noteValue (val); }
            else if (k == "pitch_keycenter") r.pitchKeycenter = noteValue (val);
            else if (k == "lovel") r.loVel = val.getIntValue(); else if (k == "hivel") r.hiVel = val.getIntValue();
            else if (k == "seq_length") r.seqLength = val.getIntValue(); else if (k == "seq_position") r.seqPosition = val.getIntValue();
            else if (k == "tune") r.tune = val.getIntValue(); else if (k == "transpose") r.transpose = val.getIntValue();
            else if (k == "volume") r.volume = val.getFloatValue(); else if (k == "pan") r.pan = val.getFloatValue() / 100.0f;
            else if (k == "loop_mode") r.loopMode = val == "loop_continuous" || val == "loop_sustain" ? 1 : 0;
            else if (k == "loop_start" || k == "loopstart") r.loopStart = val.getIntValue(); else if (k == "loop_end" || k == "loopend") r.loopEnd = val.getIntValue();
            else if (k == "offset") r.offset = val.getIntValue();
            else if (k == "ampeg_attack") r.ampegAttack = val.getFloatValue(); else if (k == "ampeg_decay") r.ampegDecay = val.getFloatValue();
            else if (k == "ampeg_sustain") r.ampegSustain = val.getFloatValue() / 100.0f; else if (k == "ampeg_release") r.ampegRelease = val.getFloatValue();
            else if (k == "cutoff") r.cutoff = val.getFloatValue();
            else if (k == "default_path") defaultPath = val.replaceCharacter ('\\', '/');
        };
        auto flush = [&] { if (inRegion && cur.sample.isNotEmpty()) out.add (cur); inRegion = false; };
        // tokenise: headers <x> and key=value pairs (values may contain spaces for sample=)
        int pos = 0; const int len = joined.length();
        while (pos < len)
        {
            while (pos < len && juce::CharacterFunctions::isWhitespace (joined[pos])) ++pos;
            if (pos >= len) break;
            if (joined[pos] == '<')
            {
                const int end = joined.indexOfChar (pos, '>'); if (end < 0) break;
                auto header = joined.substring (pos + 1, end).trim().toLowerCase(); pos = end + 1;
                flush();
                if (header == "global") { scope = 0; globalR = {}; masterR = {}; groupR = {}; }
                else if (header == "master") { scope = 1; masterR = globalR; groupR = masterR; }
                else if (header == "group") { scope = 2; groupR = masterR; }
                else if (header == "region") { scope = 3; cur = groupR; inRegion = true; }
                else scope = 0;
                continue;
            }
            // key=value; the value runs until the next token that contains '=' (so file names with spaces survive)
            const int eq = joined.indexOfChar (pos, '='); if (eq < 0) break;
            auto key = joined.substring (pos, eq).trim();
            int vEnd = eq + 1;
            while (vEnd < len)
            {
                // look ahead to the next '='; the value ends at the last whitespace before the next key
                const int nextEq = joined.indexOfChar (vEnd, '='), nextHdr = joined.indexOfChar (vEnd, '<');
                int stop = len;
                if (nextEq >= 0) { int ws = nextEq; while (ws > vEnd && ! juce::CharacterFunctions::isWhitespace (joined[ws])) --ws; stop = juce::jmin (stop, ws); }
                if (nextHdr >= 0) stop = juce::jmin (stop, nextHdr);
                vEnd = stop; break;
            }
            auto val = joined.substring (eq + 1, vEnd).trim(); pos = vEnd;
            SfzRegion& target = scope == 3 ? cur : scope == 2 ? groupR : scope == 1 ? masterR : globalR;
            apply (target, key, val);
            if (scope == 1) groupR = masterR;
        }
        flush();
        auto base = f.getParentDirectory();
        if (defaultPath.isNotEmpty()) base = base.getChildFile (defaultPath);
        for (auto& r : out) r.sample = base.getChildFile (r.sample).getFullPathName();
        return out;
    }
    inline int noteValue (const juce::String& v)
    {
        auto t = v.trim().toLowerCase();
        if (t.isEmpty()) return 60;
        if (juce::CharacterFunctions::isDigit (t[0]) || t[0] == '-') return t.getIntValue();
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
        int n = base[juce::jlimit (0, 6, (int) (t[0] - 'a'))]; int i = 1;
        if (i < t.length() && t[i] == '#') { ++n; ++i; } else if (i < t.length() && t[i] == 'b') { --n; ++i; }
        const int oct = t.substring (i).getIntValue();
        return n + (oct + 1) * 12;
    }

    //==========================================================================
    /** SF2 reader: extracts the samples of a bank to WAV files (once, into a cache folder next to the .sf2) and returns
        the regions of one preset (instrument zones with key / velocity ranges, root key, loop points, tuning, envelope). */
    struct Sf2Preset { juce::String name; int bank = 0, program = 0; int presetIndex = 0; };
    struct Sf2Reader
    {
        struct Gen { juce::uint16 op; juce::int16 amount; };
        struct Shdr { juce::String name; juce::uint32 start = 0, end = 0, loopStart = 0, loopEnd = 0, rate = 44100; juce::uint8 origPitch = 60; juce::int8 correction = 0; juce::uint16 link = 0, type = 1; };
        std::vector<Shdr> shdr; std::vector<std::pair<juce::String, int>> phdrNames; std::vector<juce::uint16> phdrBag; std::vector<std::pair<int,int>> phdrBankProg;
        std::vector<juce::uint16> pbagGen, ibagGen; std::vector<Gen> pgen, igen; std::vector<std::pair<juce::String, juce::uint16>> inst;
        juce::MemoryBlock sampleData; juce::File file; bool ok = false;

        bool open (const juce::File& f)
        {
            file = f; juce::FileInputStream in (f); if (! in.openedOk()) return false;
            auto read32 = [&] { return (juce::uint32) in.readInt(); }; auto read16 = [&] { return (juce::uint16) in.readShort(); };
            char id[5] = { 0 };
            in.read (id, 4); if (juce::String (id) != "RIFF") return false; read32(); in.read (id, 4); if (juce::String (id) != "sfbk") return false;
            while (! in.isExhausted())
            {
                in.read (id, 4); const juce::uint32 size = read32(); const juce::int64 chunkStart = in.getPosition();
                juce::String cid (id);
                if (cid == "LIST")
                {
                    in.read (id, 4); juce::String lid (id); const juce::int64 listEnd = chunkStart + size;
                    while (in.getPosition() < listEnd && ! in.isExhausted())
                    {
                        in.read (id, 4); const juce::uint32 sz = read32(); const juce::int64 st = in.getPosition(); juce::String sid (id);
                        if (lid == "sdta" && sid == "smpl") { sampleData.setSize (sz); in.read (sampleData.getData(), (int) sz); }
                        else if (sid == "shdr") { for (juce::uint32 i = 0; i + 46 <= sz; i += 46) { Shdr s; char nm[21] = { 0 }; in.read (nm, 20); s.name = juce::String (nm).trim(); s.start = read32(); s.end = read32(); s.loopStart = read32(); s.loopEnd = read32(); s.rate = read32(); s.origPitch = (juce::uint8) in.readByte(); s.correction = (juce::int8) in.readByte(); s.link = read16(); s.type = read16(); shdr.push_back (s); } }
                        else if (sid == "phdr") { for (juce::uint32 i = 0; i + 38 <= sz; i += 38) { char nm[21] = { 0 }; in.read (nm, 20); const int prog = read16(), bank = read16(); const juce::uint16 bag = read16(); read32(); read32(); read32(); phdrNames.push_back ({ juce::String (nm).trim(), (int) phdrNames.size() }); phdrBag.push_back (bag); phdrBankProg.push_back ({ bank, prog }); } }
                        else if (sid == "pbag") { for (juce::uint32 i = 0; i + 4 <= sz; i += 4) { pbagGen.push_back (read16()); read16(); } }
                        else if (sid == "pgen") { for (juce::uint32 i = 0; i + 4 <= sz; i += 4) { Gen g; g.op = read16(); g.amount = (juce::int16) read16(); pgen.push_back (g); } }
                        else if (sid == "inst") { for (juce::uint32 i = 0; i + 22 <= sz; i += 22) { char nm[21] = { 0 }; in.read (nm, 20); inst.push_back ({ juce::String (nm).trim(), read16() }); } }
                        else if (sid == "ibag") { for (juce::uint32 i = 0; i + 4 <= sz; i += 4) { ibagGen.push_back (read16()); read16(); } }
                        else if (sid == "igen") { for (juce::uint32 i = 0; i + 4 <= sz; i += 4) { Gen g; g.op = read16(); g.amount = (juce::int16) read16(); igen.push_back (g); } }
                        in.setPosition (st + sz + (sz & 1));
                    }
                }
                in.setPosition (chunkStart + size + (size & 1));
            }
            ok = ! shdr.empty() && sampleData.getSize() > 0 && ! phdrNames.empty();
            return ok;
        }
        juce::Array<Sf2Preset> presets() const
        {
            juce::Array<Sf2Preset> out;
            for (size_t i = 0; i + 1 < phdrNames.size(); ++i) { Sf2Preset p; p.name = phdrNames[i].first; p.bank = phdrBankProg[i].first; p.program = phdrBankProg[i].second; p.presetIndex = (int) i; out.add (p); }
            return out;
        }
        /** Writes one sample to the cache folder as a 16-bit mono WAV (the SF2 sample is 16-bit PCM); returns the file. */
        juce::File sampleFile (size_t index, const juce::File& cacheDir)
        {
            if (index >= shdr.size()) return {};
            auto& s = shdr[index];
            auto out = cacheDir.getChildFile (juce::File::createLegalFileName (juce::String ((int) index).paddedLeft ('0', 3) + " " + s.name) + ".wav");
            if (out.existsAsFile() && out.getSize() > 44) return out;
            cacheDir.createDirectory();
            const juce::uint32 total = (juce::uint32) (sampleData.getSize() / 2);
            if (s.start >= total || s.end <= s.start) return {};
            const juce::uint32 n = juce::jmin (s.end, total) - s.start;
            juce::AudioBuffer<float> buf (1, (int) n);
            auto* src = static_cast<const juce::int16*> (sampleData.getData()) + s.start;
            for (juce::uint32 i = 0; i < n; ++i) buf.setSample (0, (int) i, (float) (juce::int16) juce::ByteOrder::swapIfBigEndian ((juce::uint16) src[i]) / 32768.0f);
            juce::WavAudioFormat wav; juce::StringPairArray meta;
            if (s.loopEnd > s.loopStart && s.loopEnd - s.start <= n)
            {
                meta.set ("NumSampleLoops", "1"); meta.set ("Loop0Start", juce::String ((int) (s.loopStart - s.start))); meta.set ("Loop0End", juce::String ((int) (s.loopEnd - s.start)));
                meta.set ("Loop0Type", "0"); meta.set ("Loop0Identifier", "0"); meta.set ("Loop0Fraction", "0"); meta.set ("Loop0PlayCount", "0");
            }
            meta.set ("MidiUnityNote", juce::String ((int) s.origPitch)); meta.set ("MidiPitchFraction", "0"); meta.set ("Manufacturer", "0"); meta.set ("Product", "0"); meta.set ("SamplePeriod", juce::String ((int) (1.0e9 / juce::jmax (1u, s.rate)))); meta.set ("SmpteFormat", "0"); meta.set ("SmpteOffset", "0");
            out.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), (double) s.rate, 1, 16, meta, 0)))
                    writer->writeFromAudioSampleBuffer (buf, 0, (int) n);
            return out;
        }
        struct Zone { juce::File file; int loKey = 0, hiKey = 127, loVel = 0, hiVel = 127, root = 60; int tune = 0; int loopMode = 0; int loopStart = -1, loopEnd = -1; float attack = 0.001f, decay = 1.0f, sustain = 1.0f, release = 0.2f; float gainDb = 0.0f, pan = 0.0f; int sampleIndex = -1; };
        /** The zones of a preset: for every instrument the preset uses, every instrument zone with a sample. */
        juce::Array<Zone> zonesFor (int presetIndex, const juce::File& cacheDir)
        {
            juce::Array<Zone> out;
            if (presetIndex < 0 || presetIndex + 1 >= (int) phdrBag.size()) return out;
            auto timecents = [] (int tc) { return (float) std::pow (2.0, tc / 1200.0); };
            for (juce::uint16 pb = phdrBag[(size_t) presetIndex]; pb < phdrBag[(size_t) presetIndex + 1] && pb + 1 < pbagGen.size(); ++pb)
            {
                int instIndex = -1; int pKeyLo = 0, pKeyHi = 127, pVelLo = 0, pVelHi = 127;
                for (juce::uint16 g = pbagGen[pb]; g < pbagGen[pb + 1] && g < pgen.size(); ++g)
                {
                    const auto& gen = pgen[g];
                    if (gen.op == 41) instIndex = gen.amount;
                    else if (gen.op == 43) { pKeyLo = gen.amount & 0xff; pKeyHi = (gen.amount >> 8) & 0xff; }
                    else if (gen.op == 44) { pVelLo = gen.amount & 0xff; pVelHi = (gen.amount >> 8) & 0xff; }
                }
                if (instIndex < 0 || instIndex + 1 >= (int) inst.size()) continue;
                Zone globalZone; bool haveGlobal = false;
                for (juce::uint16 ib = inst[(size_t) instIndex].second; ib < inst[(size_t) instIndex + 1].second && ib + 1 < ibagGen.size(); ++ib)
                {
                    Zone z = haveGlobal ? globalZone : Zone(); int sampleId = -1; int coarse = 0, fine = 0, rootOverride = -1; int startOff = 0, endOff = 0, loopStartOff = 0, loopEndOff = 0;
                    for (juce::uint16 g = ibagGen[ib]; g < ibagGen[ib + 1] && g < igen.size(); ++g)
                    {
                        const auto& gen = igen[g];
                        switch (gen.op)
                        {
                            case 0: startOff += gen.amount; break; case 1: endOff += gen.amount; break; case 2: loopStartOff += gen.amount; break; case 3: loopEndOff += gen.amount; break;
                            case 4: startOff += gen.amount * 32768; break; case 12: endOff += gen.amount * 32768; break; case 45: loopStartOff += gen.amount * 32768; break; case 50: loopEndOff += gen.amount * 32768; break;
                            case 43: z.loKey = gen.amount & 0xff; z.hiKey = (gen.amount >> 8) & 0xff; break;
                            case 44: z.loVel = gen.amount & 0xff; z.hiVel = (gen.amount >> 8) & 0xff; break;
                            case 48: z.gainDb = -gen.amount / 10.0f; break;
                            case 17: z.pan = gen.amount / 500.0f; break;
                            case 51: coarse = gen.amount; break; case 52: fine = gen.amount; break;
                            case 54: z.loopMode = (gen.amount == 1 || gen.amount == 3) ? 1 : 0; break;
                            case 58: rootOverride = gen.amount; break;
                            case 34: z.attack = timecents (gen.amount); break; case 36: z.decay = timecents (gen.amount); break;
                            case 37: z.sustain = juce::jlimit (0.0f, 1.0f, 1.0f - gen.amount / 1000.0f); break; case 38: z.release = timecents (gen.amount); break;
                            case 53: sampleId = gen.amount; break;
                            default: break;
                        }
                    }
                    if (sampleId < 0) { globalZone = z; haveGlobal = true; continue; }
                    if ((size_t) sampleId >= shdr.size()) continue;
                    const auto& s = shdr[(size_t) sampleId];
                    // the preset layer narrows the key / velocity range
                    z.loKey = juce::jmax (z.loKey, pKeyLo); z.hiKey = juce::jmin (z.hiKey, pKeyHi); z.loVel = juce::jmax (z.loVel, pVelLo); z.hiVel = juce::jmin (z.hiVel, pVelHi);
                    if (z.loKey > z.hiKey || z.loVel > z.hiVel) continue;
                    z.root = rootOverride >= 0 ? rootOverride : (int) s.origPitch; z.tune = coarse * 100 + fine + s.correction;
                    z.file = sampleFile ((size_t) sampleId, cacheDir); z.sampleIndex = sampleId;
                    if (z.file == juce::File()) continue;
                    if (z.loopMode > 0 && s.loopEnd > s.loopStart) { z.loopStart = (int) (s.loopStart - s.start) + loopStartOff; z.loopEnd = (int) (s.loopEnd - s.start) + loopEndOff; }
                    juce::ignoreUnused (startOff, endOff);
                    out.add (z);
                }
            }
            return out;
        }
    };
}

namespace SqSynth
{
    //==========================================================================
    /** A zone sampler voice: key-tracked playback with loop + crossfade, ADSR, filter. */
    struct ZoneVoice : public VoiceBase
    {
        std::shared_ptr<SqSmp::Sample> sample; double pos = 0, pitchRatio = 1; int loopStart = -1, loopEnd = -1, xfade = 0; bool loop = false;
        float zoneGain = 1.0f, gl = 0.707f, gr = 0.707f; float eA = 0.005f, eD = 0.5f, eS = 1.0f, eR = 0.2f; float cutoffMul = 1.0f;
        float env = 0.0f; int stage = 0; SqDsp::SVF svfL, svfR;
        struct S { float cutoff = 20000, res = 0, fEnv = 0, velSens = 0.8f; int fType = 0; float tune = 0, gain = 1.0f; float fA = 0.004f, fD = 0.5f, fS = 0.3f, fR = 0.3f; } s2;
        float fEnvV = 0, fStage = 0;
        void start (int n, float vel, float, float)   { startBase (n, vel, 0, 0); env = 0; stage = 0; fEnvV = 0; fStage = 0; svfL.reset(); svfR.reset(); }
        void release() override   { VoiceBase::release(); stage = 3; fStage = 3; }
        inline void render (float* l, float* r, int n)
        {
            if (sample == nullptr) { active = false; return; }
            const auto& buf = sample->buffer; const int len = buf.getNumSamples(); const int nch = buf.getNumChannels();
            const double step = sample->sampleRate / sr * pitchRatio * (targetHz > 0 ? hz / targetHz : 1.0);   // sr is the voice rate (2x in HD), so the read speed follows it
            const float aInc = 1.0f / juce::jmax (1.0f, eA * (float) sr), dCoef = std::exp (-4.6f / juce::jmax (1.0f, eD * (float) sr)), rCoef = std::exp (-4.6f / juce::jmax (1.0f, eR * (float) sr));
            const float faInc = 1.0f / juce::jmax (1.0f, s2.fA * (float) sr), fdCoef = std::exp (-4.6f / juce::jmax (1.0f, s2.fD * (float) sr)), frCoef = std::exp (-4.6f / juce::jmax (1.0f, s2.fR * (float) sr));
            const float cut = juce::jlimit (20.0f, 20000.0f, s2.cutoff * cutoffMul); const bool filt = cut < 19500.0f || s2.fEnv != 0.0f;
            const float vg = (1.0f - s2.velSens + s2.velSens * velocity) * zoneGain * s2.gain * unisonGain;
            for (int i = 0; i < n; ++i)
            {
                // amp ADSR
                if (stage == 0) { env += aInc; if (env >= 1.0f) { env = 1.0f; stage = 1; } }
                else if (stage == 1) { env = eS + (env - eS) * dCoef; if (env - eS < 0.001f) stage = 2; }
                else if (stage == 3) { env *= rCoef; if (env < 0.0005f) { active = false; return; } }
                if (fStage == 0) { fEnvV += faInc; if (fEnvV >= 1.0f) { fEnvV = 1.0f; fStage = 1; } }
                else if (fStage == 1) fEnvV = s2.fS + (fEnvV - s2.fS) * fdCoef;
                else if (fStage == 3) fEnvV *= frCoef;
                // sample read with loop + crossfade
                if (loop && loopEnd > loopStart + 8 && pos >= loopEnd) pos -= (loopEnd - loopStart);
                if (pos >= len - 1) { active = false; return; }
                const int i0 = (int) pos; const float fr = (float) (pos - i0); const int i1 = juce::jmin (len - 1, i0 + 1);
                float xl = SqSmp::hermite (buf, 0, i0, fr, len);
                float xr = nch > 1 ? SqSmp::hermite (buf, 1, i0, fr, len) : xl;
                if (loop && xfade > 0 && loopEnd > loopStart + 8)
                {
                    const double into = pos - (loopEnd - xfade);
                    if (into > 0)   // blend with the audio before the loop start
                    {
                        const double p2 = loopStart - xfade + into; const int j0 = juce::jlimit (0, len - 2, (int) p2); const float f2 = (float) (p2 - j0), w = (float) (into / xfade);
                        xl = xl * (1.0f - w) + (buf.getSample (0, j0) + (buf.getSample (0, j0 + 1) - buf.getSample (0, j0)) * f2) * w;
                        if (nch > 1) xr = xr * (1.0f - w) + (buf.getSample (1, j0) + (buf.getSample (1, j0 + 1) - buf.getSample (1, j0)) * f2) * w; else xr = xl;
                    }
                }
                pos += step;
                const float g = env * vg;
                xl *= g; xr *= g;
                if (filt)
                {
                    const float fc = juce::jlimit (20.0f, 20000.0f, cut * std::pow (2.0f, s2.fEnv * fEnvV * 5.0f));
                    svfL.set (sr, fc, 0.5f + s2.res * 8.0f); svfR.set (sr, fc, 0.5f + s2.res * 8.0f);
                    xl = svfL.lowpass (xl); xr = svfR.lowpass (xr);
                }
                l[i] += xl * gl; r[i] += xr * gr;
            }
        }
    };

    //==========================================================================
    class SqChrysalis2Plugin : public SynthPluginBase<ZoneVoice, 32>
    {
    public:
        enum P { level, tune, velSens, aA, aD, aS, aR, fType, cutoff, res, fEnv, fA, fD, fS, fR, keyTrack, numParams };
        static constexpr const char* xmlTypeName = "sqmChrysalis2";
        static const char* getPluginName()   { return NEEDS_TRANS("Chrysalis 2"); }
        juce::String getName() const override { return "Chrysalis 2"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Chrys2"; }
        juce::String getSelectableDescription() override { return "Chrysalis 2 multi-zone sampler"; }
        double getTailLength() const override { return 4.0; }

        SqChrysalis2Plugin (te::PluginCreationInfo info) : SynthPluginBase<ZoneVoice, 32> (info)
        {
            bank.create (*this, {
                { "level", "Level", -24, 12, -3, 1, " dB", 1 }, { "tune", "Tune", -24, 24, 0, 1, " st", 1 }, { "velSens", "Velocity", 0, 1, 0.8f, 1, "", 2 },
                { "aA", "Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "aD", "Decay", 0.001f, 8, 1.0f, 0.3f, " s", 2 }, { "aS", "Sustain", 0, 1, 1.0f, 1, "", 2 }, { "aR", "Release", 0.001f, 8, 0.25f, 0.3f, " s", 2 },
                { "fType", "Filter Type", 0, 1, 0, 1, "", 0 }, { "cutoff", "Cutoff", 20, 20000, 20000, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0, 1, "", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 8, 0.5f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.3f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "keyTrack", "Key Track", 0, 1, 1, 1, "", 2 } });
            zones = state.getOrCreateChildWithName ("ZONES", nullptr);
            bankName.referTo (state, "bankName", getUndoManager(), juce::String());
            state.addListener (&zoneListener);
            quality = 0; defaultQuality = 0;   // samples are already band-limited: Eco plays them 1:1
            reloadAll();
        }
        ~SqChrysalis2Plugin() override   { state.removeListener (&zoneListener); }
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            SynthPluginBase::restorePluginStateFromValueTree (v);
            if (auto src = v.getChildWithName ("ZONES"); src.isValid())
            {
                auto old = state.getChildWithName ("ZONES"); if (old.isValid()) state.removeChild (old, nullptr);
                state.addChild (src.createCopy(), -1, nullptr); zones = state.getChildWithName ("ZONES");
            }
            reloadAll();
        }
        juce::StringArray modDestinations() const override   { return { "pitch", "cutoff", "amp", "pan" }; }
        juce::CachedValue<juce::String> bankName;

        //==========================================================================
        // Zones: ZONES > ZONE(file, root, loKey, hiKey, loVel, hiVel, tune(cents), gain(dB), pan, loopMode, loopStart, loopEnd, xfade, rr, name)
        juce::ValueTree zones;
        int numZones() const   { return zones.getNumChildren(); }
        juce::ValueTree zone (int i) const   { return zones.getChild (i); }
        int addZone (const juce::File& f, int root, int loKey, int hiKey, int loVel = 1, int hiVel = 127)
        {
            juce::ValueTree z ("ZONE");
            z.setProperty ("file", f.getFullPathName(), nullptr); z.setProperty ("name", f.getFileNameWithoutExtension(), nullptr);
            z.setProperty ("root", root, nullptr); z.setProperty ("loKey", loKey, nullptr); z.setProperty ("hiKey", hiKey, nullptr); z.setProperty ("loVel", loVel, nullptr); z.setProperty ("hiVel", hiVel, nullptr);
            z.setProperty ("tune", 0, nullptr); z.setProperty ("gain", 0.0, nullptr); z.setProperty ("pan", 0.0, nullptr); z.setProperty ("loopMode", 0, nullptr); z.setProperty ("loopStart", -1, nullptr); z.setProperty ("loopEnd", -1, nullptr); z.setProperty ("xfade", 0, nullptr); z.setProperty ("rr", 0, nullptr);
            zones.addChild (z, -1, getUndoManager());
            return zones.getNumChildren() - 1;
        }
        void removeZone (int i)   { if (juce::isPositiveAndBelow (i, zones.getNumChildren())) zones.removeChild (i, getUndoManager()); }
        void clearZones()         { while (zones.getNumChildren() > 0) zones.removeChild (0, getUndoManager()); }
        void setZoneProperty (int i, const juce::Identifier& id, const juce::var& v)   { if (auto z = zone (i); z.isValid()) z.setProperty (id, v, getUndoManager()); }

        /** Drops a folder or a list of files: root notes from the file names ("Piano C4.wav", "...-60.wav") or the WAV's unity note; keys split between the roots. */
        int addFilesAutoMapped (const juce::Array<juce::File>& files)
        {
            struct Item { juce::File f; int root; };
            std::vector<Item> items;
            for (auto& f : files)
            {
                if (! SqSmp::SampleCache::get().formats.findFormatForFileExtension (f.getFileExtension())) continue;
                int root = -1;
                auto base = f.getFileNameWithoutExtension();
                // "C4", "F#3", "Db2" tokens, or a bare MIDI number 24..108 at the end
                juce::StringArray toks; toks.addTokens (base.replaceCharacters ("_-.", "   "), " ", "");
                for (int i = toks.size() - 1; i >= 0 && root < 0; --i)
                {
                    auto t = toks[i].trim().toLowerCase();
                    if (t.isEmpty()) continue;
                    if (t.containsOnly ("0123456789")) { const int n = t.getIntValue(); if (n >= 12 && n <= 120) root = n; }
                    else if (t.length() <= 3 && juce::String ("abcdefg").containsChar (t[0]) && (t.length() == 1 || juce::String ("#b0123456789").containsChar (t[1])) && juce::CharacterFunctions::isDigit (t.getLastCharacter())) root = SqSmp::noteValue (t);
                }
                if (root < 0) if (auto s = SqSmp::SampleCache::get().load (f)) if (s->rootNote >= 0) root = s->rootNote;
                if (root < 0) root = 60;
                items.push_back ({ f, root });
            }
            if (items.empty()) return 0;
            std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.root < b.root || (a.root == b.root && a.f.getFileName() < b.f.getFileName()); });
            // split the keyboard halfway between neighbouring roots
            for (size_t i = 0; i < items.size(); ++i)
            {
                const int lo = i == 0 ? 0 : (items[i - 1].root + items[i].root) / 2 + 1;
                const int hi = i + 1 == items.size() ? 127 : (items[i].root + items[i + 1].root) / 2;
                addZone (items[i].f, items[i].root, juce::jmin (lo, items[i].root), juce::jmax (hi, items[i].root));
            }
            applyLoopPointsFromFiles();
            return (int) items.size();
        }
        void applyLoopPointsFromFiles()
        {
            for (int i = 0; i < numZones(); ++i)
            {
                auto z = zone (i);
                if ((int) z.getProperty ("loopStart", -1) >= 0) continue;
                if (auto s = SqSmp::SampleCache::get().load (juce::File (z.getProperty ("file").toString())))
                    if (s->loopStart >= 0 && s->loopEnd > s->loopStart) { z.setProperty ("loopMode", 1, nullptr); z.setProperty ("loopStart", s->loopStart, nullptr); z.setProperty ("loopEnd", s->loopEnd, nullptr); z.setProperty ("xfade", 64, nullptr); }
            }
        }
        /** Imports an SFZ file as zones (replacing the current ones). Returns the zone count. */
        int importSfz (const juce::File& f)
        {
            auto regions = SqSmp::readSfz (f); if (regions.isEmpty()) return 0;
            clearZones();
            for (auto& r : regions)
            {
                juce::File sf (r.sample); if (! sf.existsAsFile()) continue;
                const int zi = addZone (sf, r.pitchKeycenter, r.loKey, r.hiKey, r.loVel, r.hiVel);
                auto z = zone (zi);
                z.setProperty ("tune", r.tune + r.transpose * 100, nullptr); z.setProperty ("gain", (double) r.volume, nullptr); z.setProperty ("pan", (double) r.pan, nullptr);
                if (r.loopMode == 1) { z.setProperty ("loopMode", 1, nullptr); if (r.loopStart >= 0) { z.setProperty ("loopStart", r.loopStart, nullptr); z.setProperty ("loopEnd", r.loopEnd, nullptr); z.setProperty ("xfade", 32, nullptr); } }
                if (r.seqLength > 1) z.setProperty ("rr", r.seqPosition, nullptr);
                if (r.ampegAttack >= 0) z.setProperty ("attack", (double) r.ampegAttack, nullptr); if (r.ampegDecay >= 0) z.setProperty ("decay", (double) r.ampegDecay, nullptr);
                if (r.ampegSustain >= 0) z.setProperty ("sustain", (double) r.ampegSustain, nullptr); if (r.ampegRelease >= 0) z.setProperty ("release", (double) r.ampegRelease, nullptr);
            }
            applyLoopPointsFromFiles();
            bankName = f.getFileNameWithoutExtension();
            state.setProperty ("bankFile", f.getFullPathName(), nullptr); state.setProperty ("bankPreset", 0, nullptr);
            return numZones();
        }
        /** Imports one preset of an SF2 bank (replacing the zones). The bank's samples are extracted once into "<name> samples" next to the file. */
        int importSf2 (const juce::File& f, int presetIndex = 0)
        {
            SqSmp::Sf2Reader rd; if (! rd.open (f)) return 0;
            auto cacheDir = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " samples");
            auto zs = rd.zonesFor (presetIndex, cacheDir); if (zs.isEmpty()) return 0;
            clearZones();
            for (auto& z : zs)
            {
                const int zi = addZone (z.file, z.root, z.loKey, z.hiKey, z.loVel, z.hiVel);
                auto v = zone (zi);
                v.setProperty ("tune", z.tune, nullptr); v.setProperty ("gain", (double) z.gainDb, nullptr); v.setProperty ("pan", (double) z.pan, nullptr);
                if (z.loopMode > 0 && z.loopEnd > z.loopStart) { v.setProperty ("loopMode", 1, nullptr); v.setProperty ("loopStart", z.loopStart, nullptr); v.setProperty ("loopEnd", z.loopEnd, nullptr); v.setProperty ("xfade", 16, nullptr); }
                v.setProperty ("attack", (double) z.attack, nullptr); v.setProperty ("decay", (double) z.decay, nullptr); v.setProperty ("sustain", (double) z.sustain, nullptr); v.setProperty ("release", (double) z.release, nullptr);
            }
            auto ps = rd.presets();
            bankName = f.getFileNameWithoutExtension() + (presetIndex < ps.size() ? " - " + ps[presetIndex].name : juce::String());
            state.setProperty ("bankFile", f.getFullPathName(), nullptr); state.setProperty ("bankPreset", presetIndex, nullptr);
            return numZones();
        }
        static juce::Array<SqSmp::Sf2Preset> sf2Presets (const juce::File& f)   { SqSmp::Sf2Reader rd; return rd.open (f) ? rd.presets() : juce::Array<SqSmp::Sf2Preset>(); }

        //==========================================================================
        void noteOn (int note, float vel) override
        {
            const int v127 = juce::jlimit (1, 127, (int) std::lround (vel * 127.0f));
            // candidates: zones covering the key and velocity; round-robin groups rotate per key
            juce::Array<int> hits; int rrMax = 0;
            for (int i = 0; i < numZones(); ++i)
            {
                auto z = zone (i);
                if (note < (int) z.getProperty ("loKey", 0) || note > (int) z.getProperty ("hiKey", 127) || v127 < (int) z.getProperty ("loVel", 1) || v127 > (int) z.getProperty ("hiVel", 127)) continue;
                hits.add (i); rrMax = juce::jmax (rrMax, (int) z.getProperty ("rr", 0));
            }
            if (hits.isEmpty()) return;
            int rrPick = 0;
            if (rrMax > 0) { rrPick = (rrCounter[(size_t) note] % rrMax) + 1; rrCounter[(size_t) note]++; }
            for (int zi : hits)
            {
                auto z = zone (zi);
                if (rrMax > 0 && (int) z.getProperty ("rr", 0) != 0 && (int) z.getProperty ("rr", 0) != rrPick) continue;
                auto smp = sampleAt (zi); if (smp == nullptr) continue;
                auto* v = allocateVoice(); if (v == nullptr) return;
                v->sample = smp; v->unisonGain = 1.0f; v->bend = 0;
                v->start (note, vel, 0.0f, 0.0f);
                const int root = (int) z.getProperty ("root", 60);
                const double semis = ((double) (note - root) * bank[keyTrack]) + (double) z.getProperty ("tune", 0) / 100.0 + bank[tune];
                v->pitchRatio = std::pow (2.0, semis / 12.0);
                v->hz = v->targetHz = SqDsp::noteHz (note);   // pitch bend / glide act through hz vs targetHz
                v->pos = 0; v->loop = (int) z.getProperty ("loopMode", 0) > 0; v->loopStart = (int) z.getProperty ("loopStart", -1); v->loopEnd = (int) z.getProperty ("loopEnd", -1); v->xfade = (int) z.getProperty ("xfade", 0);
                if (v->loop && (v->loopStart < 0 || v->loopEnd <= v->loopStart)) { v->loopStart = 0; v->loopEnd = smp->buffer.getNumSamples() - 1; }
                if (v->loop) { v->loopEnd = juce::jmin (v->loopEnd, smp->buffer.getNumSamples() - 1); v->xfade = juce::jmin (v->xfade, v->loopEnd - v->loopStart - 1, v->loopStart); }
                v->zoneGain = juce::Decibels::decibelsToGain ((float) (double) z.getProperty ("gain", 0.0));
                const float pan = juce::jlimit (-1.0f, 1.0f, (float) (double) z.getProperty ("pan", 0.0));
                v->gl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi); v->gr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                v->eA = z.hasProperty ("attack") ? juce::jmax (bank[aA], (float) (double) z.getProperty ("attack")) : bank[aA];
                v->eD = z.hasProperty ("decay") ? (float) (double) z.getProperty ("decay") : bank[aD];
                v->eS = z.hasProperty ("sustain") ? juce::jmin (bank[aS], (float) (double) z.getProperty ("sustain")) : bank[aS];
                v->eR = z.hasProperty ("release") ? (float) (double) z.getProperty ("release") : bank[aR];
                lastZone = zi;
            }
            lastNoteNumber = note;
        }
        void updateChunk (const ModValues& m) override
        {
            for (auto& v : voices)
            {
                v.s2.cutoff = bank[cutoff]; v.s2.res = bank[res]; v.s2.fEnv = bank[fEnv]; v.s2.velSens = bank[velSens]; v.s2.fType = (int) std::lround (bank[fType]);
                v.s2.fA = bank[fA]; v.s2.fD = bank[fD]; v.s2.fS = bank[fS]; v.s2.fR = bank[fR];
                v.s2.gain = juce::Decibels::decibelsToGain (bank[level]) * juce::jmax (0.0f, 1.0f + modFor ("amp", m));
                v.cutoffMul = std::pow (2.0f, modFor ("cutoff", m) * 5.0f);
            }
        }
        void renderMaster (float*, float*, int) override {}
        std::shared_ptr<SqSmp::Sample> sampleAt (int zi)
        {
            auto path = zone (zi).getProperty ("file").toString();
            const juce::SpinLock::ScopedTryLockType sl (sampleLock); if (! sl.isLocked()) return nullptr;
            auto it = loaded.find (path); return it != loaded.end() ? it->second : nullptr;
        }
        void reloadAll()
        {
            std::map<juce::String, std::shared_ptr<SqSmp::Sample>> next;
            for (int i = 0; i < numZones(); ++i)
            {
                auto path = zone (i).getProperty ("file").toString();
                if (next.count (path)) continue;
                if (auto s = SqSmp::SampleCache::get().load (juce::File (path))) next[path] = s;
            }
            const juce::SpinLock::ScopedLockType sl (sampleLock); loaded = std::move (next);
        }
        int lastZone = -1;
    private:
        std::map<juce::String, std::shared_ptr<SqSmp::Sample>> loaded; juce::SpinLock sampleLock; std::array<int, 128> rrCounter {};
        struct ZoneListener : public juce::ValueTree::Listener
        {
            SqChrysalis2Plugin& o; explicit ZoneListener (SqChrysalis2Plugin& p) : o (p) {}
            void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override   { if (v.getType() == juce::Identifier ("ZONE") && id == juce::Identifier ("file")) o.scheduleReload(); }
            void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree&) override   { if (p.getType() == juce::Identifier ("ZONES")) o.scheduleReload(); }
            void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree&, int) override   { if (p.getType() == juce::Identifier ("ZONES")) o.scheduleReload(); }
        } zoneListener { *this };
        void scheduleReload()
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread()) { reloadAll(); return; }
            juce::MessageManager::callAsync ([sp = juce::WeakReference<SqChrysalis2Plugin> (this)] { if (sp != nullptr) sp->reloadAll(); });
        }
        JUCE_DECLARE_WEAK_REFERENCEABLE (SqChrysalis2Plugin)
    };

    /** Chrysalis SF: the same engine as its own channel type that opens into SF2 / SFZ banks. */
    class SqChrysalisSFPlugin : public SqChrysalis2Plugin
    {
    public:
        static constexpr const char* xmlTypeName = "sqmChrysalisSF";
        static const char* getPluginName()   { return NEEDS_TRANS("Chrysalis SF"); }
        juce::String getName() const override { return "Chrysalis SF"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "ChrySF"; }
        juce::String getSelectableDescription() override { return "Chrysalis SF SoundFont / SFZ player"; }
        SqChrysalisSFPlugin (te::PluginCreationInfo info) : SqChrysalis2Plugin (info) {}
        ~SqChrysalisSFPlugin() override {}
    };

    //==========================================================================
    /** A slice voice: plays a range of a deck's sample, optionally reversed / pitched / filtered, with a decay. */
    struct SliceVoice : public VoiceBase
    {
        std::shared_ptr<SqSmp::Sample> sample; double pos = 0, rate = 1; int endPos = 0, startPos = 0; bool reverse = false;
        float gain = 1.0f, gl = 1.0f, gr = 1.0f, decayCoef = 1.0f, env = 1.0f; float cut = 20000.0f; int startDelay = 0; SqDsp::SVF svfL, svfR; int deck = 0;
        void start (int n, float vel, float, float)   { startBase (n, vel, 0, 0); env = 1.0f; svfL.reset(); svfR.reset(); }
        void release() override   { VoiceBase::release(); }
        inline void render (float* l, float* r, int n)
        {
            if (sample == nullptr) { active = false; return; }
            const auto& buf = sample->buffer; const int len = buf.getNumSamples(), nch = buf.getNumChannels();
            const bool filt = cut < 19500.0f; if (filt) { svfL.set (sr, cut, 0.7f); svfR.set (sr, cut, 0.7f); }
            const double step = sample->sampleRate / sr * rate;
            const float relCoef = releasing ? std::exp (-4.6f / (0.012f * (float) sr)) : 1.0f;
            // slices overlap by 'fade' samples: slice i plays 'fade' samples past its end while fading out, slice i + 1 fades in
            // over the same samples, so consecutive slices sum back to the original audio (and cuts are click-free)
            if (startDelay > 0) { const int skip = juce::jmin (startDelay, n); startDelay -= skip; l += skip; r += skip; n -= skip; if (n <= 0) return; }
            const int fade = 32;
            const int playEnd = reverse ? endPos : juce::jmin (len, endPos + fade), playStart = reverse ? juce::jmax (0, startPos - fade) : startPos;
            for (int i = 0; i < n; ++i)
            {
                if (pos < playStart || pos >= playEnd || pos > len - 1 || pos < 0) { active = false; return; }
                const int i0 = (int) pos; const float fr = (float) (pos - i0); const int i1 = juce::jmin (len - 1, i0 + 1);
                float xl = SqSmp::hermite (buf, 0, i0, fr, len);
                float xr = nch > 1 ? SqSmp::hermite (buf, 1, i0, fr, len) : xl;
                const double p = pos;
                pos += reverse ? -step : step;
                env *= decayCoef * relCoef; if (env < 0.0005f) { active = false; return; }
                float edge = 1.0f;
                if (! reverse)
                {
                    if (startPos > 0 && p < startPos + fade) edge = (float) (p - startPos) / (float) fade;          // fade in (not at the very start of the file)
                    if (p >= endPos && endPos < len) edge = juce::jmin (edge, 1.0f - (float) (p - endPos) / (float) fade);   // fade out past the slice end
                }
                else
                {
                    if (endPos < len && p > endPos - fade) edge = (float) (endPos - p) / (float) fade;
                    if (p < startPos && startPos > 0) edge = juce::jmin (edge, 1.0f - (float) (startPos - p) / (float) fade);
                }
                const float g = gain * env * juce::jlimit (0.0f, 1.0f, edge) * unisonGain;
                xl *= g; xr *= g;
                if (filt) { xl = svfL.lowpass (xl); xr = svfR.lowpass (xr); }
                l[i] += xl * gl; r[i] += xr * gr;
            }
        }
    };

    //==========================================================================
    /** Mandible: two decks of slices. Deck A plays from C1 (36), deck B from C4 (72). The Lite variant has one deck. */
    class SqMandiblePlugin : public SynthPluginBase<SliceVoice, 24>
    {
    public:
        enum P { level, aLevel, bLevel, crossfade, aPitch, bPitch, aDecay, bDecay, aCutoff, bCutoff, aStretch, bStretch, aReverseAll, bReverseAll, numParams };
        static constexpr const char* xmlTypeName = "sqmMandible";
        static const char* getPluginName()   { return NEEDS_TRANS("Mandible"); }
        juce::String getName() const override { return "Mandible"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Mandbl"; }
        juce::String getSelectableDescription() override { return "Mandible two-deck slicer"; }
        static constexpr int maxSlices = 64, deckBaseNote[2] = { 36, 72 };
        static juce::StringArray sliceModeNames()   { return { "TRANSIENTS", "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" }; }

        SqMandiblePlugin (te::PluginCreationInfo info, bool lite = false) : SynthPluginBase<SliceVoice, 24> (info), isLite (lite)
        {
            bank.create (*this, {
                { "level", "Level", -24, 12, 0, 1, " dB", 1 }, { "aLevel", "Deck A Level", -24, 12, 0, 1, " dB", 1 }, { "bLevel", "Deck B Level", -24, 12, 0, 1, " dB", 1 }, { "crossfade", "Crossfade A/B", 0, 1, 0.5f, 1, "", 2 },
                { "aPitch", "Deck A Pitch", -24, 24, 0, 1, " st", 1 }, { "bPitch", "Deck B Pitch", -24, 24, 0, 1, " st", 1 }, { "aDecay", "Deck A Decay", 0.01f, 10, 10, 0.3f, " s", 2 }, { "bDecay", "Deck B Decay", 0.01f, 10, 10, 0.3f, " s", 2 },
                { "aCutoff", "Deck A Cutoff", 40, 20000, 20000, 0.3f, " Hz", 0 }, { "bCutoff", "Deck B Cutoff", 40, 20000, 20000, 0.3f, " Hz", 0 }, { "aStretch", "Deck A Stretch", 0, 1, 0, 1, "", 0 }, { "bStretch", "Deck B Stretch", 0, 1, 0, 1, "", 0 },
                { "aReverseAll", "Deck A Reverse", 0, 1, 0, 1, "", 0 }, { "bReverseAll", "Deck B Reverse", 0, 1, 0, 1, "", 0 } });
            for (int d = 0; d < 2; ++d) decks[d] = state.getOrCreateChildWithName (d == 0 ? "DECK_A" : "DECK_B", nullptr);
            state.addListener (&deckListener);
            quality = 0; defaultQuality = 0;   // slices play 1:1 in Eco (no resampling through the HD decimator)
            reloadAll();
        }
        ~SqMandiblePlugin() override   { state.removeListener (&deckListener); }
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            SynthPluginBase::restorePluginStateFromValueTree (v);
            for (int d = 0; d < 2; ++d)
            {
                const juce::Identifier id (d == 0 ? "DECK_A" : "DECK_B");
                if (auto src = v.getChildWithName (id); src.isValid()) { auto old = state.getChildWithName (id); if (old.isValid()) state.removeChild (old, nullptr); state.addChild (src.createCopy(), -1, nullptr); decks[d] = state.getChildWithName (id); }
            }
            reloadAll();
        }
        juce::StringArray modDestinations() const override   { return { "pitch", "cutoff", "amp", "pan" }; }
        const bool isLite; int numDecks() const   { return isLite ? 1 : 2; }

        //==========================================================================
        // Deck: DECK_x(file, bpm, bars, sliceMode, sensitivity) > SLICE(start, end, pitch, reverse, gain, cutoff, decay, mute)
        std::array<juce::ValueTree, 2> decks;
        juce::File deckFile (int d) const   { return juce::File (decks[(size_t) d].getProperty ("file").toString()); }
        int numSlices (int d) const         { return decks[(size_t) d].getNumChildren(); }
        juce::ValueTree slice (int d, int i) const   { return decks[(size_t) d].getChild (i); }
        double deckBpm (int d) const        { return (double) decks[(size_t) d].getProperty ("bpm", 0.0); }
        double deckSeconds (int d)          { auto s = deckSample (d); return s != nullptr ? s->buffer.getNumSamples() / s->sampleRate : 0.0; }

        /** Loads a file onto a deck and slices it. bars = 0 guesses the bar count from the length at the song tempo. */
        bool loadFile (int d, const juce::File& f, int sliceMode = 0, float sensitivity = 0.5f, int bars = 0)
        {
            auto s = SqSmp::SampleCache::get().load (f); if (s == nullptr) return false;
            auto& deck = decks[(size_t) d];
            deck.setProperty ("file", f.getFullPathName(), getUndoManager());
            const double secs = s->buffer.getNumSamples() / s->sampleRate;
            if (bars <= 0) { const double beatsAtSong = secs * bpm / 60.0; bars = juce::jmax (1, (int) std::lround (beatsAtSong / 4.0)); }
            deck.setProperty ("bars", bars, getUndoManager());
            // the tempo the loop was made at, to 0.01 BPM (a whole-sample file length puts the raw figure a hair off, e.g. 139.9995)
            deck.setProperty ("bpm", std::round (bars * 4.0 * 60.0 / juce::jmax (0.1, secs) * 100.0) / 100.0, getUndoManager());
            reloadAll();
            reslice (d, sliceMode, sensitivity);
            return true;
        }
        /** Slices the deck: mode 0 = transients (sensitivity 0..1), 1.. = grid. */
        int reslice (int d, int sliceMode, float sensitivity = 0.5f)
        {
            auto s = deckSample (d); if (s == nullptr) return 0;
            auto& deck = decks[(size_t) d];
            deck.setProperty ("sliceMode", sliceMode, getUndoManager()); deck.setProperty ("sensitivity", (double) sensitivity, getUndoManager());
            while (deck.getNumChildren() > 0) deck.removeChild (0, getUndoManager());
            const int len = s->buffer.getNumSamples(); std::vector<int> cuts { 0 };
            if (sliceMode == 0)
            {
                cuts = SqSmp::onsets (s->buffer, s->sampleRate, sensitivity, maxSlices);
            }
            else
            {
                static const double div[7] = { 0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
                const double beats = (double) deck.getProperty ("bars", 1) * 4.0, stepBeats = div[juce::jlimit (1, 6, sliceMode)];
                const int n = (int) std::lround (beats / stepBeats);
                for (int i = 1; i < n; ++i) cuts.push_back ((int) std::lround ((double) len * (double) i / (double) n));
            }
            while ((int) cuts.size() > maxSlices) cuts.pop_back();
            cuts.push_back (len);
            for (size_t i = 0; i + 1 < cuts.size(); ++i)
            {
                juce::ValueTree sl ("SLICE");
                sl.setProperty ("start", cuts[i], nullptr); sl.setProperty ("end", cuts[i + 1], nullptr); sl.setProperty ("pitch", 0.0, nullptr); sl.setProperty ("reverse", false, nullptr);
                sl.setProperty ("gain", 0.0, nullptr); sl.setProperty ("cutoff", 20000.0, nullptr); sl.setProperty ("decay", 10.0, nullptr); sl.setProperty ("mute", false, nullptr);
                deck.addChild (sl, -1, getUndoManager());
            }
            return numSlices (d);
        }
        void setSliceProperty (int d, int i, const juce::Identifier& id, const juce::var& v)   { if (auto s = slice (d, i); s.isValid()) s.setProperty (id, v, getUndoManager()); }
        /** Slice start times in beats of the deck's own tempo (for "dump to piano roll"). */
        juce::Array<double> sliceBeats (int d)
        {
            juce::Array<double> out; auto s = deckSample (d); if (s == nullptr) return out;
            const double bpmD = juce::jmax (1.0, deckBpm (d));
            for (int i = 0; i < numSlices (d); ++i) out.add ((double) (int) slice (d, i).getProperty ("start") / s->sampleRate * bpmD / 60.0);
            return out;
        }
        int sliceForNote (int note, int& deckOut) const
        {
            for (int d = 0; d < numDecks(); ++d) { const int i = note - deckBaseNote[d]; if (juce::isPositiveAndBelow (i, numSlices (d)) && (d == 1 || note < deckBaseNote[1] || isLite)) { deckOut = d; return i; } }
            return -1;
        }
        std::atomic<int> lastSlice { -1 }, lastDeck { 0 };
        void triggerSlice (int d, int i, float vel = 1.0f)   { pendingHit = ((d & 1) << 8) | (i & 0xff) | ((int) (vel * 127.0f) << 16) | 0x1000000; }

        //==========================================================================
        void noteOn (int note, float vel) override
        {
            int d = 0; const int i = sliceForNote (note, d); if (i < 0) return;
            startSlice (d, i, vel, note);
        }
        void startSlice (int d, int i, float vel, int note)
        {
            auto smp = deckSample (d); if (smp == nullptr) return;
            auto sl = slice (d, i); if (! sl.isValid() || (bool) sl.getProperty ("mute", false)) return;
            // one voice per deck slot retriggers (a slice cuts the previous slice from the same deck when played from the same key)
            for (auto& v : voices) if (v.active && v.deck == d && v.note == note) v.release();
            auto* v = allocateVoice(); if (v == nullptr) return;
            v->sample = smp; v->deck = d; v->unisonGain = 1.0f;
            v->start (note, vel, 0.0f, 0.0f);
            v->startDelay = eventOffsetInChunk * (hdActive ? 2 : 1);
            const bool rev = (bool) sl.getProperty ("reverse", false) != (bank[d == 0 ? aReverseAll : bReverseAll] > 0.5f);
            v->startPos = (int) sl.getProperty ("start"); v->endPos = (int) sl.getProperty ("end"); v->reverse = rev;
            v->pos = rev ? v->endPos - 1 : v->startPos;
            double stretch = bank[d == 0 ? aStretch : bStretch] > 0.5f && deckBpm (d) > 1.0 ? bpm / deckBpm (d) : 1.0;   // play at the song tempo
            if (std::abs (stretch - 1.0) < 2.0e-4) stretch = 1.0;   // a loop's whole-sample length makes its tempo a hair off; at the same tempo it plays 1:1
            v->rate = stretch * std::pow (2.0, ((double) sl.getProperty ("pitch", 0.0) + bank[d == 0 ? aPitch : bPitch]) / 12.0);
            const float xf = bank[crossfade]; const float deckG = isLite ? 1.0f : (d == 0 ? std::cos (xf * juce::MathConstants<float>::halfPi) : std::sin (xf * juce::MathConstants<float>::halfPi)) * 1.4142f;
            v->gain = juce::Decibels::decibelsToGain ((float) (double) sl.getProperty ("gain", 0.0) + bank[d == 0 ? aLevel : bLevel]) * juce::jmin (1.0f, deckG) * (0.2f + 0.8f * vel);
            const float dec = juce::jmin ((float) (double) sl.getProperty ("decay", 10.0), bank[d == 0 ? aDecay : bDecay]);
            v->decayCoef = dec >= 9.9f ? 1.0f : std::exp (-4.6f / (dec * (float) sr));
            v->cut = juce::jmin ((float) (double) sl.getProperty ("cutoff", 20000.0), bank[d == 0 ? aCutoff : bCutoff]);
            lastSlice = i; lastDeck = d; lastNoteNumber = note;
        }
        void updateChunk (const ModValues&) override
        {
            const int h = pendingHit.exchange (0);
            if (h) startSlice ((h >> 8) & 1, h & 0xff, ((h >> 16) & 0xff) / 127.0f, deckBaseNote[(h >> 8) & 1] + (h & 0xff));
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
        std::shared_ptr<SqSmp::Sample> deckSample (int d)   { const juce::SpinLock::ScopedTryLockType sl (sampleLock); if (! sl.isLocked()) return nullptr; return deckSamples[(size_t) d]; }
        void reloadAll()
        {
            std::array<std::shared_ptr<SqSmp::Sample>, 2> next;
            for (int d = 0; d < 2; ++d) { auto f = deckFile (d); if (f.existsAsFile()) next[(size_t) d] = SqSmp::SampleCache::get().load (f); }
            const juce::SpinLock::ScopedLockType sl (sampleLock); deckSamples = next;
        }
    private:
        std::array<std::shared_ptr<SqSmp::Sample>, 2> deckSamples; juce::SpinLock sampleLock; std::atomic<int> pendingHit { 0 };
        struct DeckListener : public juce::ValueTree::Listener
        {
            SqMandiblePlugin& o; explicit DeckListener (SqMandiblePlugin& p) : o (p) {}
            void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override   { if (id == juce::Identifier ("file") && (v.getType() == juce::Identifier ("DECK_A") || v.getType() == juce::Identifier ("DECK_B"))) o.scheduleReload(); }
        } deckListener { *this };
        void scheduleReload()
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread()) { reloadAll(); return; }
            juce::MessageManager::callAsync ([sp = juce::WeakReference<SqMandiblePlugin> (this)] { if (sp != nullptr) sp->reloadAll(); });
        }
        JUCE_DECLARE_WEAK_REFERENCEABLE (SqMandiblePlugin)
    };

    class SqMandibleLitePlugin : public SqMandiblePlugin
    {
    public:
        static constexpr const char* xmlTypeName = "sqmMandibleLite";
        static const char* getPluginName()   { return NEEDS_TRANS("Mandible Lite"); }
        juce::String getName() const override { return "Mandible Lite"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "MandLt"; }
        juce::String getSelectableDescription() override { return "Mandible Lite quick slicer"; }
        SqMandibleLitePlugin (te::PluginCreationInfo info) : SqMandiblePlugin (info, true) {}
        ~SqMandibleLitePlugin() override {}
    };
}

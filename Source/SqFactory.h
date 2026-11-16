#pragma once
// Squelchmoth factory content for the 1.8.1 samplers and slicers - all of it made from Squelchmoth's own drum engine
// (the 27 kits), nothing third-party:
//   Chrysalis 2   - "Kits | <kit>" (the twelve kit sounds on their keys) and "808 | <kit> 808" (the kit's 808, pitch-detected
//                   and played chromatically)
//   Chrysalis SF  - the same 54 sounds, delivered as real SoundFont banks (one small .sf2 per kit, written on first use)
//   Mandible / Lite - "<genre> | <kit> Loop": two-bar loops rendered from the Beat Lab genre grooves with that kit
// Everything is generated on first use into Documents/Squelchmoth/Factory and cached.
#include "SqSamplers2.h"
#include "SqHivePads.h"

namespace SqFactory
{
    /** Where factory content is written (the self-test points it at a temp folder). */
    inline std::function<juce::File()>& rootHook()   { static std::function<juce::File()> h; return h; }
    inline juce::File root()
    {
        auto d = rootHook() ? rootHook()() : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Squelchmoth").getChildFile ("Factory");
        d.createDirectory(); return d;
    }
    /** Renders a two-bar loop for a genre with a kit (set by FactoryLoops.h, which knows the Beat Lab grooves). */
    inline std::function<juce::File (const juce::String& genre, int kit, int variant)>& loopHook()   { static std::function<juce::File (const juce::String&, int, int)> h; return h; }

    inline int kitIndex (const juce::String& name)
    {
        for (int k = 0; k < DrumKit::numKits; ++k) if (name.equalsIgnoreCase (DrumKit::kits[k].name)) return k;
        return -1;
    }
    inline juce::Array<juce::File> kitFiles (int kit)
    {
        if (! SqHivePadsPlugin::kitFiles) return {};
        return SqHivePadsPlugin::kitFiles (kit);
    }

    /** Fundamental of a (mostly) periodic sound between t0 and t1 seconds: positive-going zero crossings, interpolated. */
    inline double detectHz (const juce::AudioBuffer<float>& b, double sr, double t0, double t1)
    {
        const int n = b.getNumSamples(); const int a = juce::jlimit (1, n - 1, (int) (t0 * sr)), z = juce::jlimit (a + 1, n - 1, (int) (t1 * sr));
        auto* p = b.getReadPointer (0);
        // a gentle one-pole low-pass keeps saturation harmonics from adding crossings
        std::vector<float> x ((size_t) (z - a)); float y = 0; const float c = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 250.0f / (float) sr);
        for (int i = a - (int) (0.02 * sr); i < a; ++i) if (i >= 0) y += c * (p[i] - y);
        for (int i = a; i < z; ++i) { y += c * (p[i] - y); x[(size_t) (i - a)] = y; }
        double first = -1, last = -1; int count = 0;
        for (size_t i = 1; i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const double at = (double) (i - 1) + (double) (-x[i - 1] / (x[i] - x[i - 1]));
                if (first < 0) first = at; else ++count;
                last = at;
            }
        return count > 0 ? (double) count * sr / (last - first) : 0.0;
    }
    inline juce::AudioBuffer<float> readFile (const juce::File& f, double& sr)
    {
        juce::AudioBuffer<float> b;
        if (auto s = SqSmp::SampleCache::get().load (f)) { b.makeCopyOf (s->buffer); sr = s->sampleRate; }
        return b;
    }
    /** The 808 of a kit: file, MIDI root and cents. */
    struct Pitched { juce::File file; int root = 36; int cents = 0; };
    inline Pitched kit808 (int kit)
    {
        Pitched out; auto files = kitFiles (kit); if (files.size() < DrumKit::numSounds) return out;
        out.file = files[11];
        double sr = 44100; auto b = readFile (out.file, sr);
        if (b.getNumSamples() > 0)
        {
            const double hz = detectHz (b, sr, 0.35, juce::jmin (1.2, b.getNumSamples() / sr - 0.05));
            if (hz > 15.0)
            {
                const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
                out.root = (int) std::lround (midi); out.cents = (int) std::lround ((out.root - midi) * 100.0);   // tune the zone so the root plays in tune
            }
        }
        return out;
    }

    //==========================================================================
    /** A minimal SoundFont 2 writer: 16-bit mono samples, one instrument per preset, zones with key / velocity range,
        root, fine tune, loop mode, release and attenuation. */
    struct Sf2Writer
    {
        struct Smp { juce::String name; juce::AudioBuffer<float> audio; int rate = 44100; int root = 60; int cents = 0; };
        struct Zn  { int sample = 0; int loKey = 0, hiKey = 127, loVel = 0, hiVel = 127; int root = -1; int fine = 0; float releaseS = 0.2f; float attenDb = 0.0f; };
        struct Pr  { juce::String name; int program = 0; std::vector<Zn> zones; };
        std::vector<Smp> samples; std::vector<Pr> presets;

        static void name20 (juce::MemoryOutputStream& o, const juce::String& s)
        {
            char buf[20] = { 0 }; auto u = s.toRawUTF8(); for (int i = 0; i < 19 && u[i] != 0; ++i) buf[i] = u[i];
            o.write (buf, 20);
        }
        static void chunk (juce::MemoryOutputStream& o, const char* id, const juce::MemoryBlock& data)
        {
            o.write (id, 4); o.writeInt ((int) data.getSize()); o.write (data.getData(), data.getSize()); if (data.getSize() & 1) o.writeByte (0);
        }
        static juce::MemoryBlock list (const char* type, const juce::MemoryBlock& body)
        {
            juce::MemoryOutputStream o; o.write (type, 4); o.write (body.getData(), body.getSize()); return o.getMemoryBlock();
        }
        bool write (const juce::File& f) const
        {
            // INFO
            juce::MemoryOutputStream info;
            { juce::MemoryOutputStream v; v.writeShort (2); v.writeShort (1); chunk (info, "ifil", v.getMemoryBlock()); }
            { juce::MemoryOutputStream v; v.write ("EMU8000\0", 8); chunk (info, "isng", v.getMemoryBlock()); }
            { juce::MemoryOutputStream v; v.write ("Squelchmoth Factory\0", 20); chunk (info, "INAM", v.getMemoryBlock()); }
            { juce::MemoryOutputStream v; v.write ("Squelchmoth\0", 12); chunk (info, "ICOP", v.getMemoryBlock()); }
            // sdta: each sample followed by 46 zero points
            juce::MemoryOutputStream smpl; std::vector<std::pair<juce::uint32, juce::uint32>> ranges; juce::uint32 at = 0;
            for (auto& s : samples)
            {
                const int n = s.audio.getNumSamples(); auto* p = s.audio.getReadPointer (0);
                for (int i = 0; i < n; ++i) smpl.writeShort ((short) juce::jlimit (-32768, 32767, (int) std::lround (p[i] * 32767.0f)));
                for (int i = 0; i < 46; ++i) smpl.writeShort (0);
                ranges.push_back ({ at, at + (juce::uint32) n }); at += (juce::uint32) n + 46;
            }
            juce::MemoryOutputStream sdta; chunk (sdta, "smpl", smpl.getMemoryBlock());
            // pdta
            juce::MemoryOutputStream phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
            int pbagN = 0, pgenN = 0, ibagN = 0, igenN = 0;
            for (size_t pi = 0; pi < presets.size(); ++pi)
            {
                auto& pr = presets[pi];
                name20 (phdr, pr.name); phdr.writeShort ((short) pr.program); phdr.writeShort (0); phdr.writeShort ((short) pbagN); phdr.writeInt (0); phdr.writeInt (0); phdr.writeInt (0);
                pbag.writeShort ((short) pgenN); pbag.writeShort (0); ++pbagN;
                pgen.writeShort (41); pgen.writeShort ((short) pi); ++pgenN;   // instrument
                name20 (inst, pr.name); inst.writeShort ((short) ibagN);
                for (auto& z : pr.zones)
                {
                    ibag.writeShort ((short) igenN); ibag.writeShort (0); ++ibagN;
                    auto gen = [&] (int op, int amount) { igen.writeShort ((short) op); igen.writeShort ((short) amount); ++igenN; };
                    gen (43, (z.loKey & 0xff) | ((z.hiKey & 0xff) << 8));
                    if (z.loVel != 0 || z.hiVel != 127) gen (44, (z.loVel & 0xff) | ((z.hiVel & 0xff) << 8));
                    if (z.attenDb > 0.0f) gen (48, (int) std::lround (z.attenDb * 10.0f));
                    if (z.fine != 0) gen (52, z.fine);
                    gen (38, (int) std::lround (1200.0 * std::log2 (juce::jmax (0.001, (double) z.releaseS))));
                    if (z.root >= 0) gen (58, z.root);
                    gen (53, z.sample);
                }
            }
            name20 (phdr, "EOP"); phdr.writeShort (0); phdr.writeShort (0); phdr.writeShort ((short) pbagN); phdr.writeInt (0); phdr.writeInt (0); phdr.writeInt (0);
            pbag.writeShort ((short) pgenN); pbag.writeShort (0);
            for (int i = 0; i < 10; ++i) pmod.writeByte (0);
            pgen.writeShort (0); pgen.writeShort (0);
            name20 (inst, "EOI"); inst.writeShort ((short) ibagN);
            ibag.writeShort ((short) igenN); ibag.writeShort (0);
            for (int i = 0; i < 10; ++i) imod.writeByte (0);
            igen.writeShort (0); igen.writeShort (0);
            for (size_t i = 0; i < samples.size(); ++i)
            {
                auto& s = samples[i];
                name20 (shdr, s.name); shdr.writeInt ((int) ranges[i].first); shdr.writeInt ((int) ranges[i].second); shdr.writeInt ((int) ranges[i].first); shdr.writeInt ((int) ranges[i].first);
                shdr.writeInt (s.rate); shdr.writeByte ((char) s.root); shdr.writeByte ((char) s.cents); shdr.writeShort (0); shdr.writeShort (1);
            }
            name20 (shdr, "EOS"); for (int i = 0; i < 5; ++i) shdr.writeInt (0); shdr.writeByte (0); shdr.writeByte (0); shdr.writeShort (0); shdr.writeShort (0);
            juce::MemoryOutputStream pdta;
            chunk (pdta, "phdr", phdr.getMemoryBlock()); chunk (pdta, "pbag", pbag.getMemoryBlock()); chunk (pdta, "pmod", pmod.getMemoryBlock()); chunk (pdta, "pgen", pgen.getMemoryBlock());
            chunk (pdta, "inst", inst.getMemoryBlock()); chunk (pdta, "ibag", ibag.getMemoryBlock()); chunk (pdta, "imod", imod.getMemoryBlock()); chunk (pdta, "igen", igen.getMemoryBlock());
            chunk (pdta, "shdr", shdr.getMemoryBlock());
            juce::MemoryOutputStream body; body.write ("sfbk", 4);
            chunk (body, "LIST", list ("INFO", info.getMemoryBlock())); chunk (body, "LIST", list ("sdta", sdta.getMemoryBlock())); chunk (body, "LIST", list ("pdta", pdta.getMemoryBlock()));
            juce::MemoryOutputStream riff; riff.write ("RIFF", 4); riff.writeInt ((int) body.getDataSize()); riff.write (body.getData(), body.getDataSize());
            auto tmp = f.getSiblingFile (f.getFileName() + ".tmp");
            if (! tmp.replaceWithData (riff.getData(), riff.getDataSize())) return false;
            return tmp.moveFileTo (f);
        }
    };

    /** Trims a one-shot's silent tail (below -66 dBFS) so banks stay small. */
    inline void trimTail (juce::AudioBuffer<float>& b, double sr)
    {
        const float thr = juce::Decibels::decibelsToGain (-66.0f); int last = 0;
        for (int i = b.getNumSamples() - 1; i >= 0; --i) if (std::abs (b.getSample (0, i)) > thr) { last = i; break; }
        const int keep = juce::jmin (b.getNumSamples(), last + (int) (0.01 * sr) + 1);
        b.setSize (1, juce::jmax (64, keep), true);
        const int fade = juce::jmin (keep, (int) (0.005 * sr)); for (int i = 0; i < fade; ++i) b.setSample (0, keep - fade + i, b.getSample (0, keep - fade + i) * (1.0f - (float) i / (float) fade));
    }

    /** Factory/SoundFonts/<kit>.sf2: preset 0 = the kit on its keys, preset 1 = the kit's 808 across the keyboard. */
    inline juce::File kitBank (int kit)
    {
        kit = juce::jlimit (0, DrumKit::numKits - 1, kit);
        auto f = root().getChildFile ("SoundFonts").getChildFile (juce::String (DrumKit::kits[kit].name) + ".sf2");
        if (f.existsAsFile() && f.getSize() > 1000) return f;
        f.getParentDirectory().createDirectory();
        auto files = kitFiles (kit); if (files.size() < DrumKit::numSounds) return {};
        Sf2Writer w;
        Sf2Writer::Pr kitP; kitP.name = DrumKit::kits[kit].name; kitP.program = 0;
        for (int i = 0; i < DrumKit::numSounds; ++i)
        {
            double sr = 44100; Sf2Writer::Smp s; s.name = juce::String (DrumKit::sounds[i].name); s.audio = readFile (files[i], sr); s.rate = (int) sr; s.root = DrumKit::sounds[i].note;
            if (s.audio.getNumSamples() == 0) return {};
            trimTail (s.audio, sr);
            w.samples.push_back (std::move (s));
            Sf2Writer::Zn z; z.sample = i; z.loKey = z.hiKey = DrumKit::sounds[i].note; z.root = DrumKit::sounds[i].note; z.releaseS = 3.0f; z.attenDb = -DrumKit::gainsDb[i];
            kitP.zones.push_back (z);
        }
        w.presets.push_back (kitP);
        auto p8 = kit808 (kit);
        Sf2Writer::Pr subP; subP.name = juce::String (DrumKit::kits[kit].name) + " 808"; subP.program = 1;
        Sf2Writer::Zn z; z.sample = 11; z.loKey = 0; z.hiKey = 127; z.root = p8.root; z.fine = p8.cents; z.releaseS = 0.15f; subP.zones.push_back (z);
        w.presets.push_back (subP);
        return w.write (f) ? f : juce::File();
    }

    //==========================================================================
    // Preset names (stable C strings for the Signature preset lists)
    struct Mandible { const char* genre; const char* kits[3]; };
    inline const std::vector<Mandible>& mandibleTable()
    {
        static const std::vector<Mandible> t {
            { "Trap",            { "Neon Trap", "Chrome Trap", "Rage Red" } },
            { "Drill",           { "Drill Smoke", "Grime Concrete", "Chrome Trap" } },
            { "Boom Bap",        { "Boom Bap Crates", "Velvet Lo-Fi", "Dust Acoustic" } },
            { "Lo-Fi",           { "Velvet Lo-Fi", "Neo-Soul Dust", "Plugg Pillow" } },
            { "R&B",             { "R&B Velvet", "Neo-Soul Dust", "Plugg Pillow" } },
            { "Afrobeats",       { "Afro Sun", "Amapiano Log", "Baile Funk" } },
            { "Amapiano",        { "Amapiano Log", "Afro Sun", "House Silk" } },
            { "Reggaeton",       { "Dembow Heat", "Baile Funk", "Afro Sun" } },
            { "Jersey Club",     { "Jersey Bounce", "Hyperpop Pink", "Rage Red" } },
            { "House",           { "House Silk", "Garage Swing", "Hive 909" } },
            { "Techno",          { "Techno Steel", "Hive 909", "Mothwing 808" } },
            { "Phonk",           { "Phonk Cowbell", "Memphis Tape", "Mothwing 808" } },
            { "Pop",             { "Hyperpop Pink", "Hive 909", "Dust Acoustic" } },
            { "Drum & Bass",     { "Garage Swing", "Grime Concrete", "Hive 909" } },
            { "Dancehall",       { "Dembow Heat", "Mothwing 808", "Baile Funk" } },
            { "Rock",            { "Rock Room", "Dust Acoustic", "Gospel Live" } },
            { "Funk / Neo-Soul", { "Neo-Soul Dust", "Gospel Live", "Bossa Brush" } } };
        return t;
    }
    inline const char* keep (const juce::String& s)   { static std::deque<std::string> pool; pool.push_back (s.toStdString()); return pool.back().c_str(); }

    inline std::vector<Signature::Preset> presetsFor (const juce::String& instrument)
    {
        std::vector<Signature::Preset> out;
        auto P = [&] (const juce::String& n, std::vector<Signature::PV> v, bool adv) { Signature::Preset p { keep (n), std::move (v), adv }; out.push_back (p); };
        if (instrument == "Chrysalis 2" || instrument == "Chrysalis SF")
        {
            for (int k = 0; k < DrumKit::numKits; ++k) P (juce::String ("Kits | ") + DrumKit::kits[k].name, { { "aR", 3.0f, false }, { "keyTrack", 1.0f, false } }, k >= 17);
            for (int k = 0; k < DrumKit::numKits; ++k) P (juce::String ("808 | ") + DrumKit::kits[k].name + " 808", { { "aR", 0.15f, false }, { "aS", 1.0f, false } }, k >= 17);
        }
        else if (instrument == "Mandible" || instrument == "Mandible Lite")
        {
            for (auto& m : mandibleTable())
                for (int i = 0; i < 3; ++i)
                    P (juce::String (m.genre) + " | " + m.kits[i] + " Loop", { { "aStretch", 1.0f, false }, { "bStretch", 1.0f, false } }, i > 0);
        }
        return out;
    }

    //==========================================================================
    /** Loads the factory content a preset name points at. Returns true when the name was a factory preset. */
    inline bool load (te::Plugin& p, const juce::String& presetName)
    {
        const auto cat = presetName.upToFirstOccurrenceOf (" | ", false, false), item = presetName.fromFirstOccurrenceOf (" | ", false, false);
        if (auto sf = dynamic_cast<SqSynth::SqChrysalisSFPlugin*> (&p))
        {
            const bool is808 = cat == "808"; const int kit = kitIndex (is808 ? item.upToLastOccurrenceOf (" 808", false, false) : item);
            if (kit < 0) return false;
            auto bank = kitBank (kit); if (bank == juce::File()) return false;
            return sf->importSf2 (bank, is808 ? 1 : 0) > 0;
        }
        if (auto c = dynamic_cast<SqSynth::SqChrysalis2Plugin*> (&p))
        {
            const bool is808 = cat == "808"; const int kit = kitIndex (is808 ? item.upToLastOccurrenceOf (" 808", false, false) : item);
            if (kit < 0) return false;
            auto files = kitFiles (kit); if (files.size() < DrumKit::numSounds) return false;
            c->clearZones();
            if (is808)
            {
                auto p8 = kit808 (kit);
                const int zi = c->addZone (p8.file, p8.root, 0, 127);
                c->setZoneProperty (zi, "tune", p8.cents); c->setZoneProperty (zi, "name", juce::String (DrumKit::kits[kit].name) + " 808");
            }
            else
                for (int i = 0; i < DrumKit::numSounds; ++i)
                {
                    const int n = DrumKit::sounds[i].note; const int zi = c->addZone (files[i], n, n, n);
                    c->setZoneProperty (zi, "gain", (double) DrumKit::gainsDb[i]); c->setZoneProperty (zi, "name", juce::String (DrumKit::sounds[i].name));
                }
            c->bankName = juce::String (DrumKit::kits[kit].name) + (is808 ? " 808" : "");
            c->state.setProperty ("bankFile", juce::var(), nullptr);
            return true;
        }
        if (auto m = dynamic_cast<SqSynth::SqMandiblePlugin*> (&p))
        {
            if (! loopHook()) return false;
            const auto kitName = item.upToLastOccurrenceOf (" Loop", false, false);
            const int kit = kitIndex (kitName); if (kit < 0) return false;
            auto a = loopHook() (cat, kit, 0); if (a == juce::File()) return false;
            m->loadFile (0, a, 0, 0.5f, 2);
            if (m->numDecks() > 1)
            {
                // deck B: the same groove's second variation on the genre's next kit - crossfade or play both
                int kitB = kit;
                for (auto& row : mandibleTable()) if (cat == row.genre) for (int i = 0; i < 3; ++i) if (kitName == row.kits[i]) kitB = kitIndex (row.kits[(i + 1) % 3]);
                if (auto b = loopHook() (cat, juce::jmax (0, kitB), 1); b != juce::File()) m->loadFile (1, b, 0, 0.5f, 2);
            }
            return true;
        }
        return false;
    }
}

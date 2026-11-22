#pragma once
// Squelchmoth 1.51 - where captures (models made by measuring a unit: see SqCapture.h) are kept: one small file each in the
// user's data folder, shared by every session.

#include <JuceHeader.h>
#include "SqCapture.h"

namespace CaptureStore
{
    inline juce::File folder()
    {
        const auto over = juce::SystemStats::getEnvironmentVariable ("SQUELCH_CAPTURES", {});
        return over.isNotEmpty() ? juce::File (over) : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Squelchmoth").getChildFile ("Captures");
    }
    inline juce::File fileFor (const juce::String& name) { return folder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".sqcapture"); }
    inline juce::StringArray list() { juce::StringArray names; for (auto& f : folder().findChildFiles (juce::File::findFiles, false, "*.sqcapture")) names.add (f.getFileNameWithoutExtension()); names.sortNatural(); return names; }
    inline juce::String pack (const std::vector<float>& v) { return juce::Base64::toBase64 (v.data(), v.size() * sizeof (float)); }
    inline std::vector<float> unpack (const juce::String& s) { juce::MemoryOutputStream out; juce::Base64::convertFromBase64 (out, s); std::vector<float> v (out.getDataSize() / sizeof (float)); if (! v.empty()) std::memcpy (v.data(), out.getData(), v.size() * sizeof (float)); return v; }

    inline bool save (const juce::String& name, const SqCapture::Model& m, juce::String& err)
    {
        if (! m.valid()) { err = "There is no model to save."; return false; }
        auto* o = new juce::DynamicObject(); juce::var v (o);
        o->setProperty ("format", 1); o->setProperty ("name", name.trim()); o->setProperty ("rate", m.rate); o->setProperty ("level", m.level); o->setProperty ("order", m.order); o->setProperty ("taps", m.taps); o->setProperty ("pre", m.pre);
        o->setProperty ("frontPeak", m.frontPeak); o->setProperty ("latency", m.latency); o->setProperty ("gainDbAt1k", m.gainDbAt1k); o->setProperty ("quietMissDb", m.quietMissDb); o->setProperty ("quietMissPlainDb", m.quietMissPlainDb);
        juce::Array<juce::var> hs; for (double h : m.harmonicDb) hs.add (h); o->setProperty ("harmonicDb", hs);
        juce::Array<juce::var> gs; for (auto& g : m.g) gs.add (pack (g)); o->setProperty ("filters", gs); if (! m.front.empty()) o->setProperty ("front", pack (m.front));
        const auto f = fileFor (name); if (! f.getParentDirectory().createDirectory()) { err = "Couldn't create " + f.getParentDirectory().getFullPathName(); return false; }
        if (! f.replaceWithText (juce::JSON::toString (v, true))) { err = "Couldn't write " + f.getFullPathName(); return false; }
        return true;
    }
    inline bool load (const juce::String& name, SqCapture::Model& m)
    {
        const auto f = fileFor (name); if (! f.existsAsFile()) return false; const auto v = juce::JSON::parse (f); if (! v.isObject()) return false;
        m = {}; m.rate = (double) v["rate"]; m.level = (double) v["level"]; m.order = (int) v["order"]; m.taps = (int) v["taps"]; m.pre = (int) v["pre"]; m.frontPeak = (double) v["frontPeak"]; m.latency = (int) v["latency"];
        m.gainDbAt1k = (double) v["gainDbAt1k"]; m.quietMissDb = (double) v["quietMissDb"]; m.quietMissPlainDb = (double) v["quietMissPlainDb"];
        if (auto* hs = v["harmonicDb"].getArray()) for (auto& h : *hs) m.harmonicDb.push_back ((double) h);
        if (auto* gs = v["filters"].getArray()) for (auto& g : *gs) m.g.push_back (unpack (g.toString()));
        if (v.hasProperty ("front")) m.front = unpack (v["front"].toString());
        for (auto& g : m.g) if ((int) g.size() != m.taps) return false;
        return m.valid() && m.rate > 0 && m.level > 0;
    }
    inline bool writeMonoWav (const juce::File& f, const std::vector<float>& x, double rate)
    {
        f.deleteFile(); std::unique_ptr<juce::FileOutputStream> out (f.createOutputStream()); if (out == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> w (juce::WavAudioFormat().createWriterFor (out.get(), rate, 1, 24, {}, 0)); if (w == nullptr) return false; out.release();
        const float* chans[1] = { x.data() }; return w->writeFromFloatArrays (chans, 1, (int) x.size());
    }
    // the recall sheet: notes kept inside the capture's own file, and a photo beside it
    inline juce::String notes (const juce::String& name) { const auto v = juce::JSON::parse (fileFor (name)); return v.isObject() ? v["notes"].toString() : juce::String(); }
    inline bool setNotes (const juce::String& name, const juce::String& text) { const auto f = fileFor (name); auto v = juce::JSON::parse (f); if (! v.isObject()) return false; v.getDynamicObject()->setProperty ("notes", text); return f.replaceWithText (juce::JSON::toString (v, true)); }
    inline juce::File photo (const juce::String& name) { for (auto* ext : { ".jpg", ".png" }) { const auto f = fileFor (name).withFileExtension (juce::String (".photo") + ext); if (f.existsAsFile()) return f; } return {}; }
    inline bool setPhoto (const juce::String& name, const juce::File& from)
    {
        if (! fileFor (name).existsAsFile() || ! from.existsAsFile() || ! juce::ImageFileFormat::loadFrom (from).isValid()) return false;
        const bool png = from.hasFileExtension ("png"); photo (name).deleteFile(); return from.copyFileTo (fileFor (name).withFileExtension (png ? ".photo.png" : ".photo.jpg"));
    }
    inline bool remove (const juce::String& name) { const auto f = fileFor (name); photo (name).deleteFile(); return f.existsAsFile() && f.deleteFile(); }
}

// Squelchmoth 1.53 - vinyl pre-master check and line-up tones (included inside the tool server class).
static SqVinyl::Format vinylFormat (const juce::String& s) { const auto t = s.toLowerCase().retainCharacters ("0123456789"); if (t.startsWith ("1245")) return SqVinyl::Format::lp45; if (t.startsWith ("745")) return SqVinyl::Format::seven45; if (t.startsWith ("733")) return SqVinyl::Format::seven33; return SqVinyl::Format::lp33; }
void registerVinylTools()
{
    add ("vinyl_check",
         "Vinyl pre-master check: measures one SIDE of a record before it goes to be cut and says what would cause trouble. By default it renders this session's mix from start to end (the session should hold one side); "
         "or give path to a finished audio file. format: 12-33 (default), 12-45, 7-45, 7-33. start_seconds / end_seconds narrow the range. "
         "It reports: side length against a pressing plant's published guidance (12-inch at 33: 18 min ideal, 20 still good, 22 trouble; at 45: 12 / 14 / 15; 7-inch: 4.5 min at 45, 6 at 33), whether the bass is centred (under 150 Hz), "
         "out-of-phase passages, hot highs in the 5-10 kHz band (sibilance), rumble below 30 Hz, a lot above 16 kHz, and whether the side gets louder toward its end. "
         "Where each measurement is called a problem is this program's judgement, not a standard; the cutting engineer has the last word. Fix what it finds with the mix tools, then run it again.",
         R"json({"type":"object","properties":{"format":{"type":"string","enum":["12-33","12-45","7-45","7-33"]},"path":{"type":"string"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             std::vector<float> L, R; double fs = 0; juce::String err, what; const auto fmt = vinylFormat (str (a, "format"));
             if (str (a, "path").isNotEmpty())
             {
                 const juce::File f (str (a, "path")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName());
                 if (! RefListen::readStereo (f, 0.0, 0.0, L, R, fs, err)) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("Couldn't read that file.")); what = f.getFileName();
             }
             else
             {
                 auto& e = *session.edit; e.getTransport().stop (false, false); const double from = juce::jmax (0.0, num (a, "start_seconds", 0.0)), to = a.hasProperty ("end_seconds") ? num (a, "end_seconds", 0.0) : e.getLength().inSeconds();
                 if (to <= from + 0.5) return ToolResult::error ("Nothing to measure in that range (the session is empty or the range is too short)");
                 if (! adviseRenderAudio ({}, { te::TimePosition::fromSeconds (from), te::TimePosition::fromSeconds (to) }, true, L, R, fs, err)) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("Couldn't render the mix."));
                 what = "this session's mix, " + juce::String (from, 1) + " to " + juce::String (to, 1) + " s";
             }
             if (L.size() < (size_t) (fs / 2) || R.size() != L.size()) return ToolResult::error ("There is no sound there to check.");
             const auto r = SqVinyl::analyze (L.data(), R.data(), (int) L.size(), fs); const auto adv = SqVinyl::advise (r, fmt); int problems = 0; for (auto& f : adv) problems += f.problem ? 1 : 0;
             juce::String s = "Vinyl check of " + what + " as one " + SqVinyl::lengthsFor (fmt).name + " side (peak " + juce::String (r.peakDb, 1) + " dBFS): " + (problems == 0 ? juce::String ("nothing that should trouble the cut.") : juce::String (problems) + (problems == 1 ? " thing to fix." : " things to fix."));
             for (auto& f : adv) s << "\n" << (f.problem ? "- FIX: " : "- ok: ") << juce::String (f.text);
             s << "\nDeliver 24-bit WAV files, one per side, with the loudest songs first. The limits used here are this program's own judgement except the side lengths (a pressing plant's published figures); your cutting engineer has the last word.";
             return ToolResult::ok (s);
         });
    add ("calibration_tones",
         "Line-up tones: adds a track 'Line-up Tones' holding steady sine tones one after another (and pink noise at the end), for lining up a tape machine, an outboard chain, meters or monitors. "
         "tones: frequencies in Hz (default 1000, 10000, 100, 15000, 50); seconds each (default 10); level_db (peak of the sines and rms of the noise, default -18 dBFS - use whatever reference level your studio works to); "
         "pink false leaves the noise out; start_seconds (default 0). The tones measure exactly what is asked (checked to a hundredth of a dB).",
         R"json({"type":"object","properties":{"tones":{"type":"array","items":{"type":"number"}},"seconds":{"type":"number"},"level_db":{"type":"number"},"pink":{"type":"boolean"},"start_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             auto& e = *session.edit; const double fs = TimeFmt::sampleRate (e), secs = juce::jlimit (1.0, 120.0, num (a, "seconds", 10.0)), level = juce::jlimit (-60.0, 0.0, num (a, "level_db", -18.0));
             std::vector<double> hz; if (auto* arr = a["tones"].getArray()) for (auto& v : *arr) { const double f = (double) v; if (f >= 10.0 && f < 0.45 * fs) hz.push_back (f); } if (hz.empty()) hz = { 1000.0, 10000.0, 100.0, 15000.0, 50.0 };
             if (hz.size() > 24) return ToolResult::error ("At most 24 tones.");
             std::vector<float> all; juce::String list; const int gap = (int) (0.5 * fs);
             for (double f : hz) { const auto t = SqVinyl::tone (f, secs, level, fs); all.insert (all.end(), t.begin(), t.end()); all.insert (all.end(), (size_t) gap, 0.0f); list << (list.isEmpty() ? "" : ", ") << (f >= 1000.0 ? juce::String (f / 1000.0) + " kHz" : juce::String (f) + " Hz"); }
             const bool noise = flag (a, "pink", true); if (noise) { const auto p = SqVinyl::pink (secs, level, fs); all.insert (all.end(), p.begin(), p.end()); }
             const auto base = session.getFile().existsAsFile() ? session.getFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelchmoth");
             const auto folder = base.getChildFile ("Tones"); folder.createDirectory(); const auto wav = folder.getNonexistentChildFile ("line-up-tones", ".wav", false);
             if (! CaptureStore::writeMonoWav (wav, all, fs)) return ToolResult::error ("Couldn't write the tones to " + wav.getFullPathName());
             juce::var t (new juce::DynamicObject()); set (t, "kind", "audio"); set (t, "name", "Line-up Tones"); const int idx = firstNumber (call ("add_track", t).text); if (idx <= 0) return ToolResult::error ("Couldn't add a track for the tones");
             juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", wav.getFullPathName()); set (im, "start_seconds", juce::jmax (0.0, num (a, "start_seconds", 0.0)));
             auto r = call ("import_audio", im); if (r.isError) return r;
             return ToolResult::ok ("Line-up tones are on track " + juce::String (idx) + " ('Line-up Tones'): " + list + ", " + juce::String (secs, 0) + " s each at " + juce::String (level, 1) + " dBFS" + (noise ? ", then pink noise at " + juce::String (level, 1) + " dBFS rms" : juce::String())
                                    + ". The track's fader and pan law change what reaches the output: leave the fader at 0 dB, and measure at the output if the level must be exact. Mute or delete the track before bouncing.");
         });
}

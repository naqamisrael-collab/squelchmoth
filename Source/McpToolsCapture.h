// Squelchmoth 1.51 - the capture tool (included inside the tool server class): measure a unit with a sweep and keep a model of it.
static juce::String captureDescribe (const juce::String& name, const SqCapture::Model& m)
{
    juce::String s = "'" + name + "': powers 1 to " + juce::String (m.order) + ", swept at " + juce::String (20.0 * std::log10 (m.level), 1) + " dBFS; delay found " + juce::String (m.latency) + " samples; gain at 1 kHz " + juce::String (m.gainDbAt1k, 2) + " dB; a 1 kHz tone at the sweep's level had its";
    const char* nth[6] = { "2nd", "3rd", "4th", "5th", "6th", "7th" }; for (size_t i = 0; i < m.harmonicDb.size() && i < 4; ++i) s << (i ? ", " : " ") << nth[i] << " harmonic at " << (m.harmonicDb[i] < -150.0 ? juce::String ("nothing") : juce::String (m.harmonicDb[i], 1) + " dB");
    s << ". " << (m.front.empty() ? "No filter in front of the distortion was called for" : "A filter in front of the distortion is used (the unit's distortion depends on pitch)") << "; the model predicts the quiet pass within " << juce::String (m.quietMissDb, 2) << " dB on average"
      << (m.front.empty() ? "." : " (" + juce::String (m.quietMissPlainDb, 2) + " dB without that filter).");
    if (m.quietMissDb > 1.0) s << " That is a poor match: this unit does more than this kind of model can hold (hard clipping, or gain that rides the signal like a compressor's) - expect its tone, not its behaviour.";
    return s;
}
void registerCaptureTools()
{
    add ("capture",
         "Capture: measure a piece of gear (or any chain of plug-ins) with a sweep and keep a light model of it that the Voltage Capture plug-in plays. "
         "action sweep: adds a track 'Capture Sweep' holding the test signal at 0 s (a rising sine about 8 s long, then the same 12 dB quieter; level_db sets its peak, default -6 - sweep at the hottest level you will use the unit at). "
         "Play that through the unit and record it back onto another track starting at 0 s (use external_fx for a hardware insert), or put plug-ins on the sweep track itself. "
         "action fit: track (the recording, or the sweep track with plug-ins on it; not panned hard right) and name - works the model out and saves it on this computer. The answer says what was measured and how well the model predicts the quiet pass. "
         "action use: track, name - puts a Voltage Capture on the track (or re-uses one there) playing that capture. action list; action remove (name). "
         "action recall: name, with notes (how the unit was set: knob positions, switches) and / or photo (path to a jpg or png of its front panel) - the recall sheet kept with the capture; with neither it reads the sheet back. window true opens the Capture window, which has all of this as buttons. "
         "What a capture holds: the unit's tone and the distortion it adds at and below the sweep's level (preamps, transformers, tape-style colour, gentle saturation). What it cannot hold: compressors and gates (gain that rides the signal), hard clipping, noise, delays and reverbs longer than 80 ms.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["sweep","fit","use","list","remove","recall"]},"track":{},"name":{"type":"string"},"notes":{"type":"string"},"photo":{"type":"string"},"window":{"type":"boolean"},"level_db":{"type":"number"},"order":{"type":"integer"}},"required":["action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action").toLowerCase(); auto& e = *session.edit; const auto name = str (a, "name").trim();
             if (action == "list") { const auto names = CaptureStore::list(); if (names.isEmpty()) return ToolResult::ok ("No captures on this computer yet. Start with capture action sweep."); juce::String s = juce::String (names.size()) + (names.size() == 1 ? " capture: " : " captures: "); for (auto& n : names) { SqCapture::Model m; s << "\n- " << (CaptureStore::load (n, m) ? captureDescribe (n, m) : "'" + n + "' (unreadable)"); const auto nt = CaptureStore::notes (n); if (nt.isNotEmpty()) s << " Recall notes: " << nt.substring (0, 300); if (CaptureStore::photo (n).existsAsFile()) s << " (has a photo)"; } return ToolResult::ok (s); }
             if (action == "remove") { if (name.isEmpty()) return ToolResult::error ("Which capture? Give its name."); return CaptureStore::remove (name) ? ToolResult::ok ("Removed the capture '" + name + "'. Plug-ins already playing it keep it until the session is reopened.") : ToolResult::error ("There is no capture called '" + name + "'."); }
             if (flag (a, "window") && ui.showWindow) ui.showWindow ("capture", true);
             if (action == "recall")
             {
                 if (name.isEmpty()) return ToolResult::error ("Which capture? Give its name."); if (! CaptureStore::fileFor (name).existsAsFile()) return ToolResult::error ("There is no capture called '" + name + "'.");
                 juce::String did;
                 if (a.hasProperty ("notes")) { if (! CaptureStore::setNotes (name, str (a, "notes"))) return ToolResult::error ("Couldn't save the notes."); did << "Notes saved. "; }
                 if (a.hasProperty ("photo")) { const juce::File f (str (a, "photo")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName()); if (! CaptureStore::setPhoto (name, f)) return ToolResult::error ("That file is not a picture this can read (jpg or png)."); did << "Photo kept with the capture. "; }
                 const auto notes = CaptureStore::notes (name); const auto ph = CaptureStore::photo (name);
                 return ToolResult::ok (did + "Recall sheet for '" + name + "': " + (notes.isNotEmpty() ? "notes: " + notes : juce::String ("no notes yet")) + "; " + (ph.existsAsFile() ? "photo: " + ph.getFullPathName() : juce::String ("no photo yet")) + ".");
             }
             if (action == "sweep")
             {
                 SqCapture::Sweep sw; sw.rate = TimeFmt::sampleRate (e); sw.highHz = juce::jmin (22000.0, 0.458 * sw.rate); sw.level = std::pow (10.0, juce::jlimit (-40.0, 0.0, num (a, "level_db", -6.0)) / 20.0);
                 const auto base = session.getFile().existsAsFile() ? session.getFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelchmoth");
                 const auto folder = base.getChildFile ("Capture"); folder.createDirectory(); const auto wav = folder.getNonexistentChildFile ("capture-sweep", ".wav", false);
                 if (! CaptureStore::writeMonoWav (wav, sw.make(), sw.rate)) return ToolResult::error ("Couldn't write the sweep to " + wav.getFullPathName());
                 juce::var t (new juce::DynamicObject()); set (t, "kind", "audio"); set (t, "name", "Capture Sweep");
                 const int idx = firstNumber (call ("add_track", t).text); if (idx <= 0) return ToolResult::error ("Couldn't add a track for the sweep");
                 juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", wav.getFullPathName()); set (im, "start_seconds", 0.0);
                 auto r = call ("import_audio", im); if (r.isError) return r;
                 e.state.setProperty ("captureLevel", sw.level, nullptr); e.state.setProperty ("captureRate", sw.rate, nullptr);
                 return ToolResult::ok ("The sweep is on track " + juce::String (idx) + " ('Capture Sweep') at 0 s: " + juce::String (sw.totalSamples() / sw.rate, 1) + " s, peaking at " + juce::String (20.0 * std::log10 (sw.level), 1) + " dBFS, then again 12 dB quieter. "
                                        "Play it through the unit and record the result onto another track from 0 s (keep the unit's own knobs where you want them captured), or put plug-ins on this track. Then: capture action fit, track, name.");
             }
             if (action == "fit")
             {
                 if (name.isEmpty()) return ToolResult::error ("Give the capture a name.");
                 auto* tr = session.findTrack (a["track"]); if (tr == nullptr) return ToolResult::error ("Which track holds the recording? (track)");
                 if (! e.state.hasProperty ("captureLevel")) return ToolResult::error ("There is no sweep in this session yet: run capture action sweep first.");
                 SqCapture::Sweep sw; sw.rate = (double) e.state["captureRate"]; sw.highHz = juce::jmin (22000.0, 0.458 * sw.rate); sw.level = (double) e.state["captureLevel"];
                 std::vector<float> L, R; double fs = 0; juce::String err; juce::Array<te::Track*> one; one.add (tr);
                 if (! adviseRenderAudio (one, { te::TimePosition(), te::TimePosition::fromSeconds (sw.totalSamples() / sw.rate + 0.25) }, false, L, R, fs, err)) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("Couldn't read that track."));
                 if (L.empty()) return ToolResult::error ("That track is silent over the sweep's time (0 to " + juce::String (sw.totalSamples() / sw.rate, 1) + " s). The recording must start at 0 s.");
                 if (std::abs (fs - sw.rate) > 1.0) return ToolResult::error ("The session's rate changed since the sweep was made; make the sweep again.");
                 // the track's own fader and pan law are not part of the unit: taken back out (the capture will be played on a track that has them again)
                 if (auto* at = dynamic_cast<te::AudioTrack*> (tr)) if (auto* vp = at->getVolumePlugin()) { float gl = 1.0f, gr = 1.0f; te::getGainsFromVolumeFaderPositionAndPan (vp->getSliderPos(), vp->getPan(), vp->getPanLaw(), gl, gr); if (gl > 1.0e-3f) for (auto& v : L) v /= gl; }
                 double pk = 0; for (float v : L) pk = std::max (pk, (double) std::abs (v)); if (pk < 1.0e-4) return ToolResult::error ("The recording is all but silent (peak " + juce::String (20.0 * std::log10 (pk + 1.0e-12), 1) + " dBFS).");
                 const auto m = SqCapture::fit (sw, L.data(), (int) L.size(), juce::jlimit (1, 7, (int) num (a, "order", 5.0)));
                 if (! CaptureStore::save (name, m, err)) return ToolResult::error (err);
                 return ToolResult::ok ("Captured " + captureDescribe (name, m) + " Recording peak " + juce::String (20.0 * std::log10 (pk), 1) + " dBFS. Next: capture action use, track, name '" + name + "' - then compare it with the unit by ear.");
             }
             if (action == "use")
             {
                 if (name.isEmpty()) return ToolResult::error ("Which capture? Give its name (capture action list shows them).");
                 SqCapture::Model probe; if (! CaptureStore::load (name, probe)) return ToolResult::error ("There is no capture called '" + name + "'.");
                 auto* tr = dynamic_cast<te::AudioTrack*> (session.findTrack (a["track"])); if (tr == nullptr) return ToolResult::error ("Which track? (track)");
                 SqVoltageCapturePlugin* vp = nullptr; for (auto pl : tr->pluginList) if (auto* c = dynamic_cast<SqVoltageCapturePlugin*> (pl)) vp = c;
                 if (vp == nullptr) { juce::var ins (new juce::DynamicObject()); set (ins, "track", tr->getName()); set (ins, "plugin", "Voltage Capture"); auto r = call ("insert_plugin", ins); if (r.isError) return r; for (auto pl : tr->pluginList) if (auto* c = dynamic_cast<SqVoltageCapturePlugin*> (pl)) vp = c; }
                 if (vp == nullptr || ! vp->useCapture (name)) return ToolResult::error ("Couldn't load that capture into Voltage Capture.");
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok ("Voltage Capture on '" + tr->getName() + "' now plays " + captureDescribe (name, probe) + " Input pushes into it (anything over the sweep's level is held there).");
             }
             return ToolResult::error ("action must be sweep, fit, use, list, remove or recall.");
         });
}

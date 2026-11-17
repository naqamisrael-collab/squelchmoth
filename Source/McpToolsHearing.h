// Squelchmoth 1.25 - `hearing`: notes as colour and number. Included inside the McpTools class body.

void registerHearingTools()
{
    add ("hearing",
         "Hearing in colour and number: the facts about a pitch, worked out exactly. Use it when the user asks what note a frequency is, how out of tune something is, what colour a note has in Squelchmoth, about overtones, beating between two tones, or which bass notes their room will exaggerate.\n"
         " action note (default): hz or note (A4, C#3, Bb2) - the nearest note and cents, its colour (pitch class = hue around the circle of fifths, octave = lightness; the same in every Squelchmoth window), critical band (Bark), ERB number and the width of the ear's filter there.\n"
         " action harmonics: hz or note, count (default 8, up to 32) - each overtone, the note it lands on and how many cents off, and its ratio.\n"
         " action beats: hz (or note) and second_hz - how fast the two beat and how that is heard.\n"
         " action room: length, width, height in metres (feet true to give them in feet), max_hz (default 200) - the room's standing waves, lowest first, with their notes.\n"
         " action colour_notes: on true / false - colours piano-roll notes by pitch with the same colours (off by default; without on it reports the setting).\n"
         " window true also opens the Hearing window showing it (Window menu > Hearing).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["note","harmonics","beats","room","colour_notes"]},"on":{"type":"boolean"},"hz":{"type":"number"},"note":{"type":"string"},"second_hz":{"type":"number"},"count":{"type":"integer"},
            "length":{"type":"number"},"width":{"type":"number"},"height":{"type":"number"},"feet":{"type":"boolean"},"max_hz":{"type":"number"},"window":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using namespace SqHear;
             const auto action = str (a, "action", "note").toLowerCase();
             if (action == "colour_notes" || action == "color_notes")
             {
                 if (has (a, "on")) { Prefs::set ("pr_pitchColours", flag (a, "on")); if (ui.refresh) ui.refresh(); }
                 return ToolResult::ok (juce::String ("Piano-roll notes are coloured by ") + (Prefs::getBool ("pr_pitchColours", false) ? "pitch (the Hearing colours: pitch class = hue, octave = lightness, velocity = how vivid)" : "velocity (the standard look)")
                                        + ". Also in the piano roll: Tools > Colour notes by pitch, or " SQ_OPT "+H.");
             }
             auto cents = [] (double c) { return (c >= 0 ? "+" : "") + juce::String (c, 1) + " cents"; };
             double hz = has (a, "hz") ? num (a, "hz") : has (a, "note") ? parseNoteOrHz (str (a, "note")) : 0.0;
             if (action == "room")
             {
                 const double k = flag (a, "feet") ? 0.3048 : 1.0;
                 const double l = num (a, "length", 0.0) * k, w = num (a, "width", 0.0) * k, h = num (a, "height", 0.0) * k;
                 if (l <= 0 || w <= 0 || h <= 0) return ToolResult::error ("action room needs length, width and height (metres, or feet true)");
                 const auto modes = roomModes (l, w, h, juce::jlimit (40.0, 500.0, num (a, "max_hz", 200.0)));
                 juce::String s = "Room " + juce::String (l, 2) + " x " + juce::String (w, 2) + " x " + juce::String (h, 2) + " m: " + juce::String ((int) modes.size()) + " standing waves up to " + juce::String (juce::roundToInt (num (a, "max_hz", 200.0))) + " Hz (speed of sound 343 m/s).";
                 int shown = 0; for (auto& m : modes) { if (++shown > 24) break; const auto n = nearestNote (m.hz); s << "\n  " << juce::String (m.hz, 1) << " Hz  " << n.name << " " << cents (n.cents) << "  " << m.kind << " (" << m.p << "," << m.q << "," << m.r << ")"; }
                 if ((int) modes.size() > 24) s << "\n  ... and " << juce::String ((int) modes.size() - 24) << " more.";
                 s << "\nAxial modes (one number) are the strongest. Bass notes near them boom or cancel depending on the seat; where two fall close together the room is uneven there.";
                 if (flag (a, "window") && ui.openHearing) ui.openHearing (0.0, 0.0, l, w, h);
                 return ToolResult::ok (s);
             }
             if (hz < 8.0 || hz > 24000.0) return ToolResult::error ("Give hz (8 to 24000) or note (like A4, C#3, Bb2)");
             const auto n = nearestNote (hz);
             if (action == "harmonics")
             {
                 const auto hs = harmonics (hz, juce::jlimit (1, 32, (int) num (a, "count", 8.0)));
                 juce::String s = "Harmonic series on " + juce::String (hz, 2) + " Hz (" + n.name + "):";
                 for (auto& p : hs) s << "\n  " << p.number << ": " << juce::String (p.hz, 1) << " Hz  " << p.note.name << " " << cents (p.note.cents) << "  ratio " << p.ratioNum << ":" << p.ratioDen << (p.interval.containsChar (':') ? juce::String() : "  (" + p.interval + ")");
                 s << "\nThe cents show how far each overtone sits from the equal-tempered note: the 3rd is almost exact, the 5th is 14 cents flat of a major third, the 7th is 31 cents flat.";
                 if (flag (a, "window") && ui.openHearing) ui.openHearing (hz, 0.0, 0.0, 0.0, 0.0);
                 return ToolResult::ok (s);
             }
             if (action == "beats")
             {
                 const double f2 = num (a, "second_hz", 0.0); if (f2 <= 0) return ToolResult::error ("action beats needs second_hz");
                 const double b = beatHz (hz, f2);
                 const juce::String how = b < 0.05 ? "in tune: no beating" : b < 15.0 ? "heard as a slow pulse; tune until it stops" : b < erbWidth (0.5 * (hz + f2)) ? "too fast to count: heard as roughness, because both tones fall inside one of the ear's filters (" + juce::String (erbWidth (0.5 * (hz + f2)), 0) + " Hz wide here)" : "far enough apart to be heard as two separate notes";
                 if (flag (a, "window") && ui.openHearing) ui.openHearing (hz, f2, 0.0, 0.0, 0.0);
                 return ToolResult::ok (juce::String (hz, 2) + " Hz and " + juce::String (f2, 2) + " Hz beat " + juce::String (b, 2) + " times a second: " + how + ". The interval between them is " + juce::String (1200.0 * std::log2 (f2 / hz), 1) + " cents.");
             }
             const auto c = noteColour (n.midi);
             static const char* const fifths[12] = { "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#", "F" };
             const int k = fifthsPosition (n.midi);
             juce::String s = juce::String (hz, 2) + " Hz is " + n.name + " " + cents (n.cents) + " (note number " + juce::String (n.midi) + ", exact pitch of " + n.name + " is " + juce::String (midiToHz (n.midi), 2) + " Hz).";
             s << "\nColour: " << c.hex() << " - position " << k << " of 12 on the circle of fifths, between " << fifths[(k + 11) % 12] << " and " << fifths[(k + 1) % 12] << "; lighter in higher octaves, more vivid when louder.";
             s << "\nCritical band: " << juce::String (bark (hz), 2) << " Bark; ERB number " << juce::String (erbNumber (hz), 2) << "; the ear's filter here is " << juce::String (erbWidth (hz), 0) << " Hz wide, so sounds closer than that to this note compete with it.";
             if (flag (a, "window") && ui.openHearing) ui.openHearing (hz, 0.0, 0.0, 0.0, 0.0);
             return ToolResult::ok (s);
         });
    add ("unmask",
         "Unmask: finds where two tracks sit on top of each other and moves one out of the way. Measures both tracks in the ear's 24 critical bands over a range and reports the region that matters to BOTH (where each is within 18 dB of its own strongest band), "
         "which track is louder there, and a starting cut for the other: centre frequency, depth and width.\n"
         " track_a, track_b (names or numbers); start/end in seconds or bars (default the first 60 seconds of the session).\n"
         " apply true puts the cut on the quieter track with a Prism EQ (one bell band), measures again and reports the change; keep a | b overrides which track keeps the range. One undo removes it.\n"
         " It measures energy overlap, not a full masking model: treat the cut as a starting point, listen, and run session_advice for the whole mix.",
         R"json({"type":"object","properties":{"track_a":{"type":["string","integer"]},"track_b":{"type":["string","integer"]},"apply":{"type":"boolean"},"keep":{"type":"string","enum":["a","b"]},
            "start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"start_bar":{"type":"number"},"end_bar":{"type":"number"}},"required":["track_a","track_b"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using namespace SqHear;
             auto& e = edit();
             auto* ta = session.findTrack (a["track_a"]); auto* tb = session.findTrack (a["track_b"]);
             if (ta == nullptr || tb == nullptr) return ToolResult::error ("unmask needs track_a and track_b: two tracks that exist (names or numbers)");
             if (ta == tb) return ToolResult::error ("track_a and track_b are the same track");
             e.getTransport().stop (false, false);
             const auto start = timeArg (a, "start").value_or (0s);
             auto end = timeArg (a, "end").value_or (te::toPosition (e.getLength()));
             if (end > start + 60s) end = start + 60s;
             if (end <= start + 0.5s) return ToolResult::error ("Nothing to measure in that range");
             const te::TimeRange range (start, end);
             auto spectrumOf = [&] (te::AudioTrack& t, BarkSpectrum& out, juce::String& err) -> bool
             {
                 juce::Array<te::Track*> set1; for (auto x : BounceExport::renderSetFor (session, t, false)) set1.addIfNotAlreadyThere (x);
                 std::vector<float> L, R; double fs = 0;
                 if (! adviseRenderAudio (set1, range, false, L, R, fs, err)) return false;
                 out = L.empty() ? BarkSpectrum() : barkSpectrum (L.data(), R.data(), (int) L.size(), fs);
                 return true;
             };
             juce::String err; BarkSpectrum sa, sb;
             if (! spectrumOf (*ta, sa, err) || ! spectrumOf (*tb, sb, err)) return ToolResult::error (err);
             if (sa.silent()) return ToolResult::error (ta->getName() + " is silent in that range");
             if (sb.silent()) return ToolResult::error (tb->getName() + " is silent in that range");
             auto o = findOverlap (sa, sb);
             if (! o.found) return ToolResult::ok (ta->getName() + " and " + tb->getName() + " do not overlap: no critical band matters to both of them (each one's strong bands are at least 18 dB down in the other). Nothing to carve.");
             if (has (a, "keep")) o.keepA = str (a, "keep").toLowerCase() != "b";
             auto* keep = o.keepA ? ta : tb; auto* cut = o.keepA ? tb : ta;
             const double cutLevel = o.keepA ? o.levelB : o.levelA, keepLevel = o.keepA ? o.levelA : o.levelB;
             const auto n = nearestNote (o.centreHz);
             juce::String s = ta->getName() + " and " + tb->getName() + " overlap from " + juce::String (juce::roundToInt (o.lowHz)) + " to " + juce::String (juce::roundToInt (o.highHz)) + " Hz (critical band"
                              + (o.firstBand == o.lastBand ? " " + juce::String (o.firstBand + 1) : "s " + juce::String (o.firstBand + 1) + " to " + juce::String (o.lastBand + 1)) + ", around " + n.name + "): "
                              + keep->getName() + " is at " + juce::String (keepLevel, 1) + " dBFS there, " + cut->getName() + " at " + juce::String (cutLevel, 1) + " dBFS.";
             s << "\nStarting point: keep " << keep->getName() << " there and cut " << cut->getName() << " by " << juce::String (o.cutDb, 1) << " dB at " << juce::String (juce::roundToInt (o.centreHz)) << " Hz, Q " << juce::String (o.q, 1) << ".";
             if (! flag (a, "apply")) return ToolResult::ok (s + "\nNothing was changed. Call again with apply true to make the cut, or keep a / b to choose the other way round.");

             juce::String why; auto np = session.createPluginByName ("Prism EQ", why);
             auto* eq = dynamic_cast<te::EqualiserPlugin*> (np.get());
             if (eq == nullptr) return ToolResult::error ("Couldn't add the EQ" + (why.isNotEmpty() ? ": " + why : juce::String()));
             cut->pluginList.insertPlugin (np, 0, nullptr);
             eq->midFreq1->setParameter ((float) o.centreHz, juce::sendNotification);
             eq->midGain1->setParameter ((float) -o.cutDb, juce::sendNotification);
             eq->midQ1->setParameter ((float) o.q, juce::sendNotification);
             BarkSpectrum after; if (! spectrumOf (*cut, after, err)) return ToolResult::error (err);
             const double now = after.regionDb (o.firstBand, o.lastBand);
             if (ui.refresh) ui.refresh();
             s << "\nDone: a Prism EQ is first in " << cut->getName() << "'s inserts with that cut. " << cut->getName() << " now measures " << juce::String (now, 1) << " dBFS in that range (was " << juce::String (cutLevel, 1) << "), "
               << juce::String (keepLevel - now, 1) << " dB under " << keep->getName() << ". Listen, then adjust the EQ or undo.";
             return ToolResult::ok (s);
         });
}

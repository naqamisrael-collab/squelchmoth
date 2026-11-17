// Squelchmoth 1.25 - `hearing`: notes as colour and number. Included inside the McpTools class body.

void registerHearingTools()
{
    add ("hearing",
         "Hearing in colour and number: the facts about a pitch, worked out exactly. Use it when the user asks what note a frequency is, how out of tune something is, what colour a note has in Squelchmoth, about overtones, beating between two tones, or which bass notes their room will exaggerate.\n"
         " action note (default): hz or note (A4, C#3, Bb2) - the nearest note and cents, its colour (pitch class = hue around the circle of fifths, octave = lightness; the same in every Squelchmoth window), critical band (Bark), ERB number and the width of the ear's filter there.\n"
         " action harmonics: hz or note, count (default 8, up to 32) - each overtone, the note it lands on and how many cents off, and its ratio.\n"
         " action beats: hz (or note) and second_hz - how fast the two beat and how that is heard.\n"
         " action room: length, width, height in metres (feet true to give them in feet), max_hz (default 200) - the room's standing waves, lowest first, with their notes.\n"
         " window true also opens the Hearing window showing it (Window menu > Hearing).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["note","harmonics","beats","room"]},"hz":{"type":"number"},"note":{"type":"string"},"second_hz":{"type":"number"},"count":{"type":"integer"},
            "length":{"type":"number"},"width":{"type":"number"},"height":{"type":"number"},"feet":{"type":"boolean"},"max_hz":{"type":"number"},"window":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using namespace SqHear;
             const auto action = str (a, "action", "note").toLowerCase();
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
}

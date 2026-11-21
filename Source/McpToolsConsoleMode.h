// Squelchmoth 1.41 - Console Mode tool (included inside the tool server class).
void registerConsoleModeTools()
{
    add ("console_mode",
         "Console Mode: runs the whole session through a console in one step. action on puts a Voltage Console (an input transformer whose iron saturates in the bass and remembers; no added delay) "
         "at the end of every track's inserts and one at the head of the master (the summing stage's output transformer, with a little crosstalk between left and right that rises with frequency). "
         "action off removes exactly the ones it placed (never one the user inserted by hand); action status says where it stands. One undo step each way. Tracks added later get theirs when it is turned on again. "
         "Options: drive (dB into every transformer, -12 to 18, default 0; level-matched), iron (size of the channel transformers, 0-100 %, default 50; smaller distorts the bass sooner), "
         "bus_iron (default 70), crosstalk (dB at 1 kHz on the master, -120 = off to -50, default -80). Each placed plug-in can then be adjusted like any other (get_plugin_parameters). "
         "The distortion is small and mostly in the bass: it is glue, not an effect. Each transformer costs processing, so on very large sessions say so.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["on","off","status"]},"drive":{"type":"number"},"iron":{"type":"number"},"bus_iron":{"type":"number"},"crosstalk":{"type":"number"}},"required":["action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action").toLowerCase();
             if (action == "status") return ToolResult::ok (ConsoleMode::describe (ConsoleMode::status (session)));
             if (action == "off") { const int n = ConsoleMode::disable (session); return ToolResult::ok (n > 0 ? "Console Mode off: removed " + juce::String (n) + " (one undo step brings them back)." : juce::String ("Console Mode was not on; nothing removed.")); }
             if (action != "on") return ToolResult::error ("action must be on, off or status.");
             ConsoleMode::Options o; o.drive = juce::jlimit (-12.0f, 18.0f, (float) num (a, "drive", 0.0)); o.iron = juce::jlimit (0.0f, 100.0f, (float) num (a, "iron", 50.0));
             o.busIron = juce::jlimit (0.0f, 100.0f, (float) num (a, "bus_iron", 70.0)); o.crosstalk = juce::jlimit (-120.0f, -50.0f, (float) num (a, "crosstalk", -80.0));
             juce::String err; const int n = ConsoleMode::enable (session, o, err);
             if (n < 0) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("Console Mode could not be turned on."));
             return ToolResult::ok ((n > 0 ? "Placed " + juce::String (n) + ". " : juce::String ("Nothing to add. ")) + ConsoleMode::describe (ConsoleMode::status (session)));
         });
}

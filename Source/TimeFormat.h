#pragma once
// Squelchmoth - the four Pro Tools time formats (Bars|Beats, Min:Secs, Timecode, Samples), formatted and parsed
// against the session's real tempo and meter map, timecode rate / start and sample rate. Used by the rulers, the
// main and sub counters, the Edit selection fields, the Spot dialog and Claude's tools.

#include <JuceHeader.h>

namespace te = tracktion;

namespace TimeFmt
{
    enum class Kind { barsBeats = 0, minSecs, timecode, samples };

    inline juce::StringArray kindNames()    { return { "Bars|Beats", "Min:Secs", "Timecode", "Samples" }; }

    //==========================================================================
    // Session-level settings stored in the Edit (Pro Tools Session Setup)
    struct TcRate { const char* name; double fps; int nominal; bool drop; };
    inline const std::array<TcRate, 6>& tcRates()
    {
        static const std::array<TcRate, 6> r {{ { "23.976", 24000.0 / 1001.0, 24, false }, { "24", 24.0, 24, false }, { "25", 25.0, 25, false },
                                                { "29.97", 30000.0 / 1001.0, 30, false }, { "29.97 Drop", 30000.0 / 1001.0, 30, true }, { "30", 30.0, 30, false } }};
        return r;
    }
    inline int tcRateIndex (const te::Edit& e)          { return juce::jlimit (0, 5, (int) e.state.getProperty ("sqmTcRate", 1)); }
    inline double tcStartSeconds (const te::Edit& e)    { return (double) e.state.getProperty ("sqmTcStart", 0.0); }
    inline double sampleRate (te::Edit& e)
    {
        const double stored = (double) e.state.getProperty ("sqmSampleRate", 0.0);
        if (stored > 0) return stored;
        auto r = e.engine.getDeviceManager().getSampleRate();
        return r > 0 ? r : 48000.0;
    }

    //==========================================================================
    inline juce::String barsBeats (te::Edit& e, te::TimePosition t)
    {
        auto bb = e.tempoSequence.toBarsAndBeats (t);
        const double beats = bb.beats.inBeats();
        int whole = (int) std::floor (beats + 1e-9);
        int tick = (int) std::llround ((beats - whole) * 960.0);
        if (tick >= 960) { tick -= 960; ++whole; }
        // after a tempo change the engine can report "beat 4.0 of a 4-beat bar" for a downbeat: carry it into the bar
        const int num = juce::jmax (1, e.tempoSequence.getTimeSigAt (t).numerator.get());
        if (whole >= num) { whole -= num; ++bb.bars; }
        return juce::String (bb.bars + 1) + "|" + juce::String (whole + 1) + "|" + juce::String (tick).paddedLeft ('0', 3);
    }

    inline juce::String minSecs (double s)
    {
        const bool neg = s < 0; s = std::abs (s);
        const int m = (int) (s / 60.0);
        const double rest = s - m * 60.0;
        return (neg ? "-" : "") + juce::String (m) + ":" + juce::String (rest, 3).paddedLeft ('0', 6);
    }

    /** SMPTE timecode (with drop-frame numbering for 29.97 DF). */
    inline juce::String timecode (const te::Edit& e, double seconds)
    {
        const auto& r = tcRates()[(size_t) tcRateIndex (e)];
        double s = seconds + tcStartSeconds (e);
        const bool neg = s < 0; s = std::abs (s);
        juce::int64 frames = (juce::int64) std::floor (s * r.fps + 1e-6);
        if (r.drop)
        {
            // standard drop-frame numbering: frames 0 and 1 are skipped each minute except every 10th minute
            const juce::int64 framesPer10 = 17982, framesPerMin = 1798;
            const juce::int64 d = frames / framesPer10, m = frames % framesPer10;
            frames += 18 * d + (m > 1 ? 2 * ((m - 2) / framesPerMin) : 0);
        }
        const int fpsN = r.nominal;
        const int ff = (int) (frames % fpsN), ss = (int) ((frames / fpsN) % 60), mm = (int) ((frames / (fpsN * 60)) % 60), hh = (int) (frames / (fpsN * 3600));
        auto two = [] (int v) { return juce::String (v).paddedLeft ('0', 2); };
        return (neg ? "-" : "") + two (hh) + ":" + two (mm) + ":" + two (ss) + (r.drop ? ";" : ":") + two (ff);
    }

    inline juce::String samples (te::Edit& e, double seconds)   { return juce::String ((juce::int64) std::llround (seconds * sampleRate (e))); }

    inline juce::String format (te::Edit& e, te::TimePosition t, Kind k)
    {
        switch (k)
        {
            case Kind::barsBeats: return barsBeats (e, t);
            case Kind::minSecs:   return minSecs (t.inSeconds());
            case Kind::timecode:  return timecode (e, t.inSeconds());
            case Kind::samples:   return samples (e, t.inSeconds());
        }
        return {};
    }

    /** Formats a duration (for selection Length fields): bars|beats as a count, others as times. */
    inline juce::String formatLength (te::Edit& e, te::TimePosition start, te::TimePosition end, Kind k)
    {
        if (k == Kind::barsBeats)
        {
            auto& ts = e.tempoSequence;
            const double beats = (ts.toBeats (end) - ts.toBeats (start)).inBeats();
            const int num = juce::jmax (1, ts.getTimeSigAt (start).numerator.get());
            const auto ticks = (juce::int64) std::llround (beats * 960.0);
            return juce::String (ticks / (960 * num)) + "|" + juce::String ((ticks / 960) % num) + "|" + juce::String (ticks % 960).paddedLeft ('0', 3);
        }
        if (k == Kind::timecode)
        {
            const auto& r = tcRates()[(size_t) tcRateIndex (e)];
            const auto frames = (juce::int64) std::floor ((end - start).inSeconds() * r.fps + 1e-6);
            auto two = [] (juce::int64 v) { return juce::String (v).paddedLeft ('0', 2); };
            return two (frames / (r.nominal * 3600)) + ":" + two ((frames / (r.nominal * 60)) % 60) + ":" + two ((frames / r.nominal) % 60) + ":" + two (frames % r.nominal);
        }
        if (k == Kind::samples) return samples (e, (end - start).inSeconds());
        return minSecs ((end - start).inSeconds());
    }

    //==========================================================================
    /** Parses text in the given format. Also accepts any format by its shape: "12.5s", "1:02.500", "01:00:00:00",
        "48000 samples" / "48000smp", "5|3|240", or a plain bar number. Returns nothing if it can't be read. */
    inline std::optional<te::TimePosition> parse (te::Edit& e, juce::String txt, Kind k)
    {
        txt = txt.trim();
        if (txt.isEmpty()) return {};
        auto& ts = e.tempoSequence;
        auto fromSecs = [] (double s) { return std::optional<te::TimePosition> (te::TimePosition::fromSeconds (juce::jmax (0.0, s))); };

        if (txt.endsWithIgnoreCase ("smp") || txt.endsWithIgnoreCase ("samples") || (k == Kind::samples && txt.containsOnly ("0123456789")))
            return fromSecs (txt.retainCharacters ("0123456789").getLargeIntValue() / sampleRate (e));
        if (txt.endsWithIgnoreCase ("s") && txt.dropLastCharacters (1).containsOnly ("0123456789.-"))
            return fromSecs (txt.dropLastCharacters (1).getDoubleValue());
        if (txt.contains ("|") || (k == Kind::barsBeats && txt.containsOnly ("0123456789.")))
        {
            auto parts = juce::StringArray::fromTokens (txt, "|", "");
            const int bar = juce::jmax (1, parts[0].getIntValue());
            const double beat = parts.size() > 1 ? juce::jmax (1.0, parts[1].getDoubleValue()) - 1.0 : 0.0;
            const double tick = parts.size() > 2 ? parts[2].getDoubleValue() / 960.0 : 0.0;
            double fracBar = parts.size() == 1 ? parts[0].getDoubleValue() - bar : 0.0;
            te::tempo::BarsAndBeats bb { bar - 1, te::BeatDuration::fromBeats (beat + tick), 0 };
            auto t = ts.toTime (bb);
            if (fracBar > 0)
            {
                const int num = juce::jmax (1, ts.getTimeSigAt (t).numerator.get());
                t = ts.toTime (ts.toBeats (t) + te::BeatDuration::fromBeats (fracBar * num));
            }
            return t;
        }
        auto colon = juce::StringArray::fromTokens (txt.replaceCharacter (';', ':'), ":", "");
        if (colon.size() == 4)   // timecode hh:mm:ss:ff
        {
            const auto& r = tcRates()[(size_t) tcRateIndex (e)];
            juce::int64 frames = ((juce::int64) colon[0].getIntValue() * 3600 + colon[1].getIntValue() * 60 + colon[2].getIntValue()) * r.nominal + colon[3].getIntValue();
            if (r.drop)
            {
                const juce::int64 totalMinutes = colon[0].getIntValue() * 60 + colon[1].getIntValue();
                frames -= 2 * (totalMinutes - totalMinutes / 10);
            }
            return fromSecs (frames / r.fps - tcStartSeconds (e));
        }
        if (colon.size() == 2 || colon.size() == 3)   // m:ss.mmm or h:mm:ss.mmm
        {
            double s = 0;
            for (auto& c : colon) s = s * 60.0 + c.getDoubleValue();
            return fromSecs (s);
        }
        if (txt.containsOnly ("0123456789.-")) return fromSecs (txt.getDoubleValue());
        return {};
    }
}

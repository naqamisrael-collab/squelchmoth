#pragma once
// Squelchmoth 1.31 - when proxies are made, and how their state is put into words (shared by the Video window, the
// video lane and the video_proxy tool).
#include <JuceHeader.h>
#include "Prefs.h"
#include "Frame.h"

namespace VideoProxy
{
    /** Options: make proxies for long or large videos without being asked (on unless switched off). */
    inline bool autoOn() { return Prefs::getInt ("frame_autoProxy", 1) != 0; }

    /** Called when a video becomes the session's picture: finds its proxy parts, and starts making the rest when the
        video is long or large and the option is on. Old unused proxies are cleared out once per run. */
    inline Frame::Proxies::Status follow (const juce::File& video, const Frame::Info& in)
    {
        static bool pruned = false; if (! pruned) { pruned = true; Frame::Proxies::prune(); }
        return Frame::Proxies::get().open (video, in, autoOn() && SqProxy::worthIt (in.width, in.seconds));
    }

    inline juce::String megabytes (juce::int64 bytes) { return bytes < 1048576 ? juce::String ((bytes + 1023) / 1024) + " KB" : juce::String ((double) bytes / 1048576.0, 1) + " MB"; }

    /** A short label for a button. */
    inline juce::String label (const Frame::Proxies::Status& s)
    {
        if (s.error.isNotEmpty()) return "Proxy failed";
        if (s.complete()) return "Proxy ready";
        if (s.wanted && s.parts > 0) return "Proxy " + juce::String (s.made * 100 / s.parts) + " %";
        return "Proxy";
    }

    /** One sentence for the assistant or a tooltip. */
    inline juce::String sentence (const juce::File& video, const Frame::Proxies::Status& s)
    {
        if (! s.known || s.parts == 0 || (s.made == 0 && ! s.wanted && s.error.isEmpty())) return "No proxy of " + video.getFileName() + " has been made.";
        juce::String t = "Proxy of " + video.getFileName() + " (" + juce::String (s.width) + " x " + juce::String (s.height) + "): " + juce::String (s.made) + " of " + juce::String (s.parts)
                         + (s.parts == 1 ? " part" : " parts") + " made (" + juce::String (s.made * 100 / s.parts) + " %), " + megabytes (s.bytes) + " on disk.";
        if (s.error.isNotEmpty()) t << " It stopped: " << s.error;
        else if (s.complete()) t << " Stills and scrubbing use it anywhere in the video.";
        else if (s.wanted) t << (s.working ? " The rest is being made in the background; the parts already made are in use." : " The rest waits (the session is playing, or the video tools are missing).");
        else t << " The rest is not being made.";
        return t;
    }
}

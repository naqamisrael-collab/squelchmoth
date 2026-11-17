#pragma once
// Squelchmoth 1.27 - every offline render goes through here.
//
// The engine's offline render waits for a clip's audio file for ever when that file has gone missing (moved, renamed,
// on a drive that is not plugged in): the app looks frozen. So before any render the clips about to be rendered are
// checked, and a missing file stops the render with a sentence that names it.

#include <JuceHeader.h>

namespace SqRender
{
    namespace eng = tracktion::engine;

    /** Empty when every audio file is there; otherwise a sentence naming the first missing one. An empty track set means all tracks. */
    inline juce::String missingAudioFile (eng::Edit& edit, const juce::BigInteger& tracksToDo, tracktion::TimeRange range)
    {
        const auto all = eng::getAllTracks (edit);
        const bool everyTrack = tracksToDo.isZero();
        for (int i = 0; i < all.size(); ++i)
        {
            if (! everyTrack && ! tracksToDo[i]) continue;
            if (auto* ct = dynamic_cast<eng::ClipTrack*> (all[i]))
                for (auto* c : ct->getClips())
                    if (auto* wc = dynamic_cast<eng::WaveAudioClip*> (c))
                        if (range.isEmpty() || c->getEditTimeRange().overlaps (range))
                            if (const auto f = wc->getOriginalFile(); f != juce::File() && ! f.existsAsFile())
                                return "The audio file for the clip \"" + c->getName() + "\" on " + all[i]->getName() + " is missing (" + f.getFullPathName() + "). Put the file back, relink it or remove the clip, then try again.";
        }
        return {};
    }

    /** Why the last guarded render did not run (empty when it ran). */
    inline juce::String& lastProblem() { static juce::String s; return s; }

    /** te::Renderer::renderToFile, refused with lastProblem() set when an audio file it needs is missing. */
    inline juce::File renderToFile (const juce::String& description, const eng::Renderer::Parameters& p)
    {
        lastProblem() = p.edit != nullptr ? missingAudioFile (*p.edit, p.tracksToDo, p.time) : juce::String();
        if (lastProblem().isNotEmpty()) return {};
        return eng::Renderer::renderToFile (description, p);
    }
}

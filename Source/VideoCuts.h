#pragma once
// Squelchmoth 1.32 - the session's picture as pieces. The picture follows the audio clips that play the video's own
// sound: split, trim, move or delete those clips with any editing tool and the picture is cut the same way.
#include <JuceHeader.h>
#include "SqCut.h"

namespace VideoCuts
{
    namespace te = tracktion;

    /** The audio file holding the video's own sound (made by video_import), if the session knows it. */
    inline juce::File soundFile (te::Edit& e)
    {
        const auto path = e.state["frameVideoWav"].toString();
        if (path.isNotEmpty()) return juce::File (path);
        const auto id = e.state["frameVideoClip"].toString();          // sessions from 1.30 / 1.31 remember only the clip
        if (id.isNotEmpty())
            for (auto t : te::getAudioTracks (e)) for (auto c : t->getClips()) if (c->itemID.toString() == id) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) return w->getOriginalFile();
        return {};
    }

    /** Every clip that plays the video's sound, in track order then time order. */
    inline std::vector<te::WaveAudioClip*> clips (te::Edit& e)
    {
        std::vector<te::WaveAudioClip*> out; const auto wav = soundFile (e); if (wav == juce::File()) return out;
        for (auto t : te::getAudioTracks (e))
        {
            std::vector<te::WaveAudioClip*> row;
            for (auto c : t->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) if (w->getOriginalFile() == wav) row.push_back (w);
            std::sort (row.begin(), row.end(), [] (te::WaveAudioClip* a, te::WaveAudioClip* b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
            out.insert (out.end(), row.begin(), row.end());
        }
        return out;
    }

    struct Picture { juce::File video; std::vector<SqCut::Piece> pieces; double slip = 0; bool followsSound = false; };

    /** The picture as it stands. videoSeconds is the video's length (for a video with no sound clips to follow). */
    inline Picture read (te::Edit& e, double videoSeconds)
    {
        Picture p; p.video = juce::File (e.state["frameVideo"].toString()); p.slip = (double) e.state.getProperty ("frameVideoSlip", 0.0);
        if (p.video == juce::File()) return p;
        p.followsSound = soundFile (e) != juce::File();
        if (p.followsSound)
        {
            for (auto* c : clips (e))
            {
                const auto pos = c->getPosition(); const double speed = c->getSpeedRatio();
                p.pieces.push_back ({ pos.getStart().inSeconds(), pos.getLength().inSeconds(), pos.getOffset().inSeconds() * speed, speed });
            }
        }
        else p.pieces.push_back ({ (double) e.state.getProperty ("frameVideoAt", 0.0), juce::jmax (0.0, videoSeconds), 0.0, 1.0 });
        return p;
    }

    /** The pieces in words, for the assistant. */
    inline juce::String describe (const Picture& p, double videoSeconds)
    {
        if (p.video == juce::File()) return "This session has no video.";
        juce::String s = p.video.getFileName() + ": ";
        if (p.pieces.empty()) return s + "no picture anywhere - every clip of its sound was deleted. Undo, or bring the video in again.";
        if (SqCut::isWhole (p.pieces, p.slip, videoSeconds)) return s + "one whole piece from " + juce::String (p.pieces[0].at, 3) + " to " + juce::String (p.pieces[0].end(), 3) + " s of the session. Not cut.";
        s << juce::String ((int) p.pieces.size()) << (p.pieces.size() == 1 ? " piece" : " pieces") << (p.followsSound ? " (the picture follows the clips of its sound)" : "") << ":";
        int n = 0;
        for (auto& x : p.pieces)
        {
            s << "\n" << ++n << ". session " << juce::String (x.at, 3) << " to " << juce::String (x.end(), 3) << " s shows the video from " << juce::String (x.srcIn - p.slip, 3) << " to " << juce::String (x.srcIn - p.slip + x.length * x.speed, 3) << " s";
            if (std::abs (x.speed - 1.0) > 1.0e-6) s << " at " << juce::String (x.speed, 3) << " x speed";
        }
        if (std::abs (p.slip) > 1.0e-9) s << "\nThe picture is slipped " << juce::String (p.slip, 3) << " s later than its sound.";
        return s;
    }
}

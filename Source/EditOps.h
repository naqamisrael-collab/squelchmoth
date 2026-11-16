#pragma once
// Squelchmoth - the Pro Tools edit commands, shared by the Edit window, the menus, the keyboard map
// and Claude's tools: Separate, Clear/Delete (with Shuffle gap closing), Cut/Copy/Paste, Duplicate,
// Repeat, Fade, Trim to selection, Nudge, Strip Silence, Consolidate, Normalise, Heal, Spot...

#include "EditState.h"

namespace EditOps
{
    using Clips = std::vector<te::Clip::Ptr>;

    //==========================================================================
    // Which tracks / clips does a command act on?
    inline juce::Array<te::AudioTrack*> targetTracks (Session& session, const TimelineState& st)
    {
        juce::Array<te::AudioTrack*> out;
        for (auto t : session.getTracks())
            if (st.isTrackSelected (t)) out.add (t);

        if (out.isEmpty())
            for (auto& c : st.selectedClips)
                if (auto at = dynamic_cast<te::AudioTrack*> (c->getClipTrack()))
                    out.addIfNotAlreadyThere (at);

        if (out.isEmpty() && st.selectedTrack != nullptr) out.add (st.selectedTrack);
        return out;
    }

    inline Clips clipsIn (te::AudioTrack& t, te::TimeRange r)
    {
        Clips out;
        for (auto c : t.getClips())
            if (c->getPosition().time.overlaps (r) && c->getPosition().getEnd() > r.getStart() && c->getPosition().getStart() < r.getEnd())
                out.push_back (c);
        return out;
    }

    inline te::SelectableList toList (const Clips& clips)
    {
        te::SelectableList l;
        for (auto& c : clips) if (c != nullptr) l.add (c.get());
        return l;
    }

    inline void tinyFades (te::Clip* c)
    {
        if (! Prefs::autoFadesOnEdits()) return;
        if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
        {
            if (ac->getFadeIn() < te::TimeDuration::fromSeconds (0.002))  ac->setFadeIn  (te::TimeDuration::fromSeconds (0.004));
            if (ac->getFadeOut() < te::TimeDuration::fromSeconds (0.002)) ac->setFadeOut (te::TimeDuration::fromSeconds (0.004));
        }
    }

    /** Moves every clip on the track whose start is >= from by delta (Shuffle behaviour). */
    inline void shiftClipsAfter (te::AudioTrack& t, te::TimePosition from, te::TimeDuration delta, te::Clip* except = nullptr)
    {
        if (delta == te::TimeDuration()) return;
        auto clips = t.getClips();
        // move in an order that never makes clips overlap each other mid-operation
        if (delta > te::TimeDuration())
            for (int i = clips.size(); --i >= 0;) { auto c = clips[i]; if (c != except && c->getPosition().getStart() >= from - 0.0005s) c->setStart (c->getPosition().getStart() + delta, false, true); }
        else
            for (auto c : clips) if (c != except && c->getPosition().getStart() >= from - 0.0005s) c->setStart (juce::jmax (te::TimePosition(), c->getPosition().getStart() + delta), false, true);
    }

    /** Pro Tools: one clip sounds at a time on a track. 'top' (the clip just moved, pasted, trimmed, imported or
        recorded) covers whatever lies underneath it: those clips are trimmed, split around it, or removed. Undoable. */
    inline int resolveOverlaps (te::Clip& top)
    {
        auto t = dynamic_cast<te::ClipTrack*> (top.getClipTrack());
        if (t == nullptr || ! t->isAudioTrack()) return 0;
        const auto r = top.getPosition().time;
        if (r.getLength() <= te::TimeDuration()) return 0;
        juce::Array<te::Clip*> under;
        for (auto c : t->getClips())
            if (c != &top && c->getPosition().getStart() < r.getEnd() - 0.00001s && c->getPosition().getEnd() > r.getStart() + 0.00001s)
                under.add (c);
        for (auto c : under)
            for (auto piece : te::deleteRegion (*c, r))
                tinyFades (piece);
        return under.size();
    }

    inline int resolveOverlaps (const Clips& tops)
    {
        int n = 0;
        for (auto& c : tops) if (c != nullptr && c->getClipTrack() != nullptr) n += resolveOverlaps (*c);
        return n;
    }

    //==========================================================================
    /** Separate (Cmd+E): at the Edit selection's edges on the selected tracks, or at the playhead. */
    inline int separate (Session& session, TimelineState& st)
    {
        if (session.edit == nullptr) return 0;
        int n = 0;
        auto& e = *session.edit;

        auto splitAt = [&] (te::AudioTrack& t, te::TimePosition at)
        {
            for (auto c : t.getClips())
                if (c->getPosition().getStart() + 0.001s < at && c->getPosition().getEnd() - 0.001s > at)
                {
                    te::Clip::Ptr keep (c);
                    auto newClip = t.splitClip (*c, at);
                    if (newClip != nullptr) { tinyFades (c); tinyFades (newClip); ++n; }
                }
        };

        if (st.timeSelection && st.timeSelection->getLength() > te::TimeDuration::fromSeconds (0.001))
        {
            for (auto t : targetTracks (session, st))
            {
                splitAt (*t, st.timeSelection->getStart());
                splitAt (*t, st.timeSelection->getEnd());
            }
            // select the pieces inside the range
            st.deselectClips();
            for (auto t : targetTracks (session, st))
                for (auto& c : clipsIn (*t, *st.timeSelection))
                    if (c->getPosition().getStart() >= st.timeSelection->getStart() - 0.002s && c->getPosition().getEnd() <= st.timeSelection->getEnd() + 0.002s)
                        st.selectClip (c, true);
        }
        else
        {
            auto at = e.getTransport().getPosition();
            Clips targets = st.selectedClips;
            if (targets.empty())
                for (auto t : targetTracks (session, st)) for (auto& c : clipsIn (*t, { at, at + 0.001s })) targets.push_back (c);
            for (auto& c : targets)
                if (auto t = dynamic_cast<te::ClipTrack*> (c->getClipTrack()))
                    if (c->getPosition().getStart() + 0.001s < at && c->getPosition().getEnd() - 0.001s > at)
                    {
                        auto nc = t->splitClip (*c, at);
                        if (nc) { tinyFades (c.get()); tinyFades (nc); ++n; }
                    }
        }
        return n;
    }

    /** Clear (Delete/Backspace with the Selector) / Delete with the Grabber. In Shuffle mode the gap closes. */
    inline int deleteSelection (Session& session, TimelineState& st, bool forceCloseGap = false)
    {
        if (session.edit == nullptr) return 0;
        bool closeGap = forceCloseGap || st.editMode == EditMode::shuffle;
        int n = 0;

        if (st.timeSelection && st.timeSelection->getLength() > te::TimeDuration::fromSeconds (0.001))
        {
            auto range = *st.timeSelection;
            for (auto t : targetTracks (session, st))
            {
                auto before = t->getClips().size();
                t->deleteRegion (range, nullptr);
                n += juce::jmax (0, before - t->getClips().size()) + 1;
                if (closeGap) shiftClipsAfter (*t, range.getEnd(), -range.getLength());
            }
            if (closeGap) st.timeSelection = te::TimeRange (range.getStart(), range.getStart());
            st.deselectClips();
            return n;
        }

        auto clips = st.selectedClips;
        st.deselectClips();
        for (auto& c : clips)
        {
            if (c == nullptr || c->getClipTrack() == nullptr) continue;
            auto t = dynamic_cast<te::AudioTrack*> (c->getClipTrack());
            auto pos = c->getPosition();
            c->removeFromParent();
            ++n;
            if (closeGap && t != nullptr) shiftClipsAfter (*t, pos.getEnd(), -pos.getLength());
        }
        return n;
    }

    //==========================================================================
    inline bool canPaste (Session& session)
    {
        return session.edit != nullptr && te::Clipboard::getInstance()->getContentWithType<te::Clipboard::Clips>() != nullptr;
    }

    inline bool copy (Session& session, TimelineState& st)
    {
        if (session.edit == nullptr) return false;
        auto content = std::make_unique<te::Clipboard::Clips>();
        Clips clips = st.selectedClips;
        te::TimeRange range;

        if (st.timeSelection && st.timeSelection->getLength() > 0.001s)
        {
            clips.clear();
            range = *st.timeSelection;
            for (auto t : targetTracks (session, st))
                for (auto& c : clipsIn (*t, range)) clips.push_back (c);
        }
        else if (! clips.empty())
        {
            range = clips.front()->getPosition().time;
            for (auto& c : clips) range = range.getUnionWith (c->getPosition().time);
        }

        if (clips.empty()) return false;
        content->addSelectedClips (toList (clips), range, te::Clipboard::Clips::AutomationLocked::no);
        te::Clipboard::getInstance()->setContent (std::move (content));
        return true;
    }

    inline bool cut (Session& session, TimelineState& st)
    {
        if (! copy (session, st)) return false;
        deleteSelection (session, st);
        return true;
    }

    inline bool paste (Session& session, TimelineState& st)
    {
        if (session.edit == nullptr) return false;
        auto& e = *session.edit;
        auto content = te::Clipboard::getInstance()->getContentWithType<te::Clipboard::Clips>();
        if (content == nullptr || content->clips.empty()) return false;

        te::Track::Ptr target;
        auto tracks = targetTracks (session, st);
        if (! tracks.isEmpty()) target = tracks.getFirst();
        else if (! session.getTracks().isEmpty()) target = session.getTracks().getFirst();
        if (target == nullptr) return false;

        auto at = st.timeSelection ? st.timeSelection->getStart() : e.getTransport().getPosition();
        te::EditInsertPoint ip (e);
        te::Clipboard::ContentType::EditPastingOptions opts (e, ip, nullptr);
        opts.startTrack = target;
        opts.startTime = at;
        opts.silent = true;

        if (st.editMode == EditMode::shuffle)
        {
            te::TimeDuration len;
            for (auto& ci : content->clips)
            {
                auto s = te::TimePosition::fromSeconds ((double) ci.state[te::IDs::start]);
                auto l = te::TimeDuration::fromSeconds ((double) ci.state[te::IDs::length]);
                len = juce::jmax (len, (s + l) - te::TimePosition());
            }
            if (auto at2 = dynamic_cast<te::AudioTrack*> (target.get()))
                shiftClipsAfter (*at2, at, len);
        }

        return content->pasteIntoEdit (opts);
    }

    //==========================================================================
    /** Duplicate (Cmd+D): copies the selection to immediately after itself. */
    inline int duplicate (Session& session, TimelineState& st, int times = 1)
    {
        if (session.edit == nullptr) return 0;
        int n = 0;

        if (st.timeSelection && st.timeSelection->getLength() > 0.001s)
        {
            auto range = *st.timeSelection;
            if (! copy (session, st)) return 0;
            auto content = te::Clipboard::getInstance()->getContentWithType<te::Clipboard::Clips>();
            for (int i = 0; i < times; ++i)
            {
                auto at = range.getEnd() + range.getLength() * (double) i;
                for (auto t : targetTracks (session, st))
                {
                    te::EditInsertPoint ip (*session.edit);
                    te::Clipboard::ContentType::EditPastingOptions opts (*session.edit, ip, nullptr);
                    opts.startTrack = t; opts.startTime = at; opts.silent = true;
                    // paste only this track's clips: rebuild a per-track clipboard
                    te::Clipboard::Clips one;
                    for (auto& c : clipsIn (*t, range)) { auto sl = toList ({ c }); one.addSelectedClips (sl, range, te::Clipboard::Clips::AutomationLocked::no); }
                    if (! one.clips.empty() && one.pasteIntoEdit (opts)) ++n;
                }
            }
            st.timeSelection = te::TimeRange (range.getEnd(), range.getEnd() + range.getLength() * (double) times);
            return n;
        }

        Clips newSel;
        for (auto& c : st.selectedClips)
        {
            if (auto t = dynamic_cast<te::ClipTrack*> (c->getClipTrack()))
            {
                auto at = c->getPosition().getEnd();
                if (st.editMode == EditMode::shuffle)
                    if (auto at2 = dynamic_cast<te::AudioTrack*> (t)) shiftClipsAfter (*at2, at, c->getPosition().getLength() * (double) times, c.get());
                for (int i = 0; i < times; ++i)
                {
                    auto stt = c->state.createCopy();
                    te::EditItemID::remapIDs (stt, nullptr, c->edit);
                    if (auto nc = t->insertClipWithState (stt))
                    {
                        nc->setStart (at + c->getPosition().getLength() * (double) i, false, true);
                        newSel.push_back (nc);
                        ++n;
                    }
                }
            }
        }
        if (! newSel.empty()) { st.deselectClips(); for (auto& c : newSel) st.selectClip (c, true); }
        return n;
    }

    //==========================================================================
    /** Hidden audio available beyond a clip's edges (Pro Tools "handles"), in seconds. Zero for stretched clips. */
    inline double handleAfter (te::AudioClipBase& c)
    {
        if (c.getAutoTempo() || std::abs (c.getSpeedRatio() - 1.0) > 1.0e-6) return 0.0;
        const double src = c.getSourceLength().inSeconds();
        return juce::jmax (0.0, src - (c.getPosition().getOffset().inSeconds() + c.getPosition().getLength().inSeconds()));
    }
    inline double handleBefore (te::AudioClipBase& c)
    {
        if (c.getAutoTempo() || std::abs (c.getSpeedRatio() - 1.0) > 1.0e-6) return 0.0;
        return juce::jmax (0.0, c.getPosition().getOffset().inSeconds());
    }

    /** Equal-power crossfade across the junction of two butting clips (Pro Tools "centred" crossfade). The clips are
        extended into each other using their hidden audio; without handles it falls back to a dip (fade out + fade in).
        Returns the crossfade length in seconds (0 = nothing done). */
    inline double crossfade (te::AudioClipBase& left, te::AudioClipBase& right, te::TimeRange range)
    {
        const auto junction = left.getPosition().getEnd();
        if (std::abs ((right.getPosition().getStart() - junction).inSeconds()) > 0.002) return 0.0;
        const double J = junction.inSeconds();
        const double wantAfter = juce::jlimit (0.0, right.getPosition().getLength().inSeconds() * 0.95, range.getEnd().inSeconds() - J);
        const double wantBefore = juce::jlimit (0.0, left.getPosition().getLength().inSeconds() * 0.95, J - range.getStart().inSeconds());
        const double extL = juce::jmin (wantAfter, handleAfter (left));     // left clip plays on into the right one
        const double extR = juce::jmin (wantBefore, handleBefore (right));  // right clip starts earlier under the left one
        const auto curve = te::AudioFadeCurve::convex;                        // equal power: no level dip for unrelated audio
        left.setFadeOutType (curve); right.setFadeInType (curve);
        if (extL + extR > 0.0005)
        {
            if (extL > 0) left.setEnd (te::TimePosition::fromSeconds (J + extL), true);
            if (extR > 0) right.setStart (te::TimePosition::fromSeconds (J - extR), true, false);
            const auto len = te::TimeDuration::fromSeconds (extL + extR);
            left.setFadeOut (len);
            right.setFadeIn (len);
            return extL + extR;
        }
        // no handles: fade each clip down to / up from the junction
        if (wantBefore > 0.0005) left.setFadeOut (te::TimeDuration::fromSeconds (wantBefore));
        if (wantAfter > 0.0005) right.setFadeIn (te::TimeDuration::fromSeconds (wantAfter));
        return wantBefore + wantAfter;
    }

    /** Crossfades every pair of butting clips whose junction lies inside the range on the given tracks. */
    inline int crossfadeJunctions (const juce::Array<te::AudioTrack*>& tracks, te::TimeRange r)
    {
        int n = 0;
        for (auto t : tracks)
        {
            auto clips = t->getClips();
            for (auto a : clips)
                for (auto b : clips)
                {
                    if (a == b) continue;
                    auto la = dynamic_cast<te::AudioClipBase*> (a), rb = dynamic_cast<te::AudioClipBase*> (b);
                    if (la == nullptr || rb == nullptr) continue;
                    const auto j = la->getPosition().getEnd();
                    if (std::abs ((rb->getPosition().getStart() - j).inSeconds()) > 0.002) continue;
                    if (j <= r.getStart() || j >= r.getEnd()) continue;
                    if (crossfade (*la, *rb, r) > 0) ++n;
                }
        }
        return n;
    }

    /** Fade (Cmd+F): fades the selected clips' edges inside the Edit selection; or sets default fades. */
    inline int fade (Session& session, TimelineState& st, double lengthSecs = -1.0)
    {
        int n = 0;
        // a selection across the boundary of two butting clips makes a crossfade (Pro Tools)
        if (st.timeSelection && st.timeSelection->getLength() > 0.001s)
        {
            juce::Array<te::AudioTrack*> ts;
            for (auto t : targetTracks (session, st)) ts.add (t);
            n = crossfadeJunctions (ts, *st.timeSelection);
            if (n > 0) return n;
        }
        auto shape = Prefs::fadeShape();
        auto type = shape == "s-curve" ? te::AudioFadeCurve::sCurve : (shape == "convex" ? te::AudioFadeCurve::convex : (shape == "concave" ? te::AudioFadeCurve::concave : te::AudioFadeCurve::linear));

        Clips targets = st.selectedClips;
        if (targets.empty() && st.timeSelection)
            for (auto t : targetTracks (session, st)) for (auto& c : clipsIn (*t, *st.timeSelection)) targets.push_back (c);

        for (auto& c : targets)
        {
            auto ac = dynamic_cast<te::AudioClipBase*> (c.get());
            if (ac == nullptr) continue;
            auto pos = ac->getPosition();
            ac->setFadeInType (type); ac->setFadeOutType (type);

            if (st.timeSelection && st.timeSelection->getLength() > 0.001s)
            {
                auto r = *st.timeSelection;
                bool touchesStart = r.getStart() <= pos.getStart() + 0.002s && r.getEnd() > pos.getStart();
                bool touchesEnd   = r.getEnd() >= pos.getEnd() - 0.002s && r.getStart() < pos.getEnd();
                if (touchesStart && ! touchesEnd)      ac->setFadeIn  (juce::jmin (pos.getLength(), r.getEnd() - pos.getStart()));
                else if (touchesEnd && ! touchesStart) ac->setFadeOut (juce::jmin (pos.getLength(), pos.getEnd() - r.getStart()));
                else if (touchesStart && touchesEnd)
                {
                    auto half = pos.getLength() * 0.5;
                    ac->setFadeIn (juce::jmin (half, te::TimeDuration::fromSeconds (lengthSecs > 0 ? lengthSecs : Prefs::fadeCommandLength())));
                    ac->setFadeOut (juce::jmin (half, te::TimeDuration::fromSeconds (lengthSecs > 0 ? lengthSecs : Prefs::fadeCommandLength())));
                }
                else continue;
            }
            else
            {
                auto len = te::TimeDuration::fromSeconds (lengthSecs > 0 ? lengthSecs : Prefs::fadeCommandLength());
                ac->setFadeIn (juce::jmin (pos.getLength() * 0.5, len));
                ac->setFadeOut (juce::jmin (pos.getLength() * 0.5, len));
            }
            ++n;
        }
        return n;
    }

    /** Trim clip(s) to the Edit selection (Cmd+T). */
    inline int trimToSelection (Session& session, TimelineState& st)
    {
        if (! st.timeSelection || st.timeSelection->getLength() < 0.001s) return 0;
        int n = 0;
        auto r = *st.timeSelection;
        for (auto t : targetTracks (session, st))
            for (auto& c : clipsIn (*t, r))
            {
                auto pos = c->getPosition();
                auto ns = juce::jmax (pos.getStart(), r.getStart());
                auto ne = juce::jmin (pos.getEnd(), r.getEnd());
                if (ne <= ns) continue;
                auto offsetShift = ns - pos.getStart();
                c->setPosition ({ { ns, ne }, pos.offset + offsetShift });
                ++n;
            }
        return n;
    }

    /** Trim start/end of the selected clips to the playhead (Opt+Shift+7 / 8 in Pro Tools; A / S keys). */
    inline int trimToPlayhead (Session& session, TimelineState& st, bool trimStart)
    {
        if (session.edit == nullptr) return 0;
        auto at = session.edit->getTransport().getPosition();
        int n = 0;
        for (auto& c : st.selectedClips)
        {
            auto pos = c->getPosition();
            if (at <= pos.getStart() || at >= pos.getEnd()) continue;
            if (trimStart) c->setPosition ({ { at, pos.getEnd() }, pos.offset + (at - pos.getStart()) });
            else           c->setEnd (at, true);
            ++n;
        }
        return n;
    }

    //==========================================================================
    /** Nudge the selection by the nudge value (+ / - keys, or , and . for one grid). */
    inline int nudge (Session& session, TimelineState& st, int direction, bool byGrid = false)
    {
        if (session.edit == nullptr) return 0;
        auto& e = *session.edit;
        auto at = st.timeSelection ? st.timeSelection->getStart().inSeconds() : (st.selectedClip ? st.selectedClip->getPosition().getStart().inSeconds() : e.getTransport().getPosition().inSeconds());
        auto step = byGrid ? st.gridSeconds (e, at) : st.nudgeSeconds (e, at);
        auto delta = te::TimeDuration::fromSeconds (step * direction);
        int n = 0;

        if (! st.selectedClips.empty())
        {
            for (auto& c : st.selectedClips)
            {
                auto ns = juce::jmax (te::TimePosition(), c->getPosition().getStart() + delta);
                c->setStart (ns, false, true);
                ++n;
            }
        }
        if (st.timeSelection)
        {
            auto s = juce::jmax (te::TimePosition(), st.timeSelection->getStart() + delta);
            st.timeSelection = te::TimeRange (s, s + st.timeSelection->getLength());
            ++n;
        }
        if (n == 0)
            e.getTransport().setPosition (juce::jmax (te::TimePosition(), e.getTransport().getPosition() + delta));
        return n;
    }

    //==========================================================================
    /** Strip Silence (Cmd+U): splits the selected audio clips at silent regions and removes them. */
    inline int stripSilence (Session& session, TimelineState& st, double thresholdDb = -1000, double minLenSecs = -1, double padSecs = -1)
    {
        if (session.edit == nullptr) return 0;
        if (thresholdDb <= -999) thresholdDb = Prefs::stripSilenceThreshold();
        if (minLenSecs < 0) minLenSecs = Prefs::stripSilenceMinLength();
        if (padSecs < 0) padSecs = Prefs::stripSilencePad();
        auto thresh = juce::Decibels::decibelsToGain ((float) thresholdDb);

        Clips targets = st.selectedClips;
        if (targets.empty() && st.timeSelection)
            for (auto t : targetTracks (session, st)) for (auto& c : clipsIn (*t, *st.timeSelection)) targets.push_back (c);

        int removed = 0;
        Clips newSelection;
        for (auto& cp : targets)
        {
            auto wc = dynamic_cast<te::WaveAudioClip*> (cp.get());
            if (wc == nullptr) continue;
            auto track = dynamic_cast<te::ClipTrack*> (wc->getClipTrack());
            if (track == nullptr) continue;

            te::AudioFile af (wc->getPlaybackFile());
            auto reader = std::unique_ptr<juce::AudioFormatReader> (session.getEngine().getAudioFileFormatManager().readFormatManager.createReaderFor (af.getFile()));
            if (reader == nullptr) continue;

            auto sr = reader->sampleRate;
            auto pos = wc->getPosition();
            auto speed = wc->getSpeedRatio();
            // analyse in 10 ms windows across the part of the file that the clip plays
            auto fileStart = (juce::int64) (pos.getOffset().inSeconds() * speed * sr);
            auto fileLen   = (juce::int64) (pos.getLength().inSeconds() * speed * sr);
            fileLen = juce::jmin (fileLen, (juce::int64) reader->lengthInSamples - fileStart);
            if (fileLen <= 0) continue;

            const int win = (int) (sr * 0.01);
            juce::AudioBuffer<float> buf ((int) reader->numChannels, win);
            std::vector<bool> loud;
            for (juce::int64 p = 0; p < fileLen; p += win)
            {
                auto n = (int) juce::jmin ((juce::int64) win, fileLen - p);
                reader->read (&buf, 0, n, fileStart + p, true, true);
                float peak = 0;
                for (int ch = 0; ch < buf.getNumChannels(); ++ch) peak = juce::jmax (peak, buf.getMagnitude (ch, 0, n));
                loud.push_back (peak >= thresh);
            }

            // build regions of sound (in clip-relative seconds)
            struct Region { double start, end; };
            std::vector<Region> regions;
            for (size_t i = 0; i < loud.size(); ++i)
                if (loud[i])
                {
                    double s = (double) i * 0.01 / speed;
                    size_t j = i;
                    while (j < loud.size() && loud[j]) ++j;
                    double e2 = (double) j * 0.01 / speed;
                    if (! regions.empty() && s - regions.back().end < minLenSecs) regions.back().end = e2;
                    else regions.push_back ({ s, e2 });
                    i = j;
                }
            // drop regions shorter than the minimum, pad the rest
            std::vector<Region> keep;
            for (auto& r : regions)
                if (r.end - r.start >= minLenSecs * 0.5)
                    keep.push_back ({ juce::jmax (0.0, r.start - padSecs), juce::jmin (pos.getLength().inSeconds(), r.end + padSecs) });
            if (keep.empty() || (keep.size() == 1 && keep[0].start <= 0.001 && keep[0].end >= pos.getLength().inSeconds() - 0.001)) continue;

            auto original = wc->state.createCopy();
            auto trackPtr = track;
            wc->removeFromParent();
            ++removed;
            for (auto& r : keep)
            {
                auto stt = original.createCopy();
                te::EditItemID::remapIDs (stt, nullptr, *session.edit);
                if (auto nc = trackPtr->insertClipWithState (stt))
                {
                    auto s = pos.getStart() + te::TimeDuration::fromSeconds (r.start);
                    auto e2 = pos.getStart() + te::TimeDuration::fromSeconds (r.end);
                    nc->setPosition ({ { s, e2 }, pos.offset + te::TimeDuration::fromSeconds (r.start) });
                    tinyFades (nc);
                    newSelection.push_back (nc);
                }
            }
        }
        if (! newSelection.empty()) { st.deselectClips(); for (auto& c : newSelection) st.selectClip (c, true); }
        return removed;
    }

    //==========================================================================
    /** Consolidate (Opt+Shift+3): renders the selected range of each target track to one new clip. */
    inline int consolidate (Session& session, TimelineState& st, juce::String& error)
    {
        if (session.edit == nullptr) return 0;
        auto& e = *session.edit;
        std::optional<te::TimeRange> range = st.timeSelection;
        if ((! range || range->getLength() < 0.001s) && ! st.selectedClips.empty())
        {
            range = st.selectedClips.front()->getPosition().time;
            for (auto& c : st.selectedClips) range = range->getUnionWith (c->getPosition().time);
        }
        if (! range || range->getLength() < 0.001s) { error = "Select a time range or some clips first."; return 0; }

        int n = 0;
        auto dir = session.getFile().getParentDirectory().getChildFile (session.getFile().getFileNameWithoutExtension() + " Audio");
        dir.createDirectory();

        for (auto t : targetTracks (session, st))
        {
            auto clips = clipsIn (*t, *range);
            if (clips.empty()) continue;

            te::Renderer::Parameters p (e);
            p.destFile = dir.getNonexistentChildFile (juce::File::createLegalFileName (t->getName() + " consolidated"), ".wav", false);
            p.audioFormat = session.getEngine().getAudioFileFormatManager().getWavFormat();
            p.bitDepth = 24;
            p.sampleRateForAudio = session.getEngine().getDeviceManager().getSampleRate();
            p.time = *range;
            p.usePlugins = false;        // clip content only, like Pro Tools: the track inserts stay live
            p.useMasterPlugins = false;
            p.canRenderInMono = false;
            p.tracksToDo = te::toBitSet (juce::Array<te::Track*> { t });

            auto f = te::Renderer::renderToFile ("Consolidate", p);
            if (! f.existsAsFile()) { error = "Render failed on " + t->getName(); continue; }

            t->deleteRegion (*range, nullptr);
            if (auto nc = t->insertWaveClip (t->getName() + " consolidated", f, { *range, {} }, false))
            {
                st.deselectClips();
                st.selectClip (nc, true);
                ++n;
            }
        }
        return n;
    }

    //==========================================================================
    /** Heal Separation (Cmd+H): rejoins adjacent pieces of the same source file. */
    inline int heal (Session& session, TimelineState& st)
    {
        juce::ignoreUnused (session);
        int n = 0;
        auto clips = st.selectedClips;
        std::sort (clips.begin(), clips.end(), [] (auto& a, auto& b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
        for (size_t i = 0; i + 1 < clips.size(); ++i)
        {
            auto a = dynamic_cast<te::WaveAudioClip*> (clips[i].get());
            auto b = dynamic_cast<te::WaveAudioClip*> (clips[i + 1].get());
            if (a == nullptr || b == nullptr || a->getClipTrack() != b->getClipTrack()) continue;
            if (a->getSourceFileReference().source != b->getSourceFileReference().source) continue;
            auto pa = a->getPosition(), pb = b->getPosition();
            // same timeline-to-source alignment?
            auto alignA = (pa.getStart().inSeconds() - pa.getOffset().inSeconds());
            auto alignB = (pb.getStart().inSeconds() - pb.getOffset().inSeconds());
            if (std::abs (alignA - alignB) > 0.002 || pb.getStart() > pa.getEnd() + 0.05s) continue;
            a->setEnd (pb.getEnd(), true);
            b->removeFromParent();
            clips.erase (clips.begin() + (long) i + 1);
            --i; ++n;
        }
        if (n > 0) { st.deselectClips(); for (auto& c : clips) st.selectClip (c, true); }
        return n;
    }

    //==========================================================================
    // 1.12: Sync Point (Pro Tools Identify Sync Point). Stored in source time (seconds into the clip's audio), so it
    // stays on the same sound when the clip is trimmed or moved. A clip has at most one.
    inline bool hasSyncPoint (const te::Clip& c)        { return c.state.hasProperty ("sqmSyncSrc"); }
    inline void clearSyncPoint (te::Clip& c)            { c.state.removeProperty ("sqmSyncSrc", &c.edit.getUndoManager()); }

    /** Where the sync point sits on the timeline, if the clip has one and it is inside the clip. */
    inline std::optional<te::TimePosition> syncPointTime (const te::Clip& c)
    {
        if (! hasSyncPoint (c)) return {};
        auto pos = c.getPosition();
        const double t = pos.getStart().inSeconds() + (double) c.state["sqmSyncSrc"] - pos.getOffset().inSeconds();
        if (t < pos.getStart().inSeconds() - 1e-9 || t > pos.getEnd().inSeconds() + 1e-9) return {};
        return te::TimePosition::fromSeconds (t);
    }

    /** Sets the sync point at a timeline time inside the clip. False if the time is outside the clip. */
    inline bool setSyncPoint (te::Clip& c, te::TimePosition at)
    {
        auto pos = c.getPosition();
        if (at < pos.getStart() || at > pos.getEnd()) return false;
        c.state.setProperty ("sqmSyncSrc", (at - pos.getStart()).inSeconds() + pos.getOffset().inSeconds(), &c.edit.getUndoManager());
        return true;
    }

    /** Spot: move a clip so its start, end or sync point lands on an exact time. */
    inline void spot (te::Clip& c, te::TimePosition to, int anchor /* 0 = start, 1 = end, 2 = sync point */)
    {
        auto pos = c.getPosition();
        if (anchor == 1) to = to - pos.getLength();
        if (anchor == 2) if (auto sp = syncPointTime (c)) to = to - (*sp - pos.getStart());
        c.setStart (juce::jmax (te::TimePosition(), to), false, true);
    }

    /** Selects everything on the target tracks (Cmd+A). */
    inline void selectAll (Session& session, TimelineState& st)
    {
        if (session.edit == nullptr) return;
        st.deselectClips();
        auto tracks = targetTracks (session, st);
        if (tracks.isEmpty()) tracks = session.getTracks();
        st.selectedTrackIDs.clear();
        for (auto t : tracks)
        {
            st.selectedTrackIDs.push_back (t->itemID);
            for (auto c : t->getClips()) st.selectClip (c, true);
        }
        auto len = session.edit->getLength();
        st.timeSelection = te::TimeRange (te::TimePosition(), te::toPosition (len));
    }

    /** Quantize note starts (and optionally lengths) in a MIDI clip. grid in beats, strength 0..1. */
    inline int quantizeMidi (te::MidiClip& mc, double grid, double strength, juce::UndoManager* um)
    {
        int n = 0;
        grid = juce::jmax (0.015625, grid);
        for (auto note : mc.getSequence().getNotes())
        {
            auto s = note->getStartBeat().inBeats();
            auto q = std::round (s / grid) * grid;
            auto ns = s + (q - s) * juce::jlimit (0.0, 1.0, strength);
            if (std::abs (ns - s) > 1e-6) { note->setStartAndLength (te::BeatPosition::fromBeats (ns), note->getLengthBeats(), um); ++n; }
        }
        return n;
    }

    /** Transport helpers shared by the Transport window and the keyboard. */
    inline te::TimePosition playStartPosition (Session& session, const TimelineState& st)
    {
        auto& tr = session.edit->getTransport();
        if (Prefs::playFromSelection() && st.timeSelection && st.timeSelection->getLength() > 0.001s)
            return st.timeSelection->getStart();
        return tr.getPosition();
    }
}

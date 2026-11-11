#pragma once
// Squelchmoth 1.13 - Score export to PDF and PNG (used by the Score window and Claude's `score` tool).

#include "ScoreModel.h"
#include "ScoreRender.h"

namespace ScoreExport
{
    /** Renders the tracks' score to a PDF (every page) or PNGs (one file per page: name.png, name-2.png ...). */
    inline bool writePages (Session& s, const juce::Array<te::AudioTrack*>& tracks, const juce::File& dest, bool pdf, bool letter, juce::String& err, int* pagesOut = nullptr)
    {
        auto parts = ScoreModel::build (s, tracks.isEmpty() ? ScoreModel::midiTracks (s) : tracks);
        if (parts.empty()) { err = "There's no MIDI to print yet."; return false; }
        std::vector<ScoreRender::PartView> views;
        for (size_t i = 0; i < parts.size(); ++i) views.push_back ({ parts[i].name, &parts[i].score, (int) i });
        auto title = s.getFile().getFileNameWithoutExtension();
        auto pages = ScoreRender::renderPages (views, title, "Squelchmoth", letter, pdf ? 200.0f : 150.0f);
        if (pagesOut) *pagesOut = (int) pages.size();
        if (pdf) return ScoreRender::writePdf (dest.withFileExtension (".pdf"), pages, letter, err);
        juce::PNGImageFormat png;
        for (size_t i = 0; i < pages.size(); ++i)
        {
            auto f = i == 0 ? dest.withFileExtension (".png") : dest.getSiblingFile (dest.getFileNameWithoutExtension() + "-" + juce::String ((int) i + 1) + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (! os.openedOk() || ! png.writeImageToStream (pages[i], os)) { err = "Couldn't write " + f.getFullPathName(); return false; }
        }
        return true;
    }
}

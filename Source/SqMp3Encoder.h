#pragma once
// Squelchmoth 1.12 - Squelchmoth's own MP3 encoder (MPEG-1 Layer III).
//
// Written for Squelchmoth from the published MP3 standard: the 32-band polyphase analysis filterbank, an 18-line MDCT
// per band with alias reduction, a psychoacoustic model per scale-factor band (spreading across Bark, tonality,
// absolute threshold of hearing), the nested rate / distortion loops with scale factors, Huffman coding with the best
// code book and region split, a bit reservoir, mid / side stereo, CBR or quality-driven VBR, an Info / Xing frame
// (length and seeking) and ID3v2.3 tags. No third-party encoder is involved; the fixed standard tables are generated
// by scripts/gen_mp3_tables.py.
//
// Input: float PCM at 32, 44.1 or 48 kHz, mono or stereo (the exporter resamples anything else).

#include <JuceHeader.h>
#include "SqMp3Tables.h"

namespace SqMp3
{
    struct Tags { juce::String title, artist, album, track, year, genre, isrc, comment; };

    struct Settings
    {
        int bitrate = 320;          // CBR kb/s (32 ... 320)
        bool vbr = false;           // quality-driven VBR instead
        int vbrQuality = 0;         // 0 (best) ... 4
        bool jointStereo = true;    // mid / side when it saves bits
    };

    static constexpr int bitrates[15] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };

    //==========================================================================
    class BitWriter
    {
    public:
        void put (uint32_t value, int n)
        {
            for (int i = n - 1; i >= 0; --i)
            {
                cur = (uint8_t) ((cur << 1) | ((value >> i) & 1u));
                if (++nbits == 8) { bytes.push_back (cur); cur = 0; nbits = 0; }
            }
        }
        void align()          { while (nbits != 0) put (0, 1); }
        size_t bitCount() const { return bytes.size() * 8 + (size_t) nbits; }
        std::vector<uint8_t> bytes;
    private:
        uint8_t cur = 0; int nbits = 0;
    };

    //==========================================================================
    class Encoder
    {
    public:
        Encoder (double sampleRate, int numChannels, Settings s) : settings (s), nch (juce::jlimit (1, 2, numChannels))
        {
            srIndex = sampleRate == 44100.0 ? 0 : sampleRate == 48000.0 ? 1 : 2;   // header order: 44.1, 48, 32
            sr = srIndex == 0 ? 44100 : srIndex == 1 ? 48000 : 32000;
            const int* sfb = srIndex == 0 ? SqMp3Tables::sfbLong44100 : srIndex == 1 ? SqMp3Tables::sfbLong48000 : SqMp3Tables::sfbLong32000;
            for (int i = 0; i < 23; ++i) sfbEdge[i] = sfb[i];
            // analysis window: C[i] = D[i] / 32, D = the prototype with the standard's sign alternation every 64 taps
            for (int i = 0; i < 512; ++i)
            {
                const double h = i <= 256 ? SqMp3Tables::prototype[i] : SqMp3Tables::prototype[512 - i];
                const bool flip = (i / 64) % 2 == 1;   // the standard's window alternates sign every 64 taps
                const double d = flip ? -h : h;
                C[i] = d / 32.0;
            }
            for (int i = 0; i < 32; ++i) for (int k = 0; k < 64; ++k) M[i][k] = std::cos ((2 * i + 1) * (k - 16) * juce::MathConstants<double>::pi / 64.0);
            for (int k = 0; k < 36; ++k) win[k] = std::sin (juce::MathConstants<double>::pi / 36.0 * (k + 0.5));
            for (int i = 0; i < 18; ++i) for (int k = 0; k < 36; ++k) mdctCos[i][k] = std::cos (juce::MathConstants<double>::pi / 72.0 * (2 * k + 1 + 18) * (2 * i + 1));
            static const double ci[8] = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037 };
            for (int i = 0; i < 8; ++i) { cs[i] = 1.0 / std::sqrt (1.0 + ci[i] * ci[i]); ca[i] = ci[i] / std::sqrt (1.0 + ci[i] * ci[i]); }
            for (auto& x : X) std::fill (std::begin (x), std::end (x), 0.0);
            for (auto& o : overlap) for (auto& b : o) std::fill (std::begin (b), std::end (b), 0.0);
            initPsy();
            for (int i = 0; i < 8207; ++i) pow43[i] = std::pow ((double) i, 4.0 / 3.0);
            if (! settings.vbr) { brIndex = 1; for (int i = 1; i < 15; ++i) if (bitrates[i] <= settings.bitrate) brIndex = i; }
        }

        /** Writes the ID3v2.3 tag and an empty Info / Xing frame (filled in by finish()). */
        void begin (juce::OutputStream& out, const Tags& tags)
        {
            writeId3 (out, tags);
            infoPos = out.getPosition();
            auto frame = makeInfoFrame (0, 0, true);
            out.write (frame.data(), frame.size());
            audioStart = out.getPosition();
        }

        /** Feeds interleaved-by-channel float samples (any count). */
        void write (juce::OutputStream& out, const float* const* ch, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < nch; ++c) pcm[c][(size_t) pcmFill] = juce::jlimit (-1.0f, 1.0f, ch[juce::jmin (c, nch - 1)][i]);
                if (++pcmFill == 1152) { encodeFrame (out); pcmFill = 0; }
            }
        }

        /** Flushes (pads the last frame and one more for the filterbank delay) and completes the Info / Xing frame. */
        void finish (juce::OutputStream& out)
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                for (int i = pcmFill; i < 1152; ++i) for (int c = 0; c < nch; ++c) pcm[c][(size_t) i] = 0.0f;
                if (pcmFill > 0 || pass == 1) encodeFrame (out);
                pcmFill = 0;
            }
            flushFrames (out, true);
            const auto end = out.getPosition();
            if (out.setPosition (infoPos))
            {
                auto frame = makeInfoFrame (framesWritten, (uint32_t) (end - infoPos), false);
                out.write (frame.data(), frame.size());
                out.setPosition (end);
            }
            out.flush();
        }

        int framesOut() const   { return framesWritten; }
        /** Average bitrate actually written (kb/s). */
        double averageKbps() const   { return framesWritten > 0 ? (double) audioBytes * 8.0 / ((double) framesWritten * 1152.0 / sr) / 1000.0 : 0.0; }

    private:
        Settings settings;
        int nch, srIndex = 0, sr = 44100, brIndex = 14;
        int sfbEdge[23] {};
        double C[512], M[32][64], win[36], mdctCos[18][36], cs[8], ca[8];
        double X[2][512];
        double overlap[2][32][18];
        float pcm[2][1152] {};
        int pcmFill = 0;
        double pow43[8207];

        // psychoacoustics per long sfb
        double athEnergy[22] {}, bandBark[22] {};

        // output
        struct PendingFrame { std::vector<uint8_t> headerAndSide; int slotBytes = 0; int64_t slotStart = 0; };
        std::deque<PendingFrame> pending;
        std::vector<uint8_t> mainStream;    // every frame's main data, back to back
        int64_t mainStreamBase = 0;         // stream position of mainStream[0]
        int64_t slotsTotal = 0;             // bytes of main-data space in every frame so far
        int64_t mainEnd = 0;                // where the next frame's main data starts in the stream
        int framesWritten = 0; int64_t audioBytes = 0;
        juce::int64 infoPos = 0, audioStart = 0;
        std::vector<uint8_t> tocBytes;
        std::vector<int64_t> frameOffsets;

        //======================================================================
        void initPsy()
        {
            for (int b = 0; b < 22; ++b)
            {
                const double f0 = sfbEdge[b] * sr / 1152.0, f1 = sfbEdge[b + 1] * sr / 1152.0, fc = std::sqrt (juce::jmax (10.0, f0) * f1);
                bandBark[b] = 13.0 * std::atan (0.00076 * fc) + 3.5 * std::atan ((fc / 7500.0) * (fc / 7500.0));
                // absolute threshold (Terhardt), referred so that a full-scale sine is about 96 dB SPL
                double athDb = 1.0e9;
                for (int k = 0; k < 8; ++k)
                {
                    const double f = juce::jmax (20.0, (f0 + (f1 - f0) * (k + 0.5) / 8.0)) / 1000.0;
                    athDb = juce::jmin (athDb, 3.64 * std::pow (f, -0.8) - 6.5 * std::exp (-0.6 * (f - 3.3) * (f - 3.3)) + 0.001 * std::pow (f, 4.0));
                }
                athDb = juce::jmin (athDb, 70.0);
                // MDCT units: a full-scale sine puts about 0.25 * 18^2 * ... into one line; calibrated so 0 dBFS -> fsRef
                const double fsRef = 1.2;    // MDCT energy of a full-scale sine (its peak line measures 1.11)
                athEnergy[b] = fsRef * std::pow (10.0, (athDb - 96.0) / 10.0) * (sfbEdge[b + 1] - sfbEdge[b]);
            }
        }

        /** Allowed noise energy per sfb for one granule's lines (the masking threshold). */
        void maskingThreshold (const double* xr, double* allowed, double qualityDb)
        {
            double e[22], flat[22];
            for (int b = 0; b < 22; ++b)
            {
                double s = 0, sl = 0; const int w = sfbEdge[b + 1] - sfbEdge[b];
                for (int i = sfbEdge[b]; i < sfbEdge[b + 1]; ++i) { const double p = xr[i] * xr[i]; s += p; sl += std::log (p + 1.0e-12); }
                e[b] = s;
                const double gm = std::exp (sl / w), am = s / w + 1.0e-12;
                flat[b] = juce::jlimit (0.0, 1.0, (10.0 * std::log10 (gm / am + 1.0e-12)) / -40.0);   // 0 = noisy, 1 = tonal
            }
            for (int b = 0; b < 22; ++b)
            {
                double spread = 0;
                for (int k = 0; k < 22; ++k)
                {
                    const double dz = bandBark[b] - bandBark[k];   // > 0: masker below the maskee
                    const double att = dz >= 0 ? 15.0 * dz : 27.0 * -dz;
                    spread += e[k] * std::pow (10.0, -att / 10.0);
                }
                const double offsetDb = 6.0 + 18.0 * flat[b] + qualityDb;   // tonal maskers mask less
                allowed[b] = juce::jmax (athEnergy[b], spread * std::pow (10.0, -offsetDb / 10.0));
            }
        }

        //======================================================================
        void encodeFrame (juce::OutputStream& out)
        {
            // 1) filterbank + MDCT for both granules
            double xr[2][2][576];   // [granule][channel][line]
            for (int gr = 0; gr < 2; ++gr)
                for (int c = 0; c < nch; ++c)
                {
                    double sb[18][32];
                    for (int t = 0; t < 18; ++t)
                    {
                        const float* src = pcm[c] + gr * 576 + t * 32;
                        std::memmove (X[c] + 32, X[c], sizeof (double) * 480);
                        for (int i = 0; i < 32; ++i) X[c][31 - i] = src[i];
                        double Y[64];
                        for (int i = 0; i < 64; ++i) { double s = 0; for (int j = 0; j < 8; ++j) s += C[i + 64 * j] * X[c][i + 64 * j]; Y[i] = s; }
                        for (int i = 0; i < 32; ++i) { double s = 0; for (int k = 0; k < 64; ++k) s += M[i][k] * Y[k]; sb[t][i] = s; }
                    }
                    for (int band = 0; band < 32; ++band)
                    {
                        double z[36];
                        for (int k = 0; k < 18; ++k)
                        {
                            double v = sb[k][band];
                            if ((band & 1) && (k & 1)) v = -v;   // frequency inversion of odd bands
                            z[k] = overlap[c][band][k]; z[k + 18] = v; overlap[c][band][k] = v;
                        }
                        for (int i = 0; i < 18; ++i) { double s = 0; for (int k = 0; k < 36; ++k) s += win[k] * z[k] * mdctCos[i][k]; xr[gr][c][band * 18 + i] = s / 9.0; }   // the decoder's IMDCT has a gain of 9
                    }
                    for (int band = 1; band < 32; ++band)   // alias reduction butterflies
                        for (int i = 0; i < 8; ++i)
                        {
                            const double bu = xr[gr][c][18 * band - 1 - i], bd = xr[gr][c][18 * band + i];
                            xr[gr][c][18 * band - 1 - i] = bu * cs[i] + bd * ca[i];   // the encoder's butterflies (the decoder undoes them)
                            xr[gr][c][18 * band + i] = bd * cs[i] - bu * ca[i];
                        }
                }

            // 2) mid / side?
            bool ms = false;
            if (nch == 2 && settings.jointStereo)
            {
                double el = 0, er = 0, ed = 0;
                for (int gr = 0; gr < 2; ++gr) for (int i = 0; i < 576; ++i) { el += xr[gr][0][i] * xr[gr][0][i]; er += xr[gr][1][i] * xr[gr][1][i]; const double d = xr[gr][0][i] - xr[gr][1][i]; ed += d * d; }
                ms = ed < 0.35 * (el + er);
                if (ms)
                    for (int gr = 0; gr < 2; ++gr)
                        for (int i = 0; i < 576; ++i)
                        {
                            const double l = xr[gr][0][i], r = xr[gr][1][i];
                            xr[gr][0][i] = (l + r) * juce::MathConstants<double>::sqrt2 * 0.5; xr[gr][1][i] = (l - r) * juce::MathConstants<double>::sqrt2 * 0.5;
                        }
            }

            // 3) quantise each granule / channel
            const int sideBytes = nch == 2 ? 32 : 17;
            const double quality = settings.vbr ? 4.0 - (double) settings.vbrQuality * 2.0 : 0.0;   // V0 keeps noise 4 dB under the mask, V4 lets it 4 dB over
            Granule g[2][2];
            const int reservoir = (int) std::clamp<int64_t> (slotsTotal - mainEnd, 0, 511);
            int frameBr = brIndex;
            if (! settings.vbr)
            {
                const int slot = slotBytes (frameBr, sideBytes);
                const int meanBits = slot * 8 / (2 * nch);
                int pool = reservoir * 8 + slot * 8;
                for (int gr = 0; gr < 2; ++gr)
                    for (int c = 0; c < nch; ++c)
                    {
                        const int left = 2 * nch - (gr * nch + c);
                        const int budget = juce::jmin (4095, juce::jmax (0, juce::jmin (pool - (left - 1) * meanBits / 2, meanBits + (reservoir * 8) / (2 * nch))));
                        quantise (xr[gr][c], g[gr][c], budget, quality);
                        pool -= g[gr][c].part23;
                    }
            }
            else
            {
                int need = 0;
                for (int gr = 0; gr < 2; ++gr) for (int c = 0; c < nch; ++c) { quantiseVbr (xr[gr][c], g[gr][c], quality); need += g[gr][c].part23; }
                frameBr = 14;
                for (int i = 1; i < 15; ++i) if (slotBytes (i, sideBytes) * 8 + reservoir * 8 >= need + 8) { frameBr = i; break; }
                if (slotBytes (frameBr, sideBytes) * 8 + reservoir * 8 < need)
                {
                    const int slot = slotBytes (14, sideBytes); int pool = reservoir * 8 + slot * 8;
                    for (int gr = 0; gr < 2; ++gr) for (int c = 0; c < nch; ++c)
                    {
                        const int left = 2 * nch - (gr * nch + c);
                        quantise (xr[gr][c], g[gr][c], juce::jmin (4095, pool / left), quality);
                        pool -= g[gr][c].part23;
                    }
                }
            }

            // 4) main data
            BitWriter md;
            for (int gr = 0; gr < 2; ++gr) for (int c = 0; c < nch; ++c) writeGranule (md, g[gr][c]);
            md.align();
            const int slot = slotBytes (frameBr, sideBytes);
            int mainBegin = (int) (slotsTotal - mainEnd);
            if (mainBegin > 511) { const int stuff = mainBegin - 511; mainStream.insert (mainStream.end(), (size_t) stuff, 0); mainEnd += stuff; mainBegin = 511; }
            jassert (mainBegin >= 0 && (int64_t) md.bytes.size() <= mainBegin + slot);

            // 5) header + side info
            BitWriter h;
            const int padding = needPadding (frameBr) ? 1 : 0;
            h.put (0xFFF, 12); h.put (1, 1); h.put (1, 2); h.put (1, 1);   // sync, MPEG-1, Layer III, no CRC
            h.put ((uint32_t) frameBr, 4); h.put ((uint32_t) srIndex, 2); h.put ((uint32_t) padding, 1); h.put (0, 1);
            h.put (nch == 2 ? (ms ? 1u : 0u) : 3u, 2); h.put (ms ? 2u : 0u, 2);   // joint stereo (MS on) / stereo / mono
            h.put (0, 1); h.put (1, 1); h.put (0, 2);                              // copyright, original, emphasis
            h.put ((uint32_t) mainBegin, 9); h.put (0, nch == 2 ? 3 : 5);
            for (int c = 0; c < nch; ++c) h.put (0, 4);                            // scfsi
            for (int gr = 0; gr < 2; ++gr)
                for (int c = 0; c < nch; ++c)
                {
                    auto& q = g[gr][c];
                    h.put ((uint32_t) q.part23, 12); h.put ((uint32_t) q.bigValues, 9); h.put ((uint32_t) q.globalGain, 8); h.put ((uint32_t) q.sfCompress, 4);
                    h.put (0, 1);   // long blocks
                    for (int r = 0; r < 3; ++r) h.put ((uint32_t) q.table[r], 5);
                    h.put ((uint32_t) q.region0, 4); h.put ((uint32_t) q.region1, 3);
                    h.put (0, 1); h.put ((uint32_t) q.sfScale, 1); h.put ((uint32_t) q.quadTable, 1);
                }
            h.align();

            PendingFrame pf; pf.headerAndSide = h.bytes; pf.slotBytes = slot + padding; pf.slotStart = slotsTotal;
            slotsTotal += pf.slotBytes;
            mainStream.insert (mainStream.end(), md.bytes.begin(), md.bytes.end());
            mainEnd += (int64_t) md.bytes.size();
            pending.push_back (std::move (pf));
            flushFrames (out, false);
        }

        int slotBytes (int br, int sideBytes) const   { return 144 * bitrates[br] * 1000 / sr - 4 - sideBytes; }

        bool needPadding (int br)
        {
            if (sr != 44100) return false;
            // 44.1 kHz frames alternate 417 / 418 bytes so the rate comes out exact
            padAcc += 144.0 * bitrates[br] * 1000.0 / sr - std::floor (144.0 * bitrates[br] * 1000.0 / sr);
            if (padAcc >= 1.0) { padAcc -= 1.0; return true; }
            return false;
        }
        double padAcc = 0;

        /** Writes every queued frame whose main-data slot is now filled (all of them at the end). */
        void flushFrames (juce::OutputStream& out, bool final)
        {
            while (! pending.empty())
            {
                auto& f = pending.front();
                const int64_t need = f.slotStart + f.slotBytes;
                if (! final && mainStreamBase + (int64_t) mainStream.size() < need) break;
                frameOffsets.push_back ((int64_t) (out.getPosition() - audioStart));
                out.write (f.headerAndSide.data(), f.headerAndSide.size());
                for (int64_t p = f.slotStart; p < need; ++p)
                {
                    const int64_t i = p - mainStreamBase;
                    const uint8_t b = (i >= 0 && i < (int64_t) mainStream.size()) ? mainStream[(size_t) i] : 0;
                    out.writeByte ((char) b);
                }
                audioBytes += (int64_t) f.headerAndSide.size() + f.slotBytes;
                const int64_t drop = std::min<int64_t> ((int64_t) mainStream.size(), need - mainStreamBase - 600);
                if (drop > 4096) { mainStream.erase (mainStream.begin(), mainStream.begin() + (ptrdiff_t) drop); mainStreamBase += drop; }
                ++framesWritten;
                pending.pop_front();
            }
        }

        //======================================================================
        struct Granule
        {
            int ix[576] {};
            uint8_t neg[576] {};
            int sf[22] {};
            int part23 = 0, bigValues = 0, globalGain = 210, sfCompress = 0, sfScale = 0, table[3] {}, region0 = 0, region1 = 0, quadTable = 0, count1End = 0;
            int part2 = 0;
        };

        /** Quantises one granule: inner loop (global gain to fit the budget) inside the outer loop (scale factors for
            bands whose noise is over the masking threshold). With vbr, the budget is the ceiling and the goal is quality. */
        /** VBR: the coarsest quantisation that keeps every band under its masking threshold (scale factors lift the
            bands that can't make it), as long as it fits the 4095-bit granule limit. */
        void quantiseVbr (const double* xr, Granule& g, double qualityDb)
        {
            double allowed[22]; maskingThreshold (xr, allowed, qualityDb);
            double xr34[576], xabs[576]; double maxAbs = 0;
            for (int i = 0; i < 576; ++i) { xabs[i] = std::abs (xr[i]); xr34[i] = std::pow (xabs[i], 0.75); maxAbs = juce::jmax (maxAbs, xabs[i]); }
            g = Granule();
            if (maxAbs < 1.0e-9) { g.globalGain = 0; return; }
            int sf[22] {};
            auto overBands = [&] (Granule& t, int gain, int* overFlags) -> int
            {
                if (quantiseWith (xr34, t, gain, true) < 0) return 99;
                const double step = std::pow (2.0, (gain - 210) / 4.0);
                int n = 0;
                for (int b = 0; b < 22; ++b)
                {
                    const double amp = std::pow (2.0, 0.5 * t.sf[b]);
                    double noise = 0;
                    for (int i = sfbEdge[b]; i < sfbEdge[b + 1]; ++i) { const double d = xabs[i] - pow43[juce::jmin (8206, t.ix[i])] * step / amp; noise += d * d; }
                    const bool o = noise > allowed[b];
                    if (overFlags) overFlags[b] = o ? 1 : 0;
                    n += o ? 1 : 0;
                }
                return n;
            };
            Granule best; bool have = false;
            for (int iter = 0; iter < 16; ++iter)
            {
                Granule t; std::copy (std::begin (sf), std::end (sf), t.sf);
                if (! chooseSfCompress (t)) break;
                int lo = 0, hi = 255, okGain = -1;
                while (lo <= hi)
                {
                    const int mid = (lo + hi) / 2;
                    const int o = overBands (t, mid, nullptr);
                    if (o == 99) lo = mid + 1;                       // values too large to code: needs a coarser step
                    else if (o == 0) { okGain = mid; lo = mid + 1; }
                    else hi = mid - 1;
                }
                if (okGain >= 0)
                {
                    overBands (t, okGain, nullptr);
                    regionsAndTables (t); t.part23 = t.part2 + countBits (t);
                    if (t.part23 <= 4095) { best = t; have = true; break; }
                }
                // can't meet every band: lift the ones that fail at the finest gain that still fits
                int flags[22] {}; overBands (t, juce::jmax (0, okGain < 0 ? 100 : okGain), flags);
                bool changed = false;
                for (int b = 0; b < 21; ++b) if (flags[b] && sf[b] < 15) { ++sf[b]; changed = true; }
                if (! changed) break;
            }
            if (! have) { quantise (xr, g, 4095, qualityDb); return; }
            g = best;
            for (int i = 0; i < 576; ++i) g.neg[i] = xr[i] < 0.0 ? 1 : 0;
        }

        void quantise (const double* xr, Granule& g, int budget, double qualityDb, bool vbr = false)
        {
            double allowed[22]; maskingThreshold (xr, allowed, qualityDb);
            double xr34[576], xabs[576]; double maxAbs = 0;
            for (int i = 0; i < 576; ++i) { xabs[i] = std::abs (xr[i]); xr34[i] = std::pow (xabs[i], 0.75); maxAbs = juce::jmax (maxAbs, xabs[i]); }
            g = Granule();
            if (maxAbs < 1.0e-9) { g.globalGain = 0; return; }

            int sf[22] {}; int bestGain = 210; Granule best; double bestOver = 1.0e30; bool haveBest = false;
            for (int outer = 0; outer < 30; ++outer)
            {
                Granule t; std::copy (std::begin (sf), std::end (sf), t.sf);
                t.sfScale = 0;
                if (! chooseSfCompress (t)) break;
                const int part2 = t.part2;
                // inner loop: binary search the smallest global gain whose bits fit
                int lo = 0, hi = 255, fit = -1;
                while (lo <= hi)
                {
                    const int mid = (lo + hi) / 2;
                    const int bits = quantiseWith (xr34, t, mid, false);
                    if (bits >= 0 && bits + part2 <= budget) { fit = mid; hi = mid - 1; } else lo = mid + 1;
                }
                if (fit < 0) { if (! haveBest) { quantiseWith (xr34, t, 255, true); t.part23 = t.part2 + countBits (t); best = t; haveBest = true; } break; }
                quantiseWith (xr34, t, fit, true);
                t.part23 = part2 + countBits (t);
                // distortion per band
                double over = 0; bool anyOver = false; int overBand[22] {};
                const double step = std::pow (2.0, (t.globalGain - 210) / 4.0);
                for (int b = 0; b < 22; ++b)
                {
                    const double amp = std::pow (2.0, 0.5 * sf[b]);
                    double noise = 0;
                    for (int i = sfbEdge[b]; i < sfbEdge[b + 1]; ++i) { const double rec = pow43[juce::jmin (8206, t.ix[i])] * step / amp; const double d = xabs[i] - rec; noise += d * d; }
                    const double r = noise / allowed[b];
                    if (r > 1.0) { anyOver = true; overBand[b] = 1; over += 10.0 * std::log10 (r); }
                }
                if (over < bestOver) { bestOver = over; best = t; haveBest = true; bestGain = fit; }
                if (! anyOver) break;
                // amplify the bands over the threshold
                bool changed = false;
                for (int b = 0; b < 21; ++b) if (overBand[b] && sf[b] < 15) { ++sf[b]; changed = true; }
                if (! changed) break;
                if (vbr && budget >= 4095 && fit <= 0) break;
            }
            juce::ignoreUnused (bestGain);
            g = best;
            for (int i = 0; i < 576; ++i) g.neg[i] = xr[i] < 0.0 ? 1 : 0;
            regionsAndTables (g);
            g.part23 = g.part2 + countBits (g, true);
        }

        /** Picks the smallest scalefac_compress that holds sf[]; sets part2. */
        bool chooseSfCompress (Granule& t)
        {
            int max1 = 0, max2 = 0;
            for (int b = 0; b < 11; ++b) max1 = juce::jmax (max1, t.sf[b]);
            for (int b = 11; b < 21; ++b) max2 = juce::jmax (max2, t.sf[b]);
            t.sf[21] = 0;
            int bestC = -1, bestBits = 1 << 30;
            for (int c = 0; c < 16; ++c)
            {
                const int s1 = SqMp3Tables::slen1[c], s2 = SqMp3Tables::slen2[c];
                if (max1 >= (1 << s1) || max2 >= (1 << s2)) continue;
                const int bits = 11 * s1 + 10 * s2;
                if (bits < bestBits) { bestBits = bits; bestC = c; }
            }
            if (bestC < 0) return false;
            t.sfCompress = bestC; t.part2 = bestBits;
            return true;
        }

        /** Quantises with a global gain; returns the estimated Huffman bits (default regions) or -1 if a value is too big. */
        int quantiseWith (const double* xr34, Granule& t, int gain, bool store)
        {
            const double q = std::pow (2.0, -(gain - 210) * 0.1875);   // (2^-(g-210)/4)^(3/4)
            int ix[576];
            for (int b = 0; b < 22; ++b)
            {
                const double amp = std::pow (2.0, 0.375 * t.sf[b]);    // (2^(0.5 sf))^(3/4)
                for (int i = sfbEdge[b]; i < sfbEdge[b + 1]; ++i)
                {
                    const double v = xr34[i] * q * amp;
                    if (v > 8206.0) return -1;
                    ix[i] = (int) (v + 0.4054);
                }
            }
            if (store) { std::copy (ix, ix + 576, t.ix); t.globalGain = gain; }
            Granule tmp; std::copy (ix, ix + 576, tmp.ix);
            regionsAndTables (tmp, false);
            return countBits (tmp);
        }

        /** Finds big_values / count1 and (when thorough) the best region split; picks the cheapest code books. */
        void regionsAndTables (Granule& g, bool thorough = true)
        {
            int last = 575;
            while (last >= 0 && g.ix[last] == 0) --last;
            int c1 = last + 1;
            c1 = (c1 + 3) & ~3; if (c1 > 576) c1 = 576;
            int start = c1;
            while (start >= 4 && std::abs (g.ix[start - 1]) <= 1 && std::abs (g.ix[start - 2]) <= 1 && std::abs (g.ix[start - 3]) <= 1 && std::abs (g.ix[start - 4]) <= 1) start -= 4;
            if (start & 1) ++start;
            g.bigValues = juce::jmin (288, start / 2);
            g.count1End = juce::jmax (g.bigValues * 2, c1);
            const int bigEnd = g.bigValues * 2;
            // count1 table
            int qa = 0, qb = 0;
            for (int i = bigEnd; i + 3 < g.count1End + 1 && i < 576; i += 4)
            {
                const int a = (g.ix[i] != 0) * 8 + (g.ix[i + 1] != 0) * 4 + (g.ix[i + 2] != 0) * 2 + (g.ix[i + 3] != 0);
                qa += SqMp3Tables::quad0[a].len; qb += SqMp3Tables::quad1[a].len;
            }
            g.quadTable = qb < qa ? 1 : 0;
            // regions
            auto edgeIndex = [&] (int count) { return sfbEdge[juce::jmin (22, count)]; };
            int bestR0 = 0, bestR1 = 0, bestBits = 1 << 30;
            if (thorough)
            {
                for (int r0 = 0; r0 < 16; ++r0)
                {
                    const int e0 = edgeIndex (r0 + 1);
                    if (e0 > bigEnd && r0 > 0) break;
                    for (int r1 = 0; r1 < 8 && r0 + r1 + 2 <= 22; ++r1)
                    {
                        const int e1 = edgeIndex (r0 + r1 + 2);
                        if (e1 > bigEnd && r1 > 0) break;
                        const int b = regionBits (g, 0, juce::jmin (e0, bigEnd)) + regionBits (g, juce::jmin (e0, bigEnd), juce::jmin (e1, bigEnd)) + regionBits (g, juce::jmin (e1, bigEnd), bigEnd);
                        if (b < bestBits) { bestBits = b; bestR0 = r0; bestR1 = r1; }
                    }
                }
            }
            else { bestR0 = 7; bestR1 = 7; while (bestR0 > 0 && edgeIndex (bestR0 + 1) > bigEnd) --bestR0; while (bestR1 > 0 && edgeIndex (bestR0 + bestR1 + 2) > bigEnd) --bestR1; }
            g.region0 = bestR0; g.region1 = bestR1;
            const int e0 = juce::jmin (edgeIndex (bestR0 + 1), bigEnd), e1 = juce::jmin (edgeIndex (bestR0 + bestR1 + 2), bigEnd);
            g.table[0] = bestTable (g, 0, e0); g.table[1] = bestTable (g, e0, e1); g.table[2] = bestTable (g, e1, bigEnd);
        }

        int regionBits (const Granule& g, int a, int b) const
        {
            const int t = bestTable (g, a, b);
            return tableBits (g, a, b, t);
        }

        int tableBits (const Granule& g, int a, int b, int t) const
        {
            if (a >= b) return 0;
            const auto& bk = SqMp3Tables::books[t];
            if (bk.codes == nullptr) return 1 << 20;
            int bits = 0;
            for (int i = a; i + 1 < b + 1 && i + 1 < 576; i += 2)
            {
                int x = std::abs (g.ix[i]), y = std::abs (g.ix[i + 1]);
                if (bk.linbits == 0 && (x >= bk.dim || y >= bk.dim)) return 1 << 20;
                if (bk.linbits > 0 && (x > 15 + (1 << bk.linbits) - 1 || y > 15 + (1 << bk.linbits) - 1)) return 1 << 20;
                const int cx = juce::jmin (x, 15), cy = juce::jmin (y, 15);
                bits += bk.codes[cx * bk.dim + cy].len;
                if (x) bits += 1 + (x >= 15 && bk.linbits > 0 ? bk.linbits : 0);
                if (y) bits += 1 + (y >= 15 && bk.linbits > 0 ? bk.linbits : 0);
            }
            return bits;
        }

        int bestTable (const Granule& g, int a, int b) const
        {
            if (a >= b) return 0;
            int mx = 0; for (int i = a; i < b; ++i) mx = juce::jmax (mx, std::abs (g.ix[i]));
            if (mx == 0) return 0;
            static const int small[] = { 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15 };
            int bestT = -1, bestB = 1 << 30;
            if (mx <= 15)
                for (int t : small)
                {
                    if (SqMp3Tables::books[t].dim <= mx) continue;
                    const int bits = tableBits (g, a, b, t);
                    if (bits < bestB) { bestB = bits; bestT = t; }
                }
            for (int t = 16; t < 32; ++t)
            {
                if (15 + (1 << SqMp3Tables::books[t].linbits) - 1 < mx) continue;
                const int bits = tableBits (g, a, b, t);
                if (bits < bestB) { bestB = bits; bestT = t; }
                if (t == 23) continue;
            }
            return bestT < 0 ? 31 : bestT;
        }

        int countBits (const Granule& g, bool) const   { return countBits (g); }
        int countBits (const Granule& g) const
        {
            const int bigEnd = g.bigValues * 2;
            const int e0 = juce::jmin (sfbEdge[juce::jmin (22, g.region0 + 1)], bigEnd), e1 = juce::jmin (sfbEdge[juce::jmin (22, g.region0 + g.region1 + 2)], bigEnd);
            int bits = tableBits (g, 0, e0, g.table[0]) + tableBits (g, e0, e1, g.table[1]) + tableBits (g, e1, bigEnd, g.table[2]);
            for (int i = bigEnd; i < g.count1End && i + 3 < 576 + 3; i += 4)
            {
                if (i >= 576) break;
                const int v[4] = { g.ix[i], i + 1 < 576 ? g.ix[i + 1] : 0, i + 2 < 576 ? g.ix[i + 2] : 0, i + 3 < 576 ? g.ix[i + 3] : 0 };
                const int a = (v[0] != 0) * 8 + (v[1] != 0) * 4 + (v[2] != 0) * 2 + (v[3] != 0);
                bits += (g.quadTable ? SqMp3Tables::quad1 : SqMp3Tables::quad0)[a].len + (v[0] != 0) + (v[1] != 0) + (v[2] != 0) + (v[3] != 0);
            }
            return bits;
        }

        //======================================================================
        void writeGranule (BitWriter& w, const Granule& g)
        {
            auto signOf = [&g] (int i) { return (uint32_t) g.neg[i]; };
            const auto startBits = w.bitCount();
            const int s1 = SqMp3Tables::slen1[g.sfCompress], s2 = SqMp3Tables::slen2[g.sfCompress];
            for (int b = 0; b < 11; ++b) w.put ((uint32_t) g.sf[b], s1);
            for (int b = 11; b < 21; ++b) w.put ((uint32_t) g.sf[b], s2);
            const int bigEnd = g.bigValues * 2;
            const int e0 = juce::jmin (sfbEdge[juce::jmin (22, g.region0 + 1)], bigEnd), e1 = juce::jmin (sfbEdge[juce::jmin (22, g.region0 + g.region1 + 2)], bigEnd);
            auto region = [&] (int a, int b, int t)
            {
                if (a >= b || t == 0) return;
                const auto& bk = SqMp3Tables::books[t];
                for (int i = a; i < b; i += 2)
                {
                    const int sx = g.ix[i], sy = g.ix[i + 1];   // ix holds magnitudes; the sign comes from the source
                    const int x = std::abs (sx), y = std::abs (sy);
                    const int cx = juce::jmin (x, 15), cy = juce::jmin (y, 15);
                    const auto& code = bk.codes[cx * bk.dim + cy];
                    w.put (code.bits, code.len);
                    if (x) { if (x >= 15 && bk.linbits > 0) w.put ((uint32_t) (x - 15), bk.linbits); w.put (signOf (i), 1); }
                    if (y) { if (y >= 15 && bk.linbits > 0) w.put ((uint32_t) (y - 15), bk.linbits); w.put (signOf (i + 1), 1); }
                }
            };
            region (0, e0, g.table[0]); region (e0, e1, g.table[1]); region (e1, bigEnd, g.table[2]);
            for (int i = bigEnd; i < g.count1End && i < 576; i += 4)
            {
                const int v[4] = { g.ix[i], i + 1 < 576 ? g.ix[i + 1] : 0, i + 2 < 576 ? g.ix[i + 2] : 0, i + 3 < 576 ? g.ix[i + 3] : 0 };
                const int a = (v[0] != 0) * 8 + (v[1] != 0) * 4 + (v[2] != 0) * 2 + (v[3] != 0);
                const auto& code = (g.quadTable ? SqMp3Tables::quad1 : SqMp3Tables::quad0)[a];
                w.put (code.bits, code.len);
                for (int k = 0; k < 4; ++k) if (v[k]) w.put (signOf (i + k), 1);
            }
            if ((int) (w.bitCount() - startBits) != g.part23) ++bitMismatches;
        }
    public:
        int bitMismatches = 0;   // granules whose written length differs from part2_3_length (must stay 0)
    private:


        //======================================================================
        std::vector<uint8_t> makeInfoFrame (int frames, uint32_t bytes, bool placeholder)
        {
            // a silent frame at the highest rate carrying the Info (CBR) / Xing (VBR) tag: frame count, byte count, TOC
            const int br = settings.vbr ? 9 : brIndex;
            const int size = 144 * bitrates[br] * 1000 / sr;
            std::vector<uint8_t> f ((size_t) size, 0);
            BitWriter h;
            h.put (0xFFF, 12); h.put (1, 1); h.put (1, 2); h.put (1, 1);
            h.put ((uint32_t) br, 4); h.put ((uint32_t) srIndex, 2); h.put (0, 1); h.put (0, 1);
            h.put (nch == 2 ? 0u : 3u, 2); h.put (0, 2); h.put (0, 1); h.put (1, 1); h.put (0, 2);
            for (size_t i = 0; i < h.bytes.size(); ++i) f[i] = h.bytes[i];
            size_t p = 4 + (nch == 2 ? 32 : 17);
            const char* id = settings.vbr ? "Xing" : "Info";
            for (int i = 0; i < 4; ++i) f[p++] = (uint8_t) id[i];
            auto be32 = [&] (uint32_t v) { f[p++] = (uint8_t) (v >> 24); f[p++] = (uint8_t) (v >> 16); f[p++] = (uint8_t) (v >> 8); f[p++] = (uint8_t) v; };
            be32 (placeholder ? 0x0Fu : 0x0Fu);   // frames, bytes, TOC, quality present
            be32 ((uint32_t) frames); be32 (bytes);
            for (int i = 0; i < 100; ++i)
            {
                uint8_t v = 0;
                if (! placeholder && frames > 0 && ! frameOffsets.empty() && bytes > 0)
                {
                    const size_t fi = juce::jmin (frameOffsets.size() - 1, (size_t) ((double) i / 100.0 * frameOffsets.size()));
                    v = (uint8_t) juce::jlimit (0, 255, (int) (256.0 * (double) frameOffsets[fi] / (double) bytes));
                }
                f[p++] = v;
            }
            be32 (settings.vbr ? (uint32_t) (settings.vbrQuality * 10) : 0u);
            const char* enc = "SQMOTH1.12";
            for (int i = 0; i < 9 && p < f.size(); ++i) f[p++] = (uint8_t) enc[i];
            return f;
        }

        static void writeId3 (juce::OutputStream& out, const Tags& t)
        {
            juce::MemoryOutputStream body;
            auto text = [&] (const char* id, const juce::String& v)
            {
                if (v.isEmpty()) return;
                juce::MemoryOutputStream fr;
                fr.writeByte (1);                                   // UTF-16 with BOM
                fr.writeByte ((char) 0xFF); fr.writeByte ((char) 0xFE);
                for (auto cp : v) { fr.writeShort ((short) (cp > 0xFFFF ? '?' : cp)); }
                writeFrame (body, id, fr);
            };
            text ("TIT2", t.title); text ("TPE1", t.artist); text ("TALB", t.album); text ("TRCK", t.track);
            text ("TYER", t.year); text ("TCON", t.genre); text ("TSRC", t.isrc); text ("TENC", "Squelchmoth");
            if (t.comment.isNotEmpty())
            {
                juce::MemoryOutputStream fr;
                fr.writeByte (1); fr.write ("eng", 3);
                fr.writeByte ((char) 0xFF); fr.writeByte ((char) 0xFE); fr.writeShort (0);   // empty description
                fr.writeByte ((char) 0xFF); fr.writeByte ((char) 0xFE);
                for (auto cp : t.comment) fr.writeShort ((short) (cp > 0xFFFF ? '?' : cp));
                writeFrame (body, "COMM", fr);
            }
            const auto size = (uint32_t) body.getDataSize() + 256;   // a little padding for later edits
            out.write ("ID3", 3); out.writeByte (3); out.writeByte (0); out.writeByte (0);
            out.writeByte ((char) ((size >> 21) & 0x7F)); out.writeByte ((char) ((size >> 14) & 0x7F)); out.writeByte ((char) ((size >> 7) & 0x7F)); out.writeByte ((char) (size & 0x7F));
            out.write (body.getData(), body.getDataSize());
            for (int i = 0; i < 256; ++i) out.writeByte (0);
        }
        static void writeFrame (juce::MemoryOutputStream& body, const char* id, juce::MemoryOutputStream& fr)
        {
            body.write (id, 4);
            const auto n = (uint32_t) fr.getDataSize();
            body.writeByte ((char) (n >> 24)); body.writeByte ((char) (n >> 16)); body.writeByte ((char) (n >> 8)); body.writeByte ((char) n);
            body.writeShort (0);
            body.write (fr.getData(), fr.getDataSize());
        }

        JUCE_DECLARE_NON_COPYABLE (Encoder)
    };

    //==========================================================================
    /** The MP3 rate a mix at this rate is encoded at (88.2 / 176.4 kHz -> 44.1, 96 / 192 -> 48). */
    inline double mp3RateFor (double sr)
    {
        if (sr == 32000.0 || sr == 44100.0 || sr == 48000.0) return sr;
        return std::fmod (sr, 11025.0) == 0.0 ? 44100.0 : 48000.0;
    }

    inline bool encodeMp3 (const juce::File& wav, const juce::File& mp3, int bitrate, int vbrQuality, const SqMp3::Tags& tags, juce::String& err)
    {
        juce::WavAudioFormat wf;
        std::unique_ptr<juce::AudioFormatReader> rd (wf.createReaderFor (wav.createInputStream().release(), true));
        if (rd == nullptr) { err = "Couldn't read the rendered mix for MP3 encoding."; return false; }
        const double inRate = rd->sampleRate, outRate = mp3RateFor (inRate), ratio = inRate / outRate;
        const int nch = (int) juce::jlimit (1u, 2u, rd->numChannels);
        mp3.deleteFile();
        auto os = mp3.createOutputStream();
        if (os == nullptr) { err = "Couldn't write " + mp3.getFullPathName(); return false; }
        SqMp3::Settings st; st.bitrate = bitrate; st.vbr = vbrQuality >= 0; st.vbrQuality = juce::jlimit (0, 4, juce::jmax (0, vbrQuality));
        SqMp3::Encoder enc (outRate, nch, st);
        enc.begin (*os, tags);
        const int block = 8192;
        juce::AudioBuffer<float> in (nch, block);
        std::vector<float> pend[2], outv[2];
        juce::WindowedSincInterpolator interp[2];
        for (juce::int64 pos = 0; pos < rd->lengthInSamples + (ratio != 1.0 ? block : 0); pos += block)
        {
            const int n = (int) std::clamp<juce::int64> (rd->lengthInSamples - pos, 0, block);
            if (n > 0) rd->read (&in, 0, n, pos, true, nch > 1); else in.clear();
            const int got = n > 0 ? n : block;   // one extra block of silence flushes the resampler
            if (ratio == 1.0)
            {
                if (n <= 0) break;
                const float* ch[2] = { in.getReadPointer (0), in.getReadPointer (nch - 1) };
                enc.write (*os, ch, n);
                continue;
            }
            for (int c = 0; c < nch; ++c) pend[c].insert (pend[c].end(), in.getReadPointer (c), in.getReadPointer (c) + got);
            const int outN = (int) std::floor ((double) ((int) pend[0].size() - 64) / ratio);
            if (outN <= 0) continue;
            int used = 0;
            for (int c = 0; c < nch; ++c) { outv[c].resize ((size_t) outN); used = interp[c].process (ratio, pend[c].data(), outv[c].data(), outN); }
            for (int c = 0; c < nch; ++c) pend[c].erase (pend[c].begin(), pend[c].begin() + used);
            const float* ch[2] = { outv[0].data(), outv[nch - 1].data() };
            enc.write (*os, ch, outN);
        }
        enc.finish (*os);
        return mp3.getSize() > 0;
    }

}

// Squelchmoth - Draft separator on raw audio (evaluation helper): split_cli in.f32 out.f32 [rate]
// in: interleaved stereo float32; out: 8 planar-interleaved channels per frame (vocals L R, drums L R, bass L R, other L R).
#include <JuceHeader.h>
#include "SqSplitDsp.h"
#include <cstdio>
int main (int argc, char** argv)
{
    if (argc < 3) return 2;
    const double sr = argc > 3 ? std::atof (argv[3]) : 44100.0;
    FILE* f = std::fopen (argv[1], "rb"); if (f == nullptr) return 1;
    std::vector<float> in; float buf[4096]; size_t got;
    while ((got = std::fread (buf, sizeof (float), 4096, f)) > 0) in.insert (in.end(), buf, buf + got);
    std::fclose (f);
    const int n = (int) in.size() / 2; std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) { L[(size_t) i] = in[(size_t) (2 * i)]; R[(size_t) i] = in[(size_t) (2 * i + 1)]; }
    auto st = SqSplit::splitBuffers (L.data(), R.data(), n, sr);
    FILE* o = std::fopen (argv[2], "wb"); if (o == nullptr) return 1;
    std::vector<float> frame (8);
    for (int i = 0; i < n; ++i) { for (int c = 0; c < 8; ++c) frame[(size_t) c] = st[(size_t) c][(size_t) i]; std::fwrite (frame.data(), sizeof (float), 8, o); }
    std::fclose (o);
    return 0;
}

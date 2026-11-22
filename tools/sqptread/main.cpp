// sqptread: lists what a Pro Tools session file holds, as JSON on standard output. A separate program so that the
// library it uses (libptformat, LGPL 2.1 or later - see ThirdParty/ptformat) stays replaceable by anyone.
// Usage: sqptread <session.ptx | .ptf | .pts>      Exit code 0 on success, 2 if the file could not be read.
#include <cstdio>
#include <string>
#include "ptformat/ptformat.h"

static std::string esc (const std::string& s)
{
    std::string o; char b[8];
    for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += (char) c; } else if (c < 0x20) { std::snprintf (b, sizeof (b), "\\u%04x", c); o += b; } else o += (char) c; }
    return o;
}
int main (int argc, char** argv)
{
    if (argc < 2) { std::fprintf (stderr, "usage: sqptread <session file>\n"); return 1; }
    PTFFormat ptf; const int rc = ptf.load (argv[1], 48000);
    if (rc != 0) { std::printf ("{\"ok\":false,\"error\":%d}\n", rc); return 2; }
    std::printf ("{\"ok\":true,\"version\":%d,\"rate\":%lld,\n\"files\":[", (int) ptf.version(), (long long) ptf.sessionrate());
    bool first = true;
    for (const auto& a : ptf.audiofiles()) { std::printf ("%s{\"index\":%d,\"name\":\"%s\",\"length\":%llu}", first ? "" : ",", (int) a.index, esc (a.filename).c_str(), (unsigned long long) a.length); first = false; }
    std::printf ("],\n\"audio\":["); first = true;
    for (const auto& t : ptf.tracks())
    {
        std::printf ("%s\n{\"track\":\"%s\",\"index\":%d,\"file\":\"%s\",\"at\":%llu,\"offset\":%llu,\"length\":%llu,\"region\":\"%s\"}", first ? "" : ",", esc (t.name).c_str(), (int) t.index, esc (t.reg.wave.filename).c_str(),
                     (unsigned long long) t.reg.startpos, (unsigned long long) t.reg.sampleoffset, (unsigned long long) t.reg.length, esc (t.reg.name).c_str()); first = false;
    }
    std::printf ("],\n\"midi\":["); first = true;
    for (const auto& t : ptf.miditracks())
    {
        std::printf ("%s\n{\"track\":\"%s\",\"index\":%d,\"at\":%llu,\"region\":\"%s\",\"notes\":[", first ? "" : ",", esc (t.name).c_str(), (int) t.index, (unsigned long long) t.reg.startpos, esc (t.reg.name).c_str()); first = false;
        bool f2 = true; for (const auto& n : t.reg.midi) { std::printf ("%s[%llu,%llu,%d,%d]", f2 ? "" : ",", (unsigned long long) n.pos, (unsigned long long) n.length, (int) n.note, (int) n.velocity); f2 = false; }
        std::printf ("]}");
    }
    std::printf ("]}\n"); return 0;
}

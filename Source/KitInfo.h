#pragma once
// Squelchmoth kit metadata (names, sounds, notes, mix gains) - shared by the Beat Lab, the rack and the Hive Pads face.
#include <JuceHeader.h>

namespace DrumKit
{
    struct Sound { const char* name; int note; };

    // Twelve slots, GM drum notes, same order in every kit (so patterns are interchangeable).
    static const Sound sounds[] = {
        { "Kick",       36 }, { "Snare",      38 }, { "Rim",        37 }, { "Clap",       39 },
        { "Closed Hat", 42 }, { "Open Hat",   46 }, { "Low Tom",    45 }, { "High Tom",   50 },
        { "Ride",       51 }, { "Crash",      49 }, { "Perc",       56 }, { "Sub 808",    35 } };
    static constexpr int numSounds = 12;
    // Mix balance inside a kit (dB) so a flat-velocity pattern already sounds balanced.
    static constexpr float gainsDb[] = { 0.0f, -2.0f, -6.0f, -4.0f, -9.0f, -9.0f, -3.0f, -3.5f, -10.0f, -8.0f, -8.0f, -1.0f };

    struct Kit { const char* name; const char* description; };
    static const Kit kits[] = {
        { "Mothwing 808",    "The classic drum machine: deep booming kick, snappy snare, metallic hats, cowbell" },
        { "Hive 909",        "Club and techno: punchy kick, cracking snare, sizzling hats, huge clap" },
        { "Chrome Trap",     "Modern hip-hop: distorted knock kick, tight rims, crisp hats, gliding 808 sub" },
        { "Velvet Lo-Fi",    "Dusty and tape-warm with vinyl crackle for chill and boom-bap" },
        { "Dust Acoustic",   "A real-kit impression: resonant kick body, buzzy snare wires, washy cymbals" },
        { "Neon Trap",       "2020s trap: hard clipped kick, bright snare and clap, needle hats, long G 808" },
        { "Drill Smoke",     "UK / NY drill: tight kick, cracking high snare, sharp hats, long sliding 808" },
        { "Afro Sun",        "Afrobeats: warm kick, tight rim, shaker hats, conga percussion, soft sub" },
        { "Amapiano Log",    "Amapiano: deep kick, wide clap, shakers and the bouncing log drum" },
        { "Dembow Heat",     "Reggaeton / dembow: punchy kick, cracking snare and rim, timbale perc" },
        { "Jersey Bounce",   "Jersey club: punchy kick, big clap, crisp hats and the bed-squeak" },
        { "Phonk Cowbell",   "Phonk / Memphis: distorted kick and 808, the pitched cowbell, lo-fi hats" },
        { "Boom Bap Crates", "Boom bap: dusty knock kick, fat snare, swung hats, crate-dug grit" },
        { "House Silk",      "House: round kick, airy clap, open hats, shaker" },
        { "Techno Steel",    "Techno: hard kick, metallic rim, steel hats, clave" },
        { "R&B Velvet",      "R&B: soft round kick, finger snaps, gentle hats, warm 808" },
        { "Rock Room",       "Rock: big room kit - punchy kick, ringing snare, deep toms, wide cymbals" },
        { "Memphis Tape",    "Memphis rap: tape-saturated kick and 808, dark snare, hissing lo-fi hats, pitched cowbell" },
        { "Plugg Pillow",    "Plugg: soft round kick, airy snare, tiny rim, gentle hats, warm short 808" },
        { "Rage Red",        "Rage: clipped kick, piercing snare and clap, bright hats, distorted long 808" },
        { "Hyperpop Pink",   "Hyperpop: hard bright kick, pitched-up snare and clap, needle hats, crushed sub" },
        { "Garage Swing",    "UK garage: tight punchy kick, snappy snare and rim, shuffled crisp hats, skippy perc" },
        { "Grime Concrete",  "Grime: square-wave kick, sharp cold snare, metallic hats, gunshot clap, deep sub" },
        { "Baile Funk",      "Baile funk: punchy kick, tamborzao toms and perc, sharp claps, short snare" },
        { "Bossa Brush",     "Bossa nova: soft kick, brushed snare, rim clicks, light hats, shaker, warm ride" },
        { "Gospel Live",     "Gospel: big live kit - deep kick, cracking snare, tom fills, wide crash and ride, room" },
        { "Neo-Soul Dust",   "Neo-soul: soft laid-back kick, dusty snare and rim, mellow hats, shaker, warm sub" } };
    static constexpr int numKits = 27;

    inline int kitIndexFor (const juce::String& name)
    {
        for (int i = 0; i < numKits; ++i) if (juce::String (kits[i].name).equalsIgnoreCase (name.trim())) return i;
        for (int i = 0; i < numKits; ++i) if (juce::String (kits[i].name).containsIgnoreCase (name.trim()) && name.trim().isNotEmpty()) return i;
        struct Alias { const char* word; int kit; };
        static const Alias aliases[] = { { "memphis", 17 }, { "plugg", 18 }, { "rage", 19 }, { "hyperpop", 20 }, { "garage", 21 }, { "grime", 22 }, { "baile", 23 }, { "funk", 23 },
                                          { "bossa", 24 }, { "brush", 24 }, { "gospel", 25 }, { "neo", 26 }, { "drill", 6 }, { "afro", 7 }, { "amapiano", 8 }, { "log", 8 }, { "reggaeton", 9 }, { "dembow", 9 }, { "jersey", 10 },
                                          { "club", 10 }, { "phonk", 11 }, { "boom", 12 }, { "crate", 12 }, { "house", 13 }, { "techno", 14 },
                                          { "r&b", 15 }, { "rnb", 15 }, { "soul", 15 }, { "rock", 16 }, { "room", 16 }, { "808", 0 }, { "909", 1 },
                                          { "trap", 2 }, { "hip", 2 }, { "lo", 3 }, { "chill", 3 }, { "acoustic", 4 }, { "real", 4 } };
        for (auto& a : aliases) if (name.containsIgnoreCase (a.word)) return a.kit;
        return 0;
    }

}

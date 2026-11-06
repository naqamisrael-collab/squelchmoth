#pragma once
// Squelchmoth - the EQ frequency reference chart (Options > EQ Frequency Chart).
//
// A detailed, interactive map of where 40 instruments and voices live, grouped by family:
//  * a precise log scale 20 Hz - 20 kHz with every ISO 1/3-octave centre (the bands of a graphic EQ) drawn and
//    labelled, octave and decade lines stronger, and the seven named regions (sub ... air) with their Hz ranges;
//  * a piano keyboard along the scale (A0 27.5 Hz - C8 4186 Hz) so notes and frequencies line up;
//  * per instrument: its note range (fundamentals) and how far its harmonics / overtones reach, the usual
//    high-pass / low-pass point, and colour-coded bands - fundamentals, sweet spots to boost, problem zones to
//    cut, air / presence - each with the engineering note;
//  * a crosshair readout anywhere on the chart: frequency, nearest note and cents, 1/3-octave band, region,
//    wavelength, period, and every instrument band that lives at that frequency;
//  * Find (instrument, problem word, frequency like 300 or 4k, or a note like A4), a family filter, pins,
//    and Save as PNG (the whole chart, every row, at 2x resolution).
// The data lives in EQRef so Claude can look things up with the eq_reference tool.

#include "Theme.h"

namespace EQRef
{
    enum Kind { fundamentals = 0, sweetSpot = 1, problem = 2, airPresence = 3 };

    struct Band { float lo, hi; const char* label; const char* note; int kind; };
    struct Row
    {
        int family;
        const char* name;
        float fLo, fHi;     // note range: lowest / highest fundamental (0 = unpitched)
        float eLo, eHi;     // where its energy really reaches, harmonics / overtones / noise included
        float hpf, lpf;     // typical high-pass / low-pass in a mix (0 = usually none)
        std::vector<Band> bands;
        const char* tip;
    };

    inline const juce::StringArray& families()
    {
        static const juce::StringArray f { "Drums & percussion", "Bass", "Guitars", "Keys & synths", "Strings & orchestral",
                                           "Brass & woodwinds", "Vocals", "Mix & effects" };
        return f;
    }

    inline const char* kindName (int k)
    {
        switch (k) { case fundamentals: return "fundamentals / body"; case sweetSpot: return "sweet spot - boost"; case problem: return "problem zone - cut"; default: return "air / presence"; }
    }

    inline const std::vector<Row>& rows()
    {
        static const std::vector<Row> r {
            // ---------------------------------------------------------------- Drums & percussion
            { 0, "Kick drum", 40, 100, 30, 10000, 30, 0,
              { { 40, 60, "sub", "Sub weight - felt more than heard. High-pass below 30-40 Hz to clean rumble", 0 },
                { 60, 110, "thump", "Boost 60-100 Hz for weight and thump (choose one spot - not the same one as the 808)", 1 },
                { 200, 450, "boxy", "Cut 250-400 Hz to remove the cardboard / boxy tone", 2 },
                { 2500, 5000, "click", "Boost 3-5 kHz for beater click so the kick cuts through on small speakers", 3 } },
              "Weight at 60-100, box at 300, click at 4k." },
            { 0, "808 / sub bass", 30.9f, 65.4f, 25, 3000, 25, 0,
              { { 30, 60, "sub", "The 808's fundamental (often 40-55 Hz). Keep it mono and clear everything else out of here", 0 },
                { 60, 120, "body", "Harmonics that let the 808 be heard on phones; saturation helps more than EQ", 1 },
                { 150, 300, "mud", "Cut 180-250 Hz if the 808 fights the kick or the vocal's low end", 2 },
                { 1000, 3000, "grit", "Drive / distortion here makes the 808 audible on laptops and earbuds", 3 } },
              "Pick who owns 50 Hz: kick or 808, never both." },
            { 0, "Snare drum", 120, 250, 100, 12000, 80, 0,
              { { 150, 250, "body", "Fundamental body and knock; boost 180-220 Hz for fatness", 0 },
                { 400, 700, "ring", "Cut narrow around 400-600 Hz for ring / honk", 2 },
                { 2000, 5000, "crack", "Boost 2.5-4 kHz for crack and stick attack", 1 },
                { 7000, 12000, "snap", "The wires sizzle 8-12 kHz; a high shelf lifts the snap", 3 } },
              "Body at 200, crack at 3k, sizzle at 10k." },
            { 0, "Toms", 60, 300, 60, 8000, 50, 0,
              { { 70, 180, "fundamentals", "Floor tom 60-100 Hz, rack toms 120-300 Hz: boost the note for punch", 0 },
                { 300, 600, "boxy", "Cut 350-500 Hz to clear boxiness and cymbal leakage", 2 },
                { 3000, 6000, "attack", "Stick attack lives at 3-6 kHz", 1 } },
              "Punch on the note, cut the box, attack at 4k." },
            { 0, "Hi-hats", 0, 0, 300, 18000, 300, 0,
              { { 200, 500, "clank", "Nothing useful lives here - high-pass at 300-500 Hz", 2 },
                { 3000, 6000, "harsh", "Hat harshness and ear fatigue 3-6 kHz - cut gently", 2 },
                { 6000, 10000, "sizzle", "Sizzle and definition 6-10 kHz", 1 },
                { 10000, 18000, "air", "Air and shimmer above 10 kHz - a wide high shelf", 3 } },
              "High-pass hard, tame 4k, open the top." },
            { 0, "Cymbals / ride / crash", 0, 0, 250, 18000, 250, 0,
              { { 250, 600, "clang", "Low clang / gong - high-pass 250-400 Hz", 2 },
                { 1000, 2500, "bell", "Ride bell ping and stick definition 1-2.5 kHz", 1 },
                { 3000, 6000, "harsh", "Crash harshness 3-6 kHz - a gentle wide dip", 2 },
                { 8000, 16000, "shimmer", "Shimmer and wash above 8 kHz", 3 } },
              "Clean the low clang, soften 4k, keep the shimmer." },
            { 0, "Clap / snap", 0, 0, 400, 12000, 200, 0,
              { { 300, 700, "boxy", "Boxy, papery low mids - cut 400-600 Hz", 2 },
                { 800, 1500, "body", "The body of the clap 800 Hz - 1.5 kHz", 0 },
                { 1500, 4000, "crack", "Crack and snap 2-4 kHz", 1 },
                { 6000, 12000, "sparkle", "Sparkle and room 6-12 kHz", 3 } },
              "High-pass at 200, the clap lives at 1-4k." },
            { 0, "Percussion / shakers", 150, 700, 100, 16000, 100, 0,
              { { 150, 400, "tone", "Conga / djembe tone 150-400 Hz (bongos up to 700 Hz)", 0 },
                { 400, 800, "boxy", "Boxy, hollow low mids - cut gently", 2 },
                { 2000, 5000, "slap", "Hand slap and attack 2-5 kHz", 1 },
                { 6000, 15000, "shaker", "Shakers and tambourine live 6-15 kHz - high-pass them at 300-500 Hz", 3 } },
              "Hand drums: tone low, slap at 3k. Shakers: all top." },
            { 0, "Overheads / room", 0, 0, 40, 18000, 100, 0,
              { { 40, 120, "spill", "Kick spill and rumble - high-pass 100-200 Hz unless the overheads ARE the kit sound", 2 },
                { 120, 250, "kit body", "Snare and tom body - the weight of the whole kit", 0 },
                { 300, 600, "boxy", "Boxy room tone - cut 300-500 Hz", 2 },
                { 2500, 5000, "harsh", "Harsh cymbal wash - dip 2.5-5 kHz", 2 },
                { 8000, 16000, "air", "Air and cymbal shimmer", 3 } },
              "High-pass, de-box, keep them airy." },
            // ---------------------------------------------------------------- Bass
            { 1, "Bass guitar", 41.2f, 392, 31, 5000, 30, 0,
              { { 40, 100, "fundamentals", "E string 41 Hz (5-string low B 31 Hz); boost 60-90 Hz for weight, high-pass below 35", 0 },
                { 100, 250, "warmth", "Warmth and thickness; cut 150-250 Hz if it clouds the kick", 1 },
                { 250, 500, "mud", "Mud / woolliness - cut 250-400 Hz for clarity", 2 },
                { 700, 1200, "growl", "Boost 700 Hz - 1 kHz for growl and note definition on small speakers", 1 },
                { 2000, 4000, "pick", "String / pick attack and finger noise 2-4 kHz", 3 } },
              "Weight at 80, mud at 300, growl at 900." },
            { 1, "Synth bass", 30, 250, 25, 8000, 25, 0,
              { { 30, 60, "sub", "Sub fundamental - keep mono, high-pass at 25-30 Hz", 0 },
                { 60, 150, "weight", "Weight and punch 60-120 Hz", 1 },
                { 200, 400, "mud", "Mud that hides the kick and vocal - cut 200-350 Hz", 2 },
                { 700, 2000, "harmonics", "Harmonics and growl - what makes the line readable on phones", 1 },
                { 3000, 6000, "buzz", "Saw buzz / fizz - low-pass or dip if it clashes with hats and vocal", 2 } },
              "Sub mono, harmonics for small speakers, low-pass the buzz." },
            { 1, "Upright / double bass", 41.2f, 350, 40, 5000, 35, 0,
              { { 41, 100, "fundamentals", "E string at 41 Hz; the notes and the weight", 0 },
                { 100, 200, "wood", "Wood and warmth of the body", 1 },
                { 200, 400, "boom", "Boom / mud from the body resonance - cut 200-300 Hz", 2 },
                { 700, 1500, "definition", "Finger attack and note definition 700 Hz - 1.5 kHz", 1 },
                { 2000, 5000, "string", "Finger, bow and string noise", 3 } },
              "Warm low, define at 1k." },
            // ---------------------------------------------------------------- Guitars
            { 2, "Electric guitar (clean)", 82.4f, 1319, 80, 10000, 80, 0,
              { { 80, 120, "low end", "High-pass 80-120 Hz to leave room for bass and kick", 2 },
                { 150, 350, "warmth", "Warmth and body", 0 },
                { 400, 700, "boxy", "Boxy, cardboard tone - cut 400-600 Hz", 2 },
                { 2000, 4000, "presence", "Clarity, chime and pick attack 2-4 kHz", 1 },
                { 5000, 10000, "sparkle", "Sparkle of clean and chorus guitars 5-10 kHz", 3 } },
              "High-pass, de-box, add chime." },
            { 2, "Electric guitar (distorted)", 82.4f, 1319, 80, 7000, 100, 9000,
              { { 80, 150, "chug", "Palm-mute chug - high-pass 90-120 Hz to leave room for bass and kick", 2 },
                { 150, 400, "body", "Body and heaviness; too much is mud", 0 },
                { 400, 800, "boxy", "Boxiness 400-600 Hz - cut, especially on doubled rhythm parts", 2 },
                { 2000, 4000, "bite", "Bite and presence 2-4 kHz - boost to cut through, cut if harsh", 1 },
                { 5000, 9000, "fizz", "Amp fizz 5-8 kHz - low-pass around 8-10 kHz", 2 } },
              "High-pass at 100, bite at 3k, low-pass the fizz." },
            { 2, "Acoustic guitar", 82.4f, 1047, 80, 15000, 80, 0,
              { { 80, 150, "boom", "Body resonance / boom - high-pass 80-100 Hz, cut 100-150 Hz if boomy", 2 },
                { 150, 300, "warmth", "Warmth and fullness", 0 },
                { 300, 500, "mud", "Cut 250-400 Hz when it sounds muddy or dull", 2 },
                { 2000, 5000, "clarity", "Clarity and pick attack 2.5-5 kHz", 1 },
                { 8000, 15000, "sparkle", "Sparkle and string detail above 8 kHz", 3 } },
              "Cut the boom, add sparkle." },
            // ---------------------------------------------------------------- Keys & synths
            { 3, "Piano", 27.5f, 4186, 27, 15000, 40, 0,
              { { 30, 120, "low end", "Lowest notes (A0 27.5 Hz); high-pass 40-60 Hz unless it's a solo piano", 0 },
                { 120, 300, "warmth", "Warmth; cut 150-250 Hz in a dense mix", 1 },
                { 300, 600, "muddy", "Boxy / muddy zone - cut gently", 2 },
                { 2000, 5000, "presence", "Presence and hammer attack 2.5-5 kHz", 1 },
                { 8000, 14000, "air", "Air and brilliance", 3 } },
              "Balance warmth against presence." },
            { 3, "Electric piano / Rhodes", 41.2f, 2637, 40, 8000, 60, 0,
              { { 100, 250, "warmth", "Warmth and body of the tines", 0 },
                { 250, 500, "mud", "Muddy low mids - cut 250-400 Hz", 2 },
                { 800, 1500, "bark", "Bark / growl when played hard 800 Hz - 1.5 kHz", 1 },
                { 2000, 5000, "bell", "Bell-like tine attack 2-4 kHz", 3 } },
              "Warm, de-mud, let the bark through." },
            { 3, "Organ", 32.7f, 2093, 32, 8000, 50, 0,
              { { 40, 120, "low end", "16' drawbar and pedals - high-pass if a bass is playing", 0 },
                { 200, 500, "mud", "Mud builds fast with full drawbars - cut 250-400 Hz", 2 },
                { 500, 1500, "growl", "Growl and Leslie drive 500 Hz - 1.5 kHz", 1 },
                { 2000, 6000, "click", "Key click and percussion 2-6 kHz", 3 } },
              "Cut the mud, keep the growl." },
            { 3, "Synth pads", 100, 2000, 60, 16000, 150, 0,
              { { 60, 200, "low end", "High-pass pads at 100-200 Hz so bass and kick stay clear", 2 },
                { 200, 500, "body", "Body; cut 300-400 Hz if the pad clouds the mix", 0 },
                { 1000, 3000, "vocal zone", "Carve a wide dip where the vocal sits (1-3 kHz)", 2 },
                { 6000, 14000, "shimmer", "Shimmer and air; a high shelf opens pads up", 3 } },
              "High-pass, dip the vocal zone, add shimmer." },
            { 3, "Synth lead", 200, 2000, 150, 16000, 150, 0,
              { { 200, 500, "body", "Body and thickness", 0 },
                { 500, 1000, "honk", "Honk / nasal 500 Hz - 1 kHz", 2 },
                { 1500, 4000, "cut", "Where a lead cuts through - the vocal lives here too: decide who owns it", 1 },
                { 5000, 8000, "fizz", "Saw fizz / harshness 5-8 kHz", 2 },
                { 8000, 16000, "air", "Air and brightness", 3 } },
              "Own 2-4k when the vocal isn't singing." },
            { 3, "Plucks / arps / bells", 260, 2093, 200, 16000, 200, 0,
              { { 200, 500, "body", "Body of the note", 0 },
                { 500, 900, "boxy", "Boxy / hollow - cut gently", 2 },
                { 2000, 5000, "attack", "Transient and pluck attack 2-5 kHz", 1 },
                { 8000, 16000, "shimmer", "Shimmer and sparkle", 3 } },
              "High-pass high, keep the attack." },
            { 3, "Mallets (marimba, vibes, glock)", 65.4f, 4186, 60, 16000, 80, 0,
              { { 100, 400, "body", "Marimba and vibes body (marimba from C2 65 Hz, glockenspiel up to C8 4.2 kHz)", 0 },
                { 400, 800, "boxy", "Boxy resonator tone - cut gently", 2 },
                { 2000, 5000, "mallet", "Mallet attack 2-5 kHz", 1 },
                { 6000, 14000, "shimmer", "Glock and vibe shimmer", 3 } },
              "Body low, attack at 3k, shimmer on top." },
            // ---------------------------------------------------------------- Strings & orchestral
            { 4, "Violin", 196, 3520, 190, 16000, 150, 0,
              { { 200, 450, "body", "G-string body and warmth 200-450 Hz", 0 },
                { 900, 1500, "nasal", "Nasal tone - a small cut around 1 kHz", 2 },
                { 2000, 5000, "brilliance", "Brilliance and bow articulation 2-5 kHz", 1 },
                { 6000, 10000, "scratch", "Bow scratch / screech 6-10 kHz - gentle cut", 2 },
                { 10000, 16000, "air", "Air and room", 3 } },
              "Warm body, tame the scratch." },
            { 4, "Viola", 131, 1760, 130, 10000, 110, 0,
              { { 150, 350, "warmth", "Warmth and body", 0 },
                { 300, 500, "muddy", "Muddy, cloudy low mids", 2 },
                { 1000, 2500, "nasal", "Nasal / scratchy 1-2.5 kHz - cut gently", 2 },
                { 5000, 10000, "sheen", "Sheen and bow detail", 3 } },
              "Warm low, tame the nasal mids." },
            { 4, "Cello", 65.4f, 1047, 65, 8000, 55, 0,
              { { 65, 200, "fundamentals", "C string at 65 Hz; body and weight 100-250 Hz", 0 },
                { 250, 500, "muddy", "Muddy, boomy in sections - cut 250-400 Hz", 2 },
                { 2000, 5000, "bow", "Bow attack and articulation 2-5 kHz", 1 },
                { 6000, 10000, "rosin", "Rosin and bow noise", 3 } },
              "Weight low, articulation at 3k." },
            { 4, "Harp", 32.7f, 3136, 32, 12000, 40, 0,
              { { 33, 150, "low strings", "Low wound strings - the bass of the harp", 0 },
                { 250, 500, "mud", "Muddy resonance - cut 250-400 Hz", 2 },
                { 2000, 5000, "pluck", "Pluck attack and clarity 2-5 kHz", 1 },
                { 8000, 12000, "shimmer", "Shimmer and glissando sparkle", 3 } },
              "Clean low mids, keep the pluck." },
            { 4, "String section", 41.2f, 3520, 40, 16000, 60, 0,
              { { 150, 300, "warmth", "Cello / viola warmth and body", 0 },
                { 300, 600, "muddy", "Cut 300-500 Hz for muddiness in big sections", 2 },
                { 1000, 2500, "nasal", "Nasal / scratchy 1-2.5 kHz - cut gently", 2 },
                { 6000, 12000, "sheen", "Sheen and bow noise; a gentle shelf", 3 } },
              "Warm low, tame nasal mids, add sheen." },
            // ---------------------------------------------------------------- Brass & woodwinds
            { 5, "Trumpet", 165, 1175, 160, 15000, 150, 0,
              { { 160, 300, "fullness", "Fullness of the low register", 0 },
                { 800, 1500, "honk", "Nasal honk around 1 kHz - cut a little", 2 },
                { 2000, 4000, "bite", "Bite and brilliance 2-4 kHz", 1 },
                { 5000, 10000, "rasp", "Rasp, edge and breath 5-10 kHz", 3 } },
              "Cut the honk, keep the bite." },
            { 5, "Trombone", 82.4f, 698, 80, 10000, 70, 0,
              { { 80, 250, "body", "Body and weight", 0 },
                { 250, 500, "mud", "Muddy low mids", 2 },
                { 500, 900, "honk", "Honk / blat 500-900 Hz", 2 },
                { 1500, 3500, "bite", "Bite and edge 1.5-3.5 kHz", 1 },
                { 5000, 9000, "rasp", "Rasp and breath", 3 } },
              "Weight low, cut the honk, edge at 2.5k." },
            { 5, "French horn", 61.7f, 698, 60, 6000, 60, 0,
              { { 80, 250, "body", "Round, warm body", 0 },
                { 250, 500, "mud", "Muddy / tubby - cut gently", 2 },
                { 1000, 3000, "edge", "Edge and definition 1-3 kHz", 1 },
                { 3500, 6000, "brassy", "Brassiness when played loud", 3 } },
              "Keep it round; add edge only to cut." },
            { 5, "Tuba", 36.7f, 349, 35, 3000, 30, 0,
              { { 37, 120, "fundamentals", "The notes (D1 37 Hz up) - shares space with the bass", 0 },
                { 120, 250, "warmth", "Warmth and roundness", 1 },
                { 250, 500, "mud", "Mud / tubbiness - cut 250-400 Hz", 2 },
                { 800, 1500, "articulation", "Edge and articulation 800 Hz - 1.5 kHz", 3 } },
              "Weight low, articulation at 1k." },
            { 5, "Saxophone", 104, 831, 100, 12000, 80, 0,
              { { 120, 350, "body", "Body - tenor from 104 Hz, alto from 139 Hz", 0 },
                { 500, 1000, "honk", "Honk / boxiness 500 Hz - 1 kHz", 2 },
                { 2000, 5000, "presence", "Presence and edge 2-5 kHz", 1 },
                { 6000, 12000, "breath", "Breath and reed buzz", 3 } },
              "Cut the honk, presence at 3k." },
            { 5, "Flute", 262, 2093, 250, 14000, 200, 0,
              { { 250, 800, "body", "Body of the low register (C4 262 Hz up)", 0 },
                { 2000, 5000, "clarity", "Clarity and articulation 2-5 kHz", 1 },
                { 6000, 12000, "breath", "Breath noise 6-12 kHz - keep a little, de-ess if hissy", 3 } },
              "High-pass at 200, clarity at 3k." },
            { 5, "Clarinet", 147, 1865, 140, 12000, 120, 0,
              { { 150, 400, "body", "Warm, woody low (chalumeau) register", 0 },
                { 500, 1000, "hollow", "Hollow / honky 500 Hz - 1 kHz", 2 },
                { 2000, 4000, "clarity", "Clarity and bite 2-4 kHz", 1 },
                { 6000, 10000, "breath", "Breath and reed", 3 } },
              "Woody low, clarity at 3k." },
            // ---------------------------------------------------------------- Vocals
            { 6, "Male vocal", 82.4f, 523, 80, 16000, 80, 0,
              { { 80, 180, "fundamentals", "Chest / fundamentals 85-180 Hz (sung notes reach about 520 Hz); high-pass at 70-90 Hz", 0 },
                { 200, 400, "boom", "Cut 200-350 Hz for boominess and proximity effect", 2 },
                { 400, 800, "nasal", "Nasal, honky 500-800 Hz - cut gently", 2 },
                { 2000, 5000, "presence", "Presence and intelligibility 2.5-5 kHz - boost to sit forward", 1 },
                { 5000, 8000, "sibilance", "Sibilance ('s', 't') 5-8 kHz - de-ess, don't EQ it heavily", 2 },
                { 10000, 16000, "air", "Air and sheen above 10 kHz - gentle shelf", 3 } },
              "High-pass 80, cut 300, presence at 3.5k, de-ess at 6k, air at 12k." },
            { 6, "Female vocal", 175, 1047, 160, 18000, 110, 0,
              { { 165, 300, "fundamentals", "Fundamentals 165-300 Hz speaking (sung notes reach about 1 kHz); high-pass at 100-120 Hz", 0 },
                { 300, 500, "muddy", "Muddy / woolly 300-450 Hz - small cut", 2 },
                { 800, 1500, "honk", "Honk / telephone 800 Hz - 1.5 kHz - cut if boxy", 2 },
                { 3000, 6000, "presence", "Presence and clarity 3-6 kHz", 1 },
                { 6000, 10000, "sibilance", "Sibilance 6-10 kHz - de-esser first", 2 },
                { 10000, 18000, "air", "Air and sparkle above 10 kHz", 3 } },
              "High-pass 110, cut 400, presence at 4k, de-ess at 7k, air at 14k." },
            { 6, "Rap vocal", 90, 300, 80, 16000, 90, 0,
              { { 80, 200, "weight", "Weight / chest; high-pass 80-100 Hz, keep some 120-180 Hz for authority", 0 },
                { 250, 450, "mud", "Mud - cut 250-400 Hz so the vocal doesn't fight the 808", 2 },
                { 1000, 2000, "forward", "Boost 1-2 kHz for an in-your-face vocal", 1 },
                { 3000, 6000, "edge", "Edge and clarity 3-6 kHz (aggressive - watch harshness)", 1 },
                { 6000, 9000, "ess", "Sibilance - de-ess around 7 kHz", 2 },
                { 10000, 16000, "air", "Air", 3 } },
              "Forward at 1.5k, edge at 4k, keep it dry and compressed." },
            { 6, "Backing vocals", 100, 1000, 100, 16000, 180, 0,
              { { 100, 300, "low end", "High-pass higher than the lead (150-250 Hz) so they sit behind it", 2 },
                { 1000, 3000, "lead zone", "Dip 1.5-3 kHz slightly so the lead vocal owns the presence", 2 },
                { 5000, 8000, "sibilance", "De-ess harder than the lead - stacked 's' sounds pile up", 2 },
                { 8000, 14000, "air", "A touch of air and wide panning", 3 } },
              "Thinner, darker in the middle, wider than the lead." },
            { 6, "Podcast / voice-over", 85, 255, 80, 14000, 80, 0,
              { { 80, 150, "chest", "Chest tone; high-pass 70-100 Hz, cut 100-150 Hz if the mic is close (proximity boom)", 0 },
                { 200, 400, "mud", "Mud / boom from close mics - cut 200-350 Hz", 2 },
                { 2000, 4000, "clarity", "Clarity and intelligibility 2-4 kHz", 1 },
                { 5000, 8000, "sibilance", "Sibilance - de-ess 5-8 kHz", 2 },
                { 9000, 14000, "air", "A little air; less than music vocals", 3 } },
              "Clean, clear, controlled: high-pass, cut mud, de-ess." },
            // ---------------------------------------------------------------- Mix & effects
            { 7, "Reverb / delay returns", 0, 0, 150, 16000, 250, 9000,
              { { 100, 500, "mud", "Low-end reverb is pure mud - high-pass the return at 200-600 Hz", 2 },
                { 800, 3000, "space", "The audible body of the room / plate - this is where depth is heard", 1 },
                { 5000, 9000, "splash", "Reverb on 's' sounds splashy - de-ess the send or low-pass at 8-10 kHz", 2 },
                { 10000, 16000, "air", "A bright plate adds air; low-pass for a darker, further-away space", 3 } },
              "High-pass and low-pass every return." },
            { 7, "Full mix / master", 0, 0, 20, 20000, 25, 0,
              { { 20, 40, "rumble", "Rumble - high-pass 20-30 Hz on the master", 2 },
                { 40, 120, "low end", "Low-end power; small shelf moves (0.5-1.5 dB) only", 0 },
                { 200, 400, "mud", "Mud builds up here - a wide 1 dB dip opens the mix", 2 },
                { 2000, 5000, "harsh", "Harshness / fatigue - a gentle dip if the mix is edgy", 2 },
                { 8000, 16000, "air", "Air shelf +0.5 to +1.5 dB for sheen", 3 } },
              "Tiny moves, wide Qs, check on three systems." } };
        return r;
    }

    struct Zone { float lo, hi; const char* name; const char* what; };
    inline const std::vector<Zone>& zones()
    {
        static const std::vector<Zone> z {
            { 20, 60, "SUB", "Felt more than heard: kick and 808 weight, rumble" },
            { 60, 250, "BASS", "Fundamentals of bass, kick and low toms; warmth and fullness" },
            { 250, 500, "LOW MIDS", "The body of most instruments - where mud and boxiness build up" },
            { 500, 2000, "MIDS", "Tone and character; honk, nasal and telephone sounds" },
            { 2000, 4000, "UPPER MIDS", "Attack, bite and intelligibility - the ear is most sensitive here" },
            { 4000, 6000, "PRESENCE", "Definition and closeness; too much is harsh and tiring" },
            { 6000, 20000, "BRILLIANCE / AIR", "Sibilance 5-9 kHz, cymbals, sparkle, and air above 10 kHz" } };
        return z;
    }

    /** ISO 266 / IEC 61260 third-octave centre frequencies - the bands of a 31-band graphic EQ. */
    inline const std::vector<float>& thirdOctaves()
    {
        static const std::vector<float> t { 20, 25, 31.5f, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                                            1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000 };
        return t;
    }
    inline bool isOctaveCentre (float hz)
    {
        for (float o : { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f }) if (std::abs (hz - o) < 0.1f) return true;
        return false;
    }
    inline bool isDecade (float hz)   { return std::abs (hz - 100.0f) < 0.1f || std::abs (hz - 1000.0f) < 0.1f || std::abs (hz - 10000.0f) < 0.1f; }

    inline juce::String trimNum (double v, int decimals)
    {
        auto s = decimals <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals);
        if (s.containsChar ('.')) { s = s.trimCharactersAtEnd ("0"); s = s.trimCharactersAtEnd ("."); }
        return s;
    }
    /** 41.2 Hz, 315 Hz, 3.15 kHz, 12.5 kHz */
    inline juce::String hzText (float hz, bool unit = true)
    {
        if (hz >= 1000.0f) return trimNum (hz / 1000.0, hz >= 10000.0f ? 1 : 2) + (unit ? " kHz" : "k");
        return trimNum (hz, hz < 100.0f ? 1 : 0) + (unit ? " Hz" : "");
    }
    /** Short scale label: 31.5, 160, 1k, 1.25k, 12.5k */
    inline juce::String scaleLabel (float hz)   { return hz >= 1000.0f ? trimNum (hz / 1000.0, 2) + "k" : trimNum (hz, 1); }

    inline double midiFor (double hz)   { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
    inline double hzForMidi (double m)  { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
    inline juce::String midiName (int n)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((n % 12) + 12) % 12]) + juce::String ((int) std::floor (n / 12.0) - 1);
    }
    inline juce::String noteName (float hz)   { return midiName ((int) std::lround (midiFor (hz))); }
    inline int centsOff (float hz)            { auto m = midiFor (hz); return (int) std::lround ((m - std::round (m)) * 100.0); }
    inline juce::String noteWithCents (float hz)
    {
        auto c = centsOff (hz);
        return noteName (hz) + (c == 0 ? juce::String() : " " + juce::String (c > 0 ? "+" : "") + juce::String (c) + " cents");
    }
    inline float nearestThird (float hz)
    {
        float best = 20.0f;
        for (auto t : thirdOctaves()) if (std::abs (std::log (t / hz)) < std::abs (std::log (best / hz))) best = t;
        return best;
    }
    inline const Zone& zoneFor (float hz)
    {
        for (auto& z : zones()) if (hz < z.hi) return z;
        return zones().back();
    }
    /** Range with its unit written once and consistently: "200 - 450 Hz", "2.5 - 5 kHz", "450 Hz - 1.2 kHz". */
    inline juce::String rangeText (float lo, float hi)
    {
        if (lo >= 1000.0f && hi >= 1000.0f) return hzText (lo, false).dropLastCharacters (1) + " - " + hzText (hi);
        if (lo < 1000.0f && hi < 1000.0f)   return hzText (lo, false) + " - " + hzText (hi);
        return hzText (lo) + " - " + hzText (hi);
    }
    /** Compact form for labels inside bands: "200-450 Hz", "2.5-5 kHz", "450 Hz-1.2 kHz". */
    inline juce::String rangeShort (float lo, float hi)   { return rangeText (lo, hi).replace (" - ", "-"); }
    /** Readout precision: 41.20 Hz, 312.4 Hz, 3.152 kHz, 12.48 kHz. */
    inline juce::String hzPrecise (float hz)
    {
        if (hz >= 10000.0f) return juce::String (hz / 1000.0, 2) + " kHz";
        if (hz >= 1000.0f)  return juce::String (hz / 1000.0, 3) + " kHz";
        if (hz >= 100.0f)   return juce::String (hz, 1) + " Hz";
        return juce::String (hz, 2) + " Hz";
    }
    inline juce::String wavelengthText (float hz)
    {
        const double m = 343.0 / hz;   // speed of sound at 20 C
        auto metric = m >= 1.0 ? juce::String (m, 2) + " m" : juce::String (m * 100.0, m >= 0.1 ? 1 : 2) + " cm";
        const double ft = m * 3.28084;
        auto imperial = ft >= 1.0 ? juce::String (ft, 2) + " ft" : juce::String (ft * 12.0, 2) + " in";
        return metric + "  (" + imperial + ")";
    }
    inline juce::String periodText (float hz)   { return juce::String (1000.0 / hz, hz < 100.0f ? 2 : 3) + " ms"; }

    /** "300", "300hz", "4k", "4 kHz", "3.15k" -> Hz (0 if it isn't a frequency). */
    inline float parseHz (const juce::String& text)
    {
        auto t = text.toLowerCase().removeCharacters (" ");
        double mult = 1.0;
        if (t.endsWith ("khz")) { mult = 1000.0; t = t.dropLastCharacters (3); }
        else if (t.endsWith ("hz")) t = t.dropLastCharacters (2);
        else if (t.endsWith ("k")) { mult = 1000.0; t = t.dropLastCharacters (1); }
        if (t.isEmpty() || ! t.containsOnly ("0123456789.") || ! t.containsAnyOf ("0123456789")) return 0.0f;
        auto v = t.getDoubleValue() * mult;
        return (v >= 10.0 && v <= 24000.0) ? (float) v : 0.0f;
    }
    /** "A4", "c#3", "Bb1" -> Hz (0 if it isn't a note name). */
    inline float parseNote (const juce::String& text)
    {
        auto t = text.trim();
        if (t.length() < 2 || t.length() > 4) return 0.0f;
        static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
        if (letter < 'A' || letter > 'G') return 0.0f;
        int n = semis[letter - 'A'];
        int i = 1;
        if (t[i] == '#') { ++n; ++i; } else if (t[i] == 'b' && t.length() > 2) { --n; ++i; }
        auto oct = t.substring (i);
        if (oct.isEmpty() || ! oct.containsOnly ("-0123456789")) return 0.0f;
        auto o = oct.getIntValue();
        if (o < 0 || o > 9) return 0.0f;
        return (float) hzForMidi ((o + 1) * 12 + n);
    }
    inline float parseFrequencyOrNote (const juce::String& text)
    {
        auto hz = parseHz (text);
        return hz > 0.0f ? hz : parseNote (text);
    }

    /** Matches at the start of a word, so "male vocal" doesn't also find "Female vocal". */
    inline bool wordMatch (const juce::String& text, const juce::String& term)
    {
        if (term.isEmpty()) return true;
        auto t = text.toLowerCase(), q = term.toLowerCase();
        for (int i = t.indexOf (q); i >= 0; i = t.indexOf (i + 1, q))
            if (i == 0 || ! juce::CharacterFunctions::isLetterOrDigit (t[i - 1])) return true;
        return false;
    }

    inline juce::String describeRow (const Row& r)
    {
        juce::String s;
        s << r.name << "  (" << families()[r.family] << ")\n";
        if (r.fLo > 0.0f) s << "  Notes / fundamentals: " << noteName (r.fLo) << " - " << noteName (r.fHi) << "  (" << rangeText (r.fLo, r.fHi) << ")\n";
        else              s << "  Unpitched (no single fundamental)\n";
        s << "  Energy incl. harmonics / overtones: " << rangeText (r.eLo, r.eHi) << "\n";
        if (r.hpf > 0.0f) s << "  Typical high-pass: around " << hzText (r.hpf) << "\n";
        if (r.lpf > 0.0f) s << "  Typical low-pass: around " << hzText (r.lpf) << "\n";
        for (auto& b : r.bands)
            s << "  - " << juce::String (b.label).toUpperCase() << " " << rangeText (b.lo, b.hi) << " [" << kindName (b.kind) << "]: " << b.note << "\n";
        s << "  In short: " << r.tip << "\n";
        return s;
    }

    inline juce::String describeFrequency (float hz)
    {
        auto& z = zoneFor (hz);
        juce::String s;
        s << hzPrecise (hz) << ": nearest note " << noteWithCents (hz) << ", 1/3-octave band " << hzText (nearestThird (hz))
          << ", region " << z.name << " (" << rangeText (z.lo, z.hi) << ") - " << z.what << ".\n"
          << "Wavelength " << wavelengthText (hz) << ", period " << periodText (hz) << ".\n"
          << "What lives here:\n";
        int n = 0;
        for (auto& r : rows())
            for (auto& b : r.bands)
                if (hz >= b.lo && hz <= b.hi) { s << "  - " << r.name << ": " << b.label << " " << rangeText (b.lo, b.hi) << " [" << kindName (b.kind) << "] - " << b.note << "\n"; ++n; }
        if (n == 0) s << "  (no instrument band in the chart - a quiet area)\n";
        return s;
    }

    /** Text lookup used by Claude (eq_reference): a frequency / note, or instruments / problem words. */
    inline juce::String lookup (const juce::String& query)
    {
        auto q = query.trim();
        if (q.isEmpty())
        {
            juce::String s ("Instruments in the EQ chart:\n");
            for (int f = 0; f < families().size(); ++f)
            {
                juce::StringArray names;
                for (auto& r : rows()) if (r.family == f) names.add (r.name);
                s << "  " << families()[f] << ": " << names.joinIntoString (", ") << "\n";
            }
            return s + "Ask for an instrument, a problem word (mud, sibilance, harsh...), a frequency (300, 4k) or a note (A4).";
        }
        if (auto hz = parseFrequencyOrNote (q); hz > 0.0f) return describeFrequency (hz);
        juce::String s;
        for (auto& r : rows())
            if (wordMatch (r.name, q) || wordMatch (families()[r.family], q))
                s << describeRow (r) << "\n";
        if (s.isNotEmpty()) return s.trimEnd();
        for (auto& r : rows())
            for (auto& b : r.bands)
                if (wordMatch (b.label, q) || wordMatch (b.note, q))
                    s << r.name << ": " << b.label << " " << rangeText (b.lo, b.hi) << " [" << kindName (b.kind) << "] - " << b.note << "\n";
        return s.isNotEmpty() ? s.trimEnd() : "Nothing in the EQ chart matches '" + q + "'. Try an instrument (snare, male vocal), a problem (mud, boxy, sibilance), a frequency (300, 4k) or a note (A4).";
    }
}

//==============================================================================
class EQChartContent : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::ScrollBar::Listener
{
public:
    using Band = EQRef::Band;
    using Row  = EQRef::Row;

    EQChartContent()
    {
        setSize (1400, 860);
        setWantsKeyboardFocus (true);

        search.setTextToShowWhenEmpty ("Find: instrument, problem, frequency or note  (snare, sibilance, 300, 4k, A4)", Theme::dim());
        search.setColour (juce::TextEditor::backgroundColourId, Theme::bg());
        search.setColour (juce::TextEditor::outlineColourId, Theme::line());
        search.setColour (juce::TextEditor::textColourId, Theme::text());
        search.setFont (Theme::font (12.0f));
        search.onTextChange = [this] { applyFilter(); };
        search.onEscapeKey  = [this] { search.clear(); applyFilter(); };
        addAndMakeVisible (search);

        family.addItem ("All instruments", 1);
        for (int i = 0; i < EQRef::families().size(); ++i) family.addItem (EQRef::families()[i], i + 2);
        family.setSelectedId (1, juce::dontSendNotification);
        family.onChange = [this] { applyFilter(); };
        family.setTooltip ("Show one family of instruments");
        addAndMakeVisible (family);

        for (auto* b : toggles())
        {
            b->setToggleState (true, juce::dontSendNotification);
            b->onClick = [this] { repaint(); };
            addAndMakeVisible (*b);
        }
        showRanges.setTooltip ("Note range (solid) and how far the harmonics / overtones reach (thin line); red shading = the usual high-pass / low-pass");

        exportBtn.onClick = [this] { exportPng(); };
        exportBtn.setTooltip ("Save the whole chart - every row - as a high-resolution PNG");
        addAndMakeVisible (exportBtn);
        clearBtn.onClick = [this] { pinnedRow = -1; pinnedHz = 0.0f; search.clear(); family.setSelectedId (1); applyFilter(); };
        clearBtn.setTooltip ("Clear the search, filter and pins (Esc)");
        addAndMakeVisible (clearBtn);

        vbar.setAutoHide (false);
        vbar.addListener (this);
        addAndMakeVisible (vbar);

        liveInstance() = this;
        if (pendingQuery().isNotEmpty()) { search.setText (pendingQuery(), juce::dontSendNotification); pendingQuery().clear(); }
        applyFilter();
    }

    ~EQChartContent() override
    {
        vbar.removeListener (this);
        if (liveInstance() == this) liveInstance() = nullptr;
    }

    /** Claude / menus: put a query in Find (opens on the next window creation if the chart isn't open). */
    static void requestQuery (const juce::String& q)
    {
        if (auto* c = liveInstance()) { c->search.setText (q, juce::dontSendNotification); c->applyFilter(); }
        else pendingQuery() = q;
    }

    //==========================================================================
    void resized() override
    {
        auto w = getWidth();
        exportBtn.setBounds (w - 140, 12, 128, 24);
        clearBtn.setBounds (w - 140 - 76, 12, 70, 24);
        int x = 16, y = 52;
        search.setBounds (x, y, juce::jlimit (240, 420, w / 4), 26); x = search.getRight() + 8;
        family.setBounds (x, y, 190, 26); x = family.getRight() + 18;
        for (auto* b : toggles())
        {
            auto tw = (int) juce::GlyphArrangement::getStringWidth (Theme::font (12.0f), b->getButtonText()) + 34;
            if (x + 16 + tw > w - 12) { x = 16 + 190 + 8 + search.getWidth() + 18; y += 26; }   // narrow window: wrap
            b->setBounds (x + 16, y + 1, tw, 24);
            x += 16 + tw + 10;
        }
        headerBottom = (float) (y + 26 + 10);
        updateScroll();
    }

    void paint (juce::Graphics& g) override   { paintChart (g, getWidth(), getHeight(), scrollY, false); }

    //==========================================================================
    void mouseMove (const juce::MouseEvent& e) override   { updateHover (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override   { updateHover (e.position); }
    void mouseExit (const juce::MouseEvent&) override
    {
        hoverHz = 0.0f; hoverItem = -1; hoverBand = nullptr; setTooltip ({});
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto L = layoutFor (getWidth(), getHeight(), false);
        if (e.position.y < L.body.getY()) return;
        setScroll (scrollY - w.deltaY * 220.0f);
        updateHover (e.position);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.mods.isPopupMenu()) { pinnedRow = -1; pinnedHz = 0.0f; repaint(); return; }
        auto L = layoutFor (getWidth(), getHeight(), false);
        auto p = e.position;
        if (p.y >= L.zones.getY() && p.y < L.body.getY() && p.x >= L.chartL && p.x <= L.chartR)
        {
            auto hz = hzForX (p.x, L);
            pinnedHz = (pinnedHz > 0.0f && std::abs (std::log2 (hz / pinnedHz)) < 1.0f / 24.0f) ? 0.0f : hz;   // click again to unpin
            repaint();
            return;
        }
        if (L.body.contains (p))
        {
            auto i = itemAt (p.y, L);
            if (i >= 0 && items[(size_t) i].row >= 0)
            {
                auto r = items[(size_t) i].row;
                pinnedRow = pinnedRow == r ? -1 : r;
                repaint();
            }
        }
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { pinnedRow = -1; pinnedHz = 0.0f; repaint(); return true; }
        if (k == juce::KeyPress::pageDownKey || k == juce::KeyPress::downKey) { setScroll (scrollY + (k == juce::KeyPress::downKey ? rowH : bodyHeight() * 0.9f)); return true; }
        if (k == juce::KeyPress::pageUpKey || k == juce::KeyPress::upKey)     { setScroll (scrollY - (k == juce::KeyPress::upKey ? rowH : bodyHeight() * 0.9f)); return true; }
        if (k == juce::KeyPress::homeKey) { setScroll (0); return true; }
        if (k == juce::KeyPress::endKey)  { setScroll (contentH); return true; }
        return false;
    }

    //==========================================================================
    void exportPng()
    {
        auto fc = std::make_shared<juce::FileChooser> ("Save the EQ chart as PNG", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("Squelchmoth EQ Chart.png"), "*.png");
        fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c)
        {
            auto f = c.getResult();
            if (f == juce::File()) return;
            writePng (f.withFileExtension ("png"));
        });
    }

    /** The whole chart (no side panel, every row, no scrolling) at 2x. */
    bool writePng (const juce::File& f)
    {
        const int w = 1800;
        auto L = layoutFor (w, 1000, true);
        const int h = (int) (L.body.getY() + contentH + 34.0f);
        juce::Image img (juce::Image::ARGB, w * 2, h * 2, true);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (2.0f));
            paintChart (g, w, h, 0.0f, true);
        }
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat png;
        return out.openedOk() && png.writeImageToStream (img, out);
    }

private:
    //==========================================================================
    struct Item { int row; int family; float y, h; };   // row -1 = family header
    struct Layout
    {
        float chartL, chartR, nameX;
        juce::Rectangle<float> zones, axis, piano, body, bottomAxis, side;
    };

    static constexpr float sideW = 320.0f, nameW = 196.0f, rowH = 36.0f, headerH = 22.0f;

    juce::TextEditor search;
    juce::ComboBox family;
    juce::ToggleButton showFund { "Fundamentals" }, showBoost { "Sweet spots (boost)" }, showCut { "Problem zones (cut)" },
                       showAir { "Air / presence" }, showRanges { "Note ranges & HPF" };
    juce::TextButton exportBtn { "Save as PNG..." }, clearBtn { "Clear" };
    juce::ScrollBar vbar { true };

    std::vector<Item> items;
    float contentH = 0.0f, scrollY = 0.0f, headerBottom = 112.0f;
    juce::String bandTerm;            // a search that matched band labels / notes rather than names
    float searchHz = 0.0f;            // a search that is a frequency or note
    float pinnedHz = 0.0f, hoverHz = 0.0f;
    int pinnedRow = -1, hoverItem = -1;
    const Band* hoverBand = nullptr;

    static EQChartContent*& liveInstance()   { static EQChartContent* p = nullptr; return p; }
    static juce::String& pendingQuery()      { static juce::String q; return q; }

    std::array<juce::ToggleButton*, 5> toggles()   { return { &showFund, &showBoost, &showCut, &showAir, &showRanges }; }

    bool kindVisible (int k) const
    {
        switch (k) { case 0: return showFund.getToggleState(); case 1: return showBoost.getToggleState(); case 2: return showCut.getToggleState(); default: return showAir.getToggleState(); }
    }

    static juce::Colour colourFor (int kind)
    {
        switch (kind) { case 0: return Theme::violet().brighter (0.3f); case 1: return Theme::mint(); case 2: return Theme::amber(); default: return Theme::acid(); }
    }
    static juce::Colour familyColour (int f)   { return Theme::nextTrackColour (f * 2 + 1); }

    float markerHz() const   { return searchHz > 0.0f ? searchHz : pinnedHz; }

    //==========================================================================
    Layout layoutFor (int w, int h, bool exporting) const
    {
        Layout L;
        const float top = exporting ? 104.0f : headerBottom;
        const float sw = exporting ? 0.0f : sideW;
        L.nameX = 12.0f;
        L.chartL = nameW + 14.0f;
        L.chartR = (float) w - (exporting ? 30.0f : sw + 44.0f);
        const float cw = L.chartR - L.chartL;
        float y = top;
        L.zones = { L.chartL, y, cw, 34.0f };  y += 36.0f;
        L.axis  = { L.chartL, y, cw, 28.0f };  y += 28.0f;
        L.piano = { L.chartL, y, cw, 26.0f };  y += 38.0f;
        L.bottomAxis = { L.chartL, (float) h - 28.0f, cw, 26.0f };
        L.body = { 8.0f, y, L.chartR - 8.0f + 6.0f, juce::jmax (40.0f, L.bottomAxis.getY() - y - 2.0f) };
        L.side = { (float) w - sw - 10.0f, top, sw, (float) h - top - 10.0f };
        return L;
    }

    float bodyHeight() const   { return layoutFor (getWidth(), getHeight(), false).body.getHeight(); }

    static float xForHz (float hz, const Layout& L)   { return L.chartL + (L.chartR - L.chartL) * (float) (std::log (hz / 20.0) / std::log (1000.0)); }
    static float hzForX (float x, const Layout& L)    { return juce::jlimit (20.0f, 20000.0f, (float) (20.0 * std::pow (1000.0, (x - L.chartL) / (L.chartR - L.chartL)))); }

    int itemAt (float y, const Layout& L) const
    {
        auto cy = y - L.body.getY() + scrollY;
        for (int i = 0; i < (int) items.size(); ++i)
            if (cy >= items[(size_t) i].y && cy < items[(size_t) i].y + items[(size_t) i].h) return i;
        return -1;
    }

    bool rowMatches (const Row& r, const juce::String& term, bool& viaBand) const
    {
        viaBand = false;
        if (term.isEmpty()) return true;
        if (EQRef::wordMatch (r.name, term) || EQRef::wordMatch (EQRef::families()[r.family], term)) return true;
        for (auto& b : r.bands)
            if (EQRef::wordMatch (b.label, term) || EQRef::wordMatch (b.note, term)) { viaBand = true; return true; }
        return false;
    }
    bool bandMatchesTerm (const Band& b) const
    {
        return bandTerm.isNotEmpty() && (EQRef::wordMatch (b.label, bandTerm) || EQRef::wordMatch (b.note, bandTerm));
    }

    void applyFilter()
    {
        auto q = search.getText().trim();
        searchHz = EQRef::parseFrequencyOrNote (q);
        auto term = searchHz > 0.0f ? juce::String() : q;
        const int fam = family.getSelectedId() - 2;
        items.clear();
        bandTerm.clear();
        bool anyNameMatch = false;
        for (auto& r : EQRef::rows()) { bool vb = false; if (rowMatches (r, term, vb) && ! vb && term.isNotEmpty()) anyNameMatch = true; }
        if (term.isNotEmpty() && ! anyNameMatch) bandTerm = term;

        float y = 0.0f;
        int lastFamily = -1;
        auto& rs = EQRef::rows();
        for (int i = 0; i < (int) rs.size(); ++i)
        {
            auto& r = rs[(size_t) i];
            if (fam >= 0 && r.family != fam) continue;
            bool vb = false;
            if (! rowMatches (r, term, vb)) continue;
            if (anyNameMatch && vb) continue;   // name search: only rows whose name / family matched
            if (r.family != lastFamily) { items.push_back ({ -1, r.family, y, headerH }); y += headerH; lastFamily = r.family; }
            items.push_back ({ i, r.family, y, rowH });
            y += rowH;
        }
        contentH = y;
        hoverItem = -1; hoverBand = nullptr;
        updateScroll();
        repaint();
    }

    void updateScroll()
    {
        auto bh = bodyHeight();
        scrollY = juce::jlimit (0.0f, juce::jmax (0.0f, contentH - bh), scrollY);
        auto L = layoutFor (getWidth(), getHeight(), false);
        vbar.setBounds ((int) (L.chartR + 14.0f), (int) L.body.getY(), 12, (int) L.body.getHeight());
        vbar.setRangeLimits (0.0, juce::jmax ((double) contentH, (double) bh));
        vbar.setCurrentRange (scrollY, bh, juce::dontSendNotification);
    }
    void setScroll (float y)
    {
        scrollY = juce::jlimit (0.0f, juce::jmax (0.0f, contentH - bodyHeight()), y);
        vbar.setCurrentRange (scrollY, bodyHeight(), juce::dontSendNotification);
        repaint();
    }
    void scrollBarMoved (juce::ScrollBar*, double newStart) override   { scrollY = (float) newStart; repaint(); }

    void updateHover (juce::Point<float> p)
    {
        auto L = layoutFor (getWidth(), getHeight(), false);
        float hz = 0.0f;
        int item = -1;
        const Band* band = nullptr;
        if (p.x >= L.chartL && p.x <= L.chartR && p.y >= L.zones.getY() && p.y <= L.bottomAxis.getBottom()) hz = hzForX (p.x, L);
        if (L.body.contains (p))
        {
            item = itemAt (p.y, L);
            if (item >= 0 && items[(size_t) item].row >= 0 && hz > 0.0f)
            {
                auto& r = EQRef::rows()[(size_t) items[(size_t) item].row];
                auto top = L.body.getY() + items[(size_t) item].y - scrollY;
                if (p.y >= top + 3.0f && p.y <= top + 3.0f + bandHeight())
                    for (auto& b : r.bands)
                        if (kindVisible (b.kind) && hz >= b.lo && hz <= b.hi) { band = &b; break; }
            }
        }
        if (hz != hoverHz || item != hoverItem || band != hoverBand)
        {
            hoverHz = hz; hoverItem = item; hoverBand = band;
            juce::String tip;
            if (band != nullptr) tip = juce::String (band->label).toUpperCase() + "  " + EQRef::rangeText (band->lo, band->hi) + "\n" + band->note;
            else if (hz > 0.0f && p.y < L.body.getY()) tip = EQRef::hzPrecise (hz) + "  " + EQRef::noteWithCents (hz) + "\nClick to pin this frequency";
            if (tip != getTooltip()) setTooltip (tip);
            repaint();
        }
    }

    static float bandHeight()   { return rowH - 15.0f; }

    //==========================================================================
    void paintChart (juce::Graphics& g, int w, int h, float scroll, bool exporting)
    {
        auto L = layoutFor (w, h, exporting);
        g.fillAll (Theme::panel());
        paintHeader (g, w, L, exporting);
        paintZones (g, L);
        paintAxis (g, L, L.axis, true);
        paintAxisCaption (g, L, L.axis);
        g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
        g.drawText ("REGION", juce::Rectangle<float> (8.0f, L.zones.getY(), L.chartL - 16.0f, L.zones.getHeight()), juce::Justification::centredRight);
        paintPiano (g, L);

        // ---- body (scrolls)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (L.body.toNearestInt());
            paintGrid (g, L, L.body);
            for (int i = 0; i < (int) items.size(); ++i)
            {
                auto& it = items[(size_t) i];
                auto y = L.body.getY() + it.y - scroll;
                if (y + it.h < L.body.getY() || y > L.body.getBottom()) continue;
                if (it.row < 0) paintFamilyHeader (g, L, it, y);
                else            paintRow (g, L, it, y, i == hoverItem && ! exporting, exporting);
            }
            if (items.empty())
            {
                g.setColour (Theme::dim()); g.setFont (Theme::font (13.0f));
                g.drawText ("Nothing matches '" + search.getText() + "'. Try an instrument, a problem word (mud, boxy, sibilance), a frequency (300, 4k) or a note (A4).",
                            L.body.reduced (20.0f), juce::Justification::centredTop);
            }
        }
        paintAxis (g, L, L.bottomAxis, false);
        paintAxisCaption (g, L, L.bottomAxis);

        // ---- frequency markers across the whole chart
        if (auto m = markerHz(); m > 0.0f) paintMarker (g, L, m, Theme::acid(), true);
        if (! exporting && hoverHz > 0.0f) paintMarker (g, L, hoverHz, Theme::text(), false);

        if (! exporting) paintSide (g, L);
    }

    void paintHeader (juce::Graphics& g, int w, const Layout& L, bool exporting)
    {
        g.setColour (Theme::text()); g.setFont (Theme::font (19.0f, true));
        g.drawText ("EQ FREQUENCY CHART", 16, 8, 400, 26, juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f));
        g.drawText ("Where every instrument and voice lives, 20 Hz - 20 kHz. Grid = ISO 1/3-octave bands (graphic EQ centres); brighter lines = octaves; brightest = 100 Hz / 1 kHz / 10 kHz.",
                    230, 10, w - 230 - (exporting ? 20 : 240), 22, juce::Justification::centredLeft);
        g.drawText ("Hover anywhere for frequency, note and what lives there. Click a row to pin its notes; click the scale to pin a frequency; right-click or Esc to clear.",
                    16, 30, w - (exporting ? 40 : 260), 18, juce::Justification::centredLeft);

        if (exporting)
        {
            // the legend (in the window it sits next to the toggles)
            int x = 16; const int y = 62;
            const char* names[] = { "Fundamentals / body", "Sweet spot - boost", "Problem zone - cut", "Air / presence" };
            g.setFont (Theme::font (12.0f));
            for (int k = 0; k < 4; ++k)
            {
                paintSwatch (g, { (float) x, (float) y + 4.0f, 16.0f, 14.0f }, k);
                g.setColour (Theme::text()); g.drawText (names[k], x + 22, y, 200, 22, juce::Justification::centredLeft);
                x += 30 + (int) juce::GlyphArrangement::getStringWidth (Theme::font (12.0f), names[k]) + 20;
            }
            paintRangeLegend (g, (float) x, (float) y);
            juce::ignoreUnused (L);
        }
        else
        {
            // colour swatches in front of the band toggles
            auto sw = [&] (juce::ToggleButton& b, int k)
            {
                auto r = b.getBounds().toFloat();
                if (k >= 0) paintSwatch (g, { r.getX() - 16.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f }, k);
                else
                {
                    g.setColour (Theme::violet().brighter (0.3f)); g.fillRoundedRectangle ({ r.getX() - 16.0f, r.getCentreY() - 1.5f, 9.0f, 3.0f }, 1.5f);
                    g.setColour (Theme::dim()); g.drawHorizontalLine ((int) r.getCentreY(), r.getX() - 7.0f, r.getX() - 2.0f);
                }
            };
            sw (showFund, 0); sw (showBoost, 1); sw (showCut, 2); sw (showAir, 3); sw (showRanges, -1);
        }
    }

    void paintRangeLegend (juce::Graphics& g, float x, float y)
    {
        g.setFont (Theme::font (12.0f));
        g.setColour (Theme::violet().brighter (0.3f)); g.fillRoundedRectangle ({ x, y + 9.5f, 26.0f, 3.5f }, 1.5f);
        g.setColour (Theme::text()); g.drawText ("Note range", (int) x + 32, (int) y, 90, 22, juce::Justification::centredLeft);
        x += 116.0f;
        g.setColour (Theme::dim()); g.drawLine (x, y + 11.0f, x + 26.0f, y + 11.0f, 1.2f);
        g.setColour (Theme::text()); g.drawText ("Harmonics / overtones", (int) x + 32, (int) y, 160, 22, juce::Justification::centredLeft);
        x += 196.0f;
        g.setColour (Theme::red().withAlpha (0.25f)); g.fillRect (juce::Rectangle<float> (x, y + 4.0f, 26.0f, 14.0f));
        g.setColour (Theme::text()); g.drawText ("Usual high-pass / low-pass", (int) x + 32, (int) y, 200, 22, juce::Justification::centredLeft);
    }

    static void paintSwatch (juce::Graphics& g, juce::Rectangle<float> r, int kind)
    {
        g.setColour (colourFor (kind)); g.fillRoundedRectangle (r, 3.0f);
        if (kind == 2) hatch (g, r);
    }
    static void hatch (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.0f) g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
    }

    void paintZones (juce::Graphics& g, const Layout& L)
    {
        for (auto& z : EQRef::zones())
        {
            auto x0 = xForHz (z.lo, L), x1 = xForHz (z.hi, L);
            auto zr = juce::Rectangle<float> (x0, L.zones.getY(), x1 - x0, L.zones.getHeight()).reduced (1.0f, 1.0f);
            bool hot = hoverHz >= z.lo && hoverHz < z.hi;
            g.setColour (hot ? Theme::panel2().brighter (0.15f) : Theme::panel2()); g.fillRoundedRectangle (zr, 3.0f);
            g.setColour (hot ? Theme::acid() : Theme::text().withAlpha (0.85f)); g.setFont (Theme::font (zr.getWidth() < 70.0f ? 8.5f : 9.5f, true));
            g.drawFittedText (z.name, zr.withHeight (17.0f).toNearestInt().reduced (2, 0), juce::Justification::centredBottom, 1, 0.7f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
            g.drawFittedText (EQRef::rangeShort (z.lo, z.hi),
                              zr.withTrimmedTop (17.0f).toNearestInt().reduced (2, 0), juce::Justification::centredTop, 1, 0.7f);
        }
    }

    /** Two-line scale: octave centres (bold) on one line, the other 1/3-octave centres on the other. */
    void paintAxis (juce::Graphics& g, const Layout& L, juce::Rectangle<float> r, bool top)
    {
        g.setColour (Theme::bg().withAlpha (0.35f)); g.fillRect (r);
        auto lineA = top ? r.withHeight (14.0f) : r.withTrimmedTop (r.getHeight() - 14.0f);   // octaves: next to the chart
        auto lineB = top ? r.withTrimmedTop (14.0f) : r.withHeight (r.getHeight() - 14.0f);
        if (top) std::swap (lineA, lineB);
        for (auto hz : EQRef::thirdOctaves())
        {
            auto x = xForHz (hz, L);
            bool oct = EQRef::isOctaveCentre (hz) || hz == 20.0f || hz == 20000.0f;
            auto& line = oct ? lineA : lineB;
            g.setColour (oct ? Theme::text() : Theme::dim()); g.setFont (Theme::font (oct ? 10.0f : 8.5f, oct));
            g.drawText (EQRef::scaleLabel (hz), juce::Rectangle<float> (x - 22.0f, line.getY(), 44.0f, line.getHeight()), juce::Justification::centred);
            g.setColour (Theme::line());
            g.drawVerticalLine ((int) x, top ? r.getBottom() - 3.0f : r.getY(), top ? r.getBottom() : r.getY() + 3.0f);
        }
    }

    void paintAxisCaption (juce::Graphics& g, const Layout& L, juce::Rectangle<float> r)
    {
        g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
        g.drawText ("FREQUENCY (Hz, k = kHz)", juce::Rectangle<float> (8.0f, r.getY(), L.chartL - 16.0f, r.getHeight()), juce::Justification::centredRight);
    }

    void paintPiano (juce::Graphics& g, const Layout& L)
    {
        auto r = L.piano;
        g.setColour (Theme::bg()); g.fillRect (r);
        const int hoverMidi = hoverHz > 0.0f ? (int) std::lround (EQRef::midiFor (hoverHz)) : -1;
        const int markMidi  = markerHz() > 0.0f ? (int) std::lround (EQRef::midiFor (markerHz())) : -1;
        auto isBlack = [] (int m) { auto n = ((m % 12) + 12) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; };
        auto xm = [&] (double m) { return xForHz ((float) EQRef::hzForMidi (m), L); };
        // white keys share the space under the black keys, like a real keyboard
        for (int m = 21; m <= 108; ++m)
        {
            if (isBlack (m)) continue;
            auto x0 = (m > 21 && isBlack (m - 1)) ? xm (m - 1) : xm (m - 0.5);
            auto x1 = (m < 108 && isBlack (m + 1)) ? xm (m + 1) : xm (m + 0.5);
            auto key = juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight());
            g.setColour (m == hoverMidi ? Theme::acid() : m == markMidi ? Theme::acid().withAlpha (0.7f) : Theme::text().withAlpha (0.85f));
            g.fillRect (key);
            g.setColour (Theme::bg().withAlpha (0.85f)); g.drawVerticalLine ((int) x0, key.getY(), key.getBottom());
        }
        for (int m = 22; m <= 107; ++m)
        {
            if (! isBlack (m)) continue;
            auto x0 = xm (m - 0.5), x1 = xm (m + 0.5);
            g.setColour (m == hoverMidi ? Theme::acid() : m == markMidi ? Theme::acid().withAlpha (0.7f) : Theme::panel());
            g.fillRect (juce::Rectangle<float> (x0 + 0.5f, r.getY(), juce::jmax (1.0f, x1 - x0 - 1.0f), r.getHeight() * 0.55f));
        }
        // octave names on every C, in the white key below the black keys; A4 = 440 Hz marked under the keyboard
        g.setFont (Theme::font (8.5f, true));
        for (int m = 24; m <= 108; m += 12)
        {
            auto x0 = xForHz ((float) EQRef::hzForMidi (m - 0.5), L);
            g.setColour (m == hoverMidi || m == markMidi ? Theme::bg() : Theme::panel());
            g.drawText (EQRef::midiName (m), juce::Rectangle<float> (x0 - 3.0f, r.getBottom() - 11.0f, 26.0f, 11.0f), juce::Justification::centredLeft);
        }
        g.setFont (Theme::font (8.5f));
        for (auto [m, name] : { std::pair<int, const char*> { 69, "A4 = 440 Hz" }, { 60, "middle C 261.6 Hz" } })
        {
            auto x = xForHz ((float) EQRef::hzForMidi (m), L);
            g.setColour (Theme::acid().withAlpha (0.9f)); g.fillRect (juce::Rectangle<float> (x - 0.5f, r.getBottom(), 1.0f, 4.0f));
            g.setColour (Theme::dim());
            g.drawText (name, juce::Rectangle<float> (m == 69 ? x + 3.0f : x - 123.0f, r.getBottom() + 1.0f, 120.0f, 11.0f), m == 69 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
        g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
        g.drawFittedText ("PIANO\nA0 27.5 Hz - C8 4.19 kHz", juce::Rectangle<float> (8.0f, r.getY(), L.chartL - 16.0f, r.getHeight()).toNearestInt(), juce::Justification::centredRight, 2);
    }

    void paintGrid (juce::Graphics& g, const Layout& L, juce::Rectangle<float> body)
    {
        // faint tint on alternate regions so the named regions read down the whole chart
        int zi = 0;
        for (auto& z : EQRef::zones())
        {
            if (zi++ % 2 == 1) { g.setColour (Theme::bg().withAlpha (0.18f)); g.fillRect (juce::Rectangle<float> (xForHz (z.lo, L), body.getY(), xForHz (z.hi, L) - xForHz (z.lo, L), body.getHeight())); }
        }
        for (auto hz : EQRef::thirdOctaves())
        {
            auto a = EQRef::isDecade (hz) ? 0.85f : EQRef::isOctaveCentre (hz) ? 0.5f : 0.2f;
            g.setColour (Theme::line().withAlpha (a));
            g.drawVerticalLine ((int) xForHz (hz, L), body.getY(), body.getBottom());
        }
    }

    void paintFamilyHeader (juce::Graphics& g, const Layout& L, const Item& it, float y)
    {
        auto r = juce::Rectangle<float> (L.body.getX(), y, L.body.getWidth(), it.h);
        g.setColour (Theme::panel2()); g.fillRect (r.reduced (0.0f, 1.0f));
        g.setColour (familyColour (it.family)); g.fillRect (r.withWidth (4.0f).reduced (0.0f, 1.0f));
        g.setColour (Theme::text()); g.setFont (Theme::font (10.5f, true));
        g.drawText (EQRef::families()[it.family].toUpperCase(), r.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
    }

    void paintRow (juce::Graphics& g, const Layout& L, const Item& it, float y, bool hovered, bool exporting)
    {
        auto& r = EQRef::rows()[(size_t) it.row];
        const bool pinned = it.row == pinnedRow && ! exporting;
        auto rowR = juce::Rectangle<float> (L.body.getX(), y, L.body.getWidth(), it.h);
        if (pinned)       { g.setColour (Theme::acid().withAlpha (0.09f)); g.fillRect (rowR); }
        else if (hovered) { g.setColour (Theme::text().withAlpha (0.05f)); g.fillRect (rowR); }
        g.setColour (Theme::line().withAlpha (0.35f)); g.drawHorizontalLine ((int) rowR.getBottom() - 1, rowR.getX(), rowR.getRight());

        // name column: family chip, name, note range
        g.setColour (familyColour (r.family)); g.fillRoundedRectangle ({ L.nameX, y + 6.0f, 3.0f, it.h - 12.0f }, 1.5f);
        auto nameR = juce::Rectangle<float> (L.nameX + 9.0f, y + 3.0f, nameW - 14.0f, 16.0f);
        g.setColour (pinned ? Theme::acid() : Theme::text()); g.setFont (Theme::font (11.5f, true));
        g.drawFittedText (r.name, nameR.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f));
        auto sub = r.fLo > 0.0f ? EQRef::noteName (r.fLo) + "-" + EQRef::noteName (r.fHi) + "  " + EQRef::hzText (r.fLo, false) + "-" + EQRef::hzText (r.fHi)
                                : "unpitched   " + EQRef::rangeShort (r.eLo, r.eHi);
        g.drawFittedText (sub, nameR.translated (0.0f, 15.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

        const float bandY = y + 3.0f, bandH = bandHeight();
        const float railY = y + it.h - 8.5f;

        // usual high-pass / low-pass
        if (showRanges.getToggleState())
        {
            if (r.hpf > 0.0f)
            {
                auto x = xForHz (r.hpf, L);
                g.setColour (Theme::red().withAlpha (0.10f)); g.fillRect (juce::Rectangle<float> (L.chartL, y + 1.0f, x - L.chartL, it.h - 3.0f));
                g.setColour (Theme::red().withAlpha (0.55f)); g.drawLine (x - 9.0f, y + it.h - 3.0f, x, y + 2.0f, 1.3f);
            }
            if (r.lpf > 0.0f)
            {
                auto x = xForHz (r.lpf, L);
                g.setColour (Theme::red().withAlpha (0.10f)); g.fillRect (juce::Rectangle<float> (x, y + 1.0f, L.chartR - x, it.h - 3.0f));
                g.setColour (Theme::red().withAlpha (0.55f)); g.drawLine (x, y + 2.0f, x + 9.0f, y + it.h - 3.0f, 1.3f);
            }
        }

        // bands
        const auto mark = markerHz();
        for (auto& b : r.bands)
        {
            if (! kindVisible (b.kind)) continue;
            auto x0 = xForHz (b.lo, L), x1 = xForHz (b.hi, L);
            auto br = juce::Rectangle<float> (x0 + 0.5f, bandY, x1 - x0 - 1.0f, bandH);
            const bool hot = &b == hoverBand;
            const bool matched = bandMatchesTerm (b) || (mark > 0.0f && mark >= b.lo && mark <= b.hi);
            const bool dimmed = (bandTerm.isNotEmpty() && ! bandMatchesTerm (b)) || (mark > 0.0f && ! (mark >= b.lo && mark <= b.hi) && bandTerm.isEmpty());
            auto c = colourFor (b.kind);
            g.setColour (c.withAlpha (hot ? 1.0f : dimmed ? 0.25f : (b.kind == 0 ? 0.62f : 0.78f)));
            g.fillRoundedRectangle (br, 3.5f);
            if (b.kind == 2) hatch (g, br);
            if (hot || matched) { g.setColour (hot ? Theme::text() : Theme::acid()); g.drawRoundedRectangle (br.reduced (0.5f), 3.5f, hot ? 1.6f : 1.2f); }

            // label: "boxy 200-450" when it fits, else the name alone
            g.setColour (juce::Colours::black.withAlpha (dimmed ? 0.5f : 0.88f));
            auto f = Theme::font (9.5f, true);
            auto label = juce::String (b.label);
            auto withRange = label + "  " + EQRef::rangeShort (b.lo, b.hi);
            auto avail = br.getWidth() - 6.0f;
            g.setFont (f);
            if (juce::GlyphArrangement::getStringWidth (f, withRange) <= avail)  g.drawText (withRange, br.reduced (3.0f, 0.0f), juce::Justification::centred);
            else if (juce::GlyphArrangement::getStringWidth (f, label) <= avail) g.drawText (label, br.reduced (3.0f, 0.0f), juce::Justification::centred);
            else { g.setFont (Theme::font (8.0f, true)); g.drawFittedText (label, br.toNearestInt().reduced (1, 0), juce::Justification::centred, 1, 0.6f); }
        }

        // note range (solid) and harmonics / overtones (thin) underneath
        if (showRanges.getToggleState())
        {
            auto ex0 = xForHz (r.eLo, L), ex1 = xForHz (r.eHi, L);
            g.setColour (Theme::dim().withAlpha (0.9f));
            g.drawLine (ex0, railY, ex1, railY, 1.2f);
            g.drawVerticalLine ((int) ex0, railY - 2.5f, railY + 2.5f);
            g.drawVerticalLine ((int) ex1, railY - 2.5f, railY + 2.5f);
            if (r.fLo > 0.0f)
            {
                auto fx0 = xForHz (r.fLo, L), fx1 = xForHz (r.fHi, L);
                g.setColour (Theme::violet().brighter (0.45f));
                g.fillRoundedRectangle ({ fx0, railY - 2.0f, juce::jmax (3.0f, fx1 - fx0), 4.0f }, 2.0f);
            }
        }
    }

    void paintMarker (juce::Graphics& g, const Layout& L, float hz, juce::Colour c, bool pinned)
    {
        auto x = xForHz (hz, L);
        g.setColour (c.withAlpha (pinned ? 0.8f : 0.55f));
        g.drawLine (x, L.zones.getY(), x, L.bottomAxis.getY(), pinned ? 1.5f : 1.0f);
        // tag on the scale
        auto text = (pinned ? EQRef::hzText (hz) : EQRef::hzPrecise (hz)) + "  " + EQRef::noteName (hz);
        auto f = Theme::font (10.0f, true);
        auto tw = juce::GlyphArrangement::getStringWidth (f, text) + 12.0f;
        auto tagY = pinned ? L.axis.getY() - 1.0f : L.axis.getY() + 13.0f;
        auto tag = juce::Rectangle<float> (juce::jlimit (L.chartL, L.chartR - tw, x - tw * 0.5f), tagY, tw, 15.0f);
        g.setColour (c); g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (Theme::bg()); g.setFont (f); g.drawText (text, tag, juce::Justification::centred);
        auto btag = tag.withY (L.bottomAxis.getY() + 1.0f);
        if (! pinned) { g.setColour (c); g.fillRoundedRectangle (btag, 3.0f); g.setColour (Theme::bg()); g.drawText (text, btag, juce::Justification::centred); }
    }

    //==========================================================================
    void paintSide (juce::Graphics& g, const Layout& L)
    {
        auto side = L.side;
        g.setColour (Theme::panel2()); g.fillRoundedRectangle (side, 6.0f);
        auto inner = side.reduced (12.0f, 10.0f);
        const float freqH = juce::jmin (300.0f, inner.getHeight() * 0.42f);
        auto freqArea = inner.removeFromBottom (freqH);
        inner.removeFromBottom (8.0f);

        // ---- the hovered or pinned instrument
        int row = (hoverItem >= 0 && hoverItem < (int) items.size()) ? items[(size_t) hoverItem].row : -1;
        if (row < 0) row = pinnedRow;
        if (row >= 0) paintRowDetails (g, inner, EQRef::rows()[(size_t) row], row == pinnedRow && hoverItem < 0);
        else          paintQuickReference (g, inner);

        // ---- what lives at the cursor / pinned frequency
        g.setColour (Theme::line()); g.drawHorizontalLine ((int) freqArea.getY() - 4, freqArea.getX(), freqArea.getRight());
        auto hz = hoverHz > 0.0f ? hoverHz : markerHz();
        if (hz > 0.0f) paintFrequencyDetails (g, freqArea, hz, hoverHz > 0.0f);
        else
        {
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.5f));
            g.drawFittedText ("Move over the chart to read any frequency: nearest note, 1/3-octave band, region, wavelength - and every instrument band that lives there.\n\n"
                              "Click the scale (or type 300, 4k or A4 in Find) to pin a frequency: matching bands light up, the rest dim.",
                              freqArea.toNearestInt(), juce::Justification::topLeft, 12);
        }
    }

    void paintRowDetails (juce::Graphics& g, juce::Rectangle<float> r, const Row& row, bool pinned)
    {
        g.setColour (Theme::acid()); g.setFont (Theme::font (14.0f, true));
        g.drawFittedText (row.name, r.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
        g.drawText (EQRef::families()[row.family] + (pinned ? "  -  pinned (click the row again to unpin)" : ""), r.removeFromTop (14.0f), juce::Justification::centredLeft);
        r.removeFromTop (4.0f);

        auto fact = [&] (const juce::String& k, const juce::String& v)
        {
            auto line = r.removeFromTop (15.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
            g.drawText (k, line.removeFromLeft (92.0f), juce::Justification::centredLeft);
            g.setColour (Theme::text()); g.setFont (Theme::font (10.0f, true));
            g.drawFittedText (v, line.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        };
        if (row.fLo > 0.0f) fact ("Notes", EQRef::noteName (row.fLo) + " - " + EQRef::noteName (row.fHi) + "   (" + EQRef::rangeText (row.fLo, row.fHi) + ")");
        else                fact ("Notes", "unpitched - no single fundamental");
        fact ("With harmonics", EQRef::rangeText (row.eLo, row.eHi));
        fact ("High-pass", row.hpf > 0.0f ? "around " + EQRef::hzText (row.hpf) : juce::String ("usually none"));
        if (row.lpf > 0.0f) fact ("Low-pass", "around " + EQRef::hzText (row.lpf));
        r.removeFromTop (4.0f);
        g.setColour (Theme::text()); g.setFont (Theme::font (10.5f, true));
        g.drawFittedText (juce::String ("In short: ") + row.tip, r.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2);
        r.removeFromTop (4.0f);

        const float each = juce::jlimit (34.0f, 52.0f, r.getHeight() / (float) juce::jmax (1, (int) row.bands.size()));
        for (auto& b : row.bands)
        {
            if (r.getHeight() < 20.0f) break;
            auto item = r.removeFromTop (each);
            const bool hot = &b == hoverBand;
            if (hot) { g.setColour (Theme::text().withAlpha (0.06f)); g.fillRoundedRectangle (item.expanded (4.0f, 0.0f), 4.0f); }
            paintSwatch (g, item.removeFromLeft (6.0f).reduced (0.0f, 3.0f), b.kind);
            item.removeFromLeft (8.0f);
            auto head = item.removeFromTop (15.0f);
            g.setColour (hot ? Theme::acid() : Theme::text()); g.setFont (Theme::font (10.5f, true));
            g.drawText (juce::String (b.label).toUpperCase() + "   " + EQRef::rangeText (b.lo, b.hi), head, juce::Justification::centredLeft);
            g.setColour (colourFor (b.kind)); g.setFont (Theme::font (8.5f, true));
            g.drawText (juce::String (EQRef::kindName (b.kind)).toUpperCase(), head, juce::Justification::centredRight);
            g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
            g.drawFittedText (b.note, item.toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
        }
    }

    void paintQuickReference (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (Theme::acid()); g.setFont (Theme::font (14.0f, true));
        g.drawText ("EQ QUICK REFERENCE", r.removeFromTop (20.0f), juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
        g.drawText ("Hover a row for its notes; click it to pin them here.", r.removeFromTop (16.0f), juce::Justification::centredLeft);
        r.removeFromTop (6.0f);
        struct Line { const char* k; const char* v; };
        const Line lines[] = {
            { "Cut first", "Cuts sound more natural than boosts; cut narrow, boost wide." },
            { "Q / width", "0.3-0.7 broad tone shaping  |  1-2 musical  |  4-10 surgical (resonances, ring)." },
            { "Gain", "Mix moves 1-4 dB; master moves 0.5-1.5 dB. Bigger usually means the source or arrangement." },
            { "High-pass", "Almost everything except kick, 808 and bass - see the red shading for where." },
            { "Own the low end", "One owner per range: kick OR 808 at 50 Hz; bass and kick share by cutting each other." },
            { "Find a problem", "Narrow boost +8 dB, sweep until it sounds worst, then cut it there." },
            { "Octaves", "Every doubling is one octave; each grid line is a third of one (1.26x)." },
            { "Check", "Headphones, monitors and a phone. Level-match before you compare." } };
        for (auto& l : lines)
        {
            if (r.getHeight() < 30.0f) break;
            g.setColour (Theme::text()); g.setFont (Theme::font (10.5f, true));
            g.drawText (l.k, r.removeFromTop (14.0f), juce::Justification::centredLeft);
            g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
            g.drawFittedText (l.v, r.removeFromTop (28.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        }
    }

    void paintFrequencyDetails (juce::Graphics& g, juce::Rectangle<float> r, float hz, bool cursor)
    {
        auto& z = EQRef::zoneFor (hz);
        g.setColour (cursor ? Theme::text() : Theme::acid()); g.setFont (Theme::font (16.0f, true));
        auto head = r.removeFromTop (22.0f);
        g.drawText (EQRef::hzPrecise (hz), head, juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
        g.drawText (cursor ? "AT THE CURSOR" : "PINNED", head, juce::Justification::centredRight);

        auto fact = [&] (const juce::String& k, const juce::String& v)
        {
            auto line = r.removeFromTop (14.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
            g.drawText (k, line.removeFromLeft (78.0f), juce::Justification::centredLeft);
            g.setColour (Theme::text()); g.setFont (Theme::font (9.5f, true));
            g.drawFittedText (v, line.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        };
        fact ("Note", EQRef::noteWithCents (hz) + "   (" + EQRef::hzPrecise ((float) EQRef::hzForMidi (std::round (EQRef::midiFor (hz)))) + ")");
        fact ("1/3 octave", EQRef::hzText (EQRef::nearestThird (hz)) + " band");
        fact ("Region", juce::String (z.name) + "  " + EQRef::rangeText (z.lo, z.hi));
        fact ("Wavelength", EQRef::wavelengthText (hz));
        fact ("Period", EQRef::periodText (hz) + "   MIDI " + juce::String ((int) std::lround (EQRef::midiFor (hz))));
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
        g.drawFittedText (z.what, r.removeFromTop (26.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        r.removeFromTop (2.0f);

        juce::StringArray lines; juce::Array<int> kinds;
        for (auto& it : items)
            if (it.row >= 0)
            {
                auto& row = EQRef::rows()[(size_t) it.row];
                for (auto& b : row.bands)
                    if (kindVisible (b.kind) && hz >= b.lo && hz <= b.hi) { lines.add (juce::String (row.name) + "  -  " + b.label); kinds.add (b.kind); }
            }
        g.setColour (Theme::text()); g.setFont (Theme::font (10.0f, true));
        g.drawText ("Lives here (" + juce::String (lines.size()) + ")", r.removeFromTop (15.0f), juce::Justification::centredLeft);
        const float lh = 13.0f;
        const int fit = juce::jmax (0, (int) (r.getHeight() / lh));
        for (int i = 0; i < lines.size() && i < fit; ++i)
        {
            auto line = r.removeFromTop (lh);
            if (i == fit - 1 && lines.size() > fit)
            {
                g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f));
                g.drawText ("+ " + juce::String (lines.size() - fit + 1) + " more - type " + EQRef::hzText (hz, false) + " in Find to see them all", line, juce::Justification::centredLeft);
                break;
            }
            g.setColour (colourFor (kinds[i])); g.fillEllipse (line.getX(), line.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (Theme::text().withAlpha (0.9f)); g.setFont (Theme::font (9.5f));
            g.drawFittedText (lines[i], line.withTrimmedLeft (11.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }
};

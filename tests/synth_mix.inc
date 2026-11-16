    const double sr = 44100, bpm = 100, beat = 60.0 / bpm; const int n = (int) (sr * 32 * beat * 1.0);
    std::vector<float> vL (n, 0), dL (n, 0), bL (n, 0), oL (n, 0), oR (n, 0);
    std::mt19937 rng (3); std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    const double tp = 2 * M_PI;
    // "vocal": a centred harmonic voice with vibrato, a new note every 2 beats, formant-ish harmonic weights
    { const double notes[] = { 220, 262, 294, 262, 330, 294, 262, 220 }; double ph = 0;
      for (int i = 0; i < n; ++i) { const double t = i / sr; const int k = (int) (t / (2 * beat)) % 8; const double f = notes[k] * (1.0 + 0.006 * std::sin (tp * 5.5 * t)); ph += tp * f / sr;
        const double tn = std::fmod (t, 2 * beat), env = std::min (1.0, tn / 0.04) * std::min (1.0, (2 * beat - tn) / 0.08); double v = 0; const double w[] = { 1, 0.7, 0.9, 0.5, 0.35, 0.2, 0.12 };
        for (int h = 1; h <= 7; ++h) v += w[h - 1] * std::sin (h * ph); vL[(size_t) i] = (float) (0.09 * env * v); } }
    // drums: kick on beats, noise snare on 2 and 4, noise hats on 8ths (centre)
    for (int b = 0; b < 32; ++b)
    {
        const int a = (int) (b * beat * sr); double ph = 0;
        for (int i = 0; i < (int) (0.25 * sr) && a + i < n; ++i) { const double t = i / sr; ph += tp * (48 + 130 * std::exp (-t * 38)) / sr; dL[(size_t) (a + i)] += (float) (0.6 * std::sin (ph) * std::exp (-t * 14)); }
        if (b % 2 == 1) for (int i = 0; i < (int) (0.15 * sr) && a + i < n; ++i) dL[(size_t) (a + i)] += uni (rng) * 0.3f * (float) std::exp (-i / sr * 26);
        for (int h = 0; h < 2; ++h) { const int ah = a + (int) (h * 0.5 * beat * sr); for (int i = 0; i < (int) (0.03 * sr) && ah + i < n; ++i) dL[(size_t) (ah + i)] += uni (rng) * 0.12f * (float) std::exp (-i / sr * 140); }
    }
    // bass: a held sine with a second harmonic, a new note every 4 beats (centre)
    { const double notes[] = { 55, 43.65, 49, 55 }; double ph = 0; for (int i = 0; i < n; ++i) { const double t = i / sr; const double f = notes[(int) (t / (4 * beat)) % 4]; ph += tp * f / sr; const double tn = std::fmod (t, 4 * beat); bL[(size_t) i] = (float) (0.35 * std::min (1.0, tn / 0.02) * (std::sin (ph) + 0.2 * std::sin (2 * ph))); } }
    // other: two different sustained chords hard left and hard right (pads)
    for (int i = 0; i < n; ++i) { const double t = i / sr; double l = 0, r = 0; for (double f : { 392.0, 493.9, 587.3 }) l += std::sin (tp * f * t); for (double f : { 349.2, 440.0, 523.3, 659.3 }) r += std::sin (tp * f * t * 1.003); oL[(size_t) i] = (float) (0.05 * l); oR[(size_t) i] = (float) (0.05 * r); }
    std::vector<float> L (n), R (n);
    for (int i = 0; i < n; ++i) { L[(size_t) i] = vL[(size_t) i] + dL[(size_t) i] + bL[(size_t) i] + oL[(size_t) i]; R[(size_t) i] = vL[(size_t) i] + dL[(size_t) i] + bL[(size_t) i] + oR[(size_t) i]; }

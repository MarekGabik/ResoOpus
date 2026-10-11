#pragma once

#include "Primitives.h"
#include <vector>
#include <cmath>

// NEBULA wavetables: three families (Vocal, Organ, Glass) of eight frames each, built once by additive synthesis
// with one band-limited copy per octave (mip levels), so high notes never alias. No JUCE dependency.
namespace dsp
{
class Wavetables
{
public:
    static constexpr int size = 2048, frames = 8, families = 3, levels = 11, maxHarm = 1023;

    // Built on first use (call from prepare, never from the audio thread)
    static const Wavetables& get()
    {
        static const Wavetables t;
        return t;
    }

    // level for an increment in cycles per sample: the highest harmonic must stay below 0.45 of the sample rate
    static int levelFor (double dt)
    {
        const double allowed = 0.45 / std::max (1.0e-9, std::abs (dt));
        int k = 0;
        while (k < levels - 1 && (double) (1024 >> k) > allowed) ++k;
        return k;
    }

    const float* table (int family, int frame, int level) const
    {
        return data.data() + ((size_t) ((family * frames + frame) * levels + level)) * size;
    }

    // phase 0..1, position 0..1 across the frames of the family
    float read (int family, double position, double phase, int level) const
    {
        const double fp = clampf ((float) position, 0.0f, 1.0f) * (frames - 1);
        const int f0 = std::min (frames - 2, (int) fp);
        const float ff = (float) (fp - f0);
        const double ip = (phase - std::floor (phase)) * size;
        const int i0 = (int) ip & (size - 1), i1 = (i0 + 1) & (size - 1);
        const float fi = (float) (ip - std::floor (ip));
        const float* a = table (family, f0, level);
        const float* b = table (family, f0 + 1, level);
        const float va = a[i0] + fi * (a[i1] - a[i0]);
        const float vb = b[i0] + fi * (b[i1] - b[i0]);
        return va + ff * (vb - va);
    }

private:
    Wavetables()
    {
        data.assign ((size_t) families * frames * levels * size, 0.0f);
        std::vector<float> sine (size);
        for (int i = 0; i < size; ++i) sine[(size_t) i] = (float) std::sin (twoPi * i / size);
        std::vector<float> amp (maxHarm + 1);
        for (int fam = 0; fam < families; ++fam)
            for (int fr = 0; fr < frames; ++fr)
            {
                spectrum (fam, fr, amp);
                float scale = 1.0f;
                for (int lv = 0; lv < levels; ++lv)
                {
                    float* t = data.data() + ((size_t) ((fam * frames + fr) * levels + lv)) * size;
                    const int top = std::min (maxHarm, 1024 >> lv);
                    for (int h = 1; h <= top; ++h)
                    {
                        const float a = amp[(size_t) h];
                        if (std::abs (a) < 1.0e-5f) continue;
                        for (int i = 0; i < size; ++i) t[i] += a * sine[(size_t) ((h * i) & (size - 1))];
                    }
                    if (lv == 0)   // one gain per frame (by loudness, peak at most 1.4), so frames and mip levels match
                    {
                        float peak = 1.0e-6f;
                        double sq = 0.0;
                        for (int i = 0; i < size; ++i) { peak = std::max (peak, std::abs (t[i])); sq += (double) t[i] * t[i]; }
                        const float rms = (float) std::sqrt (sq / size) + 1.0e-6f;
                        scale = std::min (0.5f / rms, 1.4f / peak);
                    }
                    for (int i = 0; i < size; ++i) t[i] *= scale;
                }
            }
    }

    // harmonic amplitudes of one frame
    static void spectrum (int family, int frame, std::vector<float>& a)
    {
        std::fill (a.begin(), a.end(), 0.0f);
        if (family == 0)
        {
            // VOCAL: a buzzy source through three formants (on a 130 Hz voice): hum, U, O, A, AE, E, I, ER
            static const float F[8][3] = { { 250, 900, 2300 }, { 325, 700, 2530 }, { 450, 800, 2830 }, { 800, 1150, 2900 },
                                           { 660, 1720, 2410 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 490, 1350, 1690 } };
            static const float G[8][3] = { { 1.0f, 0.15f, 0.05f }, { 1.0f, 0.5f, 0.15f }, { 1.0f, 0.7f, 0.2f }, { 1.0f, 0.8f, 0.3f },
                                           { 1.0f, 0.7f, 0.35f }, { 1.0f, 0.6f, 0.35f }, { 1.0f, 0.5f, 0.4f }, { 1.0f, 0.7f, 0.6f } };
            for (int h = 1; h <= maxHarm; ++h)
            {
                const float f = 130.0f * (float) h;
                float g = 0.015f;
                for (int k = 0; k < 3; ++k)
                {
                    const float bw = 60.0f + 0.07f * F[frame][k];
                    const float d = (f - F[frame][k]) / bw;
                    g += G[frame][k] * std::exp (-0.5f * d * d);
                }
                a[(size_t) h] = g / std::pow ((float) h, 0.5f);
            }
        }
        else if (family == 1)
        {
            // ORGAN: drawbar registrations from a soft flute to the full organ and a reed
            static const float D[8][9] = { // harmonics 1 2 3 4 5 6 8 10 12
                { 1.0f, 0, 0, 0, 0, 0, 0, 0, 0 }, { 1.0f, 0.5f, 0, 0, 0, 0, 0, 0, 0 }, { 1.0f, 0.4f, 0.6f, 0, 0, 0, 0, 0, 0 },
                { 1.0f, 0.7f, 0.5f, 0.4f, 0, 0, 0, 0, 0 }, { 1.0f, 0.7f, 0.5f, 0.4f, 0.3f, 0.3f, 0, 0, 0 },
                { 0.9f, 0.8f, 0.8f, 0.7f, 0.6f, 0.6f, 0.5f, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 0.9f, 0.8f, 0.8f, 0.7f, 0.6f, 0.6f, 0.6f, 0.4f, 0.4f } };
            static const int H[9] = { 1, 2, 3, 4, 5, 6, 8, 10, 12 };
            if (frame == 6)
                for (int h = 1; h <= 31; h += 2) a[(size_t) h] = 1.0f / std::pow ((float) h, 0.7f);   // reed: odd harmonics
            else
                for (int k = 0; k < 9; ++k) a[(size_t) H[k]] = D[frame][k];
        }
        else
        {
            // GLASS: sparse, bell-like and digital spectra
            switch (frame)
            {
                case 0: a[1] = 1.0f; a[7] = 0.3f; break;
                case 1: a[1] = 1.0f; a[5] = 0.4f; a[9] = 0.3f; break;
                case 2: a[1] = 0.8f; a[3] = 0.4f; a[11] = 0.4f; a[13] = 0.3f; break;
                case 3: a[1] = 0.7f; a[8] = 0.5f; a[9] = 0.5f; a[17] = 0.3f; break;
                case 4: for (int h = 1; h <= 63; h += 2) a[(size_t) h] = 1.0f / std::pow ((float) h, 0.4f); break;
                case 5: for (int h = 1; h <= 96; ++h) if (h % 3 != 0) a[(size_t) h] = 1.0f / (float) h; break;
                case 6: a[1] = 0.6f; for (int h = 10; h <= 18; ++h) a[(size_t) h] = 0.5f * std::exp (-0.15f * (float) ((h - 14) * (h - 14))); break;
                default: for (int h = 1; h <= 128; ++h) a[(size_t) h] = ((h * 7) % 5 == 0 ? -1.0f : 1.0f) / std::pow ((float) h, 0.3f); break;
            }
        }
    }

    std::vector<float> data;
};
}

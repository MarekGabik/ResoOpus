#pragma once

#include "Primitives.h"

// Voice engines. Allocation-free, no JUCE dependency, run at the oversampled rate.
namespace dsp
{
//==============================================================================
// Band-limited saw for the CHOIR voice. Same 4-point B-spline BLEP as ShapeOsc, but specialised: the only event
// is the wrap (a jump of -2 with no change of slope), so the correction is computed once per period and every
// other sample costs a phase increment and a 4-tap shift. Output is delayed by two samples (like ShapeOsc).
// The increment may change every sample (FM) but never goes negative.
class BlepSaw
{
public:
    void reset (double startPhase)
    {
        phase = startPhase - std::floor (startPhase);
        acc[0] = acc[1] = acc[2] = acc[3] = 0.0f;
        primed = false;
    }

    float tick (double dt)
    {
        dt = dt < 0.0 ? 0.0 : (dt > 0.45 ? 0.45 : dt);
        double p = phase + dt;
        if (p >= 1.0)
        {
            p -= 1.0;
            const double x = dt > 0.0 ? p / dt : 0.0;   // samples since the wrap, 0..1
            for (int j = 0; j < 4; ++j)
                acc[j] += (float) (-2.0 * stepResidual (x + (double) (j - 2)));
        }
        phase = p;
        const float now = (float) (2.0 * phase - 1.0);
        if (! primed) { q0 = q1 = now; primed = true; }
        const float out = q0 + acc[0];
        q0 = q1;
        q1 = now;
        acc[0] = acc[1]; acc[1] = acc[2]; acc[2] = acc[3]; acc[3] = 0.0f;
        return out;
    }

private:
    static double splineStep (double t)   // integrated cubic B-spline for -2 <= t <= 0
    {
        if (t <= -1.0) { const double u = 2.0 + t; return u * u * u * u / 24.0; }
        const double t2 = t * t;
        return 1.0 / 24.0 + 2.75 / 6.0 + (4.0 * t - 2.0 * t2 * t - 0.75 * t2 * t2) / 6.0;
    }
    static double stepResidual (double t) { return t <= -2.0 || t >= 2.0 ? 0.0 : (t < 0.0 ? splineStep (t) : -splineStep (-t)); }

    double phase = 0.0;
    float acc[4] {}, q0 = 0.0f, q1 = 0.0f;
    bool primed = false;
};

//==============================================================================
// Per-chunk settings of one voice (already modulated, snapped and transposed)
struct VoiceSettings
{
    int type = 0;                 // VP::Type
    double rootMidi = 48.0;       // after group pitch, snap, bend, TIDE transposition
    float intervals[5] {};        // semitones
    bool on[5] { true, true, true, true, true };
    float pull = 0.0f;            // TIDE pull between the oscillators 0..1
    double a4 = 440.0;
    float attack = 1.0f, release = 1.0f;   // seconds
    float target = 0.0f;          // level the envelope moves toward (max of HOLD and touch)
    float gainL = 0.5f, gainR = 0.5f;      // volume and constant-power pan
};

// CHOIR: five saws tuned as a chord around the voice root. TIDE pulls the voice down and, past half way,
// lets every saw frequency-modulate the next one (a ring of five), which turns the chord into a growl.
// The envelope is a slew toward the target level: up with ATTACK, down with RELEASE.
class ChoirVoice
{
public:
    void prepare (double fs, uint32_t seed)
    {
        sr = fs;
        Rng r;
        r.seed (seed);
        for (auto& o : osc) o.reset (r.next01());
        for (auto& y : last) y = 0.0f;
        level = 0.0f;
        primed = false;
    }

    void reset()
    {
        level = 0.0f;
        primed = false;
        for (auto& y : last) y = 0.0f;
    }

    void setSampleRate (double fs) { sr = fs; }

    float getLevel() const { return level; }
    bool isSilent() const { return level < 1.0e-5f; }

    // Adds the voice into L / R (n samples at the oversampled rate)
    void render (const VoiceSettings& s, float* L, float* R, int n)
    {
        // envelope coefficients (RC slew, ~99 % of the way after the set time)
        const float aC = (float) (1.0 - std::exp (-4.6 / (std::max (0.001, (double) s.attack) * sr)));
        const float rC = (float) (1.0 - std::exp (-4.6 / (std::max (0.001, (double) s.release) * sr)));
        if (isSilent() && s.target <= 0.0f)
        {
            level = 0.0f;
            primed = false;   // next start jumps straight to the current pitch (no sweep from the old one)
            return;
        }

        int active = 0;
        for (int i = 0; i < 5; ++i) if (s.on[i]) ++active;
        const float norm = active > 0 ? 0.42f / std::sqrt ((float) active) : 0.0f;

        // per-oscillator phase increment (ramped through the chunk: no zipper on knob moves)
        auto ramp = [this, n] (Ramp& r, float target) { if (primed) r.set (target, n); else r.reset (target); };
        for (int i = 0; i < 5; ++i)
        {
            const double hz = s.a4 * std::exp2 ((s.rootMidi + (double) s.intervals[i] - 69.0) / 12.0);
            ramp (inc[i], (float) std::min (0.45, hz / sr));
        }
        ramp (pullR, s.pull * 1.6f);
        ramp (gL, s.gainL * norm);
        ramp (gR, s.gainR * norm);
        primed = true;

        for (int k = 0; k < n; ++k)
        {
            const float pull = pullR.next();
            float sum = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                const float base = inc[i].next();
                if (! s.on[i]) { last[i] = 0.0f; continue; }
                // ring of five: each saw is modulated by the next active one (linear FM, frequency can stall at 0)
                int m = (i + 1) % 5;
                while (m != i && ! s.on[m]) m = (m + 1) % 5;
                const double dt = m == i ? (double) base : (double) base * (1.0 + (double) (pull * last[m]));
                last[i] = osc[i].tick (dt);
                sum += last[i];
            }
            level += (s.target > level ? aC : rC) * (s.target - level);
            const float amp = level * level;
            L[k] += sum * gL.next() * amp;
            R[k] += sum * gR.next() * amp;
        }
        if (! std::isfinite (level)) level = 0.0f;
    }

private:
    double sr = 96000.0;
    BlepSaw osc[5];
    float last[5] {};
    float level = 0.0f;
    bool primed = false;
    Ramp inc[5], pullR, gL, gR;
};
}

#pragma once

#include "Primitives.h"
#include <vector>

// Effects: FILTER (oversampled, stereo), ECHO and SPACE (base rate). Allocation happens only in prepare().
namespace dsp
{
inline float softClip (float x) { return x / (1.0f + std::abs (x)); }
inline float tanhFast (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

//==============================================================================
// Aggressive 12 dB filter: TPT state variable with saturating (asymmetric) integrators. Resonance does not thin
// out the lows (the input is fed with a little extra bass at high resonance), loud input or high resonance growls.
class GrowlFilter
{
public:
    void reset() { s1 = s2 = 0.0f; }

    // g = tan(pi fc / fs), res 0..1, drive 0..1, bandpass = BP output instead of LP
    float process (float x, float g, float res, float drive, bool bandpass)
    {
        const float k = 2.0f - 1.97f * res;                  // damping: 2 (none) .. 0.03 (near self-oscillation)
        x *= 1.0f + 3.0f * drive;
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2) * h;
        float v1 = g * hp;
        const float bp = sat (v1 + s1);
        s1 = sat (bp + v1);
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        if (! (std::abs (s1) < 1.0e4f) || ! (std::abs (s2) < 1.0e4f)) reset();
        const float out = bandpass ? bp * k : lp;
        return out / (1.0f + 1.5f * drive);
    }

private:
    // integrators clip a little harder on one side (the "op-amp as integrator" character)
    static float sat (float v) { return v > 0.0f ? 1.6f * tanhFast (v / 1.6f) : 1.4f * tanhFast (v / 1.4f); }
    float s1 = 0.0f, s2 = 0.0f;
};

//==============================================================================
// Lets an effect sleep while nothing comes in and its tail has died away (idle CPU)
struct Sleeper
{
    double quiet = 0.0;
    bool asleep (const float* L, const float* R, int n, double sleepAfterSamples)
    {
        float m = 0.0f;
        for (int k = 0; k < n; ++k) m = std::max (m, std::max (std::abs (L[k]), std::abs (R[k])));
        if (m > 1.0e-6f) { quiet = 0.0; return false; }
        return quiet > sleepAfterSamples;
    }
    void heard (const float* L, const float* R, int n)
    {
        float m = 0.0f;
        for (int k = 0; k < n; ++k) m = std::max (m, std::max (std::abs (L[k]), std::abs (R[k])));
        quiet = m > 1.0e-6f ? 0.0 : quiet + n;
    }
};

//==============================================================================
// Delay line with cubic (Hermite) interpolation
class DelayLine
{
public:
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.0f); w = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) % (int) buf.size(); }
    float read (double delaySamples) const   // delay >= 1
    {
        const int n = (int) buf.size();
        delaySamples = std::max (1.0, std::min (delaySamples, (double) n - 4.0));
        const double pos = (double) w - delaySamples;
        const int i = (int) std::floor (pos);
        const float f = (float) (pos - i);
        auto at = [&] (int k) { return buf[(size_t) (((i + k) % n + n) % n)]; };
        const float y0 = at (-1), y1 = at (0), y2 = at (1), y3 = at (2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    int size() const { return (int) buf.size(); }

private:
    std::vector<float> buf;
    int w = 0;
};

//==============================================================================
// Two-grain pitch shifter (crossfaded read heads on a short delay)
class PitchShifter
{
public:
    void prepare (double fs) { sr = fs; line.prepare ((int) (fs * 0.2)); win = 0.06 * fs; phase = 0.0; }
    void reset() { line.reset(); phase = 0.0; }
    float process (float x, float semitones)
    {
        line.push (x);
        const double ratio = std::exp2 (semitones / 12.0);
        phase += (1.0 - ratio) / win;
        phase -= std::floor (phase);
        const double p2 = phase + 0.5 - std::floor (phase + 0.5);
        const float a = (float) std::sin (pi * phase), b = (float) std::sin (pi * p2);
        return line.read (1.0 + phase * win) * a * a + line.read (1.0 + p2 * win) * b * b;
    }

private:
    DelayLine line;
    double sr = 48000.0, win = 2880.0, phase = 0.0;
};

//==============================================================================
// ECHO: two delay lines with cross feedback (left feeds right and back), soft limiting and damping in the loop
// (self-oscillates above ~55 % feedback without blowing up). The time of each line wobbles from an internal LFO
// or from its own output (SELF: stable resonances turn unstable). TO VOICES: the wet signal for the GRAVITY network.
class Echo
{
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& l : lines) l.prepare ((int) (fs * 2.2));
        reset();
    }
    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : damp) d.reset();
        fbL = fbR = 0.0f; selfMod[0] = selfMod[1] = 0.0f; lfo = 0.0; wet = 0.0f;
        timeR[0].reset ((float) (0.3 * sr)); timeR[1].reset ((float) (0.45 * sr));
        primed = false;
    }

    struct Settings { float time1 = 0.3f, time2 = 0.45f, feedback = 0.3f, mix = 0.0f, mod = 0.0f, dust = 0.0f; bool self = true; };

    void process (float* L, float* R, int n, const Settings& s, Rng& rng, float* wetOut = nullptr)
    {
        if (sleeper.asleep (L, R, n, 0.25 * sr))
        {
            if (wetOut != nullptr) std::fill (wetOut, wetOut + n, 0.0f);
            wet = 0.0f;
            return;
        }
        const float t1 = (float) (s.time1 * sr), t2 = (float) (s.time2 * sr);
        if (primed) { timeR[0].set (t1, n); timeR[1].set (t2, n); } else { timeR[0].reset (t1); timeR[1].reset (t2); primed = true; }
        const float fb = 1.25f * s.feedback;
        const float dampC = OnePole::coefFor (7000.0 - 4500.0 * s.dust, sr);
        const double lfoInc = 0.23 / sr;
        for (int k = 0; k < n; ++k)
        {
            lfo += lfoInc; lfo -= std::floor (lfo);
            const float lf = (float) std::sin (twoPi * lfo);
            const float m0 = s.self ? selfMod[0] : lf, m1 = s.self ? selfMod[1] : -lf;
            const double d0 = std::max (2.0, (double) timeR[0].next() * (1.0 + 0.35 * s.mod * m0));
            const double d1 = std::max (2.0, (double) timeR[1].next() * (1.0 + 0.35 * s.mod * m1));
            const float y0 = lines[0].read (d0), y1 = lines[1].read (d1);
            selfMod[0] += 0.002f * (y0 - selfMod[0]);
            selfMod[1] += 0.002f * (y1 - selfMod[1]);
            // cross feedback with damping and a soft limiter; DUST adds hiss in the loop
            const float in0 = L[k] + damp[0].lp (tanhFast (fb * y1), dampC) + s.dust * 0.003f * rng.next();
            const float in1 = R[k] + damp[1].lp (tanhFast (fb * y0), dampC);
            lines[0].push (in0);
            lines[1].push (in1);
            wet = 0.5f * (y0 + y1);
            if (wetOut != nullptr) wetOut[k] = wet;
            L[k] = L[k] * (1.0f - 0.5f * s.mix) + s.mix * y0;
            R[k] = R[k] * (1.0f - 0.5f * s.mix) + s.mix * y1;
            wetBuf[k & 255] = std::abs (y0) + std::abs (y1);
        }
        float m = 0.0f;
        for (int k = 0; k < std::min (n, 256); ++k) m = std::max (m, wetBuf[k]);
        sleeper.quiet = m > 1.0e-6f ? 0.0 : sleeper.quiet + n;
    }

    float lastWet() const { return wet; }

private:
    double sr = 48000.0, lfo = 0.0;
    DelayLine lines[2];
    OnePole damp[2];
    Ramp timeR[2];
    float fbL = 0.0f, fbR = 0.0f, selfMod[2] {}, wet = 0.0f, wetBuf[256] {};
    bool primed = false;
    Sleeper sleeper;
};

//==============================================================================
// 8-line feedback delay network reverb (Householder mix), modulated lines, damping. Used by AURORA, VOID, SATURN.
class Fdn
{
public:
    void prepare (double fs)
    {
        sr = fs;
        static const double lens[8] = { 0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0617, 0.0683, 0.0743 };
        for (int i = 0; i < 8; ++i) { lines[i].prepare ((int) (fs * 0.2)); base[i] = lens[i] * fs; }
        pre.prepare ((int) (fs * 0.6));
        reset();
    }
    void reset() { for (auto& l : lines) l.reset(); for (auto& d : damp) d.reset(); pre.reset(); for (auto& s : state) s = 0.0f; lfo = 0.0; }

    // size 0.3..2 scales the lines; decay seconds (>= 60 = freeze); predelay seconds; inject = extra signal into the loop
    void process (float inL, float inR, float& outL, float& outR, float size, float decay, float predelay, float preMod,
                  float damping, float inject = 0.0f)
    {
        lfo += 0.31 / sr; lfo -= std::floor (lfo);
        pre.push (0.5f * (inL + inR));
        const double pd = std::max (1.0, (double) predelay * sr * (1.0 + 0.3 * preMod * std::sin (twoPi * lfo * 0.37)));
        const float x = pre.read (pd) + inject;
        float y[8];
        for (int i = 0; i < 8; ++i)
        {
            const double wob = 1.0 + 0.004 * std::sin (twoPi * (lfo + i * 0.125));
            y[i] = lines[i].read (base[i] * size * wob);
        }
        const bool freeze = decay >= 59.0f;
        float sum = 0.0f;
        for (int i = 0; i < 8; ++i) sum += y[i];
        sum *= 0.25f;   // Householder: y - 2/N * sum
        for (int i = 0; i < 8; ++i)
        {
            const double len = base[i] * size / sr;
            const float g = freeze ? 1.0f : (float) std::pow (10.0, -3.0 * len / std::max (0.05, (double) decay));
            float v = (y[i] - sum) * g;
            if (! freeze) v = damp[i].lp (v, damping);
            lines[i].push ((freeze ? 0.0f : x * (i % 2 ? 0.35f : -0.35f)) + v);
        }
        outL = (y[0] - y[2] + y[4] - y[6]) * 0.5f;
        outR = (y[1] - y[3] + y[5] - y[7]) * 0.5f;
    }

private:
    double sr = 48000.0, lfo = 0.0, base[8] {};
    DelayLine lines[8], pre;
    OnePole damp[8];
    float state[8] {};
};

//==============================================================================
// SPACE: five algorithms with three shared controls X / Y / Z and BLEND (dry / wet).
//  AURORA  X octave up, Y octave down, Z decay       - shimmer reverb (pitch shifters in the loop)
//  VOID    X pre-delay, Y pre-delay wobble, Z decay   - huge dark space, FREEZE at the top of Z
//  RUNNER  X feedback, Y time, Z pitch per repeat     - repeats that rise or fall
//  SATURN  X ring frequency, Y stepping rate, Z room  - metallic ring modulation with a small room
//  COMET   X feedback, Y time, Z wobble               - dirty, saturated, self-oscillating tape-like delay
class Space
{
public:
    enum Algo { Aurora = 0, Void, Runner, Saturn, Comet };

    void prepare (double fs)
    {
        sr = fs;
        fdn.prepare (fs);
        for (auto& p : shiftUp) p.prepare (fs);
        for (auto& p : shiftDown) p.prepare (fs);
        for (auto& p : runShift) p.prepare (fs);
        for (auto& l : delay) l.prepare ((int) (fs * 2.2));
        reset();
    }
    void reset()
    {
        fdn.reset();
        for (auto& p : shiftUp) p.reset();
        for (auto& p : shiftDown) p.reset();
        for (auto& p : runShift) p.reset();
        for (auto& l : delay) l.reset();
        for (auto& d : damp) d.reset();
        fbL = fbR = 0.0f; ringPhase = 0.0; shPhase = 0.0; ringHz = 200.0; wob = 0.0; lastAlgo = -1;
        xR.reset (0.0f); yR.reset (0.0f); zR.reset (0.0f); bR.reset (0.0f);
        primed = false;
    }

    void process (float* L, float* R, int n, int algo, float x, float y, float z, float blend, float dust, Rng& rng)
    {
        if (algo != lastAlgo) { if (lastAlgo >= 0) reset(); lastAlgo = algo; }
        if (blend <= 0.0f && ! primed) return;
        if (sleeper.asleep (L, R, n, 0.25 * sr)) return;
        float tail = 0.0f;
        if (primed) { xR.set (x, n); yR.set (y, n); zR.set (z, n); bR.set (blend, n); }
        else { xR.reset (x); yR.reset (y); zR.reset (z); bR.reset (blend); primed = true; }
        const float dampC = OnePole::coefFor (9000.0 - 5000.0 * dust, sr);
        for (int k = 0; k < n; ++k)
        {
            const float X = xR.next(), Y = yR.next(), Z = zR.next(), B = bR.next();
            float wl = 0.0f, wr = 0.0f;
            const float dl = L[k], dr = R[k];
            switch (algo)
            {
                case Aurora:
                {
                    // shimmer: the reverb output is pitch shifted and fed back into the loop
                    const float inject = 0.35f * (X * shiftUp[0].process (fbL, 12.0f) + Y * shiftDown[0].process (fbL, -12.0f));
                    fdn.process (dl, dr, wl, wr, 1.2f, 1.0f + 14.0f * Z * Z, 0.02f, 0.0f, OnePole::coefFor (6500.0, sr), inject);
                    fbL = 0.5f * (wl + wr);
                    break;
                }
                case Void:
                    fdn.process (dl, dr, wl, wr, 2.0f, Z > 0.97f ? 60.0f : 2.0f + 28.0f * Z * Z, 0.5f * X, Y, OnePole::coefFor (3500.0, sr));
                    break;
                case Runner:
                {
                    const double t = (0.06 + 1.9 * Y * Y) * sr;
                    const float semis = -12.0f + 24.0f * Z;
                    const float r0 = delay[0].read (t), r1 = delay[1].read (t * 1.003);
                    delay[0].push (dl + tanhFast (0.95f * X * runShift[0].process (r0, semis)));
                    delay[1].push (dr + tanhFast (0.95f * X * runShift[1].process (r1, semis)));
                    wl = r0; wr = r1;
                    break;
                }
                case Saturn:
                {
                    // ring frequency jumps to a new random value at the stepping rate (Y = 0: steady)
                    const double target = 20.0 * std::pow (100.0, (double) X);
                    if (Y > 0.01f)
                    {
                        shPhase += (0.2 + 15.0 * Y * Y) / sr;
                        if (shPhase >= 1.0) { shPhase -= 1.0; ringHz = target * std::exp2 ((double) rng.next() * 1.5); }
                    }
                    else ringHz = target;
                    ringPhase += ringHz / sr; ringPhase -= std::floor (ringPhase);
                    const float c = (float) std::sin (twoPi * ringPhase);
                    float rl = 0.0f, rr = 0.0f;
                    fdn.process (dl * c, dr * c, rl, rr, 0.5f, 0.6f + 2.5f * Z, 0.005f, 0.0f, OnePole::coefFor (8000.0, sr));
                    wl = dl * c * (1.0f - 0.5f * Z) + rl * Z;
                    wr = dr * c * (1.0f - 0.5f * Z) + rr * Z;
                    break;
                }
                default:   // Comet
                {
                    wob += (0.6 + 4.0 * Z) / sr; wob -= std::floor (wob);
                    const double t = (0.04 + 1.0 * Y * Y) * sr * (1.0 + 0.012 * Z * std::sin (twoPi * wob) + 0.004 * Z * rng.next());
                    const float r0 = delay[0].read (t), r1 = delay[1].read (t * 1.01);
                    const float fb = 1.3f * X;
                    delay[0].push (dl + damp[0].lp (tanhFast (1.6f * fb * r1), dampC) + 0.002f * dust * rng.next());
                    delay[1].push (dr + damp[1].lp (tanhFast (1.6f * fb * r0), dampC));
                    wl = r0; wr = r1;
                    break;
                }
            }
            if (! std::isfinite (wl) || ! std::isfinite (wr)) { reset(); wl = wr = 0.0f; }
            tail = std::max (tail, std::max (std::abs (wl), std::abs (wr)));
            L[k] = dl * (1.0f - B) + wl * B * 1.2f;
            R[k] = dr * (1.0f - B) + wr * B * 1.2f;
        }
        sleeper.quiet = tail > 1.0e-6f ? 0.0 : sleeper.quiet + n;
    }

private:
    double sr = 48000.0, ringPhase = 0.0, shPhase = 0.0, ringHz = 200.0, wob = 0.0;
    Fdn fdn;
    PitchShifter shiftUp[1], shiftDown[1], runShift[2];
    DelayLine delay[2];
    OnePole damp[2];
    Ramp xR, yR, zR, bR;
    float fbL = 0.0f, fbR = 0.0f;
    int lastAlgo = -1;
    bool primed = false;
    Sleeper sleeper;
};
}

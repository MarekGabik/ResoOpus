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
// FILTER with five analog-style models. All take g = tan(pi fc / fs), res 0..1, drive 0..1 and give
// low pass / band pass / high pass. Runs in the oversampled domain.
//   GROWL  aggressive 12 dB state variable with saturating (asymmetric) integrators
//   LADDER transistor ladder, 24 dB: round, thins the lows a little at high resonance, sings at the top
//   DIODE  diode ladder, coupled stages: rubbery "squelch", the resonance bites more than it sings
//   SCREAM 12 dB with a clipped resonance path: harsh, screaming, screeches when pushed
//   SILK   clean, linear 12 dB state variable, only the input is softly limited
class AnalogFilter
{
public:
    enum Model { Growl = 0, Ladder, Diode, Scream, Silk, numModels };
    enum Mode { LowPass = 0, BandPass, HighPass };

    void reset() { s1 = s2 = 0.0f; for (auto& z : st) z = 0.0f; for (auto& z : dz) z = 0.0; dz4 = 0.0; clipPrev = 0.0f; }

    float process (float x, float g, float res, float drive, int mode, int model)
    {
        float out = 0.0f;
        switch (model)
        {
            case Ladder: out = ladder (x * (1.0f + 3.0f * drive), g, res, mode); break;
            case Diode:  out = diode (x * (1.0f + 3.0f * drive), g, res, mode); break;
            case Scream: out = scream (x * (1.0f + 4.0f * drive), g, res, mode); break;
            case Silk:   out = silk (x * (1.0f + 2.0f * drive), g, res, mode); break;
            default:     out = growl (x * (1.0f + 3.0f * drive), g, res, mode); break;
        }
        if (! (std::abs (out) < 1.0e4f)) { reset(); out = 0.0f; }
        return out / (1.0f + 1.5f * drive);
    }

private:
    // integrators clip a little harder on one side (the "op-amp as integrator" character)
    static float sat (float v) { return v > 0.0f ? 1.6f * tanhFast (v / 1.6f) : 1.4f * tanhFast (v / 1.4f); }

    float growl (float x, float g, float res, int mode)
    {
        const float k = 2.0f - 1.97f * res;                  // damping: 2 (none) .. 0.03 (near self-oscillation)
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2) * h;
        const float v1 = g * hp;
        const float bp = sat (v1 + s1);
        s1 = sat (bp + v1);
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        // band pass: unity peak at low resonance; the saturated peak cannot grow, so the gain falls only slowly with
        // the damping (a narrow band keeps its level instead of fading away)
        return mode == BandPass ? 2.0f * bp * std::pow (0.5f * k, 0.25f) : (mode == HighPass ? hp : lp);
    }

    float silk (float x, float g, float res, int mode)
    {
        x = 1.5f * tanhFast (x / 1.5f);
        const float k = 2.0f - 1.9f * res;
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2) * h;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        const float y = mode == BandPass ? 2.0f * bp * std::pow (0.5f * k, 0.4f) : (mode == HighPass ? hp : lp);
        return 1.2f * tanhFast (y / 1.2f);   // a harmonic right on the resonance must not jump out
    }

    // 12 dB with a clipped resonance path: the damping is split into a linear part and a clipped positive feedback
    // (one sample late), so the resonance compresses and screams instead of ringing cleanly
    float scream (float x, float g, float res, int mode)
    {
        const float fbk = 2.12f * std::pow (res, 1.6f);    // clipped feedback, beyond 2 = self-oscillation
        const float k = 2.0f;
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2 + fbk * clipPrev) * h;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        clipPrev = diodeClip (bp);
        const float comp = 1.0f / (1.0f + 0.6f * fbk);
        const float y = mode == BandPass ? 2.0f * bp * comp : (mode == HighPass ? hp : lp);
        return 1.3f * tanhFast (y / 1.3f);
    }
    static float diodeClip (float v) { return v / std::sqrt (1.0f + 2.5f * v * v); }

    // transistor ladder: four one-pole TPT stages, the feedback loop solved without delay, tanh at the input;
    // band and high pass by mixing the stage outputs
    float ladder (float x, float g, float res, int mode)
    {
        const float k = 4.1f * res;
        const float G = g / (1.0f + g);
        const float b = 1.0f / (1.0f + g);
        const float S = G * G * G * st[0] * b + G * G * st[1] * b + G * st[2] * b + st[3] * b;
        float u = (x * (1.0f + 0.6f * k) - k * S) / (1.0f + k * G * G * G * G);
        u = tanhFast (u);
        float y[4];
        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - st[i]) * G;
            y[i] = v + st[i];
            st[i] = y[i] + v;
            in = y[i];
        }
        // the input gain that keeps the lows of the low pass is taken back out of band and high pass
        const float back = 1.0f / (1.0f + 0.6f * k);
        if (mode == BandPass) return 4.0f * (y[1] - 2.0f * y[2] + y[3]) * std::sqrt (back) / (1.0f + 0.3f * k);
        if (mode == HighPass) return (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]) * back;
        return y[3];
    }

    // diode ladder (coupled stages, after D. Wurtz's model) with a high pass in the feedback path
    float diode (float x, float g, float res, int mode)
    {
        // double precision: at low cutoffs the stage solve divides by a tiny a
        const double k = 19.0 * res;
        const double a = g, ainv = 1.0 / a, a2 = a * a, bb = 2.0 * a + 1.0, b2 = bb * bb;
        const double c = 1.0 / (2.0 * a2 * a2 - 4.0 * a2 * b2 + b2 * b2);
        const double g0 = 2.0 * a2 * a2 * c;
        const double gg = g0 * fbB;
        const double s0 = (a2 * a * dz[0] + a2 * bb * dz[1] + dz[2] * (b2 - 2.0 * a2) * a + dz[3] * (b2 - 3.0 * a2) * bb) * c;
        const double s = fbB * s0 - dz4;
        double y5 = (gg * x + s) / (1.0 + gg * k);
        const double y0 = softClip ((float) (x - k * y5));
        y5 = gg * y0 + s;
        const double y4 = g0 * y0 + s0;
        const double y3 = (bb * y4 - dz[3]) * ainv;
        const double y2 = (bb * y3 - a * y4 - dz[2]) * ainv;
        const double y1 = (bb * y2 - a * y3 - dz[1]) * ainv;
        dz[0] += 4.0 * a * (y0 - y1 + y2);
        dz[1] += 2.0 * a * (y1 - 2.0 * y2 + y3);
        dz[2] += 2.0 * a * (y2 - 2.0 * y3 + y4);
        dz[3] += 2.0 * a * (y3 - 2.0 * y4);
        dz4 = fbB * y4 + fbA * y5;
        // stage outputs normalised to their DC gains (about 4 : 3 : 2 : 1) and mixed for band and high pass
        const double n1 = y1 * (1.0 / 3.8), n2 = y2 * (1.0 / 2.85);
        if (mode == BandPass) return (float) (5.8 * (n1 - n2));
        if (mode == HighPass)
        {
            // the coupled stages only give a gentle high pass: a 12 dB state variable high pass at the cutoff follows
            const float x2 = (float) (y0 - 2.0 * n1 + n2);
            const float kk = 1.4f, h = 1.0f / (1.0f + g * (g + kk));
            const float hp = (x2 - (kk + g) * s1 - s2) * h;
            const float v1 = g * hp, bp = v1 + s1;
            s1 = bp + v1;
            const float v2 = g * bp, lp = v2 + s2;
            s2 = lp + v2;
            return hp;
        }
        return (float) (y4 * (1.0 + 0.5 * k));
    }

    float s1 = 0.0f, s2 = 0.0f, clipPrev = 0.0f;
    float st[4] {};
    double dz[4] {}, dz4 = 0.0;
public:
    // diode ladder feedback high pass (cutoff in Hz at the running sample rate)
    void prepare (double fs) { const double K = 2.0 * pi * 90.0 / fs; fbA = (K - 2.0) / (K + 2.0); fbB = 2.0 / (K + 2.0); }
private:
    double fbA = -0.99, fbB = 1.0;
};

// Soft ceiling for signals inside a feedback loop: transparent when quiet, never louder than c
inline float ceilingClip (float x, float c) { return c * tanhFast (x / c); }

//==============================================================================
// Slowly keeps the left / right balance of a wet signal equal to the balance of its input (feedback loops drift
// to one side), +-4 dB at most
struct AutoBalance
{
    float eL = 1.0e-6f, eR = 1.0e-6f, iL = 1.0e-6f, iR = 1.0e-6f, gL = 1.0f, gR = 1.0f, c = 0.0001f, gc = 0.0001f;
    void prepare (double fs) { c = (float) (1.0 - std::exp (-1.0 / (1.5 * fs))); gc = (float) (1.0 - std::exp (-1.0 / (0.5 * fs))); }
    void reset() { eL = eR = iL = iR = 1.0e-6f; gL = gR = 1.0f; }
    void process (float& l, float& r, float inL, float inR)
    {
        eL += c * (l * l - eL);
        eR += c * (r * r - eR);
        iL += c * (inL * inL - iL);
        iR += c * (inR * inR - iR);
        // share of the energy each side should get (from the input), spread over the wet total
        const float total = eL + eR, inTotal = std::max (1.0e-12f, iL + iR);
        const float wantL = total * iL / inTotal, wantR = total * iR / inTotal;
        const float tl = clampf (std::sqrt (wantL / std::max (1.0e-12f, eL)), 0.63f, 1.58f);
        const float tr = clampf (std::sqrt (wantR / std::max (1.0e-12f, eR)), 0.63f, 1.58f);
        gL += gc * (tl - gL);
        gR += gc * (tr - gR);
        l *= gL;
        r *= gR;
    }
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
    // Two grains half a window apart (sin^2 + cos^2 = 1). More grains sound smoother on paper but cancel the
    // shifted tone itself and leave sidebands a grain-rate x grains away (measured: 4 grains put +12 st of 220 Hz
    // at 420 / 470 Hz); with two the sidebands sit at +-grain rate, heard as a gentle shimmer on the right pitch.
    // A 90 ms window keeps that rate low (an octave up: 11 Hz)
    void prepare (double fs) { sr = fs; line.prepare ((int) (fs * 0.2)); win = 0.09 * fs; phase = 0.0; }
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
    double sr = 48000.0, win = 4320.0, phase = 0.0;
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
        balance.prepare (fs);
        reset();
    }
    void reset()
    {
        balance.reset();
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
        // the more the loop holds, the more its output is turned down (steady-state gain of the loop)
        const float g = std::min (0.98f, fb);
        const float comp = std::sqrt (1.0f - g * g);
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
            const float in0 = L[k] + damp[0].lp (ceilingClip (fb * y1, 0.5f), dampC) + s.dust * 0.003f * rng.next();
            const float in1 = R[k] + damp[1].lp (ceilingClip (fb * y0, 0.5f), dampC);
            lines[0].push (in0);
            lines[1].push (in1);
            float wl = y0 * comp, wr = y1 * comp;
            balance.process (wl, wr, L[k], R[k]);
            wet = 0.5f * (wl + wr);
            if (wetOut != nullptr) wetOut[k] = wet;
            L[k] = L[k] * (1.0f - s.mix) + s.mix * wl;
            R[k] = R[k] * (1.0f - s.mix) + s.mix * wr;
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
    AutoBalance balance;
};

//==============================================================================
// Schroeder all-pass: smears a sound in time without colouring it (diffusion)
struct Allpass
{
    std::vector<float> buf;
    int w = 0;
    float g = 0.6f;
    void prepare (double fs, double ms, float gain) { buf.assign ((size_t) std::max (1, (int) (ms * 0.001 * fs)), 0.0f); w = 0; g = gain; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    float process (float x)
    {
        const float d = buf[(size_t) w];
        const float v = x + g * d;
        buf[(size_t) w] = v;
        if (++w >= (int) buf.size()) w = 0;
        return d - g * v;
    }
};

// Band pass with a constant 0 dB peak (RBJ), for ringing modes
struct Resonator
{
    float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f, x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    void reset() { x1 = x2 = y1 = y2 = 0.0f; }
    void set (double hz, double q, double fs)
    {
        const double w = twoPi * std::min (hz, 0.45 * fs) / fs, alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
        b0 = (float) (alpha / a0); a1 = (float) (-2.0 * std::cos (w) / a0); a2 = (float) ((1.0 - alpha) / a0);
    }
    float process (float x)
    {
        const float y = b0 * (x - x2) - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
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
        pre.prepare ((int) (fs * 0.7));   // 0.5 s pre-delay + 30 % wobble
        reset();
    }
    void reset() { for (auto& l : lines) l.reset(); for (auto& d : damp) d.reset(); pre.reset(); for (auto& s : state) s = 0.0f; lfo = 0.0; preLfo = 0.0; pdS = -1.0; }

    // size 0.3..2 scales the lines; decay seconds (>= 60 = freeze); predelay seconds; inject = extra signal into the loop
    void process (float inL, float inR, float& outL, float& outR, float size, float decay, float predelay, float preMod,
                  float damping, float inject = 0.0f)
    {
        lfo += 0.31 / sr; lfo -= std::floor (lfo);
        preLfo += 0.115 / sr; preLfo -= std::floor (preLfo);   // own phase: a wrapped sin (lfo * k) would jump the delay time
        pre.push (0.5f * (inL + inR));
        // the pre-delay glides to a new length (~0.15 s) like tape, so turning the knob does not click
        const double target = std::max (1.0, (double) predelay * sr * (1.0 + 0.3 * preMod * std::sin (twoPi * preLfo)));
        pdS = pdS < 0.0 ? target : pdS + (target - pdS) * (1.0 - std::exp (-1.0 / (0.15 * sr)));
        const double pd = pdS;
        const float x = pre.read (pd) + inject;
        float y[8];
        for (int i = 0; i < 8; ++i)
        {
            const double wob = 1.0 + modDepth * std::sin (twoPi * (lfo + i * 0.125));
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
public:
    double modDepth = 0.004;   // chorus of the lines (AURORA a little more: lusher)
private:
    double sr = 48000.0, lfo = 0.0, preLfo = 0.0, pdS = -1.0, base[8] {};
    DelayLine lines[8], pre;
    OnePole damp[8];
    float state[8] {};
};

//==============================================================================
// SPACE: five algorithms with three shared controls X / Y / Z and BLEND (dry / wet).
//  AURORA  X octave up, Y octave down, Z decay       - airy shimmer: diffused input, soft four-grain octaves, long bright tail
//  VOID    X pre-delay, Y pre-delay wobble, Z decay   - huge dark space, FREEZE at the top of Z
//  RUNNER  X feedback, Y time, Z pitch per repeat     - a staircase in space: ping-pong repeats that step by musical
//                                                       intervals (-12 -7 -5 0 +5 +7 +12) and melt into a small room
//  SATURN  X ring frequency, Y stepping rate, Z room  - bells and bowls: ring modulation by two harmonic sines rings
//                                                       three bowl modes; STEPS jumps by pentatonic notes
//  COMET   X feedback, Y time, Z wow & flutter        - tape echo: saturation and a darkening, thinning loop, smooth wow,
//                                                       flutter and drift, hiss from DUST
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
        static const double diffMs[4] = { 4.7, 7.3, 11.1, 16.9 };
        for (int i = 0; i < 4; ++i) diffuse[i].prepare (fs, diffMs[i], 0.62f);
        static const double runMs[2][2] = { { 3.1, 7.9 }, { 4.3, 9.7 } };
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 2; ++i) runDiff[c][i].prepare (fs, runMs[c][i], 0.5f);
        balance.prepare (fs);
        reset();
    }
    void reset()
    {
        balance.reset();
        fdn.reset();
        fdn.modDepth = 0.004;
        for (auto& p : shiftUp) p.reset();
        for (auto& p : shiftDown) p.reset();
        for (auto& p : runShift) p.reset();
        for (auto& l : delay) l.reset();
        for (auto& d : damp) d.reset();
        for (auto& d : low) d.reset();
        for (auto& a : diffuse) a.reset();
        for (auto& c : runDiff) for (auto& a : c) a.reset();
        for (auto& c : bowl) for (auto& r : c) r.reset();
        inHp.reset(); shimLp.reset(); shimLp2.reset(); shimHp.reset();
        fbL = fbR = 0.0f; ringPhase = ringPhase2 = 0.0; shPhase = 0.0; ringHz = ringTarget = 200.0; bowlHz = -1.0;
        wob = flutter = 0.0; driftS = driftTarget = 0.0f; driftCount = 0; lastAlgo = -1;
        inEnv = loopEnv = 0.0f; duck = 1.0f;
        xR.reset (0.0f); yR.reset (0.0f); zR.reset (0.0f); bR.reset (0.0f);
        primed = false;
    }

    // RUNNER: Z -> semitones per repeat. Seven musical steps, each held over most of its range (a snapped plateau)
    // with a short glide to the next, so the knob both clicks into intervals and can sweep
    static float runnerSemitones (float z)
    {
        static const float steps[7] = { -12.0f, -7.0f, -5.0f, 0.0f, 5.0f, 7.0f, 12.0f };
        const float u = clampf (z, 0.0f, 1.0f) * 6.0f;
        const int i = std::min (5, (int) u);
        const float f = clampf ((u - (float) i - 0.35f) / 0.3f, 0.0f, 1.0f);
        const float s = f * f * (3.0f - 2.0f * f);
        return steps[i] + (steps[i + 1] - steps[i]) * s;
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
        const float envC = (float) (1.0 - std::exp (-1.0 / (0.3 * sr))), duckC = (float) (1.0 - std::exp (-1.0 / (0.05 * sr)));
        fdn.modDepth = algo == Aurora ? 0.007 : 0.004;
        for (int k = 0; k < n; ++k)
        {
            const float X = xR.next(), Y = yR.next(), Z = zR.next(), B = bR.next();
            float wl = 0.0f, wr = 0.0f;
            const float dl = L[k], dr = R[k];
            // a feedback loop may be at most ~6 dB above what comes in (or -30 dBFS on its own), then it takes its
            // own feedback back (RUNNER, COMET)
            auto loopGuard = [&] (float a, float b)
            {
                inEnv += envC * (std::max (std::abs (dl), std::abs (dr)) - inEnv);
                loopEnv += envC * (std::max (std::abs (a), std::abs (b)) - loopEnv);
                const float limit = std::max (2.0f * inEnv, 0.03f);
                duck += duckC * ((loopEnv > limit ? limit / loopEnv : 1.0f) - duck);
                return duck;
            };
            switch (algo)
            {
                case Aurora:
                {
                    // light, airy shimmer: the input loses its lows (150 Hz) and its attacks (four all-passes), the
                    // octaves come from a soft four-grain shifter, are kept between 70 Hz and 5.5 kHz, and float
                    // back into a long, gently chorused tail where they keep rising (up) or sinking (down)
                    float in = 0.5f * (dl + dr);
                    in -= inHp.lp (in, OnePole::coefFor (150.0, sr));
                    for (auto& a : diffuse) in = a.process (in);
                    // the octaves of what comes in go straight into the room (clear, no pile-up of ever higher
                    // octaves); the octaves of the tail itself feed back gently, so it keeps drifting up or down
                    const float direct = 0.09f * (X * shiftUp[1].process (in, 12.0f) + Y * shiftDown[1].process (in, -12.0f));
                    in += direct;
                    float shim = X * shiftUp[0].process (fbL, 12.0f) + Y * shiftDown[0].process (fbL, -12.0f);
                    shim = shimLp.lp (shim, OnePole::coefFor (5500.0, sr));
                    shim = shimLp2.lp (shim, OnePole::coefFor (5500.0, sr));
                    shim -= shimHp.lp (shim, OnePole::coefFor (70.0, sr));
                    // the loop gain of the octaves stays under 1 even with both at 100 % (X + Y share it)
                    const float inject = ceilingClip (0.2f * shim / std::max (1.0f, 0.75f * (X + Y)), 0.5f);
                    fdn.process (in, in, wl, wr, 1.5f, 1.5f + 18.0f * Z * Z, 0.035f, 0.0f, OnePole::coefFor (7500.0, sr), inject);
                    fbL = 0.5f * (wl + wr);
                    break;
                }
                case Void:
                    fdn.process (dl, dr, wl, wr, 2.0f, Z > 0.97f ? 60.0f : 2.0f + 28.0f * Z * Z, 0.5f * X, Y, OnePole::coefFor (3500.0, sr));
                    wl *= 0.88f; wr *= 0.88f;   // the long dark tail piles up: about 1 dB under the others
                    break;
                case Runner:
                {
                    // ping-pong repeats, each one pitched by the chosen interval, rounded (6.5 kHz, 120 Hz), smeared by
                    // two all-passes and soft-saturated; a small room behind them
                    const double t = (0.06 + 1.9 * Y * Y) * sr;
                    const float semis = runnerSemitones (Z);
                    const float r0 = delay[0].read (t), r1 = delay[1].read (t);
                    const float fb = 0.92f * X * loopGuard (r0, r1);
                    auto shape = [&] (int c, float v)
                    {
                        v = runShift[c].process (v, semis);
                        v = damp[c].lp (v, OnePole::coefFor (6500.0, sr));
                        v -= low[c].lp (v, OnePole::coefFor (120.0, sr));
                        for (auto& a : runDiff[c]) v = a.process (v);
                        return ceilingClip (fb * v, 0.6f);
                    };
                    delay[0].push (dl + shape (1, r1));
                    delay[1].push (dr + shape (0, r0));
                    float rl = 0.0f, rr = 0.0f;
                    fdn.process (r0, r1, rl, rr, 0.8f, 1.8f, 0.01f, 0.0f, OnePole::coefFor (6000.0, sr));
                    const float comp = 1.0f / (1.0f + 0.8f * X * X);
                    wl = (0.8f * r0 + 0.45f * rl) * comp;
                    wr = (0.8f * r1 + 0.45f * rr) * comp;
                    break;
                }
                case Saturn:
                {
                    // the ring frequency: X, or (STEPS) a new pentatonic note above it at the stepping rate, reached
                    // with a 35 ms glide so a jump does not click
                    const double base = 20.0 * std::pow (100.0, (double) X);
                    if (Y > 0.01f)
                    {
                        shPhase += (0.2 + 15.0 * Y * Y) / sr;
                        if (shPhase >= 1.0)
                        {
                            static const int penta[9] = { 0, 2, 4, 7, 9, 12, 14, 16, 19 };
                            shPhase -= 1.0;
                            ringTarget = base * std::exp2 (penta[std::min (8, (int) (rng.next01() * 9.0f))] / 12.0);
                        }
                    }
                    else ringTarget = base;
                    ringHz += (ringTarget - ringHz) * (1.0 - std::exp (-1.0 / (0.035 * sr)));
                    ringPhase += ringHz / sr; ringPhase -= std::floor (ringPhase);
                    ringPhase2 += 2.0 * ringHz / sr; ringPhase2 -= std::floor (ringPhase2);
                    // two harmonic sines (the octave quieter): rounder than one, still clean
                    const float c = (float) (0.75 * std::sin (twoPi * ringPhase) + 0.35 * std::sin (twoPi * ringPhase2));
                    // three bowl modes (ratios of a singing bowl) ring on the ring-modulated sound
                    if (std::abs (ringHz - bowlHz) > 0.002 * bowlHz || bowlHz < 0.0)
                    {
                        bowlHz = ringHz;
                        static const double ratio[3] = { 1.0, 2.71, 5.15 }, q[3] = { 90.0, 120.0, 140.0 };
                        for (int ch = 0; ch < 2; ++ch) for (int m = 0; m < 3; ++m) bowl[ch][m].set (std::max (60.0, bowlHz * ratio[m]), q[m], sr);
                        for (int m = 0; m < 3; ++m) bowlGain[m] = (float) (bowlHz * ratio[m] < 4500.0 ? 1.0 : std::max (0.0, 1.0 - (bowlHz * ratio[m] - 4500.0) / 2000.0));
                    }
                    float ring[2] = { dl * c, dr * c }, ringing[2] = { 0.0f, 0.0f };
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        ring[ch] = damp[ch].lp (ring[ch], OnePole::coefFor (7000.0, sr));
                        // modes above ~4.5 kHz fade out: bowls, not glass splinters
                        ringing[ch] = 2.2f * bowlGain[0] * bowl[ch][0].process (ring[ch]) + 1.6f * bowlGain[1] * bowl[ch][1].process (ring[ch])
                                      + 1.1f * bowlGain[2] * bowl[ch][2].process (ring[ch]);
                    }
                    float rl = 0.0f, rr = 0.0f;
                    fdn.process (ring[0] + ringing[0], ring[1] + ringing[1], rl, rr, 0.7f, 0.8f + 3.5f * Z, 0.01f, 0.0f, OnePole::coefFor (7000.0, sr));
                    const float direct = 0.55f - 0.25f * Z;
                    wl = 1.6f * (ring[0] * direct + ringing[0] * 0.6f + rl * 0.9f * Z);
                    wr = 1.6f * (ring[1] * direct + ringing[1] * 0.6f + rr * 0.9f * Z);
                    break;
                }
                default:   // Comet
                {
                    // tape: wow (0.4-1.2 Hz), flutter (7 Hz) and a slow random drift move the tape speed smoothly;
                    // each pass saturates, loses highs (DUST darker) and lows (90 Hz) like a worn loop
                    wob += (0.4 + 0.8 * Z) / sr; wob -= std::floor (wob);
                    flutter += 7.0 / sr; flutter -= std::floor (flutter);
                    if (--driftCount <= 0) { driftTarget = rng.next(); driftCount = (int) (0.3 * sr); }
                    driftS += (float) (1.0 - std::exp (-1.0 / (0.15 * sr))) * (driftTarget - driftS);
                    const double speed = 1.0 + Z * (0.009 * std::sin (twoPi * wob) + 0.0012 * std::sin (twoPi * flutter) + 0.004 * driftS);
                    const double t = (0.04 + 1.0 * Y * Y) * sr * speed;
                    const float r0 = delay[0].read (t), r1 = delay[1].read (t * 1.01);
                    const float want = X < 0.85f ? 1.153f * X : 0.98f + 0.8f * (X - 0.85f);
                    const float fb = want * loopGuard (r0, r1);
                    auto tape = [&] (int c, float v)
                    {
                        v = damp[c].lp (v, dampC);
                        v -= low[c].lp (v, OnePole::coefFor (90.0, sr));
                        return ceilingClip (1.4f * fb * v, 0.5f) / 1.4f * 1.0f;
                    };
                    delay[0].push (dl + tape (0, r1) + 0.002f * dust * rng.next());
                    delay[1].push (dr + tape (1, r0));
                    const float g = std::min (0.95f, want);
                    const float comp = std::sqrt (1.0f - g * g);
                    wl = r0 * comp; wr = r1 * comp;
                    break;
                }
            }
            if (! std::isfinite (wl) || ! std::isfinite (wr)) { reset(); wl = wr = 0.0f; }
            balance.process (wl, wr, dl, dr);
            tail = std::max (tail, std::max (std::abs (wl), std::abs (wr)));
            L[k] = dl * (1.0f - B) + wl * B * 1.2f;
            R[k] = dr * (1.0f - B) + wr * B * 1.2f;
        }
        sleeper.quiet = tail > 1.0e-6f ? 0.0 : sleeper.quiet + n;
    }

private:
    double sr = 48000.0, ringPhase = 0.0, ringPhase2 = 0.0, shPhase = 0.0, ringHz = 200.0, ringTarget = 200.0, bowlHz = -1.0, wob = 0.0, flutter = 0.0;
    Fdn fdn;
    PitchShifter shiftUp[2], shiftDown[2], runShift[2];
    DelayLine delay[2];
    OnePole damp[2], low[2], inHp, shimLp, shimLp2, shimHp;
    Allpass diffuse[4], runDiff[2][2];
    Resonator bowl[2][3];
    float bowlGain[3] { 1.0f, 1.0f, 1.0f };
    Ramp xR, yR, zR, bR;
    float fbL = 0.0f, fbR = 0.0f, inEnv = 0.0f, loopEnv = 0.0f, duck = 1.0f, driftS = 0.0f, driftTarget = 0.0f;
    int lastAlgo = -1, driftCount = 0;
    bool primed = false;
    Sleeper sleeper;
    AutoBalance balance;
};
}

#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

// Small, allocation-free DSP building blocks. No JUCE dependency, safe for the audio thread.
namespace dsp
{
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double mtof (double midi) { return 440.0 * std::exp2 ((midi - 69.0) / 12.0); }

// Fast xorshift random, -1..1
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) ((double) s * (2.0 / 4294967295.0) - 1.0);
    }
    float next01() { return 0.5f * (next() + 1.0f); }
};

// Linear ramp toward a target over a fixed number of samples (per-chunk parameter smoothing)
struct Ramp
{
    float v = 0.0f, inc = 0.0f;
    int left = 0;
    void reset (float x) { v = x; inc = 0.0f; left = 0; }
    void set (float target, int samples)
    {
        if (samples <= 0) { reset (target); return; }
        inc = (target - v) / (float) samples;
        left = samples;
    }
    float next()
    {
        if (left > 0) { v += inc; --left; }
        return v;
    }
};

//==============================================================================
// Oscillator with continuously morphing shape (Sine, Triangle, Sharktooth, Saw, Square) and duty cycle
// on every shape. Discontinuities are corrected with 4-point BLEP (steps) and BLAMP (kinks) residuals
// derived from the cubic B-spline (aliases fall off as sinc^4 instead of sinc^2 of the 2-point PolyBLEP).
// The output is delayed by two samples so corrections reach two samples on each side of an event.
// This also makes hard sync band-limited.
class ShapeOsc
{
public:
    void reset (double startPhase = 0.0)
    {
        phase = wrap01 (startPhase);
        q0 = q1 = 0.0;
        acc[0] = acc[1] = acc[2] = acc[3] = 0.0;
        primed = false;
    }

    void setShape (float morph, float duty)
    {
        m = clampf (morph, 0.0f, 4.0f);
        d = clampf (duty, 0.03f, 0.97f);
    }

    double getPhase() const { return phase; }
    void shift (double delta) { phase = wrap01 (phase + delta); }

    // Advances one sample. Returns the output (delayed by one sample).
    // wrapped: set when the oscillator completed a cycle; wrapX = distance in samples from the wrap to now.
    float tick (double dt, bool& wrapped, double& wrapX) { return tickInternal (dt, false, 0.0, 0.0, wrapped, wrapX); }

    // Same, but restarts at resetPhase when the master wrapped (hard sync), masterX samples ago.
    float tickSynced (double dt, double masterX, double resetPhase)
    {
        bool w; double x;
        return tickInternal (dt, true, masterX, resetPhase, w, x);
    }

    // Shape value at phase p (no band limiting), used for the analytic events
    double value (double p) const
    {
        const int i = (int) m;
        const double f = m - i;
        double v = base (i, p);
        if (f > 1.0e-6 && i < 4) v = (1.0 - f) * v + f * base (i + 1, p);
        return v;
    }

    double slope (double p) const
    {
        const int i = (int) m;
        const double f = m - i;
        double s = baseSlope (i, p);
        if (f > 1.0e-6 && i < 4) s = (1.0 - f) * s + f * baseSlope (i + 1, p);
        return s;
    }

private:
    static double wrap01 (double p) { p -= std::floor (p); return p >= 1.0 ? 0.0 : p; }

    double warp (double p) const { return p < d ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d); }
    double dwarp (double p) const { return p < d ? 0.5 / d : 0.5 / (1.0 - d); }

    double base (int shape, double p) const
    {
        switch (shape)
        {
            case 0:  return -std::cos (twoPi * warp (p));
            case 1:  return p < d ? -1.0 + 2.0 * p / d : 1.0 - 2.0 * (p - d) / (1.0 - d);
            case 2:  return 0.5 * (base (1, p) + base (3, p));
            case 3:  return p < d ? -1.0 + p / d : (p - d) / (1.0 - d);
            default: return p < d ? -1.0 : 1.0;
        }
    }

    double baseSlope (int shape, double p) const
    {
        switch (shape)
        {
            case 0:  return twoPi * std::sin (twoPi * warp (p)) * dwarp (p);
            case 1:  return p < d ? 2.0 / d : -2.0 / (1.0 - d);
            case 2:  return 0.5 * (baseSlope (1, p) + baseSlope (3, p));
            case 3:  return p < d ? 1.0 / d : 1.0 / (1.0 - d);
            default: return 0.0;
        }
    }

    // Integrated cubic B-spline (step residual) and its integral (ramp residual), tau in samples from the event
    static double splineStep (double t)   // S(t) for -2 <= t <= 0
    {
        if (t <= -1.0) { const double u = 2.0 + t; return u * u * u * u / 24.0; }
        const double t2 = t * t;
        return 1.0 / 24.0 + 2.75 / 6.0 + (4.0 * t - 2.0 * t2 * t - 0.75 * t2 * t2) / 6.0;
    }
    static double splineRamp (double t)   // A(t) for -2 <= t <= 0
    {
        if (t <= -1.0) { const double u = 2.0 + t; return u * u * u * u * u / 120.0; }
        const double t2 = t * t;
        return 1.0 / 120.0 + 0.5 * (t + 1.0) + (2.0 * t2 - 0.5 * t2 * t2 - 0.15 * t2 * t2 * t - 1.65) / 6.0;
    }
    static double stepResidual (double t) { return t <= -2.0 || t >= 2.0 ? 0.0 : (t < 0.0 ? splineStep (t) : -splineStep (-t)); }
    static double rampResidual (double t) { return t <= -2.0 || t >= 2.0 ? 0.0 : splineRamp (-std::abs (t)); }

    // Event happened x samples before the current sample (0 <= x < 1): correct the jump between left and right limits
    void event (double leftV, double rightV, double leftS, double rightS, double dt, double x)
    {
        const double h = rightV - leftV;
        const double ds = (rightS - leftS) * dt;
        for (int j = 0; j < 4; ++j)
        {
            const double t = x + (double) (j - 2);
            acc[j] += h * stepResidual (t) + ds * rampResidual (t);
        }
    }

    void naturalEvents (double p0, double p1, double dt, double tEnd)
    {
        // events strictly inside (p0, p1]; p1 may exceed 1 (wrap); tEnd = distance from p1 to "now" in samples
        constexpr double e = 1.0e-9;
        double start = p0;
        while (start < p1)
        {
            const double cycleEnd = std::floor (start) + 1.0;
            const double base0 = std::floor (start);
            const double dPoint = base0 + d;
            if (dPoint > start && dPoint <= p1)
            {
                const double x = (p1 - dPoint) / dt + tEnd;
                if (x < 1.0)
                    event (value (d - e), value (d), slope (d - e), slope (d), dt, std::max (0.0, x));
            }
            if (cycleEnd <= p1)
            {
                const double x = (p1 - cycleEnd) / dt + tEnd;
                if (x < 1.0)
                    event (value (1.0 - e), value (0.0), slope (1.0 - e), slope (0.0), dt, std::max (0.0, x));
            }
            start = cycleEnd;
        }
    }

    float tickInternal (double dt, bool sync, double masterX, double resetPhase, bool& wrapped, double& wrapX)
    {
        dt = std::min (dt, 0.45);
        wrapped = false;
        wrapX = 0.0;

        if (sync)
        {
            constexpr double e = 1.0e-9;
            masterX = std::clamp (masterX, 0.0, 0.999999);
            const double pReset = phase + dt * (1.0 - masterX);       // phase reached when the master wrapped
            naturalEvents (phase, pReset, dt, masterX);
            const double a = wrap01 (pReset);
            const double r = wrap01 (resetPhase);
            event (value (a > e ? a - e : 1.0 - e), value (r), slope (a > e ? a - e : 1.0 - e), slope (r), dt, masterX);
            const double p1 = r + dt * masterX;
            naturalEvents (r, p1, dt, 0.0);
            phase = wrap01 (p1);
        }
        else
        {
            const double p1 = phase + dt;
            naturalEvents (phase, p1, dt, 0.0);
            if (p1 >= 1.0)
            {
                wrapped = true;
                wrapX = (p1 - 1.0) / dt;
            }
            phase = wrap01 (p1);
        }

        const double now = value (phase);
        if (! primed) { q0 = q1 = now; primed = true; }
        const double out = q0 + acc[0];
        q0 = q1;
        q1 = now;
        acc[0] = acc[1]; acc[1] = acc[2]; acc[2] = acc[3]; acc[3] = 0.0;
        return (float) out;
    }

    double phase = 0.0, q0 = 0.0, q1 = 0.0, acc[4] {};
    bool primed = false;
    double m = 3.0, d = 0.5;
};

//==============================================================================
// Zero-delay-feedback 4-pole ladder low pass with a saturating input stage (Moog style)
class Ladder
{
public:
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }

    // g = tan(pi fc / fs), k = feedback 0..~4.3
    float process (float x, float g, float k)
    {
        x *= 0.5f;   // headroom: nominal levels stay nearly linear, hot mixer levels and resonance saturate
        const float G = g / (1.0f + g);
        const float om = 1.0f - G;
        const float S = G * G * G * om * s[0] + G * G * om * s[1] + G * om * s[2] + om * s[3];
        const float G4 = G * G * G * G;
        const float y4est = (G4 * x + S) / (1.0f + k * G4);
        const float u = sat (x - k * y4est);

        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
        }
        return in * (1.0f + 0.5f * k) * 2.0f;
    }

private:
    static float sat (float x)
    {
        // smooth tanh approximation (Pade), accurate enough inside the loop
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    float s[4] {};
};

// TPT state variable filter (2 pole) with LP / BP / HP outputs
class Svf
{
public:
    void reset() { s1 = s2 = 0.0f; }
    struct Out { float lp, bp, hp; };

    // g = tan(pi fc / fs), R = 1 / (2Q)
    Out process (float x, float g, float R)
    {
        const float h = 1.0f / (1.0f + 2.0f * R * g + g * g);
        const float hp = (x - (2.0f * R + g) * s1 - s2) * h;
        const float bp = g * hp + s1;
        s1 = g * hp + bp;
        const float lp = g * bp + s2;
        s2 = g * bp + lp;
        if (! (std::abs (s1) < 1.0e6f) || ! (std::abs (s2) < 1.0e6f)) reset();   // guards against blow-ups
        return { lp, bp, hp };
    }

private:
    float s1 = 0.0f, s2 = 0.0f;
};

// Linkwitz-Riley 4th order section (two Butterworth SVFs in series)
class LR4
{
public:
    void reset() { a.reset(); b.reset(); }
    float lowpass (float x, float g)  { return b.process (a.process (x, g, 0.70710678f).lp, g, 0.70710678f).lp; }
    float highpass (float x, float g) { return b.process (a.process (x, g, 0.70710678f).hp, g, 0.70710678f).hp; }

private:
    Svf a, b;
};

struct OnePole
{
    float z = 0.0f;
    void reset (float v = 0.0f) { z = v; }
    float lp (float x, float coef) { z += coef * (x - z); return z; }   // coef = 1 - exp(-2 pi fc / fs)
    static float coefFor (double fc, double fs) { return (float) (1.0 - std::exp (-twoPi * fc / fs)); }
};

struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void setup (double fs) { r = (float) (1.0 - twoPi * 5.0 / fs); }
    void reset() { x1 = y1 = 0.0f; }
    float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

//==============================================================================
// Noise with a continuous colour: -1 red, -0.5 pink, 0 white, +0.5 blue, +1 violet
class ColouredNoise
{
public:
    void seed (uint32_t s) { rng.seed (s); }
    void reset() { b0 = b1 = b2 = brown = prevPink = prevWhite = 0.0f; }

    float process (float colour)
    {
        const float w = rng.next();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        const float pink = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
        brown = 0.997f * brown + w * 0.045f;
        const float blue = (pink - prevPink) * 2.2f;
        const float violet = (w - prevWhite) * 0.45f;
        prevPink = pink;
        prevWhite = w;

        const float white = w * 0.9f;
        const float c = clampf (colour, -1.0f, 1.0f) * 2.0f + 2.0f;   // 0..4
        const float pts[5] = { brown * 1.6f, pink, white, blue, violet };
        const int i = std::min (3, (int) c);
        const float f = c - (float) i;
        return pts[i] + f * (pts[i + 1] - pts[i]);
    }

private:
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, brown = 0, prevPink = 0, prevWhite = 0;
};

//==============================================================================
// Analog-style envelope: DAHDSR (ADSR when delay and hold are zero). Attack overshoots toward 1.3 like an RC
// charging circuit; decay and release are exponential.
class Envelope
{
public:
    enum Stage { Idle, Kill, Delay, Attack, Hold, Decay, Sustain, Release };

    void setSampleRate (double fs) { sr = fs; }
    void set (float delayS, float attackS, float holdS, float decayS, float sustain, float releaseS)
    {
        delayN = (int) (delayS * sr);
        holdN = (int) (holdS * sr);
        const double T = 1.3;
        aCoef = coef (std::max (0.0005, (double) attackS) / std::log (T / (T - 1.0)));
        dCoef = coef (std::max (0.0005, (double) decayS) / 4.6);
        rCoef = coef (std::max (0.0005, (double) releaseS) / 4.6);
        sus = clampf (sustain, 0.0f, 1.0f);
    }

    // fromZero: Re-Trig mode (a 1.5 ms fade to zero first to avoid clicks); otherwise starts from the current level
    void noteOn (bool fromZero)
    {
        if (fromZero && value > 0.001f)
        {
            stage = Kill;
            killStep = value / (float) std::max (1, (int) (0.0015 * sr));
        }
        else
            startDelay();
    }
    void noteOff() { if (stage != Idle) stage = Release; }
    void hardReset() { stage = Idle; value = 0.0f; }

    bool isActive() const { return stage != Idle; }
    Stage getStage() const { return stage; }
    float getValue() const { return value; }

    float next()
    {
        switch (stage)
        {
            case Idle: break;
            case Kill:
                value -= killStep;
                if (value <= 0.0f) { value = 0.0f; startDelay(); }
                break;
            case Delay:
                if (--counter <= 0) stage = Attack;
                break;
            case Attack:
                value += (float) aCoef * (1.3f - value);
                if (value >= 1.0f) { value = 1.0f; counter = holdN; stage = holdN > 0 ? Hold : Decay; }
                break;
            case Hold:
                if (--counter <= 0) stage = Decay;
                break;
            case Decay:
                value += (float) dCoef * (sus - value);
                if (std::abs (value - sus) < 1.0e-4f) { value = sus; stage = Sustain; }
                break;
            case Sustain:
                value += 0.002f * (sus - value);   // follows sustain knob changes smoothly
                break;
            case Release:
                value += (float) rCoef * (0.0f - value);
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
        }
        return value;
    }

    // Advance many samples at once (control-rate envelopes)
    float advance (int n)
    {
        for (int i = 0; i < n; ++i) next();
        return value;
    }

private:
    void startDelay()
    {
        counter = delayN;
        stage = delayN > 0 ? Delay : Attack;
    }
    double coef (double tau) const { return 1.0 - std::exp (-1.0 / (tau * sr)); }

    double sr = 48000.0;
    Stage stage = Idle;
    float value = 0.0f, sus = 1.0f, killStep = 0.0f;
    double aCoef = 0.1, dCoef = 0.01, rCoef = 0.01;
    int delayN = 0, holdN = 0, counter = 0;
};

//==============================================================================
// Low frequency oscillator (control rate)
class Lfo
{
public:
    enum Wave { Sine = 0, Triangle, RampUp, RampDown, Square };

    void reset (double startPhase) { phase = startPhase - std::floor (startPhase); }
    void advance (double hz, double seconds) { phase += hz * seconds; phase -= std::floor (phase); }
    void setPhase (double p) { phase = p - std::floor (p); }
    double getPhase() const { return phase; }

    static float shape (int wave, double p)
    {
        p -= std::floor (p);
        switch (wave)
        {
            case Sine:     return (float) std::sin (twoPi * p);
            case Triangle: return (float) (p < 0.25 ? 4.0 * p : (p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0));
            case RampUp:   return (float) (2.0 * p - 1.0);
            case RampDown: return (float) (1.0 - 2.0 * p);
            default:       return p < 0.5 ? 1.0f : -1.0f;
        }
    }

private:
    double phase = 0.0;
};

// Random generator: S+H, Noise or Perlin, with slew (control rate)
class RandomGen
{
public:
    enum Mode { SampleHold = 0, Noise, Perlin };

    void seed (uint32_t s) { rng.seed (s); g0 = rng.next(); g1 = rng.next(); target = rng.next(); }

    float process (int mode, double hz, float slew, double seconds, bool clockFromHost, double hostPhase)
    {
        bool tick = false;
        if (clockFromHost)
        {
            if (hostPhase < lastHostPhase) tick = true;
            lastHostPhase = hostPhase;
            phase = hostPhase;
        }
        else
        {
            phase += hz * seconds;
            if (phase >= 1.0) { phase -= std::floor (phase); tick = true; }
        }

        float raw = 0.0f;
        if (mode == SampleHold)
        {
            if (tick) target = rng.next();
            raw = target;
        }
        else if (mode == Noise)
        {
            raw = rng.next();
        }
        else
        {
            if (tick) { g0 = g1; g1 = rng.next(); }
            // 1D gradient noise between two random gradients, smoothstep blended
            const double t = phase;
            const double a = g0 * t, b = g1 * (t - 1.0);
            const double f = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
            raw = clampf ((float) ((a + (b - a) * f) * 2.2), -1.0f, 1.0f);
        }

        const double period = 1.0 / std::max (0.01, hz);
        const double tau = (double) slew * (double) slew * 0.02 * period;   // slew 0..10
        if (tau <= 1.0e-6) out = raw;
        else out += (float) (1.0 - std::exp (-seconds / tau)) * (raw - out);
        return out;
    }

    float get() const { return out; }

private:
    Rng rng;
    double phase = 0.0, lastHostPhase = 0.0;
    float target = 0.0f, g0 = 0.0f, g1 = 0.0f, out = 0.0f;
};
}

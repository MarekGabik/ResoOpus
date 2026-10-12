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

// Soft ceiling for signals inside a feedback loop: transparent when quiet, never louder than c
inline float ceilingClip (float x, float c) { return c * tanhFast (x / c); }

//==============================================================================
// FILTER with seven models. All take g = tan(pi fc / fs), res 0..1, drive 0..1 and a mode (low / band / high).
// Runs in the oversampled domain.
//   GROWL  op-amp state variable, 12 dB, clipping integrators: keeps the lows, gritty self-oscillation, even harmonics
//   LADDER transistor ladder, 24 dB: round, the lows thin with resonance, rings as a clean sine
//   DIODE  diode ladder, coupled stages: rubbery resonance that squeezes with level, uneven clipping
//   SCREAM Sallen-Key style 12 dB with diodes in the resonance: aggressive, screams near the top
//   SILK   clean digital state variable, linear, never self-oscillates
//   COMB   tuned feedback delay (cutoff = pitch): string, metal or hollow tube
//   VOWEL  three formants (cutoff walks U-O-A-E-I, mode = man / woman / child)

class AnalogFilter
{
public:
    enum Model { Growl = 0, Ladder, Diode, Scream, Silk, Comb, Vowel, numModels };
    enum Mode { LowPass = 0, BandPass, HighPass };

    void reset()
    {
        s1 = s2 = 0.0f; for (auto& z : st) z = 0.0f; for (auto& z : dz) z = 0.0; dz4 = 0.0; clipPrev = 0.0f; dc = 0.0f;
        if (combUsed) { std::fill (combBuf.begin(), combBuf.end(), 0.0f); combUsed = false; }   // only when COMB ran
        combW = 0; combLp = 0.0f;
        for (auto& f : form) f[0] = f[1] = 0.0f;
    }

    float process (float x, float g, float res, float drive, int mode, int model)
    {
        float out = 0.0f;
        switch (model)
        {
            case Ladder: out = ladder (x * (1.0f + 3.0f * drive), g, res, mode); break;
            case Diode:  out = diode (x * (1.0f + 3.0f * drive), g, res, mode); break;
            case Scream: out = scream (x * (1.0f + 4.0f * drive), g, res, drive, mode); break;
            case Silk:   out = silk (x * (1.0f + 2.0f * drive), g, res, mode); break;
            case Comb:   out = comb (x * (1.0f + 3.0f * drive), g, res, drive, mode); break;
            case Vowel:  out = vowel (x * (1.0f + 3.0f * drive), g, res, drive, mode); break;
            default:     out = growl (x * (1.0f + 3.0f * drive), g, res, drive, mode); break;
        }
        if (! (std::abs (out) < 1.0e4f)) { reset(); out = 0.0f; }
        // the uneven clipping of GROWL and DIODE makes a little DC: an 8 Hz high pass takes it out
        dc += dcC * (out - dc);
        return (out - dc) / (1.0f + 1.5f * drive);
    }

private:
    // integrators clip a little harder on one side (the "op-amp as integrator" character)
    static float sat (float v) { return v > 0.0f ? 1.2f * tanhFast (v / 1.2f) : 1.0f * tanhFast (v / 1.0f); }
    // an input stage that bends one half of the wave more than the other: even harmonics, a warm growl
    static float asymDrive (float v)
    {
        static const float b = 0.35f, tb = tanhFast (b), d = 1.0f - tb * tb;
        return (tanhFast (v + b) - tb) / d;
    }

    // GROWL: op-amp state variable whose integrators clip. 12 dB, keeps the lows at any resonance; drive bends the
    // wave asymmetrically (even harmonics); the damping goes slightly negative at the top, so it self-oscillates
    // and the clipping integrators make that tone gritty
    float growl (float x, float g, float res, float drive, int mode)
    {
        x += std::min (1.0f, 3.0f * drive) * (asymDrive (x) - x);   // DRIVE 0: clean, from 33 % fully bent
        const float k = 2.0f - 2.2f * res * std::sqrt (res); // damping: 2 (none) .. -0.2 (self-oscillation, clipped hard)
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
        return mode == BandPass ? 2.0f * bp * std::pow (0.5f * std::max (0.02f, k), 0.25f) : (mode == HighPass ? hp : lp);
    }

    // SILK: a clean digital state variable. Linear (the resonance does not depend on the level, no harmonics) up to
    // a soft ceiling, very narrow at the top but it never self-oscillates; drive only pushes into that ceiling
    float silk (float x, float g, float res, int mode)
    {
        const float k = 2.0f - 1.95f * res;
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2) * h;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        const float y = mode == BandPass ? 2.0f * bp * std::pow (0.5f * k, 0.4f) : (mode == HighPass ? hp : lp);
        // clean below 0.9, then a soft ceiling (a harmonic right on the resonance must not jump out)
        const float a = std::abs (y);
        return a < 0.9f ? y : std::copysign (0.9f + 0.4f * tanhFast ((a - 0.9f) / 0.4f), y);
    }

    // SCREAM: Sallen-Key style 12 dB with diodes in the resonance path. The damping is split into a linear part and a
    // clipped positive feedback (one sample late), so the resonance compresses and screams instead of ringing; the
    // input clips symmetrically (strong odd harmonics) and the harder it is driven, the more the resonance bites
    float scream (float x, float g, float res, float drive, int mode)
    {
        x += std::min (1.0f, 3.0f * drive) * (x / std::sqrt (1.0f + x * x) - x);
        // clipped feedback, beyond 2 = self-oscillation (from about 9.3 on the knob, hard at 10)
        const float r4 = res * res * res * res;
        const float fbk = 1.95f * res * (0.7f + 0.3f * res) + 0.65f * r4 * r4 * r4;
        const float k = 2.0f;
        const float h = 1.0f / (1.0f + g * (g + k));
        const float hp = (x - (k + g) * s1 - s2 + fbk * clipPrev) * h;
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        clipPrev = diodeClip (1.6f * bp) * 0.625f;        // a harder knee: the self-oscillation turns square-ish
        const float comp = 1.0f / (1.0f + 0.3f * fbk);
        const float y = mode == BandPass ? 2.0f * bp * comp : (mode == HighPass ? hp : lp);
        return 1.3f * tanhFast (y / 1.3f);
    }
    static float diodeClip (float v) { return v / std::sqrt (1.0f + 2.5f * v * v); }

    // LADDER: transistor ladder, four one-pole TPT stages, the feedback loop solved without delay, tanh at the input
    // (symmetric: odd harmonics, round). 24 dB, the lows thin out with resonance, self-oscillates as a clean sine.
    // Band and high pass by mixing the stage outputs
    float ladder (float x, float g, float res, int mode)
    {
        const float k = 4.1f * res;
        const float G = g / (1.0f + g);
        const float b = 1.0f / (1.0f + g);
        const float S = G * G * G * st[0] * b + G * G * st[1] * b + G * st[2] * b + st[3] * b;
        float u = (x * (1.0f + 0.3f * k) - k * S) / (1.0f + k * G * G * G * G);   // half the bass back: it thins with resonance
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
        const float back = 1.0f / (1.0f + 0.3f * k);
        if (mode == BandPass) return 4.0f * (y[1] - 2.0f * y[2] + y[3]) * std::sqrt (back) / (1.0f + 0.3f * k);
        if (mode == HighPass) return (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]) * back;
        return y[3];
    }

    // DIODE: diode ladder (coupled stages, after D. Wurtz's model) with a high pass in the feedback path. About 18 dB,
    // a rubbery resonance that loses the most lows and compresses with the level; the diodes clip unevenly (even and
    // odd harmonics). The coupled stages resonate up to 1.48x above their own cutoff, so with resonance the cutoff
    // is scaled back to keep the peak on the knob
    static float diodeAsym (float v) { return v >= 0.0f ? v / (1.0f + 1.3f * v) : v / (1.0f - 0.8f * v); }
    float diode (float x, float g, float res, int mode)
    {
        // double precision: at low cutoffs the stage solve divides by a tiny a
        const double k = 19.0 * res;
        const double a = g * (1.0 - 0.324 * res), ainv = 1.0 / a, a2 = a * a, bb = 2.0 * a + 1.0, b2 = bb * bb;
        const double c = 1.0 / (2.0 * a2 * a2 - 4.0 * a2 * b2 + b2 * b2);
        const double g0 = 2.0 * a2 * a2 * c;
        const double gg = g0 * fbB;
        const double s0 = (a2 * a * dz[0] + a2 * bb * dz[1] + dz[2] * (b2 - 2.0 * a2) * a + dz[3] * (b2 - 3.0 * a2) * bb) * c;
        const double s = fbB * s0 - dz4;
        double y5 = (gg * x + s) / (1.0 + gg * k);
        const double y0 = diodeAsym ((float) (x - k * y5));
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
        return (float) (y4 * (1.0 + 0.3 * k));
    }

    // COMB: a tuned feedback delay; the cutoff is its pitch (one period of delay), the resonance its feedback.
    // LOW = a damped loop (a plucked string, warm), BAND = every harmonic rings (metallic), HIGH = negative feedback:
    // only the odd harmonics ring an octave lower (hollow, like a clarinet or a tube). Drive saturates the loop
    float comb (float x, float g, float res, float drive, int mode)
    {
        if (combBuf.empty()) return x;
        combUsed = true;
        if (g < combG || g > combG || mode != combMode)   // a new pitch (exact compare: only a changed value)
        {
            combG = g; combMode = mode;
            combD = pi / std::atan ((double) g);   // fs / f, since g = tan (pi f / fs)
            combC = (float) (1.0 - std::exp (-twoPi * 3.0 / combD));   // LOW: the loop is damped at ~3x the pitch
            if (mode == LowPass)
            {
                // the damping delays the loop a little: take its phase delay at the pitch off the line, so it stays in tune
                const double w = twoPi / combD, a = 1.0 - combC;
                combD -= std::atan2 (a * std::sin (w), 1.0 - a * std::cos (w)) / w;
            }
        }
        const int n = (int) combBuf.size();
        const double d = std::max (2.0, std::min (combD, (double) n - 4.0));
        double pos = (double) combW - d;
        if (pos < 0.0) pos += n;
        const int i0 = (int) pos;
        const float fr = (float) (pos - i0), a = combBuf[(size_t) i0], b = combBuf[(size_t) ((i0 + 1) % n)];
        float delayed = a + fr * (b - a);
        const float fb = (mode == HighPass ? -1.0f : 1.0f) * (0.3f + 0.67f * res);
        if (mode == LowPass)
        {
            combLp += combC * (delayed - combLp);
            delayed = combLp;
        }
        float v = x + fb * delayed;
        v = drive > 0.0f ? ceilingClip (v, 1.5f - std::min (1.0f, drive)) : ceilingClip (v, 3.0f);
        combBuf[(size_t) combW] = v;
        if (++combW >= n) combW = 0;
        return v * std::pow (1.0f - fb * fb, 0.4f);   // keeps the level of a broad sound about the same
    }

    // VOWEL: three formant band passes (a vocal tract). The cutoff walks U - O - A - E - I (dark to bright), the
    // resonance sharpens the formants, the mode is the size of the throat: LOW = man, BAND = woman, HIGH = child
    float vowel (float x, float g, float res, float drive, int mode)
    {
        if (drive > 0.0f) x += std::min (1.0f, 3.0f * drive) * (tanhFast (x) - x);
        if (g < vowelG || g > vowelG || res < vowelRes || res > vowelRes || mode != vowelMode)
        {
            vowelG = g; vowelRes = res; vowelMode = mode;
            static const float table[5][3] = { { 300.0f, 870.0f, 2240.0f },     // U
                                               { 570.0f, 840.0f, 2410.0f },     // O
                                               { 730.0f, 1090.0f, 2440.0f },    // A
                                               { 530.0f, 1840.0f, 2480.0f },    // E
                                               { 270.0f, 2290.0f, 3010.0f } };  // I
            static const float amp[5][3] = { { 1.0f, 0.35f, 0.12f }, { 1.0f, 0.55f, 0.15f }, { 1.0f, 0.6f, 0.25f },
                                             { 1.0f, 0.55f, 0.35f }, { 1.0f, 0.4f, 0.35f } };
            const double f = std::atan ((double) g) * fsRate / pi;
            const float p = 4.0f * clampf ((float) (std::log (std::max (20.0, f) / 20.0) / std::log (1000.0)), 0.0f, 1.0f);
            const int i = std::min (3, (int) p);
            const float t = p - (float) i;
            const float size = mode == HighPass ? 1.35f : (mode == BandPass ? 1.17f : 1.0f);
            const float q = 4.0f + 26.0f * res;
            for (int k = 0; k < 3; ++k)
            {
                const double hz = size * table[i][k] * std::pow (table[i + 1][k] / table[i][k], t);
                formG[k] = (float) std::tan (pi * std::min (hz, 0.45 * fsRate) / fsRate);
                formK[k] = 1.0f / q;
                formA[k] = amp[i][k] + t * (amp[i + 1][k] - amp[i][k]);
            }
            vowelGain = 3.0f * std::pow (q / 4.0f, 0.65f);   // narrower formants pass less: make up for it
        }
        float y = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            const float gk = formG[k], kk = formK[k];
            const float h = 1.0f / (1.0f + gk * (gk + kk));
            const float hp = (x - (kk + gk) * form[k][0] - form[k][1]) * h;
            const float v1 = gk * hp, bp = v1 + form[k][0];
            form[k][0] = bp + v1;
            const float v2 = gk * bp, lp = v2 + form[k][1];
            form[k][1] = lp + v2;
            y += formA[k] * kk * bp;   // unity at each formant's centre
        }
        return ceilingClip (vowelGain * y, 1.4f);
    }

    std::vector<float> combBuf;
    int combW = 0;
    double combD = 100.0, fsRate = 48000.0;
    float combG = -1.0f, combLp = 0.0f, combC = 0.1f, vowelG = -1.0f, vowelRes = -1.0f, vowelGain = 3.0f;
    int vowelMode = -1, combMode = -1;
    bool combUsed = false;
    float form[3][2] {}, formG[3] {}, formK[3] {}, formA[3] {};

    float s1 = 0.0f, s2 = 0.0f, clipPrev = 0.0f;
    float st[4] {};
    double dz[4] {}, dz4 = 0.0;
public:
    // diode ladder feedback high pass (cutoff in Hz at the running sample rate)
    void prepare (double fs)
    {
        const double K = 2.0 * pi * 90.0 / fs; fbA = (K - 2.0) / (K + 2.0); fbB = 2.0 / (K + 2.0);
        dcC = (float) (1.0 - std::exp (-2.0 * pi * 8.0 / fs));
        fsRate = fs;
        combBuf.assign ((size_t) (fs / 18.0) + 8, 0.0f);   // the comb reaches down to 20 Hz
        combW = 0; combG = vowelG = -1.0f;
    }
private:
    double fbA = -0.99, fbB = 1.0;
    float dc = 0.0f, dcC = 0.0005f;
};


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
// ECHO: five modes on two delay lines, all with soft limiting in the loop (they sing at high FEEDBACK without
// blowing up). TO VOICES: the wet signal for the GRAVITY network.
//  DUAL       two lines with cross feedback (left feeds right and back); the times wobble from an LFO or from
//             their own output (SELF: stable resonances turn unstable)
//  TAPE       one tape loop, two playback heads (TIME 1 and TIME 2). Recording saturates and compresses, every
//             playback loses highs and lows (120 Hz .. 2.8 kHz), wow / flutter / drift move the tape speed (MOD), DUST adds tape hiss, and a new
//             time is reached slowly (the pitch slides like a tape speed change)
//  PING-PONG  the input goes into the left line only; left feeds right and right feeds left, so the repeats bounce
//  LO-FI      a degraded digital delay: the repeats lose sample rate and bits (MOD = degrade), dark, noisy, crunchy
//  MOD        chorused repeats: each line swings by up to 8 ms (MOD = depth), left and right a quarter apart
class Echo
{
public:
    enum Mode { Dual = 0, Tape, PingPong, LoFi, ModDelay };

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
        for (auto& d : damp2) d.reset();
        for (auto& d : low) d.reset();
        fbL = fbR = 0.0f; selfMod[0] = selfMod[1] = 0.0f; lfo = 0.0; wet = 0.0f;
        wow = flutter = chorus = 0.0; driftS = driftTarget = 0.0f; driftCount = 0;
        held[0] = held[1] = 0.0f; holdPhase = 0.0; lastMode = -1; hissEnv = 0.0f;
        timeR[0].reset ((float) (0.3 * sr)); timeR[1].reset ((float) (0.45 * sr));
        tapeT[0] = 0.3 * sr; tapeT[1] = 0.45 * sr;
        primed = false;
    }

    struct Settings { float time1 = 0.3f, time2 = 0.45f, feedback = 0.3f, mix = 0.0f, mod = 0.0f, dust = 0.0f; bool self = true; int mode = Dual; };

    void process (float* L, float* R, int n, const Settings& s, Rng& rng, float* wetOut = nullptr)
    {
        if (s.mode != lastMode) { if (lastMode >= 0) { const bool p = primed; reset(); primed = p; } lastMode = s.mode; }
        if (sleeper.asleep (L, R, n, 0.25 * sr))
        {
            if (wetOut != nullptr) std::fill (wetOut, wetOut + n, 0.0f);
            wet = 0.0f;
            return;
        }
        const float t1 = (float) (s.time1 * sr), t2 = (float) (s.time2 * sr);
        if (primed) { timeR[0].set (t1, n); timeR[1].set (t2, n); } else { timeR[0].reset (t1); timeR[1].reset (t2); tapeT[0] = t1; tapeT[1] = t2; primed = true; }
        const float fb = 1.25f * s.feedback;
        // the more the loop holds, the more its output is turned down (steady-state gain of the loop)
        // TAPE / LO-FI / MOD lose more in their loop (filters, saturation), so they need less of that correction
        const float loss = s.mode == Tape ? 0.8f : (s.mode == LoFi ? 0.7f : (s.mode == ModDelay ? 0.85f : 1.0f));
        const float g = std::min (0.98f, loss * fb);
        const float comp = std::sqrt (1.0f - g * g);
        const float dampC = OnePole::coefFor (7000.0 - 4500.0 * s.dust, sr);
        const double lfoInc = 0.23 / sr;
        const float envC = (float) (1.0 - std::exp (-1.0 / (0.5 * sr)));
        // TAPE: the tape speed settles over ~0.25 s; one pass keeps 120 Hz .. 2.8 kHz (darker with DUST)
        const double tapeC = 1.0 - std::exp (-1.0 / (0.25 * sr));
        const float tapeLp = OnePole::coefFor (2800.0 - 900.0 * s.dust, sr), tapeHp = OnePole::coefFor (120.0, sr);
        // LO-FI: sample rate 16 kHz .. ~2.8 kHz and 12 .. 4 bits, a dark loop
        const double holdInc = 16000.0 * std::exp2 (-2.5 * (double) s.mod) / sr;
        const float steps = std::exp2 (12.0f - 8.0f * s.mod) * 0.5f;
        const float lofiLp = OnePole::coefFor (3200.0 - 1400.0 * s.mod, sr);
        for (int k = 0; k < n; ++k)
        {
            lfo += lfoInc; lfo -= std::floor (lfo);
            const float lf = (float) std::sin (twoPi * lfo);
            const double tm0 = timeR[0].next(), tm1 = timeR[1].next();
            float y0 = 0.0f, y1 = 0.0f, wl = 0.0f, wr = 0.0f;
            // hiss (TAPE, LO-FI) follows what is in the loop, so a quiet echo can still fall asleep
            hissEnv += envC * (std::min (1.0f, 30.0f * (std::abs (L[k]) + std::abs (R[k]) + wetBuf[(k + 255) & 255])) - hissEnv);
            switch (s.mode)
            {
                case Tape:
                {
                    tapeT[0] += tapeC * (tm0 - tapeT[0]);
                    tapeT[1] += tapeC * (tm1 - tapeT[1]);
                    wow += 0.6 / sr; wow -= std::floor (wow);
                    flutter += 8.5 / sr; flutter -= std::floor (flutter);
                    if (--driftCount <= 0) { driftTarget = rng.next(); driftCount = (int) (0.4 * sr); }
                    driftS += (float) (1.0 - std::exp (-1.0 / (0.2 * sr))) * (driftTarget - driftS);
                    // both heads read the same tape, so they share its speed
                    const double speed = 1.0 + s.mod * (0.006 * std::sin (twoPi * wow) + 0.0015 * std::sin (twoPi * flutter) + 0.004 * driftS);
                    // playback: every head loses highs and lows, already on the first repeat and again on every pass
                    auto head = [&] (int h, double d)
                    {
                        float v = lines[0].read (std::max (2.0, d));
                        v = damp2[h].lp (damp[h].lp (v, tapeLp), tapeLp);
                        return v - low[h].lp (v, tapeHp);
                    };
                    y0 = head (0, tapeT[0] * speed);
                    y1 = head (1, tapeT[1] * speed);
                    const float back = 0.6f * (y0 + y1);
                    const float rec = 0.5f * (L[k] + R[k]) + fb * back + hissEnv * (s.dust * 0.004f + 0.00015f) * rng.next();
                    // recording saturates and compresses (a little more on one side, like tape)
                    lines[0].push (rec > 0.0f ? 0.7f * tanhFast (rec / 0.7f) : 0.62f * tanhFast (rec / 0.62f));
                    wl = (y0 + 0.5f * y1) * comp * 0.8f;
                    wr = (y1 + 0.5f * y0) * comp * 0.8f;
                    break;
                }
                case PingPong:
                {
                    const float m0 = s.self ? selfMod[0] : lf, m1 = s.self ? selfMod[1] : -lf;
                    y0 = lines[0].read (std::max (2.0, tm0 * (1.0 + 0.35 * s.mod * m0)));
                    y1 = lines[1].read (std::max (2.0, tm1 * (1.0 + 0.35 * s.mod * m1)));
                    // only the left line hears the input: left -> right -> left ...
                    lines[0].push (0.5f * (L[k] + R[k]) + damp[0].lp (ceilingClip (fb * y1, 0.5f), dampC) + s.dust * 0.003f * rng.next());
                    lines[1].push (damp[1].lp (ceilingClip (fb * y0, 0.5f), dampC));
                    wl = y0 * comp; wr = y1 * comp;
                    break;
                }
                case LoFi:
                {
                    y0 = lines[0].read (std::max (2.0, tm0));
                    y1 = lines[1].read (std::max (2.0, tm1));
                    // a coarse clock and few bits: aliasing and grain, dark and a little noisy
                    holdPhase += holdInc;
                    if (holdPhase >= 1.0)
                    {
                        holdPhase -= std::floor (holdPhase);
                        held[0] = std::round (y0 * steps) / steps;
                        held[1] = std::round (y1 * steps) / steps;
                    }
                    const float c0 = damp[0].lp (held[0], lofiLp), c1 = damp[1].lp (held[1], lofiLp);
                    const float noise = hissEnv * (0.001f + 0.004f * s.mod) * rng.next();
                    lines[0].push (L[k] + ceilingClip (fb * c1, 0.4f) + noise);
                    lines[1].push (R[k] + ceilingClip (fb * c0, 0.4f) + noise);
                    wl = c0 * comp; wr = c1 * comp;
                    break;
                }
                case ModDelay:
                {
                    chorus += 0.7 / sr; chorus -= std::floor (chorus);
                    const double depth = s.mod * 0.008 * sr;
                    y0 = lines[0].read (std::max (2.0, tm0 + depth * (1.0 + std::sin (twoPi * chorus))));
                    y1 = lines[1].read (std::max (2.0, tm1 + depth * (1.0 + std::cos (twoPi * chorus))));
                    const float c = OnePole::coefFor (9000.0, sr);
                    lines[0].push (L[k] + damp[0].lp (ceilingClip (fb * y1, 0.5f), c));
                    lines[1].push (R[k] + damp[1].lp (ceilingClip (fb * y0, 0.5f), c));
                    wl = y0 * comp; wr = y1 * comp;
                    break;
                }
                default:   // Dual
                {
                    const float m0 = s.self ? selfMod[0] : lf, m1 = s.self ? selfMod[1] : -lf;
                    y0 = lines[0].read (std::max (2.0, tm0 * (1.0 + 0.35 * s.mod * m0)));
                    y1 = lines[1].read (std::max (2.0, tm1 * (1.0 + 0.35 * s.mod * m1)));
                    // cross feedback with damping and a soft limiter; DUST adds hiss in the loop
                    lines[0].push (L[k] + damp[0].lp (ceilingClip (fb * y1, 0.5f), dampC) + s.dust * 0.003f * rng.next());
                    lines[1].push (R[k] + damp[1].lp (ceilingClip (fb * y0, 0.5f), dampC));
                    wl = y0 * comp; wr = y1 * comp;
                    break;
                }
            }
            selfMod[0] += 0.002f * (y0 - selfMod[0]);
            selfMod[1] += 0.002f * (y1 - selfMod[1]);
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
    double sr = 48000.0, lfo = 0.0, wow = 0.0, flutter = 0.0, chorus = 0.0, holdPhase = 0.0, tapeT[2] { 14400.0, 21600.0 };
    DelayLine lines[2];
    OnePole damp[2], damp2[2], low[2];
    Ramp timeR[2];
    float fbL = 0.0f, fbR = 0.0f, selfMod[2] {}, wet = 0.0f, wetBuf[256] {}, driftS = 0.0f, driftTarget = 0.0f, held[2] {}, hissEnv = 0.0f;
    int driftCount = 0, lastMode = -1;
    bool primed = false;
    Sleeper sleeper;
    AutoBalance balance;
};

//==============================================================================
// MOTION: movement in space, after the filters and before ECHO / SPACE. RATE (free or in tempo), DEPTH, X, MIX.
//  ORBIT     the sound circles around the listener: equal-power position, the far ear hears it a little later
//            (up to 0.65 ms) and darker (head shadow), behind it gets quieter and darker and the changing
//            distance bends the pitch a little (Doppler). The stereo width of the input (side) stays. X = width
//  PHASER    six all-pass stages per side sweep notches (+-3 octaves around 400 Hz), right a quarter behind the
//            left; X = feedback
//  ENSEMBLE  three delayed copies swing slowly (RATE) and quickly (6.2 Hz) a third apart, like the chorus of old
//            string machines; X = how far apart they sit in the stereo field
//  TREMOLO   the level pulses with a rounded sine; X = the sides together (0) or alternating (100)
//  ROTOR     a rotating speaker: a horn above 800 Hz and a drum below, each swinging (Doppler and level) for two
//            microphones a quarter apart. X = speed, slow below the middle and fast above, reached with the inertia
//            of the real thing (horn ~0.8 s, drum ~3.5 s); RATE is not used
class Motion
{
public:
    enum Mode { Orbit = 0, Phaser, Ensemble, Tremolo, Rotor };
    struct Settings { int mode = Orbit; double rateHz = 0.12; float depth = 0.7f, x = 0.5f, mix = 1.0f; bool lockPhase = false; double phase = 0.0; };

    void prepare (double fs)
    {
        sr = fs;
        for (auto& l : lines) l.prepare ((int) (fs * 0.04));
        reset();
    }
    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& f : lp) f.reset();
        for (auto& c : ap) for (auto& z : c) z = 0.0f;
        apOut[0] = apOut[1] = 0.0f;
        phase = fast = horn = drum = 0.0;
        hornHz = 0.8; drumHz = 0.7;
        dR.reset (0.0f); xR.reset (0.0f); mR.reset (0.0f);
        primed = false; lastMode = -1;
        for (auto& c : coef) c = 1.0f;
        apA[0] = apA[1] = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        if (s.mode != lastMode) { if (lastMode >= 0) { reset(); } lastMode = s.mode; }
        if (primed) { dR.set (s.depth, n); xR.set (s.x, n); mR.set (s.mix, n); }
        else { dR.reset (s.depth); xR.reset (s.x); mR.reset (s.mix); primed = true; }
        if (s.lockPhase) phase = s.phase - std::floor (s.phase);
        const double inc = s.rateHz / sr;
        for (int k = 0; k < n; ++k)
        {
            const float depth = dR.next(), xx = xR.next(), mix = mR.next();
            phase += inc; phase -= std::floor (phase);
            const double th = twoPi * phase;
            const float dl = L[k], dr = R[k];
            float wl = dl, wr = dr;
            const bool update = (count++ & 7) == 0;   // filter coefficients every 8 samples
            switch (s.mode)
            {
                case Orbit:
                {
                    const float px = (float) std::sin (th) * depth, py = (float) std::cos (th);
                    const float mid = 0.5f * (dl + dr), side = 0.5f * (dl - dr);
                    lines[0].push (mid);
                    const float a = (px + 1.0f) * 0.25f * (float) pi;
                    const float gL = 1.41421356f * std::cos (a), gR = 1.41421356f * std::sin (a);
                    const float behind = std::max (0.0f, -py) * depth;
                    const double base = 0.0012 * sr + 0.0015 * sr * depth * 0.5 * (1.0 - py);   // further away behind: Doppler
                    const double itd = 0.00065 * sr * xx * std::abs (px);
                    float yl = lines[0].read (base + (px > 0.0f ? itd : 0.0)) * gL;
                    float yr = lines[0].read (base + (px < 0.0f ? itd : 0.0)) * gR;
                    if (update)
                    {
                        // the far ear is shadowed by the head, everything behind is a little darker
                        const double back = std::pow (0.35, (double) behind);
                        const double far = std::pow (0.1, (double) (xx * std::abs (px)));
                        coef[0] = OnePole::coefFor (18000.0 * back * (px > 0.0f ? far : 1.0), sr);
                        coef[1] = OnePole::coefFor (18000.0 * back * (px < 0.0f ? far : 1.0), sr);
                    }
                    const float g = 1.0f - 0.25f * behind;
                    yl = lp[0].lp (yl, coef[0]) * g;
                    yr = lp[1].lp (yr, coef[1]) * g;
                    wl = yl + side; wr = yr - side;
                    break;
                }
                case Phaser:
                {
                    if (update)
                        for (int c = 0; c < 2; ++c)
                        {
                            const double f = 400.0 * std::exp2 (3.0 * depth * std::sin (th - c * 0.5 * pi));
                            const double t = std::tan (pi * std::min (f, 0.45 * sr) / sr);
                            apA[c] = (float) ((t - 1.0) / (t + 1.0));
                        }
                    const float fb = 0.85f * xx;
                    float in[2] = { dl, dr }, out[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        float v = in[c] + fb * apOut[c];
                        for (int st = 0; st < 6; ++st)
                        {
                            const float y = apA[c] * v + ap[c][st];
                            ap[c][st] = v - apA[c] * y;
                            v = y;
                        }
                        apOut[c] = ceilingClip (v, 2.0f);
                        out[c] = 0.65f * (in[c] + v);   // dry + shifted: the notches (a little louder for the lost energy)
                    }
                    wl = out[0]; wr = out[1];
                    break;
                }
                case Ensemble:
                {
                    fast += 6.2 / sr; fast -= std::floor (fast);
                    const float mid = 0.5f * (dl + dr), side = 0.5f * (dl - dr);
                    lines[0].push (mid);
                    float sl = 0.0f, srr = 0.0f;
                    for (int j = 0; j < 3; ++j)
                    {
                        const double off = j * twoPi / 3.0;
                        const double d = 0.007 * sr + depth * (0.0022 * sr * std::sin (th + off) + 0.00025 * sr * std::sin (twoPi * fast + off));
                        const float v = lines[0].read (d);
                        const float p = (float) (j - 1) * xx;   // -x, 0, +x
                        sl += v * (1.0f - p);
                        srr += v * (1.0f + p);
                    }
                    wl = sl * 0.42f + side * 0.6f;
                    wr = srr * 0.42f - side * 0.6f;
                    break;
                }
                case Tremolo:
                {
                    auto shape = [] (double a) { return (float) (std::tanh (1.8 * std::sin (a)) / std::tanh (1.8)); };
                    const float comp = 1.0f / (1.0f - 0.35f * depth);
                    const float gl = 1.0f - depth * (0.5f - 0.5f * shape (th));
                    const float gr = 1.0f - depth * (0.5f - 0.5f * shape (th + pi * xx));
                    wl = dl * gl * comp; wr = dr * gr * comp;
                    break;
                }
                default:   // Rotor
                {
                    // X sets the speed (slow 0.8 Hz .. fast 6.8 Hz for the horn), reached with inertia
                    const double target = 0.8 * std::pow (6.8 / 0.8, (double) clampf ((xx - 0.35f) / 0.3f, 0.0f, 1.0f));
                    hornHz += (target - hornHz) * (1.0 - std::exp (-1.0 / (0.8 * sr)));
                    drumHz += (0.85 * target - drumHz) * (1.0 - std::exp (-1.0 / (3.5 * sr)));
                    horn += hornHz / sr; horn -= std::floor (horn);
                    drum += drumHz / sr; drum -= std::floor (drum);
                    const float mid = 0.5f * (dl + dr);
                    const float c800 = OnePole::coefFor (800.0, sr);
                    const float low = lp[1].lp (lp[0].lp (mid, c800), c800), high = mid - low;
                    lines[0].push (high);
                    float out[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const double hA = twoPi * horn + c * 0.5 * pi, dA = twoPi * drum + c * 0.5 * pi;
                        const float h = lines[0].read (0.0003 * sr + 0.0006 * sr * depth * (1.0 + std::sin (hA)))
                                        * (1.0f - 0.35f * depth * (float) (0.5 - 0.5 * std::cos (hA)));
                        const float d = low * (1.0f - 0.25f * depth * (float) (0.5 + 0.5 * std::sin (dA)));
                        out[c] = 1.15f * (h + d);
                    }
                    wl = out[0]; wr = out[1];
                    break;
                }
            }
            L[k] = dl * (1.0f - mix) + wl * mix;
            R[k] = dr * (1.0f - mix) + wr * mix;
        }
    }

private:
    double sr = 48000.0, phase = 0.0, fast = 0.0, horn = 0.0, drum = 0.0, hornHz = 0.8, drumHz = 0.7;
    DelayLine lines[1];
    OnePole lp[2];
    float coef[2] { 1.0f, 1.0f }, ap[2][6] {}, apA[2] {}, apOut[2] {};
    Ramp dR, xR, mR;
    unsigned count = 0;
    int lastMode = -1;
    bool primed = false;
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
//  COMET   X density, Y length, Z scatter              - a comet's tail of grains: short pieces of the last two seconds,
//                                                       detuned, sometimes an octave away, scattered in the stereo field
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
        lastAlgo = -1;
        inEnv = loopEnv = 0.0f; duck = 1.0f;
        for (auto& gr : grains) gr.left = 0;
        grainClock = 0.0;
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
                    // a comet's tail of grains: the input goes into a 2.2 s memory; short grains (40-200 ms, Hann)
                    // are read from random places up to LENGTH back, a little detuned, sometimes an octave up or
                    // down (SCATTER), and spread in the stereo field. Part of the cloud flows back into the memory,
                    // so the tail keeps glittering after the sound has gone
                    const float mid = 0.5f * (dl + dr);
                    const double rate = 2.0 * std::pow (30.0, (double) X);            // grains per second, 2 .. 60
                    const double reach = (0.08 + 2.0 * Y * Y) * sr;                    // how far back, 80 ms .. 2.08 s
                    grainClock += rate / sr;
                    if (grainClock >= 1.0)
                    {
                        grainClock -= 1.0;
                        for (auto& gr : grains)
                            if (gr.left <= 0)
                            {
                                const double dur = sr * (0.04 + 0.16 * (1.0 - X) * (0.5 + 0.5 * rng.next01()));
                                double semis = Z * 0.35 * rng.next();                      // up to +-35 cents
                                const float pick = rng.next01();
                                if (pick < 0.3f * Z) semis += 12.0; else if (pick < 0.4f * Z) semis -= 12.0;
                                gr.ratio = std::exp2 (semis / 12.0);
                                gr.len = (int) dur; gr.left = gr.len;
                                // start far enough back that a rising grain never overtakes the write head
                                gr.delay = 4.0 + std::max (0.0, (gr.ratio - 1.0) * dur) + rng.next01() * reach;
                                const float pan = Z * 0.9f * rng.next();
                                gr.gl = std::sqrt (0.5f * (1.0f - pan)); gr.gr = std::sqrt (0.5f * (1.0f + pan));
                                break;
                            }
                    }
                    float cl = 0.0f, cr = 0.0f;
                    for (auto& gr : grains)
                    {
                        if (gr.left <= 0) continue;
                        const float ph = 1.0f - (float) gr.left / (float) gr.len;
                        const float w = 0.5f - 0.5f * std::cos ((float) twoPi * ph);
                        const float v = delay[0].read (std::min (gr.delay, 2.1 * sr)) * w;
                        cl += v * gr.gl; cr += v * gr.gr;
                        gr.delay -= gr.ratio - 1.0;   // reading faster (up) or slower (down) than the tape moves
                        --gr.left;
                    }
                    // about as loud as the input, however many grains overlap
                    const float norm = 1.0f / (float) std::sqrt (std::max (1.0, rate * (0.04 + 0.08 * (1.0 - X))));
                    cl = damp[0].lp (cl * norm, dampC); cr = damp[1].lp (cr * norm, dampC);
                    delay[0].push (mid + ceilingClip (0.45f * Y * (cl + cr), 0.5f));
                    wl = 1.6f * cl; wr = 1.6f * cr;
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
    double sr = 48000.0, ringPhase = 0.0, ringPhase2 = 0.0, shPhase = 0.0, ringHz = 200.0, ringTarget = 200.0, bowlHz = -1.0;
    Fdn fdn;
    PitchShifter shiftUp[2], shiftDown[2], runShift[2];
    DelayLine delay[2];
    OnePole damp[2], low[2], inHp, shimLp, shimLp2, shimHp;
    Allpass diffuse[4], runDiff[2][2];
    Resonator bowl[2][3];
    float bowlGain[3] { 1.0f, 1.0f, 1.0f };
    Ramp xR, yR, zR, bR;
    float fbL = 0.0f, fbR = 0.0f, inEnv = 0.0f, loopEnv = 0.0f, duck = 1.0f;
    struct Grain { double delay = 0.0, ratio = 1.0; int len = 1, left = 0; float gl = 0.7f, gr = 0.7f; };
    Grain grains[32];
    double grainClock = 0.0;
    int lastAlgo = -1;
    bool primed = false;
    Sleeper sleeper;
    AutoBalance balance;
};
}

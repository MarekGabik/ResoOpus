#pragma once

#include "Primitives.h"

// Voice engines. Allocation-free, no JUCE dependency, run at the oversampled rate.
namespace dsp
{
//==============================================================================
// Band-limited saw. Same 4-point B-spline BLEP as ShapeOsc, but specialised: the only event is the wrap
// (a jump of -2 with no change of slope), so the correction is computed once per period and every other sample
// costs a phase increment and a 4-tap shift. Output is delayed by two samples (like ShapeOsc).
// The increment may change every sample (FM) and may go negative (through-zero FM: the phase runs backwards
// and the wrap jumps the other way), so deep FM never stalls the oscillator.
class BlepSaw
{
public:
    void reset (double startPhase)
    {
        phase = startPhase - std::floor (startPhase);
        acc[0] = acc[1] = acc[2] = acc[3] = 0.0f;
        primed = false;
    }

    double getPhase() const { return phase; }

    float tick (double dt)
    {
        dt = dt < -0.45 ? -0.45 : (dt > 0.45 ? 0.45 : dt);
        double p = phase + dt;
        if (p >= 1.0)
        {
            p -= 1.0;
            const double x = dt > 0.0 ? p / dt : 0.0;   // samples since the wrap, 0..1
            for (int j = 0; j < 4; ++j)
                acc[j] += (float) (-2.0 * stepResidual (x + (double) (j - 2)));
        }
        else if (p < 0.0)
        {
            p += 1.0;   // running backwards: the wrap jumps from -1 up to +1
            const double x = (1.0 - p) / -dt;
            for (int j = 0; j < 4; ++j)
                acc[j] += (float) (2.0 * stepResidual (std::min (x, 1.0) + (double) (j - 2)));
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

// Band-limited square as the difference of two phase-locked saws half a period apart
class BlepSquare
{
public:
    void reset (double startPhase) { a.reset (startPhase); b.reset (startPhase + 0.5); }
    float tick (double dt) { return 0.5f * (a.tick (dt) - b.tick (dt)); }

private:
    BlepSaw a, b;
};

//==============================================================================
// Per-chunk settings of one voice (already modulated, snapped and transposed)
struct VoiceSettings
{
    int type = 1;                 // 0 off, 1 choir, 2 siren, 3 pulsar
    double rootMidi = 48.0;       // after pitch, snap, bend, TIDE transposition
    double a4 = 440.0;
    float attack = 1.0f, release = 1.0f;   // seconds
    float target = 0.0f;          // level the envelope moves toward (max of HOLD and touch)
    float gainL = 0.5f, gainR = 0.5f;      // volume and constant-power pan

    // CHOIR
    float intervals[5] {};        // semitones
    bool on[5] { true, true, true, true, true };
    float pull = 0.0f;            // TIDE pull between the oscillators 0..1

    // SIREN
    float sirenRate = 2.0f;       // Hz of the modulator
    float sirenFm = 0.0f;         // 0..1: up to +-3 octaves of sweep
    float sirenAm = 0.0f;         // 0..1: chops the tone with the modulator
    float sirenDiv = 1.0f;        // 1..16: sub-harmonic of the tone (flip-flop divider)
    float sirenNoise = 0.0f;      // 0..1 white noise
    float sirenWave = 0.0f;       // 0 square (two-tone) .. 1 triangle (wail)

    // PULSAR
    float sharp = 0.0f;           // 0 triangle .. 1 square (through shark fin and saw)
    float bloom = 0.0f;           // 0..1: the envelope bends pitch and brightness
    float twin = 7.0f;            // semitones of the second tone
    float twinMix = 0.0f, sub = 0.0f;      // 0..1
    float vibrato = 0.0f;         // 0..1 = 0..50 cents
};

// One voice: the three engines share the envelope, gain and pan; the type picks the engine.
// The envelope is a slew toward the target level: up with ATTACK, down with RELEASE.
// Rendering is per sample (begin() once per chunk, then tick() per sample) so the voices can frequency-modulate
// each other sample by sample (GRAVITY): tick() takes a frequency deviation in Hz and returns the voice's own
// signal (about +-1 times its envelope) for the others to use as a modulator.
class Voice
{
public:
    void prepare (double fs, uint32_t seed)
    {
        sr = fs;
        rng.seed (seed);
        Rng r;
        r.seed (seed * 31u + 7u);
        for (auto& o : saws) o.reset (r.next01());
        tone.reset (r.next01());
        sub.reset (r.next01());
        pulsar.reset (r.next01());
        twinOsc.reset (r.next01());
        subOsc.reset (r.next01());
        vibRate = 4.4 + 0.45 * (double) (seed % 7u) / 6.0 * 4.0;   // every voice its own speed (4.4-6.2 Hz)
        reset();
    }

    void reset()
    {
        level = 0.0f;
        primed = false;
        modPhase = 0.0;
        modSmooth = 0.0f;
        for (auto& y : last) y = 0.0f;
    }

    void setSampleRate (double fs) { sr = fs; primed = false; }
    float getLevel() const { return level; }
    bool isSilent() const { return level < 1.0e-5f; }

    // Prepares a chunk of n samples. Returns false when the voice is silent and stays silent (skip tick()).
    bool begin (const VoiceSettings& settings, int n)
    {
        s = settings;
        target = s.type == 0 ? 0.0f : s.target;
        if (isSilent() && target <= 0.0f)
        {
            level = 0.0f;
            primed = false;   // next start jumps straight to the current pitch (no sweep from the old one)
            active = false;
            return false;
        }
        active = true;
        // envelope coefficients (RC slew, ~99 % of the way after the set time); OFF fades out in 20 ms
        aC = (float) (1.0 - std::exp (-4.6 / (std::max (0.001, (double) s.attack) * sr)));
        rC = (float) (1.0 - std::exp (-4.6 / (std::max (0.001, s.type == 0 ? 0.02 : (double) s.release) * sr)));

        // OFF fades out with the engine that was playing; a type change re-initialises the ramps
        engine = s.type == 0 ? lastEngine : s.type;
        if (engine != lastEngine) { primed = false; lastEngine = engine; }
        if (engine == 2) beginSiren (n);
        else if (engine == 3) beginPulsar (n);
        else beginChoir (n);
        primed = true;
        return true;
    }

    // One sample: adds into L / R, returns the voice as a modulator for the others
    float tick (double fmHz, float& L, float& R)
    {
        if (! active) return 0.0f;
        const double fmDt = fmHz / sr;
        float x = 0.0f, unit = 0.0f;
        if (engine == 2) unit = x = tickSiren (fmDt);
        else if (engine == 3) unit = x = tickPulsar (fmDt);
        else { x = tickChoir (fmDt); unit = x * unitScale; }
        const float amp = level * level;
        L += x * gL.next() * amp;
        R += x * gR.next() * amp;
        if (! std::isfinite (level)) level = 0.0f;
        return unit * amp;
    }

private:
    double hzToDt (double midi, double a4) const { return std::min (0.45, a4 * std::exp2 ((midi - 69.0) / 12.0) / sr); }
    void ramp (Ramp& r, float t, int n) { if (primed) r.set (t, n); else r.reset (t); }
    void stepLevel() { level += (target > level ? aC : rC) * (target - level); }

    // CHOIR: five saws tuned as a chord. TIDE past half way lets every saw frequency-modulate the next one
    // (a ring of five), which turns the chord into a growl.
    void beginChoir (int n)
    {
        int count = 0;
        for (int i = 0; i < 5; ++i) if (s.on[i]) ++count;
        const float norm = count > 0 ? 0.42f / std::sqrt ((float) count) : 0.0f;
        unitScale = count > 0 ? 1.0f / std::sqrt ((float) count) : 0.0f;
        for (int i = 0; i < 5; ++i) ramp (inc[i], (float) hzToDt (s.rootMidi + (double) s.intervals[i], s.a4), n);
        ramp (pullR, s.pull * 1.6f, n);
        ramp (gL, s.gainL * norm, n);
        ramp (gR, s.gainR * norm, n);
        for (int i = 0; i < 5; ++i)
        {
            int m = (i + 1) % 5;
            while (m != i && ! s.on[m]) m = (m + 1) % 5;
            ringNext[i] = m;
        }
    }

    float tickChoir (double fmDt)
    {
        const float pull = pullR.next();
        float sum = 0.0f;
        float now[5];
        for (int i = 0; i < 5; ++i)
        {
            const float base = inc[i].next();
            if (! s.on[i]) { now[i] = 0.0f; continue; }
            const int m = ringNext[i];
            const double dt = (m == i ? (double) base : (double) base * (1.0 + (double) (pull * last[m]))) + fmDt;
            now[i] = saws[i].tick (dt);
            sum += now[i];
        }
        for (int i = 0; i < 5; ++i) last[i] = now[i];
        stepLevel();
        return sum;
    }

    // SIREN: a low modulator (square to triangle) sweeps a square tone exponentially (FM) and/or chops it (AM).
    // A flip-flop divider adds a sub-harmonic (tone / N); white noise for wind and surf.
    void beginSiren (int n)
    {
        ramp (inc[0], (float) hzToDt (s.rootMidi, s.a4), n);
        ramp (inc[1], s.sirenFm * 3.0f, n);           // octaves of sweep
        ramp (inc[2], s.sirenAm, n);
        ramp (inc[3], s.sirenNoise, n);
        ramp (gL, s.gainL * 0.32f, n);
        ramp (gR, s.gainR * 0.32f, n);
        modInc = (double) s.sirenRate / sr;
        div = std::max (1, (int) std::lround (s.sirenDiv));
        smooth = (float) (1.0 - std::exp (-1.0 / (0.002 * sr)));   // 2 ms edge on the square modulator
    }

    float tickSiren (double fmDt)
    {
        modPhase += modInc;
        modPhase -= std::floor (modPhase);
        const float sq = modPhase < 0.5 ? 1.0f : -1.0f;
        const float tri = (float) (modPhase < 0.5 ? 4.0 * modPhase - 1.0 : 3.0 - 4.0 * modPhase);
        modSmooth += smooth * ((sq + s.sirenWave * (tri - sq)) - modSmooth);

        const double base = (double) inc[0].next();
        const float fm = inc[1].next(), am = inc[2].next(), noise = inc[3].next();
        const double dt = base * std::exp2 ((double) (fm * modSmooth)) + fmDt;
        float x = tone.tick (dt);
        if (div > 1) x = 0.55f * x + 0.65f * sub.tick (dt / (double) div);
        x *= 1.0f - am * (0.5f - 0.5f * modSmooth);
        x += noise * 0.9f * rng.next();
        stepLevel();
        return x;
    }

    // PULSAR: one tone that morphs from triangle through shark fin and saw to square (SHARP).
    // BLOOM couples the envelope into the oscillator: it starts flat and dull and blooms into pitch and edge.
    void beginPulsar (int n)
    {
        ramp (inc[0], (float) s.rootMidi, n);
        ramp (inc[1], s.sharp, n);
        ramp (inc[2], s.bloom, n);
        ramp (inc[3], s.twinMix, n);
        ramp (inc[4], s.sub, n);
        ramp (pullR, s.vibrato * 0.5f, n);   // semitones of vibrato depth
        ramp (gL, s.gainL * 0.42f, n);
        ramp (gR, s.gainR * 0.42f, n);
        vibInc = vibRate / sr;
    }

    float tickPulsar (double fmDt)
    {
        const float root = inc[0].next(), sharp = inc[1].next(), bloom = inc[2].next();
        const float mix = inc[3].next(), subLvl = inc[4].next(), vib = pullR.next();
        stepLevel();
        const float lv = level;
        vibPhase += vibInc;
        vibPhase -= std::floor (vibPhase);
        // up to 60 cents flat and a third less edge while the voice is quiet; plus the voice's own vibrato
        const double midi = (double) root - (double) (bloom * (1.0f - lv) * 0.6f) + (double) vib * std::sin (twoPi * vibPhase);
        const float shape = 1.0f + 3.0f * std::min (1.0f, sharp * (1.0f - 0.33f * bloom * (1.0f - lv)));
        pulsar.setShape (shape, 0.5f);
        bool w; double wx;
        float x = pulsar.tick (hzToDt (midi, s.a4) + fmDt, w, wx) * (1.0f - 0.4f * mix);
        if (mix > 0.0005f)
        {
            twinOsc.setShape (shape, 0.5f);
            x += 0.7f * mix * twinOsc.tick (hzToDt (midi + (double) s.twin, s.a4) + fmDt, w, wx);
        }
        if (subLvl > 0.0005f)
        {
            subOsc.setShape (0.5f, 0.5f);   // between sine and triangle: soft
            x += 0.8f * subLvl * subOsc.tick (std::max (0.0, hzToDt (midi - 12.0, s.a4) + 0.5 * fmDt), w, wx);
        }
        return x;
    }

    double sr = 96000.0;
    VoiceSettings s;
    BlepSaw saws[5];
    BlepSquare tone, sub;
    ShapeOsc pulsar, twinOsc, subOsc;
    double vibRate = 5.0, vibPhase = 0.0, vibInc = 0.0, modPhase = 0.0, modInc = 0.0;
    Rng rng;
    float last[5] {};
    int ringNext[5] {};
    float level = 0.0f, target = 0.0f, aC = 0.0f, rC = 0.0f, modSmooth = 0.0f, smooth = 0.0f, unitScale = 0.0f;
    int engine = 1, lastEngine = 1, div = 1;
    bool primed = false, active = false;
    Ramp inc[5], pullR, gL, gR;
};
}

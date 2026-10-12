#pragma once

#include "Primitives.h"
#include "Effects.h"
#include "Wavetables.h"

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
    float sym = 0.5f;             // symmetry of the shape / pulse width 0..1

    // every type
    float character = 0.0f;       // -1 digital .. 0 clean .. +1 analog
    float gritK = 0.0f;           // GRIT for CHOIR: edge softening cutoff in multiples of the lowest saw (0 = raw)

    // QUASAR (two-operator FM)
    float qRatio = 2.0f, qIndex = 0.3f, qFb = 0.0f, qTrack = 0.5f, qFine = 0.0f;   // ratio, 0..1, 0..1, 0..1, cents

    // NEBULA (wavetables)
    int nTable = 0;
    float nPos = 0.0f, nScan = 0.3f, nDrift = 0.2f;   // 0..1, -1..1, 0..1
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
        for (int i = 0; i < 6; ++i) { oscStatic[i] = r.next(); oscDrift[i].seed (seed * 17u + (uint32_t) i * 7u + 3u); }
        wt = &Wavetables::get();
        reset();
    }

    void reset()
    {
        level = 0.0f;
        primed = false;
        modPhase = 0.0;
        modSmooth = 0.0f;
        for (auto& y : last) y = 0.0f;
        lpZ = 0.0f; holdVal = 0.0f; holdCount = 0; qPrev = 0.0f;
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
        beginCharacter (n);
        if (engine == 2) beginSiren (n);
        else if (engine == 3) beginPulsar (n);
        else if (engine == 4) beginQuasar (n);
        else if (engine == 5) beginNebula (n);
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
        if (engine == 2) unit = x = character (tickSiren (fmDt));
        else if (engine == 3) unit = x = character (tickPulsar (fmDt));
        else if (engine == 4) unit = x = character (tickQuasar (fmDt));
        else if (engine == 5) unit = x = character (tickNebula (fmDt));
        else
        {
            const float c = tickChoir (fmDt);
            unit = character (c * unitScale);
            x = unitScale > 0.0f ? unit / unitScale : 0.0f;
        }
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
        for (int i = 0; i < 5; ++i) ramp (inc[i], (float) hzToDt (s.rootMidi + (double) s.intervals[i] + scatter[i], s.a4), n);
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
        float now[5], bases[5];
        for (int i = 0; i < 5; ++i)
        {
            const float base = bases[i] = inc[i].next();
            if (! s.on[i]) { now[i] = 0.0f; continue; }
            const int m = ringNext[i];
            const double dt = (m == i ? (double) base : (double) base * (1.0 + (double) (pull * last[m]))) + fmDt;
            now[i] = saws[i].tick (dt);
            sum += now[i];
        }
        float lowest = 1.0f;
        for (int i = 0; i < 5; ++i) { last[i] = now[i]; if (s.on[i]) lowest = std::min (lowest, bases[i]); }
        stepLevel();
        // GRIT: a saw pulled down by TIDE to a few Hz is a train of clicks. Below 100 % two one-pole low passes
        // tuned to a multiple of the lowest saw soften the edges: at 50 % only the subsonic saws, at 0 % everything
        if (s.gritK <= 0.0f) { edge1 = edge2 = sum; return sum; }
        const float c = std::min (1.0f, s.gritK * 6.2831853f * lowest);
        edge1 += c * (sum - edge1);
        edge2 += c * (edge1 - edge2);
        return edge2;
    }

    // SIREN: a low modulator (square to triangle) sweeps a square tone exponentially (FM) and/or chops it (AM).
    // A flip-flop divider adds a sub-harmonic (tone / N); white noise for wind and surf.
    void beginSiren (int n)
    {
        ramp (inc[0], (float) hzToDt (s.rootMidi + scatter[0], s.a4), n);
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
        ramp (inc[0], (float) (s.rootMidi + scatter[0]), n);
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
        pulsar.setShape (shape, s.sym);
        bool w; double wx;
        float x = pulsar.tick (hzToDt (midi, s.a4) + fmDt, w, wx) * (1.0f - 0.4f * mix);
        if (mix > 0.0005f)
        {
            twinOsc.setShape (shape, s.sym);
            x += 0.7f * mix * twinOsc.tick (hzToDt (midi + (double) s.twin, s.a4) + fmDt, w, wx);
        }
        if (subLvl > 0.0005f)
        {
            subOsc.setShape (0.5f, 0.5f);   // between sine and triangle: soft
            x += 0.8f * subLvl * subOsc.tick (std::max (0.0, hzToDt (midi - 12.0, s.a4) + 0.5 * fmDt), w, wx);
        }
        return x;
    }

    // CHARACTER: analog = every oscillator detuned a little and drifting on its own, a touch of even harmonics,
    // soft saturation and rounder edges; digital = sample-rate reduction (aliasing on purpose) and fewer bits
    void beginCharacter (int n)
    {
        const double seconds = n / sr;
        const float a = std::max (0.0f, s.character);
        for (int i = 0; i < 6; ++i)
        {
            const float d = oscDrift[i].process (RandomGen::Perlin, 0.05 + 0.03 * i, 0.0f, seconds, false, 0.0);
            scatter[i] = (double) (a * (4.0f * oscStatic[i] + 6.0f * d)) / 100.0;   // semitones
        }
        ramp (charR, s.character, n);
        const double fc = 16000.0 * (1.0 - 0.7 * (double) a);
        lpC = (float) (1.0 - std::exp (-twoPi * std::min (0.45 * sr, fc) / sr));
        const float d = std::max (0.0f, -s.character);
        holdN = 1 + (int) (d * d * 28.0f * (float) (sr / 96000.0));
        quant = std::exp2 (15.0f - 12.0f * d);
    }

    float character (float x)
    {
        const float c = charR.next();
        if (c > 0.001f)
        {
            float y = x + 0.18f * c * (x * x - 0.3f);
            y = tanhFast (y * (1.0f + 1.5f * c)) / (1.0f + 0.6f * c);
            lpZ += (lpC + (1.0f - lpC) * (1.0f - c)) * (y - lpZ);   // the rounding grows with the amount
            return lpZ;
        }
        if (c < -0.001f)
        {
            if (++holdCount >= holdN) { holdCount = 0; holdVal = x; }
            const float y = holdN > 1 ? holdVal : x;
            const float q = std::round (y * quant) / quant;
            const float d = -c;
            return x + std::min (1.0f, d * 4.0f) * (q - x);   // fades in from clean over the first quarter
        }
        lpZ = x;
        return x;
    }

    // QUASAR: two sine operators, the modulator at RATIO (+ FINE cents) of the tone, phase modulation with INDEX,
    // modulator self-feedback, the index follows the level with TRACK and is limited so the sidebands stay below
    // Nyquist (no aliasing however deep)
    void beginQuasar (int n)
    {
        ramp (inc[0], (float) (s.rootMidi + scatter[0]), n);
        ramp (inc[1], s.qIndex, n);
        ramp (inc[2], s.qFb, n);
        ramp (gL, s.gainL * 0.42f, n);
        ramp (gR, s.gainR * 0.42f, n);
    }

    float tickQuasar (double fmDt)
    {
        const double midi = (double) inc[0].next();
        const float idx = inc[1].next(), fb = inc[2].next();
        stepLevel();
        const double dtc = hzToDt (midi, s.a4) + fmDt;
        const double dtm = dtc * (double) s.qRatio * std::exp2 ((double) s.qFine / 1200.0);
        cPhase += dtc; cPhase -= std::floor (cPhase);
        mPhase += dtm; mPhase -= std::floor (mPhase);
        const float m = (float) std::sin (twoPi * mPhase + 2.0 * (double) (fb * qPrev));
        qPrev = 0.5f * (qPrev + m);
        float I = 8.0f * idx * (1.0f - s.qTrack + s.qTrack * level);
        I = std::min (I, (float) std::max (0.0, 0.45 / std::max (1.0e-6, std::abs (dtm)) - 2.0));
        return (float) std::sin (twoPi * cPhase + (double) (I * m));
    }

    // NEBULA: a wavetable family; POSITION moves through the frames, SCAN lets the level move it, DRIFT wanders
    void beginNebula (int n)
    {
        ramp (inc[0], (float) (s.rootMidi + scatter[0]), n);
        ramp (inc[1], s.nPos, n);
        ramp (inc[2], s.nScan, n);
        const float wander = oscDrift[5].process (RandomGen::Perlin, 0.06, 0.0f, n / sr, false, 0.0);
        ramp (inc[3], s.nDrift * 0.35f * wander, n);
        ramp (gL, s.gainL * 0.42f, n);
        ramp (gR, s.gainR * 0.42f, n);
    }

    float tickNebula (double fmDt)
    {
        const double midi = (double) inc[0].next();
        const float pos = inc[1].next(), scan = inc[2].next(), wander = inc[3].next();
        stepLevel();
        const double dt = hzToDt (midi, s.a4) + fmDt;
        nPhase += dt;
        nPhase -= std::floor (nPhase);
        const float p = clampf (pos + scan * 0.8f * (level - 0.5f) + wander, 0.0f, 1.0f);
        return wt->read (std::min (2, std::max (0, s.nTable)), p, nPhase, Wavetables::levelFor (dt));
    }

    double sr = 96000.0;
    VoiceSettings s;
    const Wavetables* wt = nullptr;
    double scatter[6] {};
    float oscStatic[6] {};
    RandomGen oscDrift[6];
    Ramp charR;
    float lpZ = 0.0f, lpC = 1.0f, holdVal = 0.0f, quant = 32768.0f, qPrev = 0.0f;
    int holdCount = 0, holdN = 1;
    double cPhase = 0.0, mPhase = 0.0, nPhase = 0.0;
    BlepSaw saws[5];
    BlepSquare tone, sub;
    ShapeOsc pulsar, twinOsc, subOsc;
    double vibRate = 5.0, vibPhase = 0.0, vibInc = 0.0, modPhase = 0.0, modInc = 0.0;
    Rng rng;
    float last[5] {}, edge1 = 0.0f, edge2 = 0.0f;
    int ringNext[5] {};
    float level = 0.0f, target = 0.0f, aC = 0.0f, rC = 0.0f, modSmooth = 0.0f, smooth = 0.0f, unitScale = 0.0f;
    int engine = 1, lastEngine = 1, div = 1;
    bool primed = false, active = false;
    Ramp inc[5], pullR, gL, gR;
};

//==============================================================================
// LEAD: one melodic voice (the processor runs one in Mono / Legato and up to eight for Poly). Two oscillators with a
// continuous shape (sine - triangle - shark fin - saw - square), OSC 2 at an interval (or detuned) and optionally
// hard-synced to OSC 1, a sine sub one octave below, a filter per note with its own envelope (key tracking,
// velocity), ADSR envelope, glide, vibrato that fades in after a delay and deepens with key pressure.
struct LeadSettings
{
    double a4 = 440.0;
    float wave = 3.0f, octave = 0.0f;      // wave 0..4, octave -2..+2
    float osc2 = 0.07f, mix = 0.5f, sub = 0.0f;   // semitones, 0..1, 0..1
    bool sync = false;
    float attack = 0.01f, decay = 0.4f, sustain = 0.7f, release = 0.4f;
    float glide = 0.0f;                    // seconds
    float vibrato = 0.0f, vibRate = 5.0f, vibDelay = 0.4f, touchVib = 0.0f, pressure = 0.0f;   // 0..1, Hz, s, 0..1, 0..1
    float gainL = 0.5f, gainR = 0.5f;
    float velSens = 0.6f;                  // 0..1: velocity -> level and filter envelope
    float sym = 0.5f;                      // symmetry of the shape / pulse width
    // filter per note
    bool filterOn = false;                 // off when wide open with no envelope (bypassed, no colour)
    float cutoff = 20000.0f, res = 0.0f, fenvAmount = 0.0f, keyTrack = 0.5f;   // Hz, 0..1, -1..1 (x 6 octaves), 0..1
    int filterModel = 1, filterMode = 0;
    float fAttack = 0.005f, fDecay = 0.4f, fSustain = 0.3f, fRelease = 0.6f;
};

class LeadVoice
{
public:
    void prepare (double fs, uint32_t seed = 1u)
    {
        sr = fs;
        env.setSampleRate (fs);
        fenv.setSampleRate (fs);
        env.hardReset();
        fenv.hardReset();
        filter.prepare (fs);
        filter.reset();
        Rng r; r.seed (seed * 977u + 13u);
        o1.reset (r.next01()); o2.reset (r.next01()); osub.reset (0.0);
        pitch = target = 60.0; vibPhase = r.next01(); sinceOn = 0.0; velocity = 0.0f; note = -1;
    }
    void setSampleRate (double fs) { sr = fs; env.setSampleRate (fs); fenv.setSampleRate (fs); filter.prepare (fs); }
    void reset() { env.hardReset(); fenv.hardReset(); filter.reset(); note = -1; }

    // legato: an overlapping note only slides (no new envelope). fromPitch >= 0: glide in from there (Poly)
    void noteOn (double midi, float vel, bool legato, double fromPitch = -1.0)
    {
        target = midi;
        note = (int) std::lround (midi);
        velocity = vel;
        if (! legato || ! env.isActive())
        {
            if (! env.isActive())
            {
                pitch = fromPitch >= 0.0 ? fromPitch : midi;   // no glide from silence unless asked
                filter.reset();
            }
            env.noteOn (false);
            fenv.noteOn (false);
            sinceOn = 0.0;
            pcount = 0;
        }
    }
    void noteOff() { env.noteOff(); fenv.noteOff(); }
    bool isActive() const { return env.isActive(); }
    bool isReleasing() const { return env.getStage() == Envelope::Release; }
    float getLevel() const { return env.getValue(); }
    float getFilterEnv() const { return fenv.getValue() * velScale; }
    int getNote() const { return note; }
    double getPitch() const { return pitch; }

    void begin (const LeadSettings& settings, int n)
    {
        const bool wasFiltered = s.filterOn;
        s = settings;
        if (s.filterOn && ! wasFiltered) filter.reset();
        env.set (0.0f, s.attack, 0.0f, s.decay, s.sustain, s.release);
        fenv.set (0.0f, s.fAttack, 0.0f, s.fDecay, s.fSustain, s.fRelease);
        glideC = s.glide <= 0.0005f ? 1.0 : 1.0 - std::exp (-4.6 / (s.glide * sr));
        velScale = (1.0f - s.velSens) + s.velSens * velocity;
        const float amp = 0.32f * velScale;
        if (primed) { gL.set (s.gainL * amp, n); gR.set (s.gainR * amp, n); } else { gL.reset (s.gainL * amp); gR.reset (s.gainR * amp); primed = true; }
        o1.setShape (s.wave, s.sym);
        o2.setShape (s.wave, s.sym);
        osub.setShape (0.0f, 0.5f);
        baseOct = std::log2 ((double) std::max (20.0f, s.cutoff));
        osc2Ratio = std::exp2 ((double) s.osc2 / 12.0);
    }

    void tick (float& L, float& R)
    {
        if (! env.isActive()) { gL.next(); gR.next(); return; }
        pitch += glideC * (target - pitch);
        // pitch, vibrato and the oscillator increments every 4 samples (vibrato < 12 Hz, glide is smooth anyway)
        if ((pcount++ & 3) == 0)
        {
            sinceOn += 4.0 / sr;
            vibPhase += 4.0 * s.vibRate / sr;
            vibPhase -= std::floor (vibPhase);
            const double fadeIn = s.vibDelay <= 0.001f ? 1.0 : std::min (1.0, sinceOn / (double) s.vibDelay);
            const double vibSemis = ((double) s.vibrato * fadeIn + (double) (s.touchVib * s.pressure)) * std::sin (twoPi * vibPhase);
            m = pitch + 12.0 * (double) s.octave + vibSemis;
            dt1 = s.a4 * std::exp2 ((m - 69.0) / 12.0) / sr;
        }
        bool wrapped = false; double wrapX = 0.0;
        const float x1 = o1.tick (std::min (0.45, dt1), wrapped, wrapX);
        const double d2 = std::min (0.45, dt1 * osc2Ratio);
        const float x2 = s.sync && wrapped ? o2.tickSynced (d2, wrapX, 0.0) : o2.tick (d2, w2, x2w);
        float x = x1 * (1.0f - 0.5f * s.mix) + x2 * s.mix * 0.8f;
        if (s.sub > 0.0005f) x += 0.7f * s.sub * osub.tick (std::min (0.45, dt1 * 0.5), w2, x2w);
        const float e = env.next();
        const float fe = fenv.next();
        if (s.filterOn)
        {
            // cutoff: knob + key tracking + envelope (scaled by velocity), new target every 8 samples, smoothed
            if ((fcount++ & 7) == 0)
            {
                const double oct = baseOct + (double) s.keyTrack * (m - 60.0) / 12.0 + 6.0 * (double) (s.fenvAmount * fe * velScale);
                const double fc = std::min (0.45 * sr, std::max (16.0, std::exp2 (oct)));
                gTarget = (float) std::tan (pi * fc / sr);
            }
            gNow += 0.3f * (gTarget - gNow);
            x = filter.process (x, gNow, s.res, 0.0f, s.filterMode, s.filterModel);
        }
        L += x * gL.next() * e;
        R += x * gR.next() * e;
    }

private:
    double sr = 96000.0, pitch = 60.0, target = 60.0, glideC = 1.0, vibPhase = 0.0, sinceOn = 0.0, x2w = 0.0, baseOct = 14.0;
    double m = 60.0, dt1 = 0.0, osc2Ratio = 1.0;
    unsigned pcount = 0;
    bool w2 = false, primed = false;
    float velocity = 0.0f, velScale = 1.0f, gTarget = 0.5f, gNow = 0.5f;
    int note = -1;
    unsigned fcount = 0;
    LeadSettings s;
    ShapeOsc o1, o2, osub;
    Envelope env, fenv;
    AnalogFilter filter;
    Ramp gL, gR;
};
}

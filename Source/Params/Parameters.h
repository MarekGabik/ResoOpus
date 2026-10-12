#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All automatable parameters are described by one table per section (ParamDef). The tables are append-only:
// IDs are stable strings and must never be renamed; new parameters go to the END of their table,
// because modulation destinations are stored as table positions (see Mod::destCode).

enum class Kind { Float, Bool, Choice };

enum class Unit
{
    Plain,      // value with one decimal
    Scale10,    // 0..10
    Semitones,
    Hz,
    Seconds,    // shown as ms below one second
    Percent,    // stored 0..100
    Decibels,
    Pan,        // -1..1
    Note,       // MIDI note number with cents ("D2 +4")
    Interval,   // semitones relative to the voice root ("+7 st", "+19 -8c")
    Tide,       // 0..100 %: transposition down, then pull (FM) between the oscillators
    Divider,    // 1..16 sub-harmonic ("off", "1:3")
    Gravity,    // 0..100 % with its zone ("ORBIT 42 %")
    Wave,       // continuous oscillator shape 0..4
    Choice,
    Toggle
};

struct ParamDef
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    Unit unit;
    float skew = 1.0f;          // < 1 = finer at the start; for symmetric: finer around the centre
    bool symmetric = false;
    bool logarithmic = false;   // frequency and time style mapping
    juce::StringArray choices {};
    const char* tip = "";
};

namespace Params
{
    constexpr int numVoices = 4;        // two pairs: 1-2 and 3-4
    constexpr int numOscs = 5;          // CHOIR oscillators per voice
    constexpr int numModSlots = 24;

    const std::vector<ParamDef>& voiceDefs();    // prefixed "v1_" .. "v4_"
    const std::vector<ParamDef>& globalDefs();
    const std::vector<ParamDef>& modSlotDefs();  // prefixed "m01_" .. "m24_"

    juce::String voiceID (int voice, const juce::String& suffix);   // voice 0..3 -> "v1_..."
    juce::String modID (int slot, const juce::String& suffix);      // slot 0.. -> "m01_..."

    // Factory default of a voice parameter can differ per voice (tuning, type, pan, chord, hold)
    float voiceDefault (int voice, int tableIndex);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Lookup of the definition behind a full parameter ID (with prefix)
    const ParamDef* findDef (const juce::String& fullID);

    juce::String format (const ParamDef&, float realValue, bool withUnit = true);
    float parse (const ParamDef&, const juce::String& text, float fallback);   // "2.5k", "A4", "120 ms", "D2+4", "L 30"

    juce::String noteName (double midiNote, bool withCents = false);   // 60 = C3 (Ableton convention)

    // CHOIR chord templates (intervals of oscillators 1-5 in semitones)
    const juce::StringArray& chordNames();
    const float* chordIntervals (int chord);   // 5 values
    int matchChord (const float* intervals);  // -1 = custom

    // FILL: scales used to spread the four voices
    const juce::StringArray& scaleNames();
    const juce::Array<int>& scaleSteps (int scale);   // semitone offsets within one octave

    // TIDE: transposition (semitones, <= 0) and pull between the oscillators (0..1)
    inline float tideSemitones (float tide01)
    {
        const float t = juce::jlimit (0.0f, 1.0f, tide01);
        return t <= 0.5f ? -24.0f * std::pow (t * 2.0f, 1.3f) : -24.0f - 36.0f * (t - 0.5f) * 2.0f;
    }
    // GRAVITY zones (0..1): STILL < 0.25 <= ORBIT < 0.55 <= TURBULENCE < 0.85 <= COLLAPSE
    juce::String gravityZone (float g01);
    inline int gravityZoneIndex (float g01) { return g01 < 0.25f ? 0 : (g01 < 0.55f ? 1 : (g01 < 0.85f ? 2 : 3)); }
    // Frequency deviation (Hz per unit of modulator) at a GRAVITY setting: 0 in STILL, growing through the zones
    inline float gravityDeviation (float g01)
    {
        const float g = juce::jlimit (0.0f, 1.0f, g01);
        if (g < 0.25f) return 0.0f;
        if (g < 0.55f) { const float t = (g - 0.25f) / 0.3f; return 40.0f * t * t; }
        if (g < 0.85f) { const float t = (g - 0.55f) / 0.3f; return 40.0f + 360.0f * t * std::sqrt (t); }   // stronger just before COLLAPSE
        const float t = (g - 0.85f) / 0.15f;
        return 400.0f + 600.0f * t * t;
    }
    // Share of the ring (all four voices in one loop) vs the pairs, for PATH = Auto
    inline float gravityRing (float g01) { return juce::jlimit (0.0f, 1.0f, (g01 - 0.55f) / 0.15f); }
    // Feedback of the instrument's output into the modulators
    inline float gravityFeedback (float g01) { return g01 < 0.55f ? 0.0f : (g01 < 0.85f ? 0.5f * (g01 - 0.55f) / 0.3f : 0.5f + 0.5f * (g01 - 0.85f) / 0.15f); }

    inline float tidePull (float tide01)
    {
        const float t = juce::jlimit (0.0f, 1.0f, tide01);
        return t <= 0.5f ? 0.0f : std::pow ((t - 0.5f) * 2.0f, 1.5f);
    }
}

// Modulation sources, destinations and functions
namespace Mod
{
    // Source list is append-only (stored as choice index)
    enum Source { None = 0, Velocity, ModWheel, Pressure, PitchBend, Constant, Lfo1, Lfo2, Random, LeadEnv, ModEnv, numSources };

    const juce::StringArray& sourceNames();
    juce::String shortSourceName (int src);
    juce::Colour sourceColour (int src);
    bool isBipolar (int src);

    // Destination codes: 0 = none, block * 200 + table index. Blocks 1..4 = voices, 5 = global table.
    constexpr int blockSize = 200;
    constexpr int globalBlock = 5;
    int destCode (const juce::String& fullParamID);
    juce::String destParamID (int code);
    juce::String destName (int code);
    juce::String blockName (int block);
    juce::Array<int> allDestinations();   // modulatable (float) parameters
    constexpr int maxDestCode = (globalBlock + 1) * blockSize - 1;

    const juce::StringArray& functionNames();
    double divisionBeats (int index);     // LFO / RANDOM sync divisions in quarter notes
    enum Function { FnNone = 0, Scale, Offset, LowClip, HighClip, Exponential, NormExponential,
                    LowPass, HighPass, Slew, LowSlew, HighSlew, SampleHold, Bounce };
}

// Positions in the tables above (checked by the tests against the IDs). Keep in sync, append only.
namespace VP
{
    enum : int
    {
        type = 0, tune, int1, int2, int3, int4, int5, on1, on2, on3, on4, on5,
        tide, attack, release, volume, pan, hold,
        siren_rate, siren_fm, siren_am, siren_div, siren_noise, siren_wave,
        sharp, bloom, twin, twin_mix, sub, vibrato,
        sym, character, q_ratio, q_index, q_fb, q_track, q_fine, n_table, n_pos, n_scan, n_drift,
        count
    };
    enum Type { Off = 0, Choir, Siren, Pulsar, Quasar, Nebula };
}

namespace GP
{
    enum : int
    {
        master_vol = 0, oversampling, play_mode, touch, plates_base, snap, snap_scale, snap_root, a4, pb_range, pitch,
        gravity, gravity_path, dust,
        flt_cutoff, flt_res, flt_drive, flt_mode, flt_spread,
        echo_time1, echo_time2, echo_fb, echo_mix, echo_mod, echo_self, echo_voices,
        space_algo, space_x, space_y, space_z, space_blend,
        lead_on, lead_wave, lead_octave, lead_osc2, lead_mix, lead_sync, lead_sub,
        lead_attack, lead_decay, lead_sustain, lead_release,
        lead_glide, lead_vib, lead_vib_rate, lead_vib_delay, lead_touch_vib, lead_level, lead_pan,
        arp_on, arp_mode, arp_rate, arp_octaves, arp_latch,
        flt_model,
        drone_on, lead_voices, lead_vel,
        lead_flt_cutoff, lead_flt_res, lead_flt_env, lead_flt_key, lead_flt_model, lead_flt_mode,
        lead_fenv_attack, lead_fenv_decay, lead_fenv_sustain, lead_fenv_release,
        flt2_on, flt2_cutoff, flt2_res, flt2_drive, flt2_mode, flt2_model, flt_route,
        lfo1_rate, lfo1_shape, lfo1_sync, lfo1_div, lfo1_phase, lfo1_retrig,
        lfo2_rate, lfo2_shape, lfo2_sync, lfo2_div, lfo2_phase, lfo2_retrig,
        rnd_rate, rnd_mode, rnd_slew, rnd_sync, rnd_div,
        lead_sym, grit,
        menv_attack, menv_decay, menv_sustain, menv_release,
        echo_mode,
        mot_on, mot_mode, mot_rate, mot_sync, mot_div, mot_depth, mot_x, mot_mix,
        count
    };
    enum PlayMode { Plates = 0, Keys };
    enum Touch { TouchVelocity = 0, TouchAftertouch, TouchBoth };
    enum Snap { SnapFree = 0, SnapChromatic, SnapScale };
    enum Path { PathAuto = 0, PathPairs, PathRing };
    enum Route { RouteSerial = 0, RouteParallel, RouteSplit, RouteStereo };
}

namespace MP
{
    enum : int { on = 0, src, dst, amt, bi, ctl, ctlamt, fn, fnamt, count };
}

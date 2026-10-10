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
    constexpr int numVoices = 8;
    constexpr int numGroups = 2;
    constexpr int voicesPerGroup = 4;
    constexpr int numOscs = 5;          // CHOIR oscillators per voice
    constexpr int numModSlots = 24;

    const std::vector<ParamDef>& voiceDefs();    // prefixed "v1_" .. "v8_"
    const std::vector<ParamDef>& groupDefs();    // prefixed "g1_" / "g2_"
    const std::vector<ParamDef>& globalDefs();
    const std::vector<ParamDef>& modSlotDefs();  // prefixed "m01_" .. "m24_"

    juce::String voiceID (int voice, const juce::String& suffix);   // voice 0..7 -> "v1_..."
    juce::String groupID (int group, const juce::String& suffix);   // group 0/1 -> "g1_..."
    juce::String modID (int slot, const juce::String& suffix);      // slot 0.. -> "m01_..."

    // Factory default of a voice parameter can differ per voice (tuning, type, pan, chord)
    float voiceDefault (int voice, int tableIndex);
    float groupDefault (int group, int tableIndex);

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

    // FILL: scales used to spread the four voices of a group
    const juce::StringArray& scaleNames();
    const juce::Array<int>& scaleSteps (int scale);   // semitone offsets within one octave

    // TIDE: transposition (semitones, <= 0) and pull between the oscillators (0..1)
    inline float tideSemitones (float tide01)
    {
        const float t = juce::jlimit (0.0f, 1.0f, tide01);
        return t <= 0.5f ? -24.0f * std::pow (t * 2.0f, 1.3f) : -24.0f - 36.0f * (t - 0.5f) * 2.0f;
    }
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
    enum Source { None = 0, Velocity, ModWheel, Pressure, PitchBend, Constant, numSources };

    const juce::StringArray& sourceNames();
    juce::String shortSourceName (int src);
    juce::Colour sourceColour (int src);
    bool isBipolar (int src);

    // Destination codes: 0 = none, block * 200 + table index.
    // Blocks 1..8 = voices, 9..10 = groups, 11 = global table.
    constexpr int blockSize = 200;
    constexpr int firstGroupBlock = 9, globalBlock = 11;
    int destCode (const juce::String& fullParamID);
    juce::String destParamID (int code);
    juce::String destName (int code);
    juce::String blockName (int block);
    juce::Array<int> allDestinations();   // modulatable (float) parameters
    constexpr int maxDestCode = (globalBlock + 1) * blockSize - 1;

    const juce::StringArray& functionNames();
    enum Function { FnNone = 0, Scale, Offset, LowClip, HighClip, Exponential, NormExponential,
                    LowPass, HighPass, Slew, LowSlew, HighSlew, SampleHold, Bounce };
}

// Positions in the tables above (checked by the tests against the IDs). Keep in sync, append only.
namespace VP
{
    enum : int
    {
        type = 0, tune, int1, int2, int3, int4, int5, on1, on2, on3, on4, on5,
        tide, attack, release, volume, pan,
        count
    };
    enum Type { Choir = 0, Siren, Pulsar };
}

namespace GRP
{
    enum : int { hold = 0, pitch, count };
}

namespace GP
{
    enum : int
    {
        master_vol = 0, oversampling, play_mode, touch, plates_base, snap, snap_scale, snap_root, a4, pb_range,
        count
    };
    enum PlayMode { Plates = 0, Keys };
    enum Touch { TouchVelocity = 0, TouchAftertouch, TouchBoth };
    enum Snap { SnapFree = 0, SnapChromatic, SnapScale };
}

namespace MP
{
    enum : int { on = 0, src, dst, amt, bi, ctl, ctlamt, fn, fnamt, count };
}

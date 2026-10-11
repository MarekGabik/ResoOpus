#include "Parameters.h"

using namespace juce;

namespace
{
ParamDef F (const char* id, const char* name, float mn, float mx, float df, Unit u, const char* tip,
            float skew = 1.0f, bool sym = false, bool log = false)
{
    ParamDef d { id, name, Kind::Float, mn, mx, df, u, skew, sym, log, {}, tip };
    return d;
}
ParamDef LogF (const char* id, const char* name, float mn, float mx, float df, Unit u, const char* tip)
{
    return F (id, name, mn, mx, df, u, tip, 1.0f, false, true);
}
ParamDef B (const char* id, const char* name, bool df, const char* tip)
{
    ParamDef d { id, name, Kind::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, Unit::Toggle, 1.0f, false, false, {}, tip };
    return d;
}
ParamDef C (const char* id, const char* name, StringArray choices, int df, const char* tip)
{
    ParamDef d { id, name, Kind::Choice, 0.0f, (float) (choices.size() - 1), (float) df, Unit::Choice, 1.0f, false, false, choices, tip };
    return d;
}

const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
const std::vector<ParamDef>& Params::voiceDefs()
{
    static const std::vector<ParamDef> defs = []
    {
        std::vector<ParamDef> d;
        d.push_back (C ("type", "Type", { "Off", "Choir", "Siren", "Pulsar" }, 1,
                        "Off: the voice is silent. Choir: five saw oscillators tuned as a chord. Siren: sirens, birds, monsters and noise. Pulsar: one tone from soft to sharp that blooms with its envelope"));
        d.push_back (F ("tune", "Tune", 12.0f, 108.0f, 50.0f, Unit::Note,
                        "Pitch of the voice (the root of its chord). Free and microtonal; Shift-drag for cents, double-click to type a note like D2+12"));
        static const char* intNames[] = { "Osc 1", "Osc 2", "Osc 3", "Osc 4", "Osc 5" };
        static const char* intIds[] = { "int1", "int2", "int3", "int4", "int5" };
        for (int i = 0; i < Params::numOscs; ++i)
            d.push_back (F (intIds[i], intNames[i], -24.0f, 36.0f, 0.0f, Unit::Interval,
                            "Interval of this oscillator above the voice root in semitones (Shift-drag for cents). CHORD fills all five at once"));
        static const char* onIds[] = { "on1", "on2", "on3", "on4", "on5" };
        static const char* onNames[] = { "Osc 1 On", "Osc 2 On", "Osc 3 On", "Osc 4 On", "Osc 5 On" };
        for (int i = 0; i < Params::numOscs; ++i)
            d.push_back (B (onIds[i], onNames[i], true, "Switches this oscillator of the chord on or off"));
        d.push_back (F ("tide", "Tide", 0.0f, 100.0f, 0.0f, Unit::Tide,
                        "Pulls the voice down: up to 50 % it transposes the whole chord down, beyond 50 % it sinks further and the oscillators start to pull on each other (growl, metallic overtones, crackle at the bottom)"));
        d.push_back (LogF ("attack", "Attack", 0.001f, 30.0f, 3.0f, Unit::Seconds, "How slowly the voice swells in (also when HOLD brings it in)"));
        d.push_back (LogF ("release", "Release", 0.001f, 30.0f, 6.0f, Unit::Seconds, "How long the voice fades out after you let go"));
        d.push_back (F ("volume", "Volume", -60.0f, 6.0f, -2.0f, Unit::Decibels, "Level of the voice in the mix", 2.5f));
        d.push_back (F ("pan", "Pan", -1.0f, 1.0f, 0.0f, Unit::Pan, "Position of the voice left / right"));
        d.push_back (F ("hold", "Hold", 0.0f, 100.0f, 0.0f, Unit::Percent,
                        "Drone level of this voice without touching anything. Playing the voice makes it louder than HOLD"));
        // SIREN
        d.push_back (LogF ("siren_rate", "Siren Rate", 0.05f, 300.0f, 0.4f, Unit::Hz,
                           "Speed of the siren's modulator: slow sweeps, warbles, buzzing (above ~30 Hz it becomes a new timbre)"));
        d.push_back (F ("siren_fm", "Siren FM", 0.0f, 100.0f, 20.0f, Unit::Percent, "How far the modulator sweeps the pitch (up to +-3 octaves)"));
        d.push_back (F ("siren_am", "Siren AM", 0.0f, 100.0f, 0.0f, Unit::Percent, "How much the modulator chops the tone into beeps"));
        d.push_back (F ("siren_div", "Divider", 1.0f, 16.0f, 1.0f, Unit::Divider,
                        "Adds a sub-harmonic of the tone (1:2 = octave below, 1:3, 1:5 ... growl and monsters)", 0.5f));
        d.push_back (F ("siren_noise", "Noise", 0.0f, 100.0f, 0.0f, Unit::Percent, "White noise: wind, surf, static"));
        d.push_back (F ("siren_wave", "Siren Wave", 0.0f, 100.0f, 0.0f, Unit::Percent, "Modulator shape: 0 = square (two-tone siren), 100 = triangle (wail)"));
        // PULSAR
        d.push_back (F ("sharp", "Sharp", 0.0f, 100.0f, 20.0f, Unit::Percent, "Shape of the tone: triangle (soft) through shark fin and saw to square (sharp)"));
        d.push_back (F ("bloom", "Bloom", 0.0f, 100.0f, 40.0f, Unit::Percent,
                        "The envelope bends the tone: it starts a little flat and dull and blooms into pitch and edge as it swells"));
        d.push_back (F ("twin", "Twin", -12.0f, 24.0f, 7.0f, Unit::Interval,
                        "Interval of a second pulsar tone above the first (a few cents off unison = slow beating, 7 = fifth)"));
        d.push_back (F ("twin_mix", "Blend", 0.0f, 100.0f, 0.0f, Unit::Percent, "How much of the twin tone you hear"));
        d.push_back (F ("sub", "Sub", 0.0f, 100.0f, 0.0f, Unit::Percent, "A soft tone one octave below"));
        d.push_back (F ("vibrato", "Vibrato", 0.0f, 100.0f, 0.0f, Unit::Percent,
                        "Pitch vibrato up to 50 cents; every voice has its own slightly different speed, so they never wobble together"));
        return d;
    }();
    return defs;
}

const std::vector<ParamDef>& Params::globalDefs()
{
    static const std::vector<ParamDef> defs = []
    {
        std::vector<ParamDef> d;
        d.push_back (F ("master_vol", "Volume", -60.0f, 12.0f, 0.0f, Unit::Decibels, "Output volume", 2.5f));
        d.push_back (C ("oversampling", "Oversampling", { "1x", "2x", "4x" }, 1, "Voices run at a higher rate: less aliasing, more CPU"));
        d.push_back (C ("play_mode", "Play Mode", { "Plates", "Keys" }, 0,
                        "Plates: four white keys (C3-F3) play the four voices with their own tuning. Keys: a played note retunes a voice to that note"));
        d.push_back (C ("touch", "Touch", { "Velocity", "Aftertouch", "Both" }, 0,
                        "What makes a voice louder above HOLD: how hard you strike (velocity), how hard you press after the strike (aftertouch), or both"));
        d.push_back (F ("plates_base", "Plates From", 36.0f, 84.0f, 60.0f, Unit::Note, "Lowest key of the four plates (white keys from here)"));
        d.push_back (C ("snap", "Snap", { "Free", "Chromatic", "Scale" }, 0,
                        "Free: microtonal tuning. Chromatic: voices snap to semitones. Scale: voices snap to the notes of the chosen scale"));
        d.push_back (C ("snap_scale", "Scale", Params::scaleNames(), 2, "Scale used by SNAP and FILL"));
        d.push_back (C ("snap_root", "Root", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 2, "Root note of the scale"));
        d.push_back (F ("a4", "Reference A", 415.0f, 466.0f, 440.0f, Unit::Hz, "Concert pitch of A (440 Hz standard, 432 and 415 for other tunings)"));
        d.push_back (F ("pb_range", "Pitch Bend Range", 0.0f, 24.0f, 2.0f, Unit::Semitones, "Pitch bend range in semitones (all voices)"));
        d.push_back (F ("pitch", "Pitch", -24.0f, 24.0f, 0.0f, Unit::Semitones, "Transposes all four voices together; their intervals stay the same"));
        d.push_back (F ("gravity", "Gravity", 0.0f, 100.0f, 15.0f, Unit::Gravity,
                        "From tuning to chaos. STILL: pure tuning, slow beating. ORBIT: the voices of a pair pull on each other (FM), the tone trembles. "
                        "TURBULENCE: all four voices close into one loop and the output feeds back. COLLAPSE: the loop runs away and pulses on its own"));
        d.push_back (C ("gravity_path", "Gravity Path", { "Auto", "Pairs", "Ring" }, 0,
                        "Who pulls on whom. Auto: pairs (1-2, 3-4) up to the middle, then one ring of all four. Pairs or Ring: fixed"));
        d.push_back (F ("dust", "Dust", 0.0f, 100.0f, 20.0f, Unit::Percent,
                        "The analog life: slow drift of the tuning, voices leaking into each other and a breath of background noise. 0 = clean digital"));
        // FILTER
        d.push_back (LogF ("flt_cutoff", "Cutoff", 20.0f, 20000.0f, 20000.0f, Unit::Hz, "Filter cutoff (left and right, see SPREAD)"));
        d.push_back (F ("flt_res", "Resonance", 0.0f, 10.0f, 0.0f, Unit::Scale10, "Filter resonance; near 10 it sings on its own and keeps the lows"));
        d.push_back (F ("flt_drive", "Drive", 0.0f, 10.0f, 0.0f, Unit::Scale10, "Drives the filter into its dirty, growling saturation"));
        d.push_back (C ("flt_mode", "Filter Mode", { "Low Pass", "Band Pass", "High Pass" }, 0,
                        "Low pass: darker. Band pass: thin, vocal. High pass: takes the lows away for clear, high drones"));
        d.push_back (F ("flt_spread", "Spread", 0.0f, 100.0f, 10.0f, Unit::Percent, "Detunes the left and right filters against each other: a wider stereo image"));
        // ECHO
        d.push_back (LogF ("echo_time1", "Echo Time 1", 0.005f, 2.0f, 0.34f, Unit::Seconds, "Delay time of the left line"));
        d.push_back (LogF ("echo_time2", "Echo Time 2", 0.005f, 2.0f, 0.52f, Unit::Seconds, "Delay time of the right line"));
        d.push_back (F ("echo_fb", "Echo Feedback", 0.0f, 100.0f, 35.0f, Unit::Percent,
                        "Repeats; above ~55 % the echo starts to sing on its own (standing waves you can play with TIME)"));
        d.push_back (F ("echo_mix", "Echo Mix", 0.0f, 100.0f, 0.0f, Unit::Percent, "Dry / echo"));
        d.push_back (F ("echo_mod", "Echo Mod", 0.0f, 100.0f, 10.0f, Unit::Percent, "How much the delay times wobble (from the source below)"));
        d.push_back (C ("echo_self", "Echo Mod Source", { "Self", "LFO" }, 0,
                        "Self: the echo modulates its own time (resonances turn unstable). LFO: a slow sine"));
        d.push_back (F ("echo_voices", "Echo To Voices", 0.0f, 100.0f, 0.0f, Unit::Percent,
                        "Feeds the echo back into the voices as frequency modulation (works with GRAVITY: the whole instrument becomes one loop)"));
        // SPACE
        d.push_back (C ("space_algo", "Space", { "Aurora", "Void", "Runner", "Saturn", "Comet" }, 1,
                        "Aurora: shimmer. Void: huge dark space, freeze. Runner: rising / falling repeats. Saturn: metallic ring. Comet: dirty singing delay"));
        d.push_back (F ("space_x", "Space X", 0.0f, 100.0f, 30.0f, Unit::Percent, "First control of the SPACE algorithm (see its label)"));
        d.push_back (F ("space_y", "Space Y", 0.0f, 100.0f, 30.0f, Unit::Percent, "Second control of the SPACE algorithm (see its label)"));
        d.push_back (F ("space_z", "Space Z", 0.0f, 100.0f, 55.0f, Unit::Percent, "Third control of the SPACE algorithm (see its label)"));
        d.push_back (F ("space_blend", "Space Blend", 0.0f, 100.0f, 30.0f, Unit::Percent, "Dry / SPACE"));
        // LEAD
        d.push_back (B ("lead_on", "Lead", false,
                        "The melodic LEAD layer. When on, every key except the four plates plays it (in KEYS mode all keys play it)"));
        d.push_back (F ("lead_wave", "Lead Wave", 0.0f, 4.0f, 3.0f, Unit::Wave, "Sine - triangle - shark fin - saw - square, morphs in between"));
        d.push_back (C ("lead_octave", "Lead Octave", { "-2", "-1", "0", "+1", "+2" }, 2, "Octave of the lead"));
        d.push_back (F ("lead_osc2", "Osc 2", -12.0f, 12.0f, 0.07f, Unit::Interval, "Interval of the second oscillator (a few cents = chorus, 7 = fifth)"));
        d.push_back (F ("lead_mix", "Osc 2 Level", 0.0f, 100.0f, 50.0f, Unit::Percent, "Level of the second oscillator"));
        d.push_back (B ("lead_sync", "Sync", false, "Hard sync: OSC 2 restarts with every cycle of OSC 1. Set OSC 2 a few semitones up for the classic sync tear"));
        d.push_back (F ("lead_sub", "Sub", 0.0f, 100.0f, 0.0f, Unit::Percent, "A sine one octave below"));
        d.push_back (LogF ("lead_attack", "Lead Attack", 0.001f, 10.0f, 0.01f, Unit::Seconds, "Lead envelope attack"));
        d.push_back (LogF ("lead_decay", "Lead Decay", 0.001f, 30.0f, 0.6f, Unit::Seconds, "Lead envelope decay"));
        d.push_back (F ("lead_sustain", "Lead Sustain", 0.0f, 100.0f, 70.0f, Unit::Percent, "Lead envelope sustain level"));
        d.push_back (LogF ("lead_release", "Lead Release", 0.001f, 30.0f, 0.8f, Unit::Seconds, "Lead envelope release"));
        d.push_back (F ("lead_glide", "Glide", 0.0f, 2.0f, 0.08f, Unit::Seconds, "Time the pitch slides between overlapping notes", 0.4f));
        d.push_back (F ("lead_vib", "Vibrato", 0.0f, 100.0f, 15.0f, Unit::Percent, "Vibrato depth (100 % = one semitone)"));
        d.push_back (LogF ("lead_vib_rate", "Vibrato Rate", 0.5f, 12.0f, 5.2f, Unit::Hz, "Vibrato speed"));
        d.push_back (F ("lead_vib_delay", "Vibrato Delay", 0.0f, 3.0f, 0.5f, Unit::Seconds, "The vibrato fades in over this time after each new note", 0.5f));
        d.push_back (F ("lead_touch_vib", "Touch Vibrato", 0.0f, 100.0f, 40.0f, Unit::Percent, "Extra vibrato from pressing harder (aftertouch)"));
        d.push_back (F ("lead_level", "Lead Volume", -60.0f, 6.0f, -3.0f, Unit::Decibels, "Level of the lead in the mix", 2.5f));
        d.push_back (F ("lead_pan", "Lead Pan", -1.0f, 1.0f, 0.0f, Unit::Pan, "Position of the lead left / right"));
        d.push_back (B ("arp_on", "Arp", false, "Arpeggiator: plays the held lead keys one after another, locked to the song tempo"));
        d.push_back (C ("arp_mode", "Arp Direction", { "Up", "Down", "Ping-Pong", "Random" }, 0, "Order of the arpeggio"));
        d.push_back (C ("arp_rate", "Arp Rate", { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" }, 3, "Note length of the arpeggio (song tempo)"));
        d.push_back (C ("arp_octaves", "Arp Octaves", { "1", "2", "3", "4" }, 0, "How many octaves the arpeggio climbs"));
        d.push_back (B ("arp_latch", "Latch", false, "Keeps the arpeggio running after you let go of the keys"));
        // appended later (parameter IDs stay in their order)
        d.push_back (C ("flt_model", "Filter Model", { "Growl", "Ladder", "Diode", "Scream", "Silk" }, 0,
                        "Character of the filter. Growl: dirty 12 dB that growls. Ladder: round 24 dB transistor ladder that sings. "
                        "Diode: rubbery squelch, bites more than it rings. Scream: harsh 12 dB, screeches when pushed. Silk: clean and smooth"));
        d.push_back (B ("drone_on", "Drone", true,
                        "The four drone voices. Off with the LEAD on = LEAD only: every key, the plates too, plays the LEAD"));
        d.push_back (C ("lead_voices", "Lead Voices", { "Mono", "Legato", "Poly 4", "Poly 8" }, 1,
                        "Mono: one note, every note restarts the envelopes. Legato: one note, overlapping notes only slide. Poly: chords and melodies over the drone"));
        d.push_back (F ("lead_vel", "Lead Velocity", 0.0f, 100.0f, 60.0f, Unit::Percent, "How much velocity sets the level and the filter envelope of each note"));
        d.push_back (LogF ("lead_flt_cutoff", "Lead Cutoff", 20.0f, 20000.0f, 20000.0f, Unit::Hz, "Cutoff of the filter of every lead note"));
        d.push_back (F ("lead_flt_res", "Lead Resonance", 0.0f, 10.0f, 0.0f, Unit::Scale10, "Resonance of the lead filter"));
        d.push_back (F ("lead_flt_env", "Lead Filter Env", -100.0f, 100.0f, 0.0f, Unit::Percent,
                        "How far the filter envelope moves the cutoff of each note (100 % = 6 octaves up, negative = down)"));
        d.push_back (F ("lead_flt_key", "Lead Key Track", 0.0f, 100.0f, 50.0f, Unit::Percent, "The cutoff follows the played note (100 % = one octave per octave)"));
        d.push_back (C ("lead_flt_model", "Lead Filter Model", { "Growl", "Ladder", "Diode", "Scream", "Silk" }, 1, "Character of the lead filter (same models as FILTER)"));
        d.push_back (C ("lead_flt_mode", "Lead Filter Mode", { "Low Pass", "Band Pass", "High Pass" }, 0, "Low pass, band pass or high pass for every lead note"));
        d.push_back (LogF ("lead_fenv_attack", "Lead Filter Attack", 0.001f, 10.0f, 0.005f, Unit::Seconds, "Filter envelope attack"));
        d.push_back (LogF ("lead_fenv_decay", "Lead Filter Decay", 0.001f, 30.0f, 0.4f, Unit::Seconds, "Filter envelope decay"));
        d.push_back (F ("lead_fenv_sustain", "Lead Filter Sustain", 0.0f, 100.0f, 30.0f, Unit::Percent, "Filter envelope sustain level"));
        d.push_back (LogF ("lead_fenv_release", "Lead Filter Release", 0.001f, 30.0f, 0.6f, Unit::Seconds, "Filter envelope release"));
        d.push_back (B ("flt2_on", "Filter B", false, "Second filter. ROUTING sets how it works with FILTER A"));
        d.push_back (LogF ("flt2_cutoff", "Filter B Cutoff", 20.0f, 20000.0f, 250.0f, Unit::Hz, "Cutoff of FILTER B"));
        d.push_back (F ("flt2_res", "Filter B Resonance", 0.0f, 10.0f, 0.0f, Unit::Scale10, "Resonance of FILTER B"));
        d.push_back (F ("flt2_drive", "Filter B Drive", 0.0f, 10.0f, 0.0f, Unit::Scale10, "Drive of FILTER B"));
        d.push_back (C ("flt2_mode", "Filter B Mode", { "Low Pass", "Band Pass", "High Pass" }, 2, "Low pass, band pass or high pass"));
        d.push_back (C ("flt2_model", "Filter B Model", { "Growl", "Ladder", "Diode", "Scream", "Silk" }, 4, "Character of FILTER B"));
        d.push_back (C ("flt_route", "Filter Routing", { "Serial", "Parallel", "Split", "Stereo" }, 0,
                        "Serial: A then B (e.g. low pass + high pass = a band). Parallel: A and B side by side, mixed. "
                        "Split: the drone through A, the LEAD through B. Stereo: A on the left, B on the right"));
        return d;
    }();
    return defs;
}

const std::vector<ParamDef>& Params::modSlotDefs()
{
    static const std::vector<ParamDef> defs = []
    {
        std::vector<ParamDef> d;
        d.push_back (B ("on", "On", true, "Modulation on / off"));
        d.push_back (C ("src", "Source", Mod::sourceNames(), 0, "Modulation source"));
        ParamDef dst { "dst", "Destination", Kind::Float, 0.0f, (float) Mod::maxDestCode, 0.0f, Unit::Plain, 1.0f, false, false, {}, "Modulated parameter" };
        d.push_back (dst);
        d.push_back (F ("amt", "Amount", -1.0f, 1.0f, 0.25f, Unit::Pan, "Depth (negative = inverted)"));
        d.push_back (B ("bi", "Bipolar", true, "Bipolar: swings around the knob position. Unipolar: only one direction"));
        d.push_back (C ("ctl", "Controller", Mod::sourceNames(), 0, "Second source that scales this modulation (like a VCA)"));
        d.push_back (F ("ctlamt", "Controller Amount", -1.0f, 1.0f, 1.0f, Unit::Pan, "How much the controller scales the modulation"));
        d.push_back (C ("fn", "Function", Mod::functionNames(), 0, "Shapes the modulation signal"));
        d.push_back (F ("fnamt", "Function Amount", 0.0f, 100.0f, 50.0f, Unit::Percent, "Function strength"));
        return d;
    }();
    return defs;
}

//==============================================================================
float Params::voiceDefault (int v, int i)
{
    v = jlimit (0, numVoices - 1, v);
    // two low choirs (fifths, octaves), a pulsar above them and a slow siren for the plates
    static const float type[] = { 1, 1, 3, 2 };
    static const float tune[] = { 38, 45, 50, 57 };          // D1 A1 D2 A2
    static const float pan[]  = { -0.3f, 0.3f, -0.15f, 0.4f };
    static const float hold[] = { 70, 55, 0, 0 };
    static const int chord[]  = { 2, 1, 2, 2 };              // Fifths, Octaves
    switch (i)
    {
        case VP::type: return type[v];
        case VP::tune: return tune[v];
        case VP::pan:  return pan[v];
        case VP::hold: return hold[v];
        case VP::int1: case VP::int2: case VP::int3: case VP::int4: case VP::int5:
            return chordIntervals (chord[v])[i - VP::int1];
        case VP::on4:  return v == 1 ? 0.0f : 1.0f;   // the octave choir is thinner
        case VP::on5:  return v == 0 ? 0.0f : 1.0f;
        case VP::siren_wave: return v == 3 ? 100.0f : voiceDefs()[(size_t) i].def;   // a slow wail
        default:       return voiceDefs()[(size_t) i].def;
    }
}

String Params::voiceID (int v, const String& suffix) { return "v" + String (v + 1) + "_" + suffix; }
String Params::modID (int slot, const String& suffix) { return "m" + String (slot + 1).paddedLeft ('0', 2) + "_" + suffix; }

const ParamDef* Params::findDef (const String& id)
{
    auto find = [] (const std::vector<ParamDef>& defs, const String& s) -> const ParamDef*
    {
        for (auto& d : defs) if (s == d.id) return &d;
        return nullptr;
    };
    if (id.length() > 3 && id[0] == 'v' && id[1] >= '1' && id[1] <= '4' && id[2] == '_') return find (voiceDefs(), id.substring (3));
    if (id.length() > 4 && id[0] == 'm' && CharacterFunctions::isDigit (id[1]) && id[3] == '_') return find (modSlotDefs(), id.substring (4));
    return find (globalDefs(), id);
}

//==============================================================================
String Params::gravityZone (float g01)
{
    static const char* names[] = { "STILL", "ORBIT", "TURBULENCE", "COLLAPSE" };
    return names[gravityZoneIndex (g01)];
}

const StringArray& Params::chordNames()
{
    static const StringArray n { "Unison", "Octaves", "Fifths", "Major", "Minor" };
    return n;
}

const float* Params::chordIntervals (int c)
{
    static const float t[5][5] = {
        { 0.0f, 0.07f, -0.07f, 0.12f, -0.12f },   // unison with a slow chorus beat
        { 0.0f, 12.0f, 24.0f, -12.0f, 36.0f },
        { 0.0f, 7.0f, 12.0f, 19.0f, 24.0f },
        { 0.0f, 4.0f, 7.0f, 12.0f, 16.0f },
        { 0.0f, 3.0f, 7.0f, 12.0f, 15.0f } };
    return t[jlimit (0, 4, c)];
}

int Params::matchChord (const float* iv)
{
    for (int c = 0; c < chordNames().size(); ++c)
    {
        bool same = true;
        for (int i = 0; i < numOscs; ++i) same = same && std::abs (iv[i] - chordIntervals (c)[i]) < 0.005f;
        if (same) return c;
    }
    return -1;
}

const StringArray& Params::scaleNames()
{
    static const StringArray n { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Harmonic Minor",
                                 "Pentatonic Major", "Pentatonic Minor", "Whole Tone", "Hirajoshi", "Double Harmonic" };
    return n;
}

const Array<int>& Params::scaleSteps (int s)
{
    static const Array<int> steps[] = {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 4, 7, 9 },
        { 0, 3, 5, 7, 10 }, { 0, 2, 4, 6, 8, 10 }, { 0, 2, 3, 7, 8 }, { 0, 1, 4, 5, 7, 8, 11 } };
    return steps[jlimit (0, 11, s)];
}

//==============================================================================
static NormalisableRange<float> makeRange (const ParamDef& d)
{
    if (d.logarithmic)
    {
        return NormalisableRange<float> (d.min, d.max,
            [] (float a, float b, float x) { return a * std::pow (b / a, x); },
            [] (float a, float b, float v) { return std::log (jlimit (a, b, v) / a) / std::log (b / a); },
            [] (float a, float b, float v) { return jlimit (a, b, v); });
    }
    return NormalisableRange<float> (d.min, d.max, 0.0f, d.skew, d.symmetric);
}

static void addParam (std::vector<std::unique_ptr<RangedAudioParameter>>& list, const ParamDef& d, const String& id,
                      const String& name, float def)
{
    const int version = 1;
    if (d.kind == Kind::Bool)
        list.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, version }, name, def > 0.5f));
    else if (d.kind == Kind::Choice)
        list.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, version }, name, d.choices, (int) def));
    else if (String (d.id) == "dst")
        list.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, version }, name, 0, Mod::maxDestCode, 0,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return Mod::destName (v); })
                                         .withAutomatable (false)));
    else
    {
        const ParamDef* p = &d;
        list.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, version }, name, makeRange (d), def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([p] (float v, int) { return Params::format (*p, v); })
                .withValueFromStringFunction ([p] (const String& t) { return Params::parse (*p, t, p->def); })));
    }
}

AudioProcessorValueTreeState::ParameterLayout Params::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> list;
    for (int v = 0; v < numVoices; ++v)
        for (size_t i = 0; i < voiceDefs().size(); ++i)
        {
            auto& d = voiceDefs()[i];
            addParam (list, d, voiceID (v, d.id), "V" + String (v + 1) + " " + d.name, voiceDefault (v, (int) i));
        }
    for (auto& d : globalDefs())
        addParam (list, d, d.id, d.name, d.def);
    for (int s = 0; s < numModSlots; ++s)
        for (auto& d : modSlotDefs())
            addParam (list, d, modID (s, d.id), "Mod " + String (s + 1) + " " + d.name, d.def);
    return { list.begin(), list.end() };
}

//==============================================================================
String Params::noteName (double midi, bool withCents)
{
    const int n = roundToInt (midi);
    String s = String (noteNames[((n % 12) + 12) % 12]) + String ((int) std::floor (n / 12.0) - 2);
    if (withCents)
    {
        const int cents = roundToInt ((midi - n) * 100.0);
        if (cents != 0) s << " " << (cents > 0 ? "+" : "-") << std::abs (cents);
    }
    return s;
}

// "A3", "C#2+20", "Db1 -5" -> MIDI note (60 = C3)
static double parseNote (const String& t, bool& ok)
{
    auto s = t.trim().removeCharacters (" ");
    ok = false;
    if (s.isEmpty()) return 0.0;
    const juce_wchar first = CharacterFunctions::toUpperCase (s[0]);
    if (! (first >= 'A' && first <= 'G')) return 0.0;
    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    int pc = base[first - 'A'];
    int i = 1;
    if (i < s.length() && s[i] == '#') { ++pc; ++i; }
    else if (i < s.length() && s[i] == 'b') { --pc; ++i; }
    int sign = 1, oct = 0;
    if (i < s.length() && s[i] == '-' && i + 1 < s.length() && CharacterFunctions::isDigit (s[i + 1])) { sign = -1; ++i; }
    if (i >= s.length() || ! CharacterFunctions::isDigit (s[i])) return 0.0;
    while (i < s.length() && CharacterFunctions::isDigit (s[i])) oct = oct * 10 + (s[i++] - '0');
    double cents = 0.0;
    if (i < s.length()) cents = s.substring (i).getDoubleValue();
    ok = true;
    return (sign * oct + 2) * 12 + pc + cents / 100.0;
}

String Params::format (const ParamDef& d, float v, bool withUnit)
{
    auto u = [withUnit] (const char* s) { return withUnit ? String (s) : String(); };
    switch (d.unit)
    {
        case Unit::Plain:     return String (v, 1);
        case Unit::Scale10:   return String (v, 1);
        case Unit::Semitones: return (v > 0.004f ? "+" : "") + String (std::abs (v) < 0.005f ? 0.0f : v, std::abs (v - std::round (v)) < 0.005f ? 0 : 2) + u (" st");
        case Unit::Hz:
            if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + u (" kHz");
            return String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + u (" Hz");
        case Unit::Seconds:
            if (v < 0.01f) return String (v * 1000.0f, 1) + u (" ms");
            if (v < 1.0f) return String (roundToInt (v * 1000.0f)) + u (" ms");
            return String (v, v < 10.0f ? 1 : 0) + u (" s");
        case Unit::Percent:   return String (roundToInt (v)) + u (" %");
        case Unit::Decibels:
            if (v <= d.min + 0.01f && d.min <= -59.0f) return "-inf" + u (" dB");
            return String (std::abs (v) < 0.05f ? 0.0f : v, 1) + u (" dB");
        case Unit::Pan:
            if (String (d.id).endsWith ("pan"))
            {
                const int p = roundToInt (v * 100.0f);
                return p == 0 ? String ("C") : (p < 0 ? "L " + String (-p) : "R " + String (p));
            }
            return (v > 0.0049f ? "+" : "") + String (roundToInt (v * 100.0f)) + u (" %");
        case Unit::Note:      return noteName (v, true);
        case Unit::Divider:   return roundToInt (v) <= 1 ? String ("off") : "1:" + String (roundToInt (v));
        case Unit::Wave:
        {
            static const char* names[] = { "Sine", "Triangle", "Shark", "Saw", "Square" };
            const int i = jlimit (0, 4, (int) std::floor (v + 0.0001f));
            const float f = v - (float) i;
            if (f < 0.04f || i == 4) return names[i];
            if (f > 0.96f) return names[i + 1];
            return String (names[i]) + ">" + names[i + 1];
        }
        case Unit::Gravity:   return withUnit ? gravityZone (v / 100.0f) + " " + String (roundToInt (v)) + " %" : String (roundToInt (v));
        case Unit::Interval:
        {
            const int st = roundToInt (v);
            const int cents = roundToInt ((v - (float) st) * 100.0f);
            const String s = (st > 0 ? "+" : "") + String (st);
            if (cents == 0) return s + u (" st");
            return s + " " + (cents > 0 ? "+" : "-") + String (std::abs (cents)) + "c";
        }
        case Unit::Tide:
        {
            if (v <= 50.0f) return v < 0.5f ? String ("0") : String (Params::tideSemitones (v / 100.0f) / 12.0f, 1) + u (" oct");
            return "PULL " + String (roundToInt (Params::tidePull (v / 100.0f) * 100.0f)) + u (" %");
        }
        case Unit::Choice:    return d.choices[jlimit (0, d.choices.size() - 1, roundToInt (v))];
        case Unit::Toggle:    return v > 0.5f ? "On" : "Off";
    }
    return String (v);
}

float Params::parse (const ParamDef& d, const String& text, float fallback)
{
    auto t = text.trim();
    auto lower = t.toLowerCase();
    if (t.isEmpty()) return fallback;
    const double num = t.retainCharacters ("0123456789.-+").getDoubleValue();
    const bool hasDigit = t.containsAnyOf ("0123456789");
    auto clampV = [&d] (double v) { return (float) jlimit ((double) d.min, (double) d.max, v); };

    switch (d.unit)
    {
        case Unit::Note:
        {
            bool ok = false;
            const double n = parseNote (t, ok);
            if (ok) return clampV (n);
            if (lower.contains ("hz") && hasDigit) return clampV (69.0 + 12.0 * std::log2 (std::max (1.0, num) / 440.0));
            return hasDigit ? clampV (num) : fallback;
        }
        case Unit::Hz:
        {
            bool ok = false;
            const double n = parseNote (t, ok);
            if (ok) return clampV (440.0 * std::pow (2.0, (n - 69.0) / 12.0));
            if (! hasDigit) return fallback;
            return clampV (lower.containsChar ('k') ? num * 1000.0 : num);
        }
        case Unit::Seconds:
            if (! hasDigit) return fallback;
            if (lower.contains ("ms")) return clampV (num / 1000.0);
            if (lower.endsWith ("s")) return clampV (num);
            return clampV (num >= 1.0 ? num / 1000.0 : num);   // bare numbers >= 1 are milliseconds
        case Unit::Decibels:
            if (lower.contains ("inf")) return d.min;
            return hasDigit ? clampV (num) : fallback;
        case Unit::Pan:
            if (String (d.id).endsWith ("pan"))
            {
                if (lower == "c") return 0.0f;
                if (lower.startsWith ("l")) return clampV (-std::abs (num) / 100.0);
                if (lower.startsWith ("r")) return clampV (std::abs (num) / 100.0);
            }
            return hasDigit ? clampV (num / 100.0) : fallback;
        case Unit::Interval:
        {
            // "+19 -8c" -> 18.92; "7" -> 7; "+4.5" -> 4.5
            if (! hasDigit) return fallback;
            if (lower.endsWithChar ('c') && t.containsChar (' '))
            {
                const double st = t.upToFirstOccurrenceOf (" ", false, false).getDoubleValue();
                const double ct = t.fromFirstOccurrenceOf (" ", false, false).retainCharacters ("0123456789.-+").getDoubleValue();
                return clampV (st + ct / 100.0);
            }
            return clampV (t.retainCharacters ("0123456789.-+").getDoubleValue());
        }
        case Unit::Choice:
            for (int i = 0; i < d.choices.size(); ++i)
                if (d.choices[i].equalsIgnoreCase (t)) return (float) i;
            for (int i = 0; i < d.choices.size(); ++i)
                if (d.choices[i].startsWithIgnoreCase (t)) return (float) i;
            return hasDigit ? clampV (num) : fallback;
        case Unit::Toggle:
            return lower == "on" || lower == "1" || lower == "true" ? 1.0f : 0.0f;
        case Unit::Divider:
            if (lower == "off") return 1.0f;
            if (! hasDigit) return fallback;
            return clampV (t.containsChar (':') ? t.fromLastOccurrenceOf (":", false, false).getDoubleValue() : num);
        case Unit::Gravity:
        case Unit::Wave:
        case Unit::Tide:
        case Unit::Plain: case Unit::Scale10: case Unit::Semitones: case Unit::Percent:
            return hasDigit ? clampV (num) : fallback;
    }
    return fallback;
}

//==============================================================================
const StringArray& Mod::sourceNames()
{
    static const StringArray s { "None", "Velocity", "Mod Wheel", "Pressure", "Pitch Bend", "Constant" };
    return s;
}

String Mod::shortSourceName (int src) { return sourceNames()[jlimit (0, numSources - 1, src)]; }

Colour Mod::sourceColour (int src)
{
    static const uint32 c[] = { 0xff5e5f68, 0xffffb86b, 0xff7fd4ff, 0xffe6e6ea, 0xff8f8cff, 0xff9a9ba4 };
    return Colour (c[jlimit (0, numSources - 1, src)]);
}

bool Mod::isBipolar (int src) { return src == PitchBend; }

int Mod::destCode (const String& id)
{
    auto find = [] (const std::vector<ParamDef>& defs, const String& s) -> int
    {
        for (size_t i = 0; i < defs.size(); ++i) if (s == defs[i].id) return (int) i;
        return -1;
    };
    if (id.length() > 3 && id[0] == 'v' && id[1] >= '1' && id[1] <= '4' && id[2] == '_')
    {
        const int i = find (Params::voiceDefs(), id.substring (3));
        return i < 0 ? 0 : (id[1] - '0') * blockSize + i;
    }
    const int i = find (Params::globalDefs(), id);
    return i < 0 ? 0 : globalBlock * blockSize + i;
}

String Mod::destParamID (int code)
{
    const int block = code / blockSize, i = code % blockSize;
    if (block >= 1 && block <= Params::numVoices)
        return i < (int) Params::voiceDefs().size() ? Params::voiceID (block - 1, Params::voiceDefs()[(size_t) i].id) : String();
    if (block == globalBlock)
        return i < (int) Params::globalDefs().size() ? String (Params::globalDefs()[(size_t) i].id) : String();
    return {};
}

String Mod::blockName (int block)
{
    if (block >= 1 && block <= Params::numVoices) return "Voice " + String (block);
    return "Global";
}

String Mod::destName (int code)
{
    const auto id = destParamID (code);
    if (id.isEmpty()) return code == 0 ? "None" : "-";
    const int block = code / blockSize;
    auto* d = Params::findDef (id);
    if (d == nullptr) return "-";
    return block == globalBlock ? String (d->name) : blockName (block) + " " + d->name;
}

Array<int> Mod::allDestinations()
{
    Array<int> a;
    for (int v = 1; v <= Params::numVoices; ++v)
        for (size_t i = 0; i < Params::voiceDefs().size(); ++i)
            if (Params::voiceDefs()[i].kind == Kind::Float) a.add (v * blockSize + (int) i);
    for (size_t i = 0; i < Params::globalDefs().size(); ++i)
        if (Params::globalDefs()[i].kind == Kind::Float) a.add (globalBlock * blockSize + (int) i);
    return a;
}

const StringArray& Mod::functionNames()
{
    static const StringArray f { "None", "Scale", "Offset", "Low Clip", "High Clip", "Exponential", "Norm Exponential",
                                 "Low Pass", "High Pass", "Slew", "Low Slew", "High Slew", "Sample & Hold", "Bounce" };
    return f;
}

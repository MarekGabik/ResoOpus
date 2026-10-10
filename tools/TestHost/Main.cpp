// Offline test harness for ResoOpus.
// 1) Parameters: table order, IDs, formatting and parsing, product names
// 2) Sound engine: default patch (drones without MIDI), tuning, snap, aliasing, TIDE, plates, keys, touch,
//    robustness, state, silent preset change, presets
// 3) CPU benchmark
// 4) Editor snapshots and gestures
// 5) Loads the built VST3 through JUCE's hosting layer
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "State/FactoryPresets.h"
#include "DSP/Primitives.h"

using namespace juce;

static int failures = 0;
static File snapshotDir;

// Shared CI runners are slow and noisy: timing limits get 3x headroom there (functional limits stay strict)
static double timing (double limit) { return std::getenv ("CI") != nullptr ? 3.0 * limit : limit; }

static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << what << std::endl;
    if (! ok) ++failures;
}

static void pumpMessages (int ms = 30) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void setParam (ResoOpusProcessor& p, const String& id, float realValue)
{
    if (auto* rp = p.param (id))
    {
        rp->setValueNotifyingHost (rp->convertTo0to1 (realValue));
        return;
    }
    std::cout << "  ! unknown parameter " << id << std::endl;
    ++failures;
}

static std::unique_ptr<ResoOpusProcessor> makeProc (double sr = 48000.0, int block = 512)
{
    auto p = std::make_unique<ResoOpusProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

struct Ev { double time; MidiMessage msg; };

static AudioBuffer<float> render (ResoOpusProcessor& p, double seconds, std::vector<Ev> events = {}, int block = 512)
{
    const double sr = p.getSampleRate();
    const int n = (int) (seconds * sr);
    AudioBuffer<float> out (2, n);
    out.clear();
    AudioBuffer<float> buf (2, block);
    std::sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.time < b.time; });
    size_t ei = 0;
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        buf.setSize (2, len, false, false, true);
        buf.clear();
        MidiBuffer midi;
        while (ei < events.size() && (int) (events[ei].time * sr) < pos + len)
        {
            midi.addEvent (events[ei].msg, std::max (0, (int) (events[ei].time * sr) - pos));
            ++ei;
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, len);
    }
    return out;
}

static std::vector<Ev> note (int n, double on, double off, float vel = 0.8f)
{
    return { { on, MidiMessage::noteOn (1, n, vel) }, { off, MidiMessage::noteOff (1, n) } };
}

static bool allFinite (const AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

static double peakDb (const AudioBuffer<float>& b, int start = 0, int end = -1)
{
    if (end < 0) end = b.getNumSamples();
    float pk = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < end; ++i) pk = std::max (pk, std::abs (b.getSample (c, i)));
    return 20.0 * std::log10 (pk + 1e-12);
}

static double rmsDb (const AudioBuffer<float>& b, int start = 0, int end = -1)
{
    if (end < 0) end = b.getNumSamples();
    double s = 0.0;
    int count = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < end; ++i, ++count) s += (double) b.getSample (c, i) * b.getSample (c, i);
    return 10.0 * std::log10 (s / std::max (1, count) + 1e-20);
}

// magnitude spectrum (dB) of the mid signal, Blackman-Harris window
static std::vector<double> spectrum (const AudioBuffer<float>& b, int start, int order)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.35875 - 0.48829 * std::cos (2 * MathConstants<double>::pi * i / N) + 0.14128 * std::cos (4 * MathConstants<double>::pi * i / N)
                         - 0.01168 * std::cos (6 * MathConstants<double>::pi * i / N);
        data[(size_t) i] = (float) ((b.getSample (0, start + i) + b.getSample (1, start + i)) * 0.5 * w);
    }
    fft.performFrequencyOnlyForwardTransform (data.data());
    std::vector<double> db ((size_t) N / 2);
    for (int i = 0; i < N / 2; ++i) db[(size_t) i] = 20.0 * std::log10 (data[(size_t) i] + 1e-12);
    return db;
}

// Frequency of the strongest peak between lo and hi Hz (parabolic interpolation on the dB spectrum)
static double peakFrequency (const AudioBuffer<float>& b, double sr, double lo, double hi, int start)
{
    const int order = 16;
    const auto db = spectrum (b, start, order);
    const double binHz = sr / (double) (1 << order);
    size_t best = (size_t) (lo / binHz);
    for (size_t i = (size_t) (lo / binHz); i < (size_t) (hi / binHz); ++i) if (db[i] > db[best]) best = i;
    const double a = db[best - 1], c = db[best], d = db[best + 1];
    const double off = 0.5 * (a - d) / (a - 2.0 * c + d);
    return ((double) best + off) * binHz;
}

static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// Only voice 1 sounds, as one plain saw at full HOLD: a clean object for measurements
static void soloVoice1 (ResoOpusProcessor& p, float tune, int type = VP::Choir)
{
    for (int v = 1; v < Params::numVoices; ++v) setParam (p, Params::voiceID (v, "type"), (float) VP::Off);
    setParam (p, "v1_hold", 100.0f);
    setParam (p, "v1_type", (float) type);
    setParam (p, "v1_siren_fm", 0.0f);
    setParam (p, "v1_bloom", 0.0f);
    setParam (p, "v1_tune", tune);
    setParam (p, "v1_attack", 0.001f);
    setParam (p, "v1_volume", -6.0f);
    setParam (p, "v1_pan", 0.0f);
    for (int i = 1; i <= 5; ++i) { setParam (p, "v1_int" + String (i), 0.0f); setParam (p, "v1_on" + String (i), i == 1 ? 1.0f : 0.0f); }
}

//==============================================================================
static void testParameters()
{
    std::cout << "Parameters" << std::endl;
    const StringArray voiceOrder { "type", "tune", "int1", "int2", "int3", "int4", "int5", "on1", "on2", "on3", "on4", "on5",
                                   "tide", "attack", "release", "volume", "pan", "hold",
                                   "siren_rate", "siren_fm", "siren_am", "siren_div", "siren_noise", "siren_wave", "sharp", "bloom" };
    bool ok = (int) Params::voiceDefs().size() == VP::count && voiceOrder.size() == VP::count;
    for (int i = 0; i < voiceOrder.size() && ok; ++i) ok = voiceOrder[i] == Params::voiceDefs()[(size_t) i].id;
    check (ok, "voice table matches the VP index constants (" + String ((int) Params::voiceDefs().size()) + " per voice)");
    const StringArray globalOrder { "master_vol", "oversampling", "play_mode", "touch", "plates_base", "snap", "snap_scale", "snap_root", "a4", "pb_range", "pitch" };
    ok = (int) Params::globalDefs().size() == GP::count;
    for (int i = 0; i < globalOrder.size() && ok; ++i) ok = globalOrder[i] == Params::globalDefs()[(size_t) i].id;
    check (ok, "global table matches the GP index constants (" + String ((int) Params::globalDefs().size()) + ")");

    ResoOpusProcessor p;
    StringArray ids;
    for (auto* prm : p.getParameters()) if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm)) ids.addIfNotAlreadyThere (wp->paramID);
    const int expected = Params::numVoices * VP::count + GP::count + Params::numModSlots * MP::count;
    check (p.getParameters().size() == expected && ids.size() == expected, String (expected) + " parameters with unique IDs");
    bool defsOk = true;
    for (auto& id : ids) defsOk = defsOk && Params::findDef (id) != nullptr;
    check (defsOk, "every parameter ID resolves to its definition");

    auto def = [] (const char* id) { return *Params::findDef (id); };
    check (std::abs (Params::parse (def ("v1_tune"), "D2+4", 0) - 50.04f) < 0.001f, "parse note \"D2+4\" = MIDI 50.04 (Ableton: C3 = 60)");
    check (std::abs (Params::parse (def ("v1_tune"), "A3", 0) - 69.0f) < 0.001f && std::abs (Params::parse (def ("a4"), "A3", 0) - 440.0f) < 0.01f,
           "\"A3\" = MIDI 69 = 440 Hz");
    check (std::abs (Params::parse (def ("v1_int4"), "+19 -8c", 0) - 18.92f) < 0.001f, "parse interval \"+19 -8c\"");
    check (std::abs (Params::parse (def ("v1_attack"), "120 ms", 0) - 0.12f) < 1e-4f && std::abs (Params::parse (def ("v1_pan"), "L 30", 0) + 0.3f) < 1e-4f,
           "parse \"120 ms\" and pan \"L 30\"");
    check (Params::format (def ("v1_tune"), 50.04f) == "D2 +4" && Params::format (def ("v1_int4"), 18.92f) == "+19 -8c"
           && Params::format (def ("v1_int2"), 7.0f) == "+7 st", "format: \"D2 +4\", \"+19 -8c\", \"+7 st\"");
    check (Params::format (def ("v1_tide"), 50.0f) == "-2.0 oct" && Params::format (def ("v1_tide"), 75.0f).startsWith ("PULL"),
           "TIDE shows octaves up to 50 % and PULL above (" + Params::format (def ("v1_tide"), 75.0f) + ")");
    check (Mod::destParamID (Mod::destCode ("v3_tide")) == "v3_tide" && Mod::destParamID (Mod::destCode ("v4_hold")) == "v4_hold"
           && Mod::destParamID (Mod::destCode ("master_vol")) == "master_vol", "modulation destination codes round-trip");
    float iv[5];
    for (int i = 0; i < 5; ++i) iv[i] = Params::chordIntervals (3)[i];
    check (Params::matchChord (iv) == 3 && Params::chordNames()[3] == "Major", "CHORD templates are recognised (Major)");
}

// Nothing the user sees may name the hardware that inspired the instrument
static void testProductNames()
{
    std::cout << "Product names" << std::endl;
    const StringArray banned { "lyra", "soma", "solar", "elta", "srapa", "polivoks", "cathedral", "infinity", "magic", "ringer", "hyper", "kreimer" };
    StringArray texts;
    auto addDefs = [&texts] (const std::vector<ParamDef>& defs)
    {
        for (auto& d : defs) { texts.add (d.name); texts.add (d.tip); texts.addArray (d.choices); }
    };
    addDefs (Params::voiceDefs()); addDefs (Params::globalDefs()); addDefs (Params::modSlotDefs());
    texts.addArray (Params::chordNames()); texts.addArray (Params::scaleNames()); texts.addArray (Mod::sourceNames());
    for (auto& pr : factoryPresets()) { texts.add (pr.name); texts.add (pr.category); texts.add (pr.description); }
    String hits;
    for (auto& t : texts)
        for (auto& b : banned)
            if (t.toLowerCase().contains (b)) hits << " \"" << t.substring (0, 40) << "\"";
    check (hits.isEmpty(), "no parameter, tooltip, preset or menu text names the inspiring hardware" + hits);
}

static void testDefaultPatch()
{
    std::cout << "Default patch (no MIDI at all)" << std::endl;
    auto p = makeProc();
    auto out = render (*p, 8.0);
    const double sr = p->getSampleRate();
    check (allFinite (out), "output is finite");
    const double start = rmsDb (out, 0, (int) (0.05 * sr));
    const double pk = peakDb (out, (int) (5.0 * sr), out.getNumSamples());
    check (start < -40.0, "fades in instead of starting with a click (first 50 ms " + String (start, 1) + " dB)");
    check (pk > -14.0 && pk < -3.0, "drones on its own at a sensible level after 5 s (peak " + String (pk, 1) + " dBFS)");
    check (p->liveLevel[0].load() > 0.5f && p->liveLevel[1].load() > 0.4f && p->liveLevel[2].load() < 0.001f,
           "the two choirs drone, voices 3-4 (HOLD 0) wait for their plates");
}

static void testTuning()
{
    std::cout << "Tuning (single saw, FFT peak with interpolation)" << std::endl;
    const double sr = 48000.0;
    double worst = 0.0;
    for (float n : { 40.0f, 57.0f, 69.0f, 81.5f })
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, n);
        auto out = render (*p, 2.0);
        const double ref = 440.0 * std::pow (2.0, (n - 69.0) / 12.0);
        const double f = peakFrequency (out, sr, ref * 0.8, ref * 1.25, (int) (0.4 * sr));
        worst = std::max (worst, std::abs (cents (f, ref)));
    }
    check (worst < 1.0, "TUNE E1, A2, A3, A4 +50c: worst error " + String (worst, 3) + " cents (< 1)");

    auto p = makeProc (sr);
    soloVoice1 (*p, 57.0f);
    setParam (*p, "a4", 432.0f);
    setParam (*p, "v1_on1", 0.0f); setParam (*p, "v1_on2", 1.0f); setParam (*p, "v1_int2", 7.0f);
    setParam (*p, "pitch", -12.0f);
    auto out = render (*p, 2.0);
    const double ref = 432.0 * std::pow (2.0, (57.0 + 7.0 - 12.0 - 69.0) / 12.0);
    const double f = peakFrequency (out, sr, ref * 0.8, ref * 1.25, (int) (0.4 * sr));
    check (std::abs (cents (f, ref)) < 1.0, "interval +7, PITCH -12 and A = 432 Hz combine exactly (" + String (cents (f, ref), 3) + " cents)");
}

static void testSnap()
{
    std::cout << "Snap" << std::endl;
    const double sr = 48000.0;
    auto measure = [sr] (float tune, int snap, int scale, int root)
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, tune);
        setParam (*p, "snap", (float) snap);
        setParam (*p, "snap_scale", (float) scale);
        setParam (*p, "snap_root", (float) root);
        auto out = render (*p, 2.0);
        return peakFrequency (out, sr, 100.0, 400.0, (int) (0.4 * sr));
    };
    const double chrom = measure (57.3f, 1, 0, 0);
    check (std::abs (cents (chrom, 220.0)) < 1.0, "Chromatic: A2 +30c plays A2 (" + String (cents (chrom, 220.0), 2) + " cents)");
    const double dor = measure (58.4f, 2, 2, 2);   // D dorian: A#2+40 -> B2
    const double b2 = 440.0 * std::pow (2.0, (59.0 - 69.0) / 12.0);
    check (std::abs (cents (dor, b2)) < 1.0, "Scale D Dorian: A#2 +40c snaps to B2 (" + String (cents (dor, b2), 2) + " cents)");
    const double free = measure (57.3f, 0, 0, 0);
    check (std::abs (cents (free, 220.0) - 30.0) < 1.0, "Free keeps microtones (+" + String (cents (free, 220.0), 2) + " cents)");
}

static double aliasDb (ResoOpusProcessor& p, float midi)
{
    const double sr = p.getSampleRate();
    soloVoice1 (p, midi);
    auto out = render (p, 1.2);
    const auto db = spectrum (out, (int) (0.4 * sr), 15);
    const double binHz = sr / 32768.0;
    const double f0 = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    const double fund = db[(size_t) std::lround (f0 / binHz)];
    double worst = -300.0;
    for (size_t i = (size_t) (100.0 / binHz); i < (size_t) (10000.0 / binHz); ++i)
    {
        const double f = i * binHz;
        const double h = f / f0;
        if (std::abs (h - std::round (h)) * f0 < 6.0 * binHz) continue;   // harmonic (window main lobe)
        worst = std::max (worst, db[i]);
    }
    return fund - worst;
}

static void testAliasing()
{
    std::cout << "Aliasing (CHOIR saw at 1976 Hz, 48 kHz, strongest alias below 10 kHz vs fundamental)" << std::endl;
    double res[3];
    for (int os = 0; os < 3; ++os)
    {
        auto p = makeProc();
        setParam (*p, "oversampling", (float) os);
        res[os] = aliasDb (*p, 95.0f);
    }
    check (res[0] > 70.0, "1x: aliases " + String (res[0], 1) + " dB below the fundamental (> 70)");
    check (res[1] > 85.0, "2x: aliases " + String (res[1], 1) + " dB below the fundamental (> 85)");
    check (res[2] > 85.0, "4x: aliases " + String (res[2], 1) + " dB below the fundamental (> 85)");
}

static void testTide()
{
    std::cout << "TIDE" << std::endl;
    const double sr = 48000.0;
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f);
        setParam (*p, "v1_tide", 50.0f);
        auto out = render (*p, 2.0);
        const double f = peakFrequency (out, sr, 80.0, 140.0, (int) (0.4 * sr));
        check (std::abs (cents (f, 110.0)) < 1.0, "50 %: the voice sinks exactly two octaves (" + String (f, 2) + " Hz)");
    }
    // two oscillators a fifth apart: pull adds sidebands (many more spectral peaks)
    auto peaks = [sr] (float tide)
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 81.0f);
        setParam (*p, "v1_on2", 1.0f);
        setParam (*p, "v1_int2", 7.0f);
        setParam (*p, "v1_tide", tide);
        auto out = render (*p, 2.0);
        const auto db = spectrum (out, (int) (0.5 * sr), 15);
        const double binHz = sr / 32768.0;
        double mx = -300.0;
        for (size_t i = (size_t) (40.0 / binHz); i < (size_t) (8000.0 / binHz); ++i) mx = std::max (mx, db[i]);
        int count = 0;
        for (size_t i = (size_t) (40.0 / binHz) + 1; i < (size_t) (8000.0 / binHz); ++i)
            if (db[i] > mx - 50.0 && db[i] > db[i - 1] && db[i] >= db[i + 1]) ++count;
        return count;
    };
    const int still = peaks (50.0f), pulled = peaks (80.0f);
    check (pulled > still * 2, "above 50 % the oscillators pull on each other: " + String (still) + " -> " + String (pulled) + " spectral peaks");

    auto p = makeProc (sr);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        setParam (*p, Params::voiceID (v, "type"), (float) VP::Choir); setParam (*p, Params::voiceID (v, "tide"), 100.0f);
        setParam (*p, Params::voiceID (v, "attack"), 0.01f); setParam (*p, Params::voiceID (v, "hold"), 100.0f);
    }
    auto out = render (*p, 6.0);
    check (allFinite (out) && peakDb (out) < 0.0, "all four voices at TIDE 100 % for 6 s: finite, peak " + String (peakDb (out), 1) + " dBFS");
}

static void testSiren()
{
    std::cout << "SIREN" << std::endl;
    const double sr = 48000.0;
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Siren);
        auto out = render (*p, 2.0);
        const double f = peakFrequency (out, sr, 300.0, 600.0, (int) (0.4 * sr));
        check (std::abs (cents (f, 440.0)) < 1.0, "steady tone sits on TUNE (" + String (cents (f, 440.0), 2) + " cents)");
    }
    {
        // square modulator at 0.5 Hz, FM 50 % = +-1.5 octaves: the pitch alternates between the two ends
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Siren);
        setParam (*p, "v1_siren_fm", 50.0f);
        setParam (*p, "v1_siren_rate", 0.5f);
        setParam (*p, "v1_siren_wave", 0.0f);
        auto out = render (*p, 2.0);
        const double hi = 440.0 * std::pow (2.0, 1.5), lo = 440.0 * std::pow (2.0, -1.5);
        const double fHi = peakFrequency (out, sr, hi * 0.8, hi * 1.25, (int) (0.1 * sr));
        const double fLo = peakFrequency (out, sr, lo * 0.8, lo * 1.25, (int) (1.1 * sr));
        check (std::abs (cents (fHi, hi)) < 5.0 && std::abs (cents (fLo, lo)) < 5.0,
               "FM 50 %: two-tone siren jumps +-1.5 octaves (" + String (fHi, 1) + " / " + String (fLo, 1) + " Hz)");
    }
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Siren);
        setParam (*p, "v1_siren_div", 3.0f);
        auto out = render (*p, 2.0);
        const auto db = spectrum (out, (int) (0.4 * sr), 15);
        const double binHz = sr / 32768.0;
        auto at = [&] (double f) { double m = -300.0; for (int d = -3; d <= 3; ++d) m = std::max (m, db[(size_t) (std::lround (f / binHz) + d)]); return m; };
        check (at (440.0 / 3.0) > at (440.0) - 12.0, "DIVIDER 1:3 adds a strong sub-harmonic at 147 Hz (" + String (at (440.0 / 3.0) - at (440.0), 1) + " dB vs the tone)");
    }
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Siren);
        setParam (*p, "v1_siren_am", 100.0f);
        setParam (*p, "v1_siren_rate", 2.0f);
        auto out = render (*p, 2.0);
        const int q = (int) (0.25 * sr);   // half a modulator period
        const double on = rmsDb (out, (int) (1.0 * sr) + q / 8, (int) (1.0 * sr) + q - q / 8);
        const double off = rmsDb (out, (int) (1.0 * sr) + q + q / 8, (int) (1.0 * sr) + 2 * q - q / 8);
        check (std::abs (on - off) > 30.0, "AM 100 %: the tone is chopped into beeps (" + String (std::abs (on - off), 1) + " dB on / off)");
    }
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 95.0f, VP::Siren);
        auto out = render (*p, 1.2);
        const auto db = spectrum (out, (int) (0.4 * sr), 15);
        const double binHz = sr / 32768.0, f0 = 440.0 * std::pow (2.0, 26.0 / 12.0);
        double worst = -300.0;
        for (size_t i = (size_t) (100.0 / binHz); i < (size_t) (10000.0 / binHz); ++i)
        {
            const double h = i * binHz / f0;
            if (std::abs (h - std::round (h)) * f0 < 6.0 * binHz) continue;
            worst = std::max (worst, db[i]);
        }
        const double res = db[(size_t) std::lround (f0 / binHz)] - worst;
        check (res > 80.0, "square tone at 1976 Hz, 2x: aliases " + String (res, 1) + " dB below (> 80)");
    }
}

static void testPulsar()
{
    std::cout << "PULSAR" << std::endl;
    const double sr = 48000.0;
    auto third = [sr] (float sharp)
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 57.0f, VP::Pulsar);
        setParam (*p, "v1_sharp", sharp);
        auto out = render (*p, 2.0);
        const auto db = spectrum (out, (int) (0.4 * sr), 15);
        const double binHz = sr / 32768.0;
        auto at = [&] (double f) { double m = -300.0; for (int d = -3; d <= 3; ++d) m = std::max (m, db[(size_t) (std::lround (f / binHz) + d)]); return m; };
        return at (660.0) - at (220.0);
    };
    const double soft = third (0.0f), sharp = third (100.0f);
    check (soft < -17.0 && sharp > -11.0, "SHARP: triangle (3rd harmonic " + String (soft, 1) + " dB) to square (" + String (sharp, 1) + " dB)");
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Pulsar);
        setParam (*p, "v1_hold", 0.0f);
        setParam (*p, "v1_attack", 4.0f);
        setParam (*p, "v1_bloom", 100.0f);
        auto out = render (*p, 6.0, { { 0.0, MidiMessage::noteOn (1, 60, 1.0f) } });
        const double early = peakFrequency (out, sr, 350.0, 500.0, (int) (0.3 * sr));
        const double late = peakFrequency (out, sr, 350.0, 500.0, (int) (4.5 * sr));
        check (cents (early, 440.0) < -15.0 && std::abs (cents (late, 440.0)) < 3.0,
               "BLOOM: swells in flat (" + String (cents (early, 440.0), 1) + " c) and settles on pitch (" + String (cents (late, 440.0), 1) + " c)");
    }
    {
        auto p = makeProc (sr);
        soloVoice1 (*p, 69.0f, VP::Off);
        auto out = render (*p, 2.0, note (60, 0.1, 1.5));
        check (rmsDb (out) < -100.0, "TYPE OFF: the voice stays silent even with HOLD 100 % and its plate played");
    }
}

static void testPlates()
{
    std::cout << "Plates and touch" << std::endl;
    auto silentGroups = [] (ResoOpusProcessor& p)
    {
        for (int v = 0; v < Params::numVoices; ++v)
        {
            setParam (p, Params::voiceID (v, "type"), (float) VP::Choir);
            setParam (p, Params::voiceID (v, "attack"), 0.05f);
            setParam (p, Params::voiceID (v, "hold"), 0.0f);
        }
    };
    {
        auto p = makeProc();
        silentGroups (*p);
        render (*p, 1.0, { { 0.0, MidiMessage::noteOn (1, 60, 0.9f) }, { 0.0, MidiMessage::noteOn (1, 62, 0.9f) }, { 0.0, MidiMessage::noteOn (1, 61, 0.9f) } });
        bool others = true;
        for (int v = 2; v < Params::numVoices; ++v) others = others && p->liveLevel[v].load() < 0.001f;
        check (p->liveLevel[0].load() > 0.5f && p->liveLevel[1].load() > 0.5f && others, "C3 and D3 play voices 1 and 2; the black key C#3 plays nothing");
        check (p->plateNote (3) == 65, "the fourth plate is F3");
        render (*p, 3.0, { { 0.0, MidiMessage::noteOff (1, 60) } });
        check (p->liveLevel[0].load() < 0.2f && p->liveLevel[1].load() > 0.5f, "releasing C3 fades voice 1 only");
    }
    auto levelFor = [&silentGroups] (int touchMode, float vel, int pressure)
    {
        auto p = makeProc();
        silentGroups (*p);
        setParam (*p, "touch", (float) touchMode);
        std::vector<Ev> ev { { 0.0, MidiMessage::noteOn (1, 60, vel) } };
        if (pressure >= 0) ev.push_back ({ 0.01, MidiMessage::channelPressureChange (1, pressure) });
        auto out = render (*p, 1.5, ev);
        return rmsDb (out, (int) (1.0 * 48000), out.getNumSamples());
    };
    const double soft = levelFor (0, 0.2f, -1), hard = levelFor (0, 1.0f, -1);
    check (hard > soft + 6.0, "Velocity: hard strike louder than soft (" + String (soft, 1) + " -> " + String (hard, 1) + " dB)");
    const double light = levelFor (1, 1.0f, 0), pressed = levelFor (1, 1.0f, 127);
    check (pressed > light + 6.0, "Aftertouch: pressing harder after the strike makes it louder (" + String (light, 1) + " -> " + String (pressed, 1) + " dB)");
    {
        auto p = makeProc();
        setParam (*p, "v1_hold", 40.0f);
        render (*p, 4.0);
        const float hold = p->liveLevel[0].load();
        render (*p, 2.0, { { 0.0, MidiMessage::noteOn (1, 60, 1.0f) } });
        check (hold > 0.35f && p->liveLevel[0].load() > hold + 0.3f, "playing a plate rises above HOLD (" + String (hold, 2) + " -> " + String (p->liveLevel[0].load(), 2) + ")");
    }
}

static void testKeys()
{
    std::cout << "Keys mode" << std::endl;
    const double sr = 48000.0;
    auto p = makeProc (sr);
    setParam (*p, "play_mode", 1.0f);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        setParam (*p, Params::voiceID (v, "type"), (float) VP::Choir);
        setParam (*p, Params::voiceID (v, "hold"), 0.0f);
        setParam (*p, Params::voiceID (v, "attack"), 0.01f);
        for (int i = 2; i <= 5; ++i) setParam (*p, Params::voiceID (v, "on" + String (i)), 0.0f);
    }
    setParam (*p, "v1_int1", 0.0f);
    auto out = render (*p, 2.0, { { 0.0, MidiMessage::noteOn (1, 64, 0.9f) } });
    const double ref = 440.0 * std::pow (2.0, (64.0 - 69.0) / 12.0);
    const double f = peakFrequency (out, sr, ref * 0.8, ref * 1.25, (int) (0.5 * sr));
    check (std::abs (cents (f, ref)) < 1.0, "a played E3 retunes a voice to E3 (" + String (cents (f, ref), 2) + " cents)");
    render (*p, 0.1, { { 0.0, MidiMessage::noteOn (1, 67, 0.9f) } });
    int gated = 0;
    for (int v = 0; v < Params::numVoices; ++v) if (p->liveGate[v].load()) ++gated;
    check (gated == 2, "a second key takes another voice (" + String (gated) + " voices held)");
}

static void testRobustness()
{
    std::cout << "Robustness" << std::endl;
    bool ok = true;
    double worst = -200.0;
    for (double sr : { 44100.0, 96000.0, 192000.0 })
        for (int block : { 1, 17, 512, 4096 })
        {
            auto p = makeProc (sr, block);
            auto out = render (*p, block == 1 ? 0.3 : 1.5, note (60, 0.0, 0.2), block);
            ok = ok && allFinite (out);
            worst = std::max (worst, peakDb (out));
        }
    check (ok && worst < 12.0, "44.1-192 kHz, blocks 1..4096: finite output (peak " + String (worst, 1) + " dBFS)");
    {
        auto p = makeProc();
        AudioBuffer<float> empty (2, 0);
        MidiBuffer m;
        p->processBlock (empty, m);
        check (true, "zero-length block accepted");
    }
    auto p = makeProc();
    for (int v = 0; v < Params::numVoices; ++v)
    {
        setParam (*p, Params::voiceID (v, "volume"), 6.0f);
        setParam (*p, Params::voiceID (v, "attack"), 0.001f);
        setParam (*p, Params::voiceID (v, "tide"), 70.0f);
        setParam (*p, Params::voiceID (v, "hold"), 100.0f);
        setParam (*p, Params::voiceID (v, "siren_fm"), 100.0f);
        setParam (*p, Params::voiceID (v, "siren_noise"), 100.0f);
        setParam (*p, Params::voiceID (v, "sharp"), 100.0f);
    }
    setParam (*p, "master_vol", 12.0f);
    auto out = render (*p, 2.0);
    check (allFinite (out) && peakDb (out) <= -0.25, "everything at maximum is caught by the soft limiter (" + String (peakDb (out), 2) + " dBFS)");
}

static void testState()
{
    std::cout << "State" << std::endl;
    auto a = makeProc();
    a->loadFactoryPreset (3);
    setParam (*a, "v4_tune", 61.37f);
    a->setPresetName ("My Drone");
    MemoryBlock mb;
    a->getStateInformation (mb);
    auto b = makeProc();
    b->setStateInformation (mb.getData(), (int) mb.getSize());
    int diff = 0;
    for (int i = 0; i < a->getParameters().size(); ++i)
        if (std::abs (a->getParameters()[i]->getValue() - b->getParameters()[i]->getValue()) > 1e-6f) ++diff;
    check (diff == 0 && b->getPresetName() == "My Drone", "save / restore: all parameters and the preset name identical (" + String (diff) + " differ)");
}

static void testSilentChange()
{
    std::cout << "Silent preset change" << std::endl;
    auto p = makeProc();
    p->loadFactoryPreset (1);
    auto a = render (*p, 3.0);
    float ownStep = 0.0f;   // the drone's own sharpest edge (saw wraps), measured before the fade
    for (int i = (int) (2.0 * 48000); i < a.getNumSamples(); ++i) ownStep = std::max (ownStep, std::abs (a.getSample (0, i) - a.getSample (0, i - 1)));
    p->changeSilently ([&p] { p->loadFactoryPreset (2); });   // Slow Bloom: silent until played
    auto b = render (*p, 0.01);
    pumpMessages (40);
    auto c = render (*p, 0.3);
    float maxStep = 0.0f;
    for (int i = 1; i < b.getNumSamples(); ++i) maxStep = std::max (maxStep, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
    check (p->getPresetName() == "Slow Bloom", "preset applied after the fade");
    check (rmsDb (c) < -100.0, "the old drone is cleared (" + String (rmsDb (c), 1) + " dB)");
    check (maxStep <= ownStep * 1.02f + 0.002f, "the fade adds no click: max step " + String (maxStep, 4) + " vs the drone's own " + String (ownStep, 4));
}

static void testPresets()
{
    std::cout << "Factory presets" << std::endl;
    const auto& all = factoryPresets();
    bool idsOk = true;
    {
        ResoOpusProcessor p;
        for (auto& pr : all)
        {
            for (auto& v : pr.values) if (p.param (v.first) == nullptr) { idsOk = false; std::cout << "    unknown id " << v.first << " in " << pr.name << std::endl; }
            for (auto& m : pr.mods) if (Mod::destCode (m.dest) == 0) { idsOk = false; std::cout << "    unknown destination " << m.dest << std::endl; }
        }
    }
    check (idsOk, String ((int) all.size()) + " presets reference valid parameters");
    for (int i = 0; i < (int) all.size(); ++i)
    {
        auto p = makeProc();
        p->loadFactoryPreset (i);
        auto out = render (*p, 9.0, { { 0.5, MidiMessage::noteOn (1, 60, 0.9f) }, { 0.5, MidiMessage::noteOn (1, 64, 0.7f) },
                                      { 1.0, MidiMessage::noteOn (1, 67, 0.8f) }, { 7.0, MidiMessage::noteOff (1, 60) },
                                      { 7.0, MidiMessage::noteOff (1, 64) }, { 7.0, MidiMessage::noteOff (1, 67) } });
        const double pk = peakDb (out);
        check (allFinite (out) && pk > -30.0 && pk < -1.0, String (all[(size_t) i].category) + " / " + all[(size_t) i].name + ": peak " + String (pk, 1) + " dBFS");
    }
}

static double cpuPercent (int os, bool allChoir, double seconds = 6.0)
{
    auto p = makeProc (48000.0, 256);
    setParam (*p, "oversampling", (float) os);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        if (allChoir)
        {
            setParam (*p, Params::voiceID (v, "type"), (float) VP::Choir);
            for (int i = 1; i <= 5; ++i) setParam (*p, Params::voiceID (v, "on" + String (i)), 1.0f);
            setParam (*p, Params::voiceID (v, "tide"), 60.0f);
        }
        setParam (*p, Params::voiceID (v, "hold"), 80.0f);
    }
    render (*p, 0.5, {}, 256);
    double best = 1e9;
    for (int run = 0; run < 3; ++run)
    {
        const double t0 = Time::getMillisecondCounterHiRes();
        render (*p, seconds, {}, 256);
        best = std::min (best, (Time::getMillisecondCounterHiRes() - t0) / (seconds * 1000.0) * 100.0);
    }
    return best;
}

static void testCpu()
{
    std::cout << "CPU (48 kHz, block 256, best of 3)" << std::endl;
    const double def = cpuPercent (1, false), full = cpuPercent (1, true), full4 = cpuPercent (2, true);
    check (def < timing (4.0), "default patch, 2x: " + String (def, 2) + " % of one core");
    check (full < timing (8.0), "4 CHOIR voices x 5 saws with pull, 2x: " + String (full, 2) + " % of one core");
    check (full4 < timing (15.0), "4 CHOIR voices x 5 saws with pull, 4x: " + String (full4, 2) + " % of one core");

    auto p = makeProc (48000.0, 256);
    for (int v = 0; v < Params::numVoices; ++v) setParam (*p, Params::voiceID (v, "hold"), 0.0f);
    render (*p, 0.5, {}, 256);
    const double t0 = Time::getMillisecondCounterHiRes();
    render (*p, 10.0, {}, 256);
    const double idle = (Time::getMillisecondCounterHiRes() - t0) / 100.0;
    check (idle < timing (1.0), "idle (HOLD 0, nothing played): " + String (idle, 3) + " % of one core");
}

//==============================================================================
static MouseEvent mouse (Component& c, Point<float> pos, Point<float> down, bool dragged, ModifierKeys mods = ModifierKeys (ModifierKeys::leftButtonModifier))
{
    return MouseEvent (Desktop::getInstance().getMainMouseSource(), pos, mods,
                       MouseInputSource::defaultPressure, MouseInputSource::defaultOrientation, MouseInputSource::defaultRotation,
                       MouseInputSource::defaultTiltX, MouseInputSource::defaultTiltY, &c, &c,
                       Time::getCurrentTime(), down, Time::getCurrentTime(), 1, dragged);
}

static void saveShot (Component& c, const String& name, float scale = 1.0f)
{
    if (snapshotDir == File()) return;
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    PNGImageFormat png;
    auto f = snapshotDir.getChildFile (name + ".png");
    f.deleteFile();
    FileOutputStream os (f);
    png.writeImageToStream (img, os);
}

static void testEditor()
{
    std::cout << "Editor" << std::endl;
    auto p = makeProc();
    render (*p, 4.0, { { 3.0, MidiMessage::noteOn (1, 62, 0.9f) } });
    std::unique_ptr<AudioProcessorEditor> ed (p->createEditor());
    auto* editor = dynamic_cast<ResoOpusEditor*> (ed.get());
    check (editor != nullptr && ed->getWidth() > 0, "editor created (" + String (ed->getWidth()) + " x " + String (ed->getHeight()) + ")");
    if (editor == nullptr) return;
    ed->setSize (ResoOpusEditor::logicalWidth, ResoOpusEditor::logicalHeight);
    for (int i = 0; i < 4; ++i) { render (*p, 0.05); pumpMessages (40); }

    saveShot (*ed, "drone", 2.0f);   // first paint also builds the cached textures
    const double t0 = Time::getMillisecondCounterHiRes();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    const double ms = Time::getMillisecondCounterHiRes() - t0;
    check (ms < timing (120.0), "full redraw at 2x scale: " + String (ms, 1) + " ms");

    auto* page = editor->getDronePage();
    auto* k = page->findKnob ("v1_tide");
    check (k != nullptr, "TIDE knob of voice 1 found");
    if (k != nullptr)
    {
        const float before = p->getParamReal ("v1_tide");
        const Point<float> c ((float) k->getWidth() * 0.5f, 40.0f);
        k->mouseDown (mouse (*k, c, c, false));
        k->mouseDrag (mouse (*k, c.translated (0.0f, -40.0f), c, true));
        k->mouseUp (mouse (*k, c.translated (0.0f, -40.0f), c, true));
        check (p->getParamReal ("v1_tide") > before + 5.0f, "dragging TIDE up raises it (" + String (before, 1) + " -> " + String (p->getParamReal ("v1_tide"), 1) + " %)");
    }
    if (auto* t = page->findKnob ("v1_tune"))
    {
        const Point<float> c ((float) t->getWidth() * 0.5f, 40.0f);
        t->mouseDoubleClick (mouse (*t, c, c, false));
        if (auto* te = dynamic_cast<TextEditor*> (t->getChildComponent (0)))
        {
            te->setText ("D2+12");
            te->onReturnKey();
            pumpMessages (30);
        }
        check (std::abs (p->getParamReal ("v1_tune") - 50.12f) < 0.01f, "typing \"D2+12\" into TUNE sets D2 +12 cents");
    }

    page->selectVoice (2);
    pumpMessages (40);
    saveShot (*ed, "drone-pulsar-selected", 2.0f);
    check (page->getSelectedVoice() == 2, "clicking a voice strip selects it for the detail panel");
    page->selectVoice (3);
    pumpMessages (40);
    saveShot (*ed, "drone-siren-selected", 2.0f);
    page->selectVoice (0);
    p->applyChord (0, 3);
    check (std::abs (p->getParamReal ("v1_int2") - 4.0f) < 0.001f && std::abs (p->getParamReal ("v1_int5") - 16.0f) < 0.001f,
           "CHORD Major fills the intervals 0 4 7 12 16");
    p->fillVoices (2, 2);
    check (std::abs (p->getParamReal ("v1_tune") - 38.0f) < 0.01f && std::abs (p->getParamReal ("v2_tune") - 41.0f) < 0.01f
           && std::abs (p->getParamReal ("v4_tune") - 48.0f) < 0.01f, "FILL D Dorian spreads the voices over D F A C");

    editor->setModDrawer (true);
    pumpMessages (60);
    check (editor->isModDrawerOpen(), "modulation panel opens");
    editor->setModDrawer (false);

    editor->setScalePercent (70);
    check (std::abs (ed->getWidth() - roundToInt (ResoOpusEditor::logicalWidth * 0.7f)) <= 1, "window size 70 % (" + String (ed->getWidth()) + " px)");
    ed.reset();
}

static void testHostedPlugin (const String& path)
{
    std::cout << "Hosted VST3: " << path << std::endl;
    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    KnownPluginList list;
    VST3PluginFormat vst3;
    list.scanAndAddFile (path, true, types, vst3);
    check (types.size() == 1 && types[0]->isInstrument, "VST3 scanned as an instrument");
    if (types.isEmpty()) return;
    String error;
    auto instance = fm.createPluginInstance (*types[0], 48000.0, 512, error);
    check (instance != nullptr, "VST3 instantiated " + error);
    if (instance == nullptr) return;
    instance->prepareToPlay (48000.0, 512);
    AudioBuffer<float> buf (2, 512);
    float pk = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        buf.clear();
        MidiBuffer midi;
        instance->processBlock (buf, midi);
        pk = std::max (pk, buf.getMagnitude (0, 512));
    }
    check (pk > 0.01f, "VST3 drones without any MIDI (peak " + String (Decibels::gainToDecibels (pk), 1) + " dBFS after 2 s)");
    check (instance->getParameters().size() >= 300, "VST3 exposes " + String (instance->getParameters().size()) + " parameters");
    instance->releaseResources();
}

//==============================================================================
int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI gui;
    auto root = File::getSpecialLocation (File::currentExecutableFile);
    while (root.exists() && ! root.getChildFile ("CMakeLists.txt").existsAsFile()) root = root.getParentDirectory();
    if (root.exists())
    {
        snapshotDir = root.getChildFile ("snapshots");
        snapshotDir.createDirectory();
    }
    if (argc > 1 && String (argv[1]) == "--audit")
    {
        // Changes every voice, group and global parameter on its own and reports the ones that leave the sound unchanged
        auto renderWith = [] (const String& id, int mode)
        {
            auto p = makeProc();
            setParam (*p, "v1_tune", 57.3f);       // off the grid so SNAP has something to do
            // engine parameters only matter for their type: switch the voice to it
            const int v = id.startsWith ("v") ? id[1] - '1' : -1;
            if (v >= 0 && id.contains ("siren")) setParam (*p, Params::voiceID (v, "type"), (float) VP::Siren);
            if (v >= 0 && (id.endsWith ("sharp") || id.endsWith ("bloom"))) setParam (*p, Params::voiceID (v, "type"), (float) VP::Pulsar);
            if (v >= 0 && (id.contains ("_int") || id.contains ("_on") || id.endsWith ("tide"))) setParam (*p, Params::voiceID (v, "type"), (float) VP::Choir);
            setParam (*p, "snap", id.startsWith ("snap_") ? 2.0f : 0.0f);
            for (int k = 0; k < Params::numVoices; ++k) setParam (*p, Params::voiceID (k, "attack"), 0.3f);
            if (id.contains ("_int"))   // an interval only matters when its oscillator is on
                setParam (*p, id.replace ("_int", "_on"), 1.0f);
            if (id.isNotEmpty())
            {
                auto* rp = p->param (id);
                const float v = rp->getValue();
                float nv;
                if (auto* c = dynamic_cast<AudioParameterChoice*> (rp)) nv = rp->convertTo0to1 ((float) ((c->getIndex() + (mode + 1)) % c->choices.size()));
                else if (dynamic_cast<AudioParameterBool*> (rp)) nv = v > 0.5f ? 0.0f : 1.0f;
                else nv = mode == 0 ? (v < 0.5f ? 0.85f : 0.15f) : (v < 0.5f ? 0.6f : 0.35f);
                rp->setValueNotifyingHost (nv);
            }
            // all eight plates (so every RELEASE is heard), aftertouch and pitch bend
            std::vector<Ev> ev { { 0.4, MidiMessage::channelPressureChange (1, 90) }, { 0.5, MidiMessage::pitchWheel (1, 12000) } };
            for (int k = 0; k < Params::numVoices; ++k)
            {
                static const int whiteKeys[] = { 60, 62, 64, 65 };   // the default plates C3-F3
                const int n = whiteKeys[k];
                ev.push_back ({ 0.2, MidiMessage::noteOn (1, n, k % 2 ? 0.5f : 1.0f) });
                ev.push_back ({ 1.4, MidiMessage::noteOff (1, n) });
            }
            return render (*p, 2.4, ev);
        };
        auto diffDb = [] (const AudioBuffer<float>& base, const AudioBuffer<float>& b)
        {
            const double ref = rmsDb (base);
            AudioBuffer<float> d (base);
            for (int c = 0; c < 2; ++c) d.addFrom (c, 0, b, c, 0, b.getNumSamples(), -1.0f);
            return rmsDb (d) - ref;
        };
        StringArray ids;
        for (int v = 0; v < Params::numVoices; ++v) for (auto& d : Params::voiceDefs()) ids.add (Params::voiceID (v, d.id));
        for (auto& d : Params::globalDefs()) ids.add (d.id);
        const auto base = renderWith ({}, 0);
        int effective = 0;
        for (auto& id : ids)
        {
            const double d = std::max (diffDb (base, renderWith (id, 0)), diffDb (base, renderWith (id, 1)));
            if (d < -60.0) std::printf ("NO EFFECT  %-18s %.1f dB\n", id.toRawUTF8(), d);
            else ++effective;
        }
        std::printf ("%d of %d parameters change the sound\n", effective, ids.size());
        return effective == ids.size() ? 0 : 1;
    }
    if (argc > 1 && String (argv[1]) == "--bench")
    {
        for (int os = 0; os < 3; ++os)
            std::printf ("%dx: default %.2f %%, 4 choirs %.2f %%\n", 1 << os, cpuPercent (os, false), cpuPercent (os, true));
        return 0;
    }
    testParameters();
    testProductNames();
    testDefaultPatch();
    testTuning();
    testSnap();
    testAliasing();
    testTide();
    testSiren();
    testPulsar();
    testPlates();
    testKeys();
    testRobustness();
    testState();
    testSilentChange();
    testPresets();
    testCpu();
    testEditor();
    if (argc > 1)
        testHostedPlugin (String (CharPointer_UTF8 (argv[1])));
    std::cout << std::endl << (failures == 0 ? "ALL TESTS PASSED" : String (failures) + " TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "State/FactoryPresets.h"

using namespace juce;

namespace
{
bool isBlackKey (int n) { const int pc = ((n % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

// Soft limiter: transparent below -2.5 dBFS, gently rounds into a -0.3 dBFS ceiling
inline float softLimit (float x)
{
    constexpr float t = 0.75f, room = 0.216f;
    const float a = std::abs (x);
    if (a <= t) return x;
    const float y = t + room * std::tanh ((a - t) / room);
    return x < 0.0f ? -y : y;
}
}

ResoOpusProcessor::ResoOpusProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", Params::createLayout())
{
    for (int v = 0; v < Params::numVoices; ++v)
        for (int i = 0; i < VP::count; ++i)
        {
            const auto id = Params::voiceID (v, Params::voiceDefs()[(size_t) i].id);
            voiceRaw[v][i] = apvts.getRawParameterValue (id);
            voiceParam[v][i] = apvts.getParameter (id);
            jassert (voiceRaw[v][i] != nullptr);
        }
    for (int i = 0; i < GP::count; ++i)
    {
        globalRaw[i] = apvts.getRawParameterValue (Params::globalDefs()[(size_t) i].id);
        globalParam[i] = apvts.getParameter (Params::globalDefs()[(size_t) i].id);
        jassert (globalRaw[i] != nullptr);
    }
    for (int s = 0; s < Params::numModSlots; ++s)
        for (int i = 0; i < MP::count; ++i)
            modRaw[s][i] = apvts.getRawParameterValue (Params::modID (s, Params::modSlotDefs()[(size_t) i].id));

    for (auto& v : liveDest) v.store (-1.0f);
    for (auto& v : liveSource) v.store (0.0f);
    for (int v = 0; v < Params::numVoices; ++v) { liveLevel[v].store (0.0f); liveGate[v].store (false); liveRoot[v].store (-1.0f); }

    uiState.setProperty ("presetName", "Init", nullptr);
    history = std::make_unique<StateHistory> (*this);
}

ResoOpusProcessor::~ResoOpusProcessor()
{
    silentChange.stopTimer();
    cancelPendingUpdate();
}

bool ResoOpusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void ResoOpusProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    zeros.setSize (2, chunkSize);
    zeros.clear();
    mixBuf.setSize (2, chunkSize * 4);
    for (int f = 0; f < 2; ++f)
    {
        oversamplers[f] = std::make_unique<juce::dsp::Oversampling<float>> (2, f + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[f]->initProcessing ((size_t) chunkSize);
    }
    for (auto& d : dc) { d.setup (sr); d.reset(); }

    osIndex = jlimit (0, 2, (int) globalRaw[GP::oversampling]->load());
    for (int v = 0; v < Params::numVoices; ++v)
    {
        voices[v].prepare (sr * (1 << osIndex), 0x51ed2701u + (uint32_t) v * 7919u);
        drift[v].seed (0x9e37u + (uint32_t) v * 104729u);
        voiceMod[v] = 0.0f;
    }
    hiss[0].seed (0x1234567u); hiss[1].seed (0x7654321u);
    hiss[0].reset(); hiss[1].reset();
    lastMono = 0.0f;
    networkPrimed = false;
    matrix.reset();
    keyboardState.reset();
    for (auto& t : touch) t = Touch();
    pendingLatency = computeLatency();
    setLatencySamples (pendingLatency);
}

void ResoOpusProcessor::setOversampling (int index)
{
    osIndex = index;
    for (auto& c : voices) c.setSampleRate (sr * (1 << osIndex));
    for (auto& o : oversamplers) if (o) o->reset();
    pendingLatency = computeLatency();
    triggerAsyncUpdate();
}

int ResoOpusProcessor::computeLatency() const
{
    return osIndex == 0 ? 0 : (int) std::lround (oversamplers[osIndex - 1]->getLatencyInSamples());
}

void ResoOpusProcessor::changeSilently (std::function<void()> change)
{
    silentTarget = 0.0f;
    silentChange.stopTimer();
    silentChange.action = [this, change]
    {
        change();
        resetRequest = true;
        silentTarget = 1.0f;
    };
    silentChange.startTimer (isNonRealtime() ? 1 : 25);
}

void ResoOpusProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
}

//==============================================================================
int ResoOpusProcessor::plateNote (int voice) const
{
    int n = roundToInt (getParamReal ("plates_base"));
    while (isBlackKey (n)) ++n;
    for (int k = 0; k < voice; ++k) { ++n; while (isBlackKey (n)) ++n; }
    return n;
}

void ResoOpusProcessor::noteOn (int note, float vel)
{
    lastVelocity = vel;
    int v = -1;
    if ((int) globalRaw[GP::play_mode]->load() == GP::Plates)
    {
        int n = roundToInt (globalRaw[GP::plates_base]->load());
        while (isBlackKey (n)) ++n;
        for (int k = 0; k < Params::numVoices; ++k)
        {
            if (n == note) { v = k; break; }
            ++n;
            while (isBlackKey (n)) ++n;
        }
        if (v < 0) return;
    }
    else
    {
        // KEYS: the next free voice that is not OFF takes the note (round robin), else the next one
        for (int pass = 0; pass < 2 && v < 0; ++pass)
            for (int k = 0; k < Params::numVoices; ++k)
            {
                const int c = (keysNext + k) % Params::numVoices;
                if ((int) voiceRaw[c][VP::type]->load() == VP::Off) continue;
                if (pass == 0 && touch[c].gate) continue;
                v = c;
                break;
            }
        if (v < 0) return;
        keysNext = (v + 1) % Params::numVoices;
        touch[v].keyRoot = (double) note;
    }
    touch[v].gate = true;
    touch[v].note = note;
    touch[v].velocity = vel;
    touch[v].pressure = 0.0f;
    sustained[v] = false;
}

void ResoOpusProcessor::noteOff (int note)
{
    for (int v = 0; v < Params::numVoices; ++v)
        if (touch[v].gate && touch[v].note == note)
        {
            if (sustain) sustained[v] = true;
            else touch[v].gate = false;
        }
}

void ResoOpusProcessor::releaseAll()
{
    for (int v = 0; v < Params::numVoices; ++v) { touch[v].gate = false; sustained[v] = false; }
}

void ResoOpusProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchBend = jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f);
    else if (m.isChannelPressure())
        pressure = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
    {
        for (auto& t : touch)
            if (t.gate && t.note == m.getNoteNumber()) t.pressure = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;
        if (cc == 1) modWheel = v;
        else if (cc == 64)
        {
            const bool was = sustain;
            sustain = v >= 0.5f;
            if (was && ! sustain)
                for (int k = 0; k < Params::numVoices; ++k)
                    if (sustained[k]) { touch[k].gate = false; sustained[k] = false; }
        }
        else if (cc == 120 || cc == 123)
        {
            releaseAll();
            if (cc == 120) for (auto& c : voices) c.reset();
        }
    }
}

double ResoOpusProcessor::snapNote (double midi) const
{
    const int mode = (int) curG[GP::snap];
    if (mode == GP::SnapChromatic) return std::round (midi);
    if (mode != GP::SnapScale) return midi;
    const auto& steps = Params::scaleSteps ((int) curG[GP::snap_scale]);
    const int root = (int) curG[GP::snap_root];
    double best = midi, dist = 1.0e9;
    const int base = (int) std::floor (midi) - 13;
    for (int n = base; n <= base + 26; ++n)
    {
        const int pc = ((n - root) % 12 + 12) % 12;
        if (! steps.contains (pc)) continue;
        if (std::abs (n - midi) < dist) { dist = std::abs (n - midi); best = n; }
    }
    return best;
}

//==============================================================================
void ResoOpusProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const double t0 = Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 2)
        return;

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (resetRequest.exchange (false))
    {
        for (auto& c : voices) c.reset();
        for (auto& m : voiceMod) m = 0.0f;
        lastMono = 0.0f;
        for (auto& d : dc) d.reset();
        for (auto& o : oversamplers) if (o) o->reset();
        releaseAll();
        for (auto& t : touch) t.keyRoot = -1.0;
    }

    // on-screen wheels
    const float ub = uiPitchBend.load(), uw = uiModWheel.load();
    if (std::abs (ub - lastUiBend) > 1.0e-6f) { pitchBend = ub; lastUiBend = ub; }
    if (std::abs (uw - lastUiWheel) > 1.0e-6f) { modWheel = uw; lastUiWheel = uw; }

    const int wantOs = jlimit (0, 2, (int) globalRaw[GP::oversampling]->load());
    if (wantOs != osIndex)
        setOversampling (wantOs);

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        while (it != midi.cend() && (*it).samplePosition <= pos)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        int next = std::min (n, pos + chunkSize);
        if (it != midi.cend())
            next = std::min (next, std::max (pos + 1, (*it).samplePosition));
        processChunk (buffer, pos, next - pos);
        pos = next;
    }
    while (it != midi.cend()) { handleMidi ((*it).getMessage()); ++it; }

    float pk = 0.0f;
    for (int c = 0; c < 2; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    meterPeak = std::max (meterPeak.load() - 0.5f, Decibels::gainToDecibels (pk, -100.0f));
    for (int v = 0; v < Params::numVoices; ++v)
    {
        liveLevel[v] = voices[v].getLevel();
        liveGate[v] = touch[v].gate;
        liveRoot[v] = (float) settings[v].rootMidi;
    }
    for (int i = 0; i < Mod::numSources; ++i) liveSource[i] = sources[i];

    const double elapsed = Time::getMillisecondCounterHiRes() - t0;
    const float load = (float) (elapsed / (1000.0 * n / sr) * 100.0);
    cpuLoad = cpuLoad.load() * 0.9f + load * 0.1f;
}

void ResoOpusProcessor::processChunk (AudioBuffer<float>& out, int start, int len)
{
    const double seconds = len / sr;

    // 1. sources
    float anyPressure = pressure;
    for (auto& t : touch) if (t.gate) anyPressure = std::max (anyPressure, t.pressure);
    sources[Mod::None] = 0.0f;
    sources[Mod::Velocity] = lastVelocity;
    sources[Mod::ModWheel] = modWheel;
    sources[Mod::Pressure] = anyPressure;
    sources[Mod::PitchBend] = pitchBend;
    sources[Mod::Constant] = 1.0f;

    // 2. matrix
    for (int s = 0; s < Params::numModSlots; ++s)
    {
        auto& sl = slots[s];
        sl.on = modRaw[s][MP::on]->load() > 0.5f;
        sl.src = jlimit (0, Mod::numSources - 1, (int) modRaw[s][MP::src]->load());
        sl.dst = jlimit (0, Mod::maxDestCode, (int) modRaw[s][MP::dst]->load());
        sl.amt = modRaw[s][MP::amt]->load();
        sl.bipolar = modRaw[s][MP::bi]->load() > 0.5f;
        sl.ctl = jlimit (0, Mod::numSources - 1, (int) modRaw[s][MP::ctl]->load());
        sl.ctlAmt = modRaw[s][MP::ctlamt]->load();
        sl.fn = (int) modRaw[s][MP::fn]->load();
        sl.fnAmt = modRaw[s][MP::fnamt]->load() / 100.0f;
    }
    matrix.process (slots, sources, seconds, modOut);

    // 3. modulated parameter values
    for (int v = 0; v < Params::numVoices; ++v)
        for (int i = 0; i < VP::count; ++i) curV[v][i] = voiceRaw[v][i]->load();
    for (int i = 0; i < GP::count; ++i) curG[i] = globalRaw[i]->load();

    for (int s = 0; s < Params::numModSlots; ++s)
    {
        if (! slots[s].active()) continue;
        const int d = slots[s].dst;
        if (! modTouched[d]) { modTouched[d] = true; modSum[d] = 0.0f; }
        modSum[d] += modOut[s];
    }
    for (int s = 0; s < Params::numModSlots; ++s)
    {
        if (! slots[s].active()) continue;
        const int d = slots[s].dst;
        if (! modTouched[d]) continue;
        modTouched[d] = false;
        const int block = d / Mod::blockSize, idx = d % Mod::blockSize;
        RangedAudioParameter* prm = nullptr;
        float* target = nullptr;
        if (block >= 1 && block <= Params::numVoices && idx < VP::count) { prm = voiceParam[block - 1][idx]; target = &curV[block - 1][idx]; }
        else if (block == Mod::globalBlock && idx < GP::count) { prm = globalParam[idx]; target = &curG[idx]; }
        if (prm == nullptr) continue;
        const float norm = jlimit (0.0f, 1.0f, prm->convertTo0to1 (*target) + modSum[d]);
        *target = prm->convertFrom0to1 (norm);
        if (editorOpen.load (std::memory_order_relaxed)) liveDest[d].store (norm, std::memory_order_relaxed);
    }

    // 4. voice settings
    const int touchMode = (int) curG[GP::touch];
    const float gravity = curG[GP::gravity] / 100.0f, dust = curG[GP::dust] / 100.0f;
    liveGravity.store (gravity, std::memory_order_relaxed);
    // slow tuning drift: a little beating in STILL, more with DUST (cents)
    const float driftCents = 4.0f * std::min (1.0f, gravity / 0.25f) + 8.0f * dust;
    const double bend = (double) (pitchBend * curG[GP::pb_range]);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        auto& s = settings[v];
        auto& t = touch[v];
        const float* p = curV[v];
        if (! t.gate && t.keyRoot >= 0.0 && voices[v].isSilent()) t.keyRoot = -1.0;   // KEYS: back to TUNE after the release

        s.type = (int) p[VP::type];
        if (s.type != lastType[v]) { if (lastType[v] >= 0 && voices[v].isSilent()) voices[v].reset(); lastType[v] = s.type; }
        const bool choir = s.type == VP::Choir;
        const double base = (t.keyRoot >= 0.0 ? t.keyRoot : (double) p[VP::tune]) + (double) curG[GP::pitch];
        const float dr = drift[v].process (::dsp::RandomGen::Perlin, 0.11 + 0.03 * v, 4.0f, seconds, false, 0.0);
        s.rootMidi = snapNote (base) + bend + (choir ? (double) Params::tideSemitones (p[VP::tide] / 100.0f) : 0.0)
                     + (double) (dr * driftCents / 100.0f);
        s.pull = choir ? Params::tidePull (p[VP::tide] / 100.0f) : 0.0f;
        s.sirenRate = p[VP::siren_rate];
        s.sirenFm = p[VP::siren_fm] / 100.0f;
        s.sirenAm = p[VP::siren_am] / 100.0f;
        s.sirenDiv = p[VP::siren_div];
        s.sirenNoise = p[VP::siren_noise] / 100.0f;
        s.sirenWave = p[VP::siren_wave] / 100.0f;
        s.sharp = p[VP::sharp] / 100.0f;
        s.bloom = p[VP::bloom] / 100.0f;
        s.twin = p[VP::twin];
        s.twinMix = p[VP::twin_mix] / 100.0f;
        s.sub = p[VP::sub] / 100.0f;
        s.vibrato = p[VP::vibrato] / 100.0f;
        for (int i = 0; i < Params::numOscs; ++i)
        {
            s.intervals[i] = p[VP::int1 + i];
            s.on[i] = p[VP::on1 + i] > 0.5f;
        }
        s.a4 = curG[GP::a4];

        const float hold = p[VP::hold] / 100.0f;
        float level = 0.0f;
        if (t.gate)
        {
            const float pr = std::max (t.pressure, pressure);
            switch (touchMode)
            {
                case GP::TouchAftertouch: level = 0.35f + 0.65f * pr; break;
                case GP::TouchBoth:       level = jlimit (0.0f, 1.0f, 0.25f + 0.45f * t.velocity + 0.6f * pr); break;
                default:                  level = 0.35f + 0.65f * t.velocity; break;
            }
        }
        // a touch always adds on top of HOLD (a share of the remaining headroom), so the plate swell is audible
        s.target = t.gate ? hold + (1.0f - hold) * level : hold;
        // a light touch swells in slower (like a finger barely on a sensor)
        s.attack = p[VP::attack] * (t.gate ? 1.6f - 0.6f * level : 1.0f);
        s.release = p[VP::release];
        const float gain = p[VP::volume] <= -59.9f ? 0.0f : Decibels::decibelsToGain (p[VP::volume]);
        const float ang = (jlimit (-1.0f, 1.0f, p[VP::pan]) + 1.0f) * MathConstants<float>::pi * 0.25f;
        s.gainL = gain * std::cos (ang) * MathConstants<float>::sqrt2;
        s.gainR = gain * std::sin (ang) * MathConstants<float>::sqrt2;
    }

    // 5. voices (oversampled)
    float* outL = out.getWritePointer (0) + start;
    float* outR = out.getWritePointer (1) + start;
    {
        const int path = (int) curG[GP::gravity_path];
        const float ring = path == GP::PathPairs ? 0.0f : (path == GP::PathRing ? 1.0f : Params::gravityRing (gravity));
        const int count = len << osIndex;
        auto set = [this, count] (::dsp::Ramp& r, float t) { if (networkPrimed) r.set (t, count); else r.reset (t); };
        set (devRamp, Params::gravityDeviation (gravity));
        set (ringRamp, ring);
        set (fbRamp, Params::gravityFeedback (gravity));
        set (dustRamp, 6.0f * dust);   // Hz of leakage between the voices
        networkPrimed = true;
    }
    if (osIndex == 0)
        renderVoices (outL, outR, len);
    else
    {
        auto& os = *oversamplers[osIndex - 1];
        juce::dsp::AudioBlock<const float> zin (zeros.getArrayOfReadPointers(), 2, (size_t) len);
        auto up = os.processSamplesUp (zin);
        renderVoices (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
        float* chans[2] = { outL, outR };
        juce::dsp::AudioBlock<float> down (chans, 2, (size_t) len);
        os.processSamplesDown (down);
    }

    // 6. output
    const float master = curG[GP::master_vol] <= -59.9f ? 0.0f : Decibels::decibelsToGain (curG[GP::master_vol]);
    const float silentTargetNow = silentTarget.load (std::memory_order_relaxed);
    const float silentStep = (float) (1.0 / (0.006 * sr));
    for (int i = 0; i < len; ++i)
    {
        if (silentGain < silentTargetNow) silentGain = std::min (silentTargetNow, silentGain + silentStep);
        else if (silentGain > silentTargetNow) silentGain = std::max (silentTargetNow, silentGain - silentStep);
        const float g = master * silentGain;
        const float breath = dust * 0.002f;   // background noise, up to -54 dBFS
        float l = (dc[0].process (outL[i]) + breath * hiss[0].process (-0.5f)) * g;
        float r = (dc[1].process (outR[i]) + breath * hiss[1].process (-0.5f)) * g;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        outL[i] = softLimit (l);
        outR[i] = softLimit (r);
    }
}

// GRAVITY: every voice is frequency-modulated by its partner (pairs 1-2, 3-4) and/or the next voice (ring 1-2-3-4-1),
// one sample late, plus the instrument's own output in TURBULENCE and COLLAPSE. DUST leaks all voices into each other.
void ResoOpusProcessor::renderVoices (float* L, float* R, int count)
{
    std::fill (L, L + count, 0.0f);
    std::fill (R, R + count, 0.0f);
    bool on[Params::numVoices];
    bool any = false;
    for (int v = 0; v < Params::numVoices; ++v) { on[v] = voices[v].begin (settings[v], count); any = any || on[v]; }
    if (! any)
    {
        for (auto& m : voiceMod) m = 0.0f;
        lastMono = 0.0f;
        for (int k = 0; k < count; ++k) { devRamp.next(); ringRamp.next(); fbRamp.next(); dustRamp.next(); }
        return;
    }
    for (int k = 0; k < count; ++k)
    {
        const float dev = devRamp.next(), ring = ringRamp.next(), dustDev = dustRamp.next();
        const float fb = fbRamp.next() * lastMono;
        const float total = voiceMod[0] + voiceMod[1] + voiceMod[2] + voiceMod[3];
        float now[Params::numVoices];
        for (int v = 0; v < Params::numVoices; ++v)
        {
            if (! on[v]) { now[v] = 0.0f; continue; }
            const float in = (1.0f - ring) * voiceMod[v ^ 1] + ring * voiceMod[(v + 1) % Params::numVoices] + fb;
            const double fmHz = (double) (dev * in + dustDev * (total - voiceMod[v]));
            now[v] = voices[v].tick (fmHz, L[k], R[k]);
        }
        for (int v = 0; v < Params::numVoices; ++v) voiceMod[v] = now[v];
        lastMono = jlimit (-1.5f, 1.5f, (L[k] + R[k]) * 1.2f);
    }
}

//==============================================================================
void ResoOpusProcessor::setParamReal (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
    else
        jassertfalse;
}

float ResoOpusProcessor::getParamReal (const String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void ResoOpusProcessor::applyChord (int voice, int chord)
{
    const float* iv = Params::chordIntervals (chord);
    for (int i = 0; i < Params::numOscs; ++i)
        setParamReal (Params::voiceID (voice, "int" + String (i + 1)), iv[i]);
}

void ResoOpusProcessor::fillVoices (int scale, int root)
{
    // degrees 1-3-5-7 of the scale from the root, starting at C1 + root
    const auto& steps = Params::scaleSteps (scale);
    const int baseNote = 36 + root;
    for (int k = 0; k < Params::numVoices; ++k)
    {
        const int degree = k * 2;
        const int note = baseNote + steps[degree % steps.size()] + 12 * (degree / steps.size());
        setParamReal (Params::voiceID (k, "tune"), (float) note);
    }
    setParamReal ("snap_scale", (float) scale);
    setParamReal ("snap_root", (float) root);
}

int ResoOpusProcessor::findFreeSlot() const
{
    for (int s = 0; s < Params::numModSlots; ++s)
        if ((int) getParamReal (Params::modID (s, "src")) == 0 || (int) getParamReal (Params::modID (s, "dst")) == 0)
            return s;
    return -1;
}

int ResoOpusProcessor::addModulation (int source, const String& destID, float amount)
{
    const int code = Mod::destCode (destID);
    if (code == 0 || source <= 0) return -1;
    for (int s = 0; s < Params::numModSlots; ++s)   // same routing already there: just enable it
        if ((int) getParamReal (Params::modID (s, "src")) == source && (int) getParamReal (Params::modID (s, "dst")) == code)
        {
            setParamReal (Params::modID (s, "on"), 1.0f);
            return s;
        }
    const int s = findFreeSlot();
    if (s < 0) return -1;
    for (auto& d : Params::modSlotDefs())
        if (auto* p = param (Params::modID (s, d.id))) p->setValueNotifyingHost (p->getDefaultValue());
    setParamReal (Params::modID (s, "src"), (float) source);
    setParamReal (Params::modID (s, "dst"), (float) code);
    setParamReal (Params::modID (s, "amt"), amount);
    setParamReal (Params::modID (s, "on"), 1.0f);
    return s;
}

void ResoOpusProcessor::removeModulation (int s)
{
    for (auto& d : Params::modSlotDefs())
        if (auto* p = param (Params::modID (s, d.id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

Array<int> ResoOpusProcessor::slotsForDestination (const String& destID) const
{
    Array<int> a;
    const int code = Mod::destCode (destID);
    if (code == 0) return a;
    for (int s = 0; s < Params::numModSlots; ++s)
        if ((int) getParamReal (Params::modID (s, "dst")) == code && (int) getParamReal (Params::modID (s, "src")) > 0)
            a.add (s);
    return a;
}

//==============================================================================
void ResoOpusProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

int ResoOpusProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

void ResoOpusProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < getNumPrograms())
        loadFactoryPreset (index);
}

const String ResoOpusProcessor::getProgramName (int index)
{
    if (index >= 0 && index < getNumPrograms()) return factoryPresets()[(size_t) index].name;
    return {};
}

void ResoOpusProcessor::loadFactoryPreset (int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int) all.size()) return;
    const auto& pr = all[(size_t) index];
    resetToDefaults();
    for (auto& v : pr.values)
        if (auto* p = param (v.first))
            p->setValueNotifyingHost (p->convertTo0to1 (v.second));
    int slot = 0;
    for (auto& m : pr.mods)
    {
        if (slot >= Params::numModSlots) break;
        auto set = [&] (const char* id, float v) { if (auto* p = param (Params::modID (slot, id))) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set ("on", 1.0f);
        set ("src", (float) m.source);
        set ("dst", (float) Mod::destCode (m.dest));
        set ("amt", m.amount);
        set ("bi", m.bipolar ? 1.0f : 0.0f);
        set ("fn", (float) m.function);
        set ("fnamt", m.functionAmount);
        set ("ctl", (float) m.controller);
        ++slot;
    }
    currentPreset = index;
    uiState.setProperty ("presetName", String (pr.name), nullptr);
    uiState.setProperty ("presetIndex", index, nullptr);
    if (history) history->resync();
}

void ResoOpusProcessor::getStateInformation (MemoryBlock& dest)
{
    ValueTree root ("ResoOpus");
    root.setProperty ("version", JucePlugin_VersionString, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    if (history) root.appendChild (history->toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void ResoOpusProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = ValueTree::fromXml (*xml);
    if (! root.hasType ("ResoOpus")) return;
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    auto ui = root.getChildWithName ("UI");
    if (ui.isValid()) uiState.copyPropertiesAndChildrenFrom (ui, nullptr);
    currentPreset = (int) uiState.getProperty ("presetIndex", 0);
    if (history)
    {
        history->fromValueTree (root.getChildWithName ("AB"));
        history->resync();
    }
}

AudioProcessorEditor* ResoOpusProcessor::createEditor() { return new ResoOpusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResoOpusProcessor(); }

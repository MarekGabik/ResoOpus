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
    presets = std::make_unique<PresetManager> (*this);

    // the user's default preset (a host session restores its own state on top of it)
    const auto def = presets->getDefaultKey();
    if (def.isNotEmpty() && loadUserDefault)
        if (const int i = presets->indexOf (def); i > 0) presets->load (i, false);
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
    echo.prepare (sr);
    space.prepare (sr);
    for (int i = 0; i < maxLead; ++i) leads[i].prepare (sr * (1 << osIndex), 0x7a11u + (uint32_t) i * 131u);
    leadSusNum = 0; arpNote = -1; lastLeadPitch = -1.0;
    lfos[0].reset (0.0); lfos[1].reset (0.0); lfoRng.seed (0x51f0u); motionRnd.seed (0x7e57u); chunkPpq = hostPpq = 0.0;
    leadNum = arpNum = 0;
    arpPos = 0.0; arpGate = false;
    for (auto& f : filters) { f.prepare (sr * (1 << osIndex)); f.reset(); }
    for (auto& f : filtersB) { f.prepare (sr * (1 << osIndex)); f.reset(); }
    filterPrimed = false;
    echoPrevLen = 0;
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
    for (auto& l : leads) l.setSampleRate (sr * (1 << osIndex));
    for (auto& f : filters) f.prepare (sr * (1 << osIndex));
    for (auto& f : filtersB) f.prepare (sr * (1 << osIndex));
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
    noteTrigger = true;
    int v = -1;
    const bool leadOn = globalRaw[GP::lead_on]->load() > 0.5f;
    if (globalRaw[GP::drone_on]->load() < 0.5f)
    {
        if (leadOn) leadNoteOn (note, vel);   // LEAD only: every key, the plates too
        return;
    }
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
        if (v < 0) { if (leadOn) leadNoteOn (note, vel); return; }   // every other key plays the LEAD
    }
    else if (leadOn) { leadNoteOn (note, vel); return; }
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
    leadNoteOff (note);
    for (int v = 0; v < Params::numVoices; ++v)
        if (touch[v].gate && touch[v].note == note)
        {
            if (sustain) sustained[v] = true;
            else touch[v].gate = false;
        }
}

void ResoOpusProcessor::releaseAll()
{
    leadNum = 0;
    if (globalRaw[GP::arp_latch]->load() < 0.5f) arpNum = 0;
    leadSusNum = 0;
    leadStopAll();
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
            {
                for (int k = 0; k < Params::numVoices; ++k)
                    if (sustained[k]) { touch[k].gate = false; sustained[k] = false; }
                const int n = leadSusNum;
                leadSusNum = 0;
                for (int i = 0; i < n; ++i) leadNoteOff (leadSus[i]);
            }
        }
        else if (cc == 120 || cc == 123)
        {
            releaseAll();
            if (cc == 120) { for (auto& c : voices) c.reset(); for (auto& l : leads) l.reset(); arpNum = 0; }
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
        for (auto& f : filters) f.reset();
        for (auto& f : filtersB) f.reset();
        echo.reset();
        space.reset();
        echoPrevLen = 0;
        for (auto& d : dc) d.reset();
        for (auto& o : oversamplers) if (o) o->reset();
        releaseAll();
        for (auto& t : touch) t.keyRoot = -1.0;
    }

    // on-screen wheels
    const float ub = uiPitchBend.load(), uw = uiModWheel.load();
    if (std::abs (ub - lastUiBend) > 1.0e-6f) { pitchBend = ub; lastUiBend = ub; }
    if (std::abs (uw - lastUiWheel) > 1.0e-6f) { modWheel = uw; lastUiWheel = uw; }

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm = jlimit (20.0, 999.0, *bpm);
            hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) hostPpq = *ppq;
        }
    chunkPpq = hostPpq;

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

// LFO 1 / 2 and RANDOM, once per chunk. In tempo they follow the song position while the host plays.
void ResoOpusProcessor::runMotion (double seconds)
{
    const double beatsNow = chunkPpq;
    chunkPpq += seconds * hostBpm / 60.0;
    for (int l = 0; l < 2; ++l)
    {
        const int base = l == 0 ? GP::lfo1_rate : GP::lfo2_rate;
        const auto raw = [this, base] (int i) { return globalRaw[base + i]->load(); };
        const bool sync = raw (2) > 0.5f;
        const double beats = Mod::divisionBeats ((int) raw (3));
        const double hz = sync ? hostBpm / 60.0 / beats : (double) raw (0);
        const double start = (double) raw (4) / 360.0;
        const double before = lfos[l].getPhase();
        if (noteTrigger && raw (5) > 0.5f) lfos[l].setPhase (start);
        else if (sync && hostPlaying) lfos[l].setPhase (beatsNow / beats + start);
        else lfos[l].advance (hz, seconds);
        const int shape = (int) raw (1);
        if (lfos[l].getPhase() < before || (noteTrigger && raw (5) > 0.5f)) lfoHold[l] = lfoRng.next();   // S&H: new step every cycle
        sources[Mod::Lfo1 + l] = shape >= 5 ? lfoHold[l] : ::dsp::Lfo::shape (shape, lfos[l].getPhase());
    }
    {
        const bool sync = globalRaw[GP::rnd_sync]->load() > 0.5f;
        const double beats = Mod::divisionBeats ((int) globalRaw[GP::rnd_div]->load());
        const double hz = sync ? hostBpm / 60.0 / beats : (double) globalRaw[GP::rnd_rate]->load();
        static const int modes[] = { ::dsp::RandomGen::Perlin, ::dsp::RandomGen::SampleHold, ::dsp::RandomGen::Noise };
        const int mode = modes[jlimit (0, 2, (int) globalRaw[GP::rnd_mode]->load())];
        const double hostPhase = beatsNow / beats;
        sources[Mod::Random] = motionRnd.process (mode, hz, globalRaw[GP::rnd_slew]->load(), seconds,
                                                  sync && hostPlaying, hostPhase - std::floor (hostPhase));
    }
    sources[Mod::LeadEnv] = leads[leadLast].isActive() ? leads[leadLast].getFilterEnv() : 0.0f;
    noteTrigger = false;
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
    runMotion (seconds);

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
    runArp (len);
    {
        auto& L = leadSettings;
        float anyPressure = pressure;
        for (auto& t : touch) anyPressure = std::max (anyPressure, t.pressure);
        L.a4 = curG[GP::a4];
        L.wave = curG[GP::lead_wave];
        L.octave = curG[GP::lead_octave] - 2.0f;
        L.osc2 = curG[GP::lead_osc2]; L.mix = curG[GP::lead_mix] / 100.0f; L.sub = curG[GP::lead_sub] / 100.0f;
        L.sync = curG[GP::lead_sync] > 0.5f;
        L.attack = curG[GP::lead_attack]; L.decay = curG[GP::lead_decay]; L.sustain = curG[GP::lead_sustain] / 100.0f; L.release = curG[GP::lead_release];
        L.glide = curG[GP::lead_glide];
        L.vibrato = curG[GP::lead_vib] / 100.0f; L.vibRate = curG[GP::lead_vib_rate]; L.vibDelay = curG[GP::lead_vib_delay];
        L.touchVib = curG[GP::lead_touch_vib] / 100.0f; L.pressure = anyPressure;
        const float gain = curG[GP::lead_on] < 0.5f || curG[GP::lead_level] <= -59.9f ? 0.0f : Decibels::decibelsToGain (curG[GP::lead_level]);
        const float ang = (jlimit (-1.0f, 1.0f, curG[GP::lead_pan]) + 1.0f) * MathConstants<float>::pi * 0.25f;
        L.gainL = gain * std::cos (ang) * MathConstants<float>::sqrt2;
        L.gainR = gain * std::sin (ang) * MathConstants<float>::sqrt2;
        L.velSens = curG[GP::lead_vel] / 100.0f;
        L.cutoff = curG[GP::lead_flt_cutoff]; L.res = curG[GP::lead_flt_res] / 10.0f;
        L.fenvAmount = curG[GP::lead_flt_env] / 100.0f; L.keyTrack = curG[GP::lead_flt_key] / 100.0f;
        L.filterModel = (int) curG[GP::lead_flt_model]; L.filterMode = (int) curG[GP::lead_flt_mode];
        L.filterOn = L.cutoff < 19900.0f || std::abs (L.fenvAmount) > 0.001f || L.filterMode != 0;
        L.fAttack = curG[GP::lead_fenv_attack]; L.fDecay = curG[GP::lead_fenv_decay];
        L.fSustain = curG[GP::lead_fenv_sustain] / 100.0f; L.fRelease = curG[GP::lead_fenv_release];
        if (curG[GP::lead_voices] >= 2.0f) { L.gainL *= 0.7f; L.gainR *= 0.7f; }   // chords sum up
        if (curG[GP::lead_on] < 0.5f && anyLeadActive()) leadStopAll();
        // Poly 4: voices 5-8 let go when switching down
        const int maxV = curG[GP::lead_voices] >= 3.0f ? maxLead : (curG[GP::lead_voices] >= 2.0f ? 4 : 1);
        for (int i = maxV; i < maxLead; ++i) if (leads[i].isActive() && ! leads[i].isReleasing()) leads[i].noteOff();
        liveLead.store (leads[leadLast].isActive() ? (float) leads[leadLast].getPitch() + 12.0f * L.octave : -1.0f, std::memory_order_relaxed);
    }
    // slow tuning drift: a little beating in STILL, more with DUST (cents)
    const float driftCents = 4.0f * std::min (1.0f, gravity / 0.25f) + 8.0f * dust;
    const double bend = (double) (pitchBend * curG[GP::pb_range]);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        auto& s = settings[v];
        auto& t = touch[v];
        const float* p = curV[v];
        if (! t.gate && t.keyRoot >= 0.0 && voices[v].isSilent()) t.keyRoot = -1.0;   // KEYS: back to TUNE after the release

        s.type = curG[GP::drone_on] > 0.5f ? (int) p[VP::type] : VP::Off;   // drone off: the voices fade out (20 ms)
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
        toVoices = curG[GP::echo_voices] / 100.0f;

        // filter coefficients at the oversampled rate (left / right spread apart)
        const double osRate = sr * (1 << osIndex);
        const float spread = 0.25f * curG[GP::flt_spread] / 100.0f;
        for (int c = 0; c < 2; ++c)
        {
            const double fc = std::min (0.45 * osRate, (double) curG[GP::flt_cutoff] * (c == 0 ? 1.0 - spread : 1.0 + spread));
            const float g = (float) std::tan (MathConstants<double>::pi * fc / osRate);
            if (filterPrimed) cutoffRamp[c].set (g, count); else cutoffRamp[c].reset (g);
        }
        for (int c = 0; c < 2; ++c)
        {
            const double fc = std::min (0.45 * osRate, (double) curG[GP::flt2_cutoff] * (c == 0 ? 1.0 - spread : 1.0 + spread));
            const float g = (float) std::tan (MathConstants<double>::pi * fc / osRate);
            if (filterPrimed) cutoffRampB[c].set (g, count); else cutoffRampB[c].reset (g);
        }
        auto setR = [this, count] (::dsp::Ramp& r, float t) { if (filterPrimed) r.set (t, count); else r.reset (t); };
        setR (resRamp, curG[GP::flt_res] / 10.0f);
        setR (driveRamp, curG[GP::flt_drive] / 10.0f);
        setR (resRampB, curG[GP::flt2_res] / 10.0f);
        setR (driveRampB, curG[GP::flt2_drive] / 10.0f);
        filterPrimed = true;
    }
    if (osIndex == 0)
    {
        float* lL = mixBuf.getWritePointer (0);
        float* lR = mixBuf.getWritePointer (1);
        renderVoices (outL, outR, lL, lR, len);
        applyFilter (outL, outR, lL, lR, len);
    }
    else
    {
        auto& os = *oversamplers[osIndex - 1];
        juce::dsp::AudioBlock<const float> zin (zeros.getArrayOfReadPointers(), 2, (size_t) len);
        auto up = os.processSamplesUp (zin);
        float* lL = mixBuf.getWritePointer (0);
        float* lR = mixBuf.getWritePointer (1);
        renderVoices (up.getChannelPointer (0), up.getChannelPointer (1), lL, lR, (int) up.getNumSamples());
        applyFilter (up.getChannelPointer (0), up.getChannelPointer (1), lL, lR, (int) up.getNumSamples());
        float* chans[2] = { outL, outR };
        juce::dsp::AudioBlock<float> down (chans, 2, (size_t) len);
        os.processSamplesDown (down);
    }

    // 6. effects at the base rate: DC blocker, ECHO, SPACE
    for (int i = 0; i < len; ++i) { outL[i] = dc[0].process (outL[i]); outR[i] = dc[1].process (outR[i]); }
    {
        ::dsp::Echo::Settings es;
        es.time1 = curG[GP::echo_time1]; es.time2 = curG[GP::echo_time2];
        es.feedback = curG[GP::echo_fb] / 100.0f; es.mix = curG[GP::echo_mix] / 100.0f;
        es.mod = curG[GP::echo_mod] / 100.0f; es.self = (int) curG[GP::echo_self] == 0; es.dust = dust;
        echo.process (outL, outR, len, es, fxRng, echoPrev);   // the echo is remembered for the voices (one chunk late)
        echoPrevLen = len;
        space.process (outL, outR, len, (int) curG[GP::space_algo], curG[GP::space_x] / 100.0f, curG[GP::space_y] / 100.0f,
                       curG[GP::space_z] / 100.0f, curG[GP::space_blend] / 100.0f, dust, fxRng);
    }

    // 7. output
    const float master = curG[GP::master_vol] <= -59.9f ? 0.0f : Decibels::decibelsToGain (curG[GP::master_vol]);
    const float silentTargetNow = silentTarget.load (std::memory_order_relaxed);
    const float silentStep = (float) (1.0 / (0.006 * sr));
    for (int i = 0; i < len; ++i)
    {
        if (silentGain < silentTargetNow) silentGain = std::min (silentTargetNow, silentGain + silentStep);
        else if (silentGain > silentTargetNow) silentGain = std::max (silentTargetNow, silentGain - silentStep);
        const float g = master * silentGain;
        const float breath = dust * 0.002f;   // background noise, up to -54 dBFS
        float l = (outL[i] + breath * hiss[0].process (-0.5f)) * g;
        float r = (outR[i] + breath * hiss[1].process (-0.5f)) * g;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        outL[i] = softLimit (l);
        outR[i] = softLimit (r);
    }
}

// GRAVITY: every voice is frequency-modulated by its partner (pairs 1-2, 3-4) and/or the next voice (ring 1-2-3-4-1),
// one sample late, plus the instrument's own output in TURBULENCE and COLLAPSE. DUST leaks all voices into each other.
void ResoOpusProcessor::renderVoices (float* L, float* R, float* leadL, float* leadR, int count)
{
    std::fill (L, L + count, 0.0f);
    std::fill (R, R + count, 0.0f);
    std::fill (leadL, leadL + count, 0.0f);
    std::fill (leadR, leadR + count, 0.0f);
    bool on[Params::numVoices];
    bool any = false;
    for (int v = 0; v < Params::numVoices; ++v) { on[v] = voices[v].begin (settings[v], count); any = any || on[v]; }
    bool leadOnNow[maxLead];
    bool anyLead = false;
    for (int i = 0; i < maxLead; ++i)
    {
        leads[i].begin (leadSettings, count);
        leadOnNow[i] = leads[i].isActive();
        anyLead = anyLead || leadOnNow[i];
    }
    if (! any && ! anyLead)
    {
        for (auto& m : voiceMod) m = 0.0f;
        lastMono = 0.0f;
        for (int k = 0; k < count; ++k) { devRamp.next(); ringRamp.next(); fbRamp.next(); dustRamp.next(); }
        return;
    }
    for (int k = 0; k < count; ++k)
    {
        const float dev = devRamp.next(), ring = ringRamp.next(), dustDev = dustRamp.next();
        const float echoIn = echoPrevLen > 0 ? toVoices * 2.0f * echoPrev[std::min (echoPrevLen - 1, k >> osIndex)] : 0.0f;
        const float fb = fbRamp.next() * lastMono + echoIn;
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
        if (anyLead)
            for (int i = 0; i < maxLead; ++i)
                if (leadOnNow[i]) leads[i].tick (leadL[k], leadR[k]);
        lastMono = jlimit (-1.5f, 1.5f, (L[k] + R[k] + leadL[k] + leadR[k]) * 1.2f);
    }
}

//==============================================================================
// LEAD: monophonic, last-note priority; letting go of a key returns to the previous held one (legato, with glide)
void ResoOpusProcessor::leadNoteOn (int note, float vel)
{
    for (int i = 0; i < leadNum; ++i)
        if (leadHeld[i] == note) { for (int j = i; j < leadNum - 1; ++j) { leadHeld[j] = leadHeld[j + 1]; leadVel[j] = leadVel[j + 1]; } --leadNum; break; }
    if (leadNum >= 16) { for (int j = 0; j < 15; ++j) { leadHeld[j] = leadHeld[j + 1]; leadVel[j] = leadVel[j + 1]; } leadNum = 15; }
    const bool wasHeld = leadNum > 0;
    leadHeld[leadNum] = note;
    leadVel[leadNum] = vel;
    ++leadNum;
    if (globalRaw[GP::arp_on]->load() > 0.5f)
    {
        // a new chord after letting go (latched) replaces the pattern
        if (! wasHeld) arpNum = 0;
        int n = 0;
        for (int i = 0; i < leadNum; ++i) arpNotes[n++] = leadHeld[i];
        std::sort (arpNotes, arpNotes + n);
        arpNum = n;
        if (! wasHeld) { arpPos = 0.0; arpStep = 0; arpDir = 1; arpGate = false; }
        return;
    }
    leadPlay (note, vel, wasHeld);
}

void ResoOpusProcessor::leadNoteOff (int note)
{
    int found = -1;
    for (int i = 0; i < leadNum; ++i) if (leadHeld[i] == note) found = i;
    if (found < 0) return;
    if (sustain) { if (leadSusNum < 16) leadSus[leadSusNum++] = note; return; }
    for (int j = found; j < leadNum - 1; ++j) { leadHeld[j] = leadHeld[j + 1]; leadVel[j] = leadVel[j + 1]; }
    --leadNum;
    if (globalRaw[GP::arp_on]->load() > 0.5f)
    {
        if (globalRaw[GP::arp_latch]->load() > 0.5f) return;   // the pattern keeps running
        int n = 0;
        for (int i = 0; i < leadNum; ++i) arpNotes[n++] = leadHeld[i];
        std::sort (arpNotes, arpNotes + n);
        arpNum = n;
        return;
    }
    if (leadMode() >= 2) { leadStop (note); return; }
    if (leadNum == 0) leads[0].noteOff();
    else leads[0].noteOn ((double) leadHeld[leadNum - 1], leadVel[leadNum - 1], true);   // back to the previous key
}

bool ResoOpusProcessor::anyLeadActive() const
{
    for (auto& l : leads) if (l.isActive()) return true;
    return false;
}

void ResoOpusProcessor::leadPlay (int note, float vel, bool legato)
{
    const int mode = leadMode();
    if (mode < 2)
    {
        // Mono restarts the envelopes on every note (glides if GLIDE is set), Legato only slides overlapping notes
        leads[0].noteOn ((double) note, vel, mode == 1 && legato);
        leadLast = 0;
        lastLeadPitch = (double) note;
        return;
    }
    const int maxV = mode >= 3 ? maxLead : 4;
    int pick = -1;
    for (int i = 0; i < maxV && pick < 0; ++i) if (leads[i].isActive() && leads[i].getNote() == note) pick = i;
    for (int i = 0; i < maxV && pick < 0; ++i) if (! leads[i].isActive()) pick = i;
    if (pick < 0)
    {
        float quietest = 2.0f;
        for (int i = 0; i < maxV; ++i)
            if (leads[i].isReleasing() && leads[i].getLevel() < quietest) { quietest = leads[i].getLevel(); pick = i; }
    }
    if (pick < 0)
    {
        uint32_t oldest = 0xffffffffu;
        for (int i = 0; i < maxV; ++i) if (leadAge[i] < oldest) { oldest = leadAge[i]; pick = i; }
    }
    const double from = curG[GP::lead_glide] > 0.0005f && lastLeadPitch >= 0.0 ? lastLeadPitch : -1.0;
    leads[pick].noteOn ((double) note, vel, false, from);
    leadAge[pick] = ++leadClock;
    leadLast = pick;
    lastLeadPitch = (double) note;
}

void ResoOpusProcessor::leadStop (int note)
{
    if (leadMode() < 2) { leads[0].noteOff(); return; }
    for (auto& l : leads) if (l.isActive() && ! l.isReleasing() && l.getNote() == note) l.noteOff();
}

void ResoOpusProcessor::leadStopAll()
{
    for (auto& l : leads) l.noteOff();
    arpGate = false;
    arpNote = -1;
}

// ARP: steps through the pattern at the song tempo; each step retriggers the envelope and lets go at half the step
void ResoOpusProcessor::runArp (int len)
{
    const bool on = curG[GP::arp_on] > 0.5f && curG[GP::lead_on] > 0.5f;
    if (! on)
    {
        if (arpWasOn) { leadStopAll(); }
        arpWasOn = false;
        return;
    }
    arpWasOn = true;
    if (arpNum == 0) { if (arpGate) { leadStop (arpNote); arpGate = false; } return; }
    static const double beats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    const double stepLen = sr * 60.0 / hostBpm * beats[jlimit (0, 5, (int) curG[GP::arp_rate])];
    const int octaves = 1 + (int) curG[GP::arp_octaves];
    const int total = arpNum * octaves;
    if (arpPos <= 0.0)   // start of a step
    {
        const int mode = (int) curG[GP::arp_mode];
        int idx = 0;
        if (mode == 0) idx = arpStep % total;
        else if (mode == 1) idx = total - 1 - arpStep % total;
        else if (mode == 2) { const int span = std::max (1, 2 * total - 2); const int pp = arpStep % span; idx = pp < total ? pp : span - pp; }
        else idx = (int) (arpRng.next01() * (float) total) % total;
        const int note = arpNotes[idx % arpNum] + 12 * (idx / arpNum);
        if (arpGate) leadStop (arpNote);
        leadPlay (note, leadNum > 0 ? leadVel[leadNum - 1] : 0.8f, false);
        arpNote = note;
        arpGate = true;
        ++arpStep;
    }
    arpPos += len;
    if (arpGate && arpPos >= 0.5 * stepLen) { leadStop (arpNote); arpGate = false; }
    if (arpPos >= stepLen) arpPos = 0.0;
}

void ResoOpusProcessor::applyFilter (float* L, float* R, const float* leadL, const float* leadR, int count)
{
    const int mode = (int) curG[GP::flt_mode], modeB = (int) curG[GP::flt2_mode];
    const int model = (int) curG[GP::flt_model], modelB = (int) curG[GP::flt2_model];
    if (model != filterModel) { filterModel = model; for (auto& f : filters) f.reset(); }   // each model keeps its own state
    if (modelB != filterModelB) { filterModelB = modelB; for (auto& f : filtersB) f.reset(); }
    const bool useB = curG[GP::flt2_on] > 0.5f;
    const int route = useB ? (int) curG[GP::flt_route] : -1;
    for (int k = 0; k < count; ++k)
    {
        const float gl = cutoffRamp[0].next(), gr = cutoffRamp[1].next(), res = resRamp.next(), drive = driveRamp.next();
        const float bl = cutoffRampB[0].next(), br = cutoffRampB[1].next(), resB = resRampB.next(), driveB = driveRampB.next();
        float xl = L[k] + leadL[k], xr = R[k] + leadR[k];
        switch (route)
        {
            case GP::RouteSerial:
                xl = filtersB[0].process (filters[0].process (xl, gl, res, drive, mode, model), bl, resB, driveB, modeB, modelB);
                xr = filtersB[1].process (filters[1].process (xr, gr, res, drive, mode, model), br, resB, driveB, modeB, modelB);
                break;
            case GP::RouteParallel:
                xl = 0.7f * (filters[0].process (xl, gl, res, drive, mode, model) + filtersB[0].process (xl, bl, resB, driveB, modeB, modelB));
                xr = 0.7f * (filters[1].process (xr, gr, res, drive, mode, model) + filtersB[1].process (xr, br, resB, driveB, modeB, modelB));
                break;
            case GP::RouteSplit:
                xl = filters[0].process (L[k], gl, res, drive, mode, model) + filtersB[0].process (leadL[k], bl, resB, driveB, modeB, modelB);
                xr = filters[1].process (R[k], gr, res, drive, mode, model) + filtersB[1].process (leadR[k], br, resB, driveB, modeB, modelB);
                break;
            case GP::RouteStereo:
                xl = filters[0].process (xl, gl, res, drive, mode, model);
                xr = filtersB[1].process (xr, br, resB, driveB, modeB, modelB);
                break;
            default:   // FILTER B off
                xl = filters[0].process (xl, gl, res, drive, mode, model);
                xr = filters[1].process (xr, gr, res, drive, mode, model);
                break;
        }
        L[k] = xl;
        R[k] = xr;
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

void ResoOpusProcessor::presetApplied (const String& name, const String& key, int factoryIndex)
{
    if (factoryIndex >= 0) currentPreset = factoryIndex;
    uiState.setProperty ("presetName", name, nullptr);
    uiState.setProperty ("presetKey", key, nullptr);
    uiState.setProperty ("presetIndex", currentPreset, nullptr);
    if (history) history->resync();
}

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
    // factory presets come first in the manager's list
    if (index >= 0 && index < (int) factoryPresets().size()) presets->load (index, false);
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
    presets->setCurrent (getPresetKey(), getPresetName());
    if (history)
    {
        history->fromValueTree (root.getChildWithName ("AB"));
        history->resync();
    }
}

AudioProcessorEditor* ResoOpusProcessor::createEditor() { return new ResoOpusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResoOpusProcessor(); }

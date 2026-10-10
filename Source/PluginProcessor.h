#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Params/Parameters.h"
#include "DSP/Voices.h"
#include "DSP/ModMatrix.h"
#include "State/StateHistory.h"

class ResoOpusProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ResoOpusProcessor();
    ~ResoOpusProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ResoOpus"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Presets
    void loadFactoryPreset (int index);
    // Fades the output out, applies the change, clears the voices and fades back in (no clicks)
    void changeSilently (std::function<void()> change);
    void resetToDefaults();
    juce::String getPresetName() const { return uiState.getProperty ("presetName", "Init").toString(); }
    void setPresetName (const juce::String& n) { uiState.setProperty ("presetName", n, nullptr); }

    // Parameter helpers (message thread)
    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }
    void setParamReal (const juce::String& id, float value);
    float getParamReal (const juce::String& id) const;

    // Voice helpers (message thread)
    void applyChord (int voice, int chord);              // CHORD: fills the five intervals of a CHOIR voice
    void fillVoices (int scale, int root);               // FILL: spreads the four voices over a scale
    int plateNote (int voice) const;                     // MIDI note of the plate that plays this voice

    // Modulation helpers (message thread)
    int addModulation (int source, const juce::String& destParamID, float amount);   // returns slot or -1
    void removeModulation (int slot);
    juce::Array<int> slotsForDestination (const juce::String& destParamID) const;
    int findFreeSlot() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };
    std::unique_ptr<StateHistory> history;
    juce::MidiKeyboardState keyboardState;

    // UI -> audio
    std::atomic<float> uiPitchBend { 0.0f }, uiModWheel { 0.0f };
    std::atomic<bool> editorOpen { false };

    // audio -> UI (live values)
    std::atomic<float> liveDest[Mod::maxDestCode + 1];   // modulated normalised value per destination code (-1 = not modulated)
    std::atomic<float> liveSource[Mod::numSources];
    std::atomic<float> liveLevel[Params::numVoices];     // envelope level of every voice 0..1
    std::atomic<bool> liveGate[Params::numVoices];       // voice touched (plate or key held)
    std::atomic<float> liveRoot[Params::numVoices];      // sounding root (MIDI note), for the keyboard in KEYS mode
    std::atomic<float> meterPeak { -100.0f };
    std::atomic<float> cpuLoad { 0.0f };

    static constexpr int chunkSize = 32;

private:
    void handleAsyncUpdate() override;
    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void releaseAll();
    void processChunk (juce::AudioBuffer<float>& out, int start, int len);
    void setOversampling (int index);
    int computeLatency() const;
    double snapNote (double midi) const;

    std::atomic<float>* voiceRaw[Params::numVoices][VP::count] {};
    std::atomic<float>* globalRaw[GP::count] {};
    std::atomic<float>* modRaw[Params::numModSlots][MP::count] {};
    juce::RangedAudioParameter* voiceParam[Params::numVoices][VP::count] {};
    juce::RangedAudioParameter* globalParam[GP::count] {};

    dsp::Voice voices[Params::numVoices];
    dsp::VoiceSettings settings[Params::numVoices];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];   // 2x, 4x
    int osIndex = 1;
    juce::AudioBuffer<float> zeros, mixBuf;
    dsp::DcBlocker dc[2];

    ModMatrix matrix;
    ModMatrix::Slot slots[Params::numModSlots];
    float sources[Mod::numSources] {};
    float modOut[Params::numModSlots] {};
    float modSum[Mod::maxDestCode + 1] {};
    bool modTouched[Mod::maxDestCode + 1] {};
    float curV[Params::numVoices][VP::count] {}, curG[GP::count] {};
    int lastType[Params::numVoices] { -1, -1, -1, -1 };

    // touch state per voice
    struct Touch
    {
        bool gate = false;
        int note = -1;              // MIDI note holding the voice
        float velocity = 0.0f, pressure = 0.0f;
        double keyRoot = -1.0;      // KEYS mode: note the voice was retuned to (-1 = use TUNE)
    };
    Touch touch[Params::numVoices];
    int keysNext = 0;               // KEYS mode round robin
    bool sustain = false;
    bool sustained[Params::numVoices] {};
    float modWheel = 0.0f, pressure = 0.0f, pitchBend = 0.0f, lastVelocity = 0.0f;
    float lastUiBend = 0.0f, lastUiWheel = 0.0f;

    double sr = 48000.0;
    int currentPreset = 0;
    std::atomic<int> pendingLatency { 0 };
    std::atomic<float> silentTarget { 1.0f };
    std::atomic<bool> resetRequest { false };
    float silentGain = 1.0f;
    struct SilentChange : juce::Timer
    {
        std::function<void()> action;
        void timerCallback() override { stopTimer(); if (auto a = std::move (action)) a(); }
    } silentChange;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResoOpusProcessor)
};

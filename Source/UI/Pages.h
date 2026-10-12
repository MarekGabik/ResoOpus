#pragma once

#include "Widgets.h"
#include "Params/Parameters.h"
#include <juce_dsp/juce_dsp.h>

class ResoOpusProcessor;

// One panel page. Owns its controls; refreshLive() is called by the editor timer.
class Page : public juce::Component
{
public:
    explicit Page (ResoOpusProcessor& p) : proc (p) {}
    virtual void refreshLive();
    void setModEditCallback (std::function<void (int)> cb);
    Knob* findKnob (const juce::String& paramID) const;

    static constexpr int width = 1164, height = 540;

protected:
    Knob* knob (const juce::String& id, const juce::String& label, juce::Colour, Knob::Style = {});
    ModulePanel* panel (const juce::String& title, juce::Colour, juce::Rectangle<int> bounds, int source = -1, const juce::String& right = {});

    ResoOpusProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::Array<Knob*> knobs;
};

// Label + value box with a small arrow; click opens a menu (TYPE, CHORD)
class SelectorPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    SelectorPill (const juce::String& label, std::function<juce::String()> text, std::function<void (SelectorPill&)> onClick);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String label, shown;
    std::function<juce::String()> getText;
    std::function<void (SelectorPill&)> click;
};

// Small LED that switches a bool parameter (oscillator on / off next to its knob label)
class LedDot : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedDot (ResoOpusProcessor&, const juce::String& paramID);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool on = false;
};

// GRAVITY: the big centre knob. Zones STILL / ORBIT / TURBULENCE / COLLAPSE around it, value arc in the zone colour,
// modulation ring (live value), the four voices as dots on an orbit that pull together as GRAVITY rises.
// Drag vertically (Shift = fine), wheel, double-click = type, Cmd/Ctrl-click = default, right-click = PATH and modulation,
// drop a modulation source on it.
class GravityView : public juce::Component, public juce::SettableTooltipClient, public juce::DragAndDropTarget
{
public:
    explicit GravityView (ResoOpusProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    float getValue01() const { return value; }

private:
    juce::Point<float> centre() const { return { getWidth() * 0.5f, getHeight() * 0.5f - 12.0f }; }
    void showMenu();
    ResoOpusProcessor& proc;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::unique_ptr<juce::TextEditor> editor;
    float value = 0.0f, live = 0.0f, dragStart = 0.0f;
    float levels[Params::numVoices] {};
    int types[Params::numVoices] {};
    double phase = 0.0, lastTime = 0.0, jitter = 0.0;
    bool dropHover = false;
    juce::Path pairs[2];          // voice against its partner (X / Y): 1-2 top left, 3-4 top right
    juce::Rectangle<float> pairBox (int pair) const;
    void buildPairs();
};

// FX: FILTER, ECHO and SPACE. The labels of SPACE X / Y / Z follow the chosen algorithm.
class FxPage : public Page
{
public:
    explicit FxPage (ResoOpusProcessor&);
    void refreshLive() override;

private:
    Knob* xyz[3] {};
    ModulePanel* spacePanel = nullptr;
    ModulePanel* filterPanel = nullptr;
    ModulePanel* filterBPanel = nullptr;
    juce::Label* spaceInfo = nullptr;
    juce::Array<juce::Component*> filterBControls;
    Knob* rates[3] {};      // LFO 1, LFO 2, RANDOM: RATE (free) and DIVISION (tempo), one of them is dimmed
    Knob* divisions[3] {};
    int shownAlgo = -1, shownModel = -1, shownModelB = -1;
};

// LEAD: oscillators, filter per note with its envelope, amp envelope, voices / glide / vibrato, ARP
class LeadPage : public Page
{
public:
    explicit LeadPage (ResoOpusProcessor&);
    void refreshLive() override;

private:
    juce::Array<ModulePanel*> leadPanels;
    ModulePanel* filterPanel = nullptr;
    int shownModel = -1;
};

class DronePage : public Page
{
public:
    explicit DronePage (ResoOpusProcessor&);
    void refreshLive() override;
    void selectVoice (int v);
    int getSelectedVoice() const { return selected; }
    GravityView* getGravityView() { return gravity; }

private:
    // Volume of a voice as a thin bar (drag to change) with the pan position and a level meter below
    struct VolBar : public juce::Component, public juce::SettableTooltipClient
    {
        VolBar (ResoOpusProcessor&, int voice);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        bool refresh (float level, float hold);
        ResoOpusProcessor& proc;
        juce::RangedAudioParameter* vol = nullptr;
        juce::RangedAudioParameter* pan = nullptr;
        float shownVol = -1.0f, shownPan = -2.0f, shownLevel = -1.0f, shownHold = -1.0f;
        juce::Colour colour;
    };

    // Live signal of a voice: two periods of its root (a steady picture), a faint afterglow, height = level.
    // Click switches to the spectrum (harmonics, chord, growl).
    struct VoiceScope : public juce::Component, public juce::SettableTooltipClient
    {
        VoiceScope (ResoOpusProcessor& p, int v);
        void update (float level);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        ResoOpusProcessor& proc;
        int voice;
        bool spectrumMode = false, silent = false;
        juce::Path frames[4];      // newest first
        juce::Colour colour;
        juce::dsp::FFT fft { 11 };
        std::vector<float> fftData = std::vector<float> (4096);
    };

    // One voice strip: light plate header ("CHOIR  1"), TUNE + character knob, HOLD + PAN, volume bar. Click to select.
    struct Column : public ModulePanel
    {
        Column (DronePage& o, int v) : ModulePanel ("CHOIR", Palette::choir, -1, juce::String (v + 1)), owner (o), voice (v) {}
        void mouseDown (const juce::MouseEvent&) override { owner.selectVoice (voice); }
        void paintOverChildren (juce::Graphics&) override;
        DronePage& owner;
        int voice;
        Knob* character[6] {};   // per type: -, TIDE, FM, SHARP, INDEX, POSITION
        Knob* tune = nullptr;
        juce::Array<juce::Component*> controls;
        VolBar* vol = nullptr;
        VoiceScope* scope = nullptr;
        int type = -1;
        bool selected = false;
    };

    struct Detail
    {
        juce::Array<juce::Component*> all, byType[6];   // per type; all = every control of this voice
        LedRadio* chord = nullptr;
    };

    int voiceType (int v) const;
    void showFillMenu (juce::Component* target);

    struct MasterPanel : public ModulePanel
    {
        MasterPanel (DronePage& o) : ModulePanel ("MASTER", Palette::master, -1, "FILL"), owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override { if (e.y < headerHeight && e.x > getWidth() - 80) owner.showFillMenu (this); }
        DronePage& owner;
    };

    Column* columns[Params::numVoices] {};
    Detail details[Params::numVoices];
    ModulePanel* detailPanel = nullptr;
    GravityView* gravity = nullptr;
    juce::Label* offNote = nullptr;
    int selected = 0;
    int shownType[Params::numVoices] { -1, -1, -1, -1 };
};

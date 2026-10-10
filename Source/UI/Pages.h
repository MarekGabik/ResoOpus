#pragma once

#include "Widgets.h"
#include "Params/Parameters.h"

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

// GRAVITY panel. Phase 1: the eight voices as dots on an orbit (brightness = level); the knob arrives in phase 3.
class GravityView : public juce::Component
{
public:
    explicit GravityView (ResoOpusProcessor& p) : proc (p) {}
    void refresh();
    void paint (juce::Graphics&) override;

private:
    ResoOpusProcessor& proc;
    float levels[Params::numVoices] {};
    int types[Params::numVoices] {};
    double phase = 0.0, lastTime = 0.0;
};

class DronePage : public Page
{
public:
    explicit DronePage (ResoOpusProcessor&);
    void refreshLive() override;
    void selectVoice (int v);
    int getSelectedVoice() const { return selected; }

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
        bool refresh (float level);
        ResoOpusProcessor& proc;
        juce::RangedAudioParameter* vol = nullptr;
        juce::RangedAudioParameter* pan = nullptr;
        float shownVol = -1.0f, shownPan = -2.0f, shownLevel = -1.0f;
        juce::Colour colour;
    };

    // Level of a voice over the last ~8 s (what ATTACK, RELEASE and HOLD do), HOLD as a dashed line
    struct LevelTrace : public juce::Component, public juce::SettableTooltipClient
    {
        LevelTrace() { setTooltip ("Level of this voice over the last 8 seconds: the rise is ATTACK, the fall is RELEASE, the dashed line is HOLD"); }
        void push (float level, float hold);
        void paint (juce::Graphics&) override;
        static constexpr int size = 240;
        float history[size] {};
        int head = 0;
        float hold = 0.0f;
        bool flat = false;
        juce::Colour colour;
    };

    // One voice strip: light plate header ("CHOIR  1"), TUNE + character knob, HOLD + PAN, volume bar. Click to select.
    struct Column : public ModulePanel
    {
        Column (DronePage& o, int v) : ModulePanel ("CHOIR", Palette::choir, -1, juce::String (v + 1)), owner (o), voice (v) {}
        void mouseDown (const juce::MouseEvent&) override { owner.selectVoice (voice); }
        void paintOverChildren (juce::Graphics&) override;
        DronePage& owner;
        int voice;
        Knob* character[4] {};   // per type: -, TIDE, FM, SHARP
        juce::Array<juce::Component*> controls;
        VolBar* vol = nullptr;
        LevelTrace* trace = nullptr;
        int type = -1;
        bool selected = false;
    };

    struct Detail
    {
        juce::Array<juce::Component*> all, byType[4];   // byType[0] = common to every type that sounds
        LedRadio* chord = nullptr;
    };

    int voiceType (int v) const;
    void showFillMenu (juce::Component* target);

    Column* columns[Params::numVoices] {};
    Detail details[Params::numVoices];
    ModulePanel* detailPanel = nullptr;
    GravityView* gravity = nullptr;
    juce::Label* offNote = nullptr;
    int selected = 0;
    int shownType[Params::numVoices] { -1, -1, -1, -1 };
};

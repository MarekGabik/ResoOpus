#pragma once

#include "Widgets.h"

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
    float levels[8] {};
    int types[8] {};
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

    // One voice strip: light plate header ("CHOIR  1"), TUNE, character knob, volume bar. Click to select.
    struct Column : public ModulePanel
    {
        Column (DronePage& o, int v) : ModulePanel ("CHOIR", Palette::choir, -1, juce::String (v + 1)), owner (o), voice (v) {}
        void mouseDown (const juce::MouseEvent&) override { owner.selectVoice (voice); }
        void paintOverChildren (juce::Graphics&) override;
        DronePage& owner;
        int voice;
        Knob* tune = nullptr;
        Knob* tide = nullptr;
        VolBar* vol = nullptr;
        int type = -1;
        float level = 0.0f;
        bool selected = false;
    };

    struct Detail
    {
        juce::Array<juce::Component*> choirOnly, all;
        juce::Component* type = nullptr;
        LedRadio* chord = nullptr;
    };

    struct GroupPanel : public ModulePanel
    {
        GroupPanel (DronePage& o, int g, const juce::String& t) : ModulePanel (t, Palette::group, -1, "FILL"), owner (o), group (g) {}
        void mouseDown (const juce::MouseEvent&) override;
        DronePage& owner;
        int group;
    };

    void showFillMenu (int group, juce::Component* target);
    int voiceType (int v) const;

    Column* columns[8] {};
    Detail details[8];
    ModulePanel* detailPanel = nullptr;
    GravityView* gravity = nullptr;
    juce::Label* phaseNote = nullptr;
    int selected = 0;
    int shownType[8] { -1, -1, -1, -1, -1, -1, -1, -1 };
};

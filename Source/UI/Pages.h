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

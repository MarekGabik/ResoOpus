#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

class ResoOpusProcessor;

// Moog-flavoured rotary knob in the Gavr style: scale ticks, coloured value arc, knurled body with a metal cap.
// The outer ring shows modulation (colour of the source); drag the ring to change the depth.
// Vertical drag (Shift = fine), wheel, double-click = type a value, Cmd/Ctrl-click = default,
// right-click = menu, drop a modulation source (drag handle in a CNTRL header) to modulate.
class Knob : public juce::Component,
             public juce::SettableTooltipClient,
             public juce::DragAndDropTarget
{
public:
    struct Style
    {
        bool bipolar = false;      // value arc grows from the centre
        bool small = false;
        bool drive = false;        // red zone from 7 to 10 (mixer)
        juce::String lo, hi;       // scale end labels
        juce::StringArray marks;   // labels around the knob ("~saw" etc. draw waveform icons)
        int width = 0;             // 0 = default
    };

    Knob (ResoOpusProcessor&, const juce::String& paramID, const juce::String& label, juce::Colour, Style);
    ~Knob() override;

    static int preferredWidth (const Style& s)  { return s.width > 0 ? s.width : (s.small ? 80 : 96); }
    static int preferredHeight (const Style& s) { return (s.small ? 84 : 98) + (s.marks.isEmpty() ? 0 : 12); }

    // Called by the editor timer: picks up modulation routings and live values; repaints only on change
    void refreshLive();

    std::function<void (int slot)> onEditModulation;
    const juce::String& getParamID() const { return paramID; }
    void setLabel (const juce::String& l) { if (l != label) { label = l; repaint(); } }

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    static void drawWaveIcon (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, juce::Colour);

private:
    struct ModInfo
    {
        int slot = -1, src = 0;
        float amt = 0.0f;
        bool on = true, bipolar = true;
        bool operator== (const ModInfo& o) const { return slot == o.slot && src == o.src && std::abs (amt - o.amt) < 1.0e-6f && on == o.on && bipolar == o.bipolar; }
    };

    juce::Point<float> centre() const;
    float radius() const { return style.small ? 16.0f : 21.0f; }
    bool hitsRing (juce::Point<float>) const;
    void showMenu();
    void showEditor();
    void setNormalised (float v);
    juce::String modText (const ModInfo&) const;

    ResoOpusProcessor& proc;
    juce::String paramID, label;
    juce::Colour colour;
    Style style;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int destCode = 0;
    bool isChoice = false;

    float value = 0.0f;   // normalised
    juce::Array<ModInfo> mods;
    float live = -1.0f;

    bool hover = false, ringHover = false, dragging = false, ringDragging = false, dropHover = false;
    float dragStartValue = 0.0f, dragStartY = 0.0f;
    int ringSlot = -1;
    juce::RangedAudioParameter* ringParam = nullptr;
    std::unique_ptr<juce::TextEditor> editor;
};

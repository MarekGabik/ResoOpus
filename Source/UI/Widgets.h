#pragma once

#include "Knob.h"
#include <map>

class ResoOpusProcessor;

// Square "glass" push button with a gold light (Moog-style toggle), attached to a bool parameter
class LedButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedButton (ResoOpusProcessor&, const juce::String& paramID, const juce::String& label);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String label;
    bool on = false;
};

// Button + LED list for a choice parameter (click the button to step, click a name to select)
class LedRadio : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedRadio (ResoOpusProcessor&, const juce::String& paramID, const juce::String& label, juce::StringArray names = {});
    // Action version (not bound to a parameter): get = lit entry (-1 = none), set = apply entry
    LedRadio (const juce::String& label, juce::StringArray names, std::function<int()> get, std::function<void (int)> set);
    void refresh();   // action version: re-reads the lit entry
    // a waveform pictogram (Icons::wave names) in front of every entry, each in its own colour
    void setIcons (juce::StringArray iconNames, juce::Array<juce::Colour> colours);
    void setColumns (int columns);   // long lists: entries fill column by column
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String label;
    juce::StringArray names, icons;
    juce::Array<juce::Colour> iconColours;
    int index = 0;
    int numChoices = 1;
    int columns = 1;
    int rowsPerColumn() const { return (names.size() + columns - 1) / columns; }
    float columnWidth() const;
    std::function<int()> getIndex;
    std::function<void (int)> setIndex;
    void choose (int i);
    void updateSize();
};

// Small handle in a module header: drag it onto a knob to create a modulation
class SourceHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SourceHandle (int sourceIndex);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    // set by the editor: how many routings use a source, and what a click (not a drag) opens
    static inline std::function<int (int)> countFor;
    static inline std::function<void (int)> onOpen;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int source;
    bool dragging = false;
};

// Panel with a light "label plate" header (Moog nod) and a thin colour line (Gavr)
class ModulePanel : public juce::Component
{
public:
    ModulePanel (const juce::String& title, juce::Colour colour, int modSource = -1, const juce::String& rightText = {});

    // Lays out children in a grid: column weights, items row-major (nullptr = empty cell)
    void setGrid (std::vector<float> columnWeights, std::vector<juce::Component*> items);
    void setSpan (juce::Component* item, int columns);   // a wide item centred over several grid columns
    void setRightText (const juce::String& t) { rightText = t; repaint(); }
    // makes the right header text a select button (pill with an arrow); called with the panel as menu target
    std::function<void (juce::Component&)> onRightClick;
    void mouseDown (const juce::MouseEvent&) override;
    juce::Rectangle<float> rightButtonArea() const;
    const juce::String& getTitle() const { return title; }
    void setTitleAndColour (const juce::String& t, juce::Colour c) { title = t; colour = c; repaint(); }
    void setEnvelopeIcon (int stages) { envIcon = stages; repaint(); }   // 4 = ADSR, 6 = DAHDSR
    void setLiveValue (float v);   // header dot follows the source (LFO / random)

    static constexpr int headerHeight = 25;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title, rightText;
    juce::Colour colour;
    int source;
    int envIcon = 0;
    float live = 1.0f;
    std::unique_ptr<SourceHandle> handle;
    std::vector<float> weights;
    std::vector<juce::Component*> items;
    std::map<juce::Component*, int> spans;
};

// Vertical group (e.g. SYNC above KB RESET)
class Stack : public juce::Component
{
public:
    Stack (std::vector<juce::Component*> children, int gap = 10, bool leftAligned = false);
    void resized() override;

private:
    std::vector<juce::Component*> kids;
    int gap;
    bool left;
};

// Mixer drive amount (CP-3 saturation) as a small bar
class DriveMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevel (float v);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
};

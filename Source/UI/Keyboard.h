#pragma once

#include "Theme.h"

class ResoOpusProcessor;

namespace KeyboardNames
{
    juce::String note (int midi);             // Ableton convention: 60 = C3
    juce::String range (int from, int to);    // "C3-C4"
}

// Bottom performance strip: pitch bend and mod wheel strips, a 25-key keyboard (velocity from where you strike:
// lower = louder, glissando by dragging) with the eight PLATES marked by voice number and type colour,
// octave shift, play mode and touch source.
class KeyboardPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KeyboardPanel (ResoOpusProcessor&);
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr int numKeys = 25;

private:
    enum class Part { None, Bend, Wheel, Keys, Range, PlayMode, TouchSrc, OctDown, OctUp };
    Part partAt (juce::Point<float>) const;
    juce::Rectangle<float> bendArea() const, wheelArea() const, keysArea() const, rangeArea() const;
    juce::Rectangle<float> playArea() const, touchArea() const, octDownArea() const, octUpArea() const;
    juce::Rectangle<float> keyRect (int i) const;
    int keyAt (juce::Point<float>) const;
    int plateVoice (int midi) const;   // voice played by this key in PLATES mode, or -1
    void play (int key, juce::Point<float>);
    void stopNote();

    ResoOpusProcessor& proc;
    int startNote = 48;
    int playing = -1;
    Part dragPart = Part::None;
    float bend = 0.0f, wheel = 0.0f, rangeDragStart = 0.0f;
    int rangeStartNote = 48;
    bool lit[numKeys] {};
    int plates[8] {};
    int types[8] {};
    float levels[8] {};
    bool platesMode = true;
    int touchMode = 0;
};

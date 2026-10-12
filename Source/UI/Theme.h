#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Gavr family palette (shared with ResoQBand) plus the ResoOpus panel tokens
namespace Palette
{
    // Calm, neutral studio background; colour is reserved for information
    inline const juce::Colour bgTop       { 0xff111216 };
    inline const juce::Colour bgBottom    { 0xff0b0c0f };
    inline const juce::Colour bar         { 0xff08090b };
    inline const juce::Colour panel       { 0xf0141519 };
    inline const juce::Colour panelEdge   { 0xff2a2b32 };
    inline const juce::Colour hairline    { 0x14ffffff };
    inline const juce::Colour button      { 0xff1c1d22 };
    inline const juce::Colour buttonHover { 0xff2a2b32 };
    inline const juce::Colour text        { 0xffe6e6ea };
    inline const juce::Colour textDim     { 0xff9a9ba4 };
    inline const juce::Colour textFaint   { 0xff5e5f68 };
    inline const juce::Colour curve       { 0xfff4c95d };   // family accent (gold)
    inline const juce::Colour red         { 0xffff5c6c };
    inline const juce::Colour blue        { 0xff4cb2ff };
    inline const juce::Colour green       { 0xff5ee08f };

    // ResoOpus panel
    inline const juce::Colour stageTop    { 0xff17181c };
    inline const juce::Colour stageBottom { 0xff0f1013 };
    inline const juce::Colour module      { 0xf516171b };
    inline const juce::Colour moduleEdge  { 0xff34353c };
    inline const juce::Colour plateTop    { 0xffc4c1b9 };
    inline const juce::Colour plateBottom { 0xffa8a59d };
    inline const juce::Colour plateText   { 0xff141416 };
    inline const juce::Colour label       { 0xffc9cad1 };
    inline const juce::Colour scale       { 0xff8b8c95 };
    inline const juce::Colour wood        { 0xff6b4426 };

    // sound sources on the SYNTH pages
    inline const juce::Colour osc         { 0xff38d6f0 };
    inline const juce::Colour sub         { 0xffb98bff };
    inline const juce::Colour noise       { 0xffff74b8 };
    inline const juce::Colour mixer       { 0xffff9a4d };
    inline const juce::Colour filter      { 0xfff4c95d };
    inline const juce::Colour voicing     { 0xffa5e844 };
    inline const juce::Colour fx1         { 0xffff9a4d };
    inline const juce::Colour fx2         { 0xff4aa8ff };

    // ResoOpus: voice types, GRAVITY zones, sections
    inline const juce::Colour choir       { 0xff38d6f0 };
    inline const juce::Colour siren       { 0xffff74b8 };
    inline const juce::Colour pulsar      { 0xffb98bff };
    inline const juce::Colour quasar      { 0xffffb86b };
    inline const juce::Colour nebula      { 0xff6ee7b7 };
    inline const juce::Colour lead        { 0xffa5e844 };
    inline const juce::Colour group       { 0xffc9cad1 };
    inline const juce::Colour gravity     { 0xff4aa8ff };
    inline const juce::Colour master      { 0xffe6e6ea };
    inline const juce::Colour zoneStill   { 0xff5ee0c8 };
    inline const juce::Colour zoneOrbit   { 0xff4aa8ff };
    inline const juce::Colour zoneTurb    { 0xffff9a4d };
    inline const juce::Colour zoneCollapse{ 0xffff5c6c };
    juce::Colour voiceType (int type);   // VP::Type -> colour

    juce::Colour forBand (int index);
    juce::Colour heat (float t);   // intensity colour map 0..1: dark violet -> magenta -> red -> orange
}

namespace Fonts
{
    juce::Font regular (float size);
    juce::Font bold (float size);
    juce::Font mono (float size);      // values: Hz, dB, ms
}

namespace Icons
{
    juce::Path power();
    juce::Path headphones();
    juce::Path close();
    juce::Path undo();
    juce::Path redo();
    juce::Path fullScreen();
    juce::Path resize();
    juce::Path scissors();
    juce::Path chevron (bool right);
    juce::Path chevronDown();
    juce::Path drag();
    juce::Path star();
    juce::Path pin();
    juce::Path menu();
    juce::Path dots();
    juce::Path user();
    // Small waveform pictograms in a unit square (stroke them): sine, tri, shark, saw / rampup, rampdown, square,
    // pulse, sh, noise, flat, chord (CHOIR), siren, morph (PULSAR), fm (QUASAR), table (NEBULA), lp, bp, hp
    juce::Path wave (const juce::String& name);

    // Fills a unit-square icon path into the given area
    void fill (juce::Graphics&, const juce::Path& unitPath, juce::Rectangle<float> area, juce::Colour);

    // Draws a unit-square icon path into the given area
    void draw (juce::Graphics&, const juce::Path& unitPath, juce::Rectangle<float> area, juce::Colour, float strokeWidth = 1.5f);
}

namespace Cursors
{
    // Big up/down arrow with a ring arc: "drag up or down to change the modulation depth"
    const juce::MouseCursor& ringDrag();
}

class ResoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ResoLookAndFeel();

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

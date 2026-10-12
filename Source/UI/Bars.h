#pragma once

#include "Theme.h"
#include "State/GlobalSettings.h"

class ResoOpusProcessor;

// Top bar: logo, page tabs, undo/redo, A/B + Copy, preset browser, Mod panel, Help
class TopBar : public juce::Component, public juce::TooltipClient, private juce::ChangeListener
{
public:
    TopBar (ResoOpusProcessor&, GlobalSettings&);
    ~TopBar() override;

    std::function<void (int)> onTab;
    std::function<void()> onMod;
    std::function<void (int)> onScale;       // percent
    std::function<int()> getScale;
    std::function<bool()> isModOpen;
    std::function<void()> onPresets;          // opens / closes the preset browser
    std::function<bool()> isBrowserOpen;

    void setPage (int p) { page = p; repaint(); }
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltipFor (juce::Point<float>) const;
    juce::String getTooltip() override { return getTooltipFor (getMouseXYRelative().toFloat()); }

    static constexpr int height = 36;
    static const juce::StringArray& tabNames();
    juce::Rectangle<float> area (int item) const;

    enum Item { None = -1, Tab0 = 0, Undo = 10, Redo, AB, Copy, Prev, Name, Next, Mod, Help, Tips, LeadPower, DronePower };

    juce::Rectangle<float> leadPowerArea() const { return area (LeadPower); }
    juce::Rectangle<float> dronePowerArea() const { return area (DronePower); }
private:
    bool leadOn = false, droneOn = true;
    int modCount = 0;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    int itemAt (juce::Point<float>) const;
    void showHelpMenu();

    ResoOpusProcessor& proc;
    GlobalSettings& settings;
    int page = 0, hover = None;
    juce::String shownName;
    bool shownModified = false;
};

// Bottom bar: play mode, touch, snap, current preset, oversampling, CPU, output level
class BottomBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BottomBar (ResoOpusProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr int height = 28;

private:
    enum Item { None = -1, Play, TouchSrc, SnapMode, OS };
    juce::Rectangle<float> area (int item) const;
    int itemAt (juce::Point<float>) const;

    ResoOpusProcessor& proc;
    float cpu = 0.0f, peak = -100.0f;
    juce::String preset;
    int shown[4] { -1, -1, -1, -1 };
};

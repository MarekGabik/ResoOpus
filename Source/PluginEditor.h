#pragma once

#include "PluginProcessor.h"
#include "State/GlobalSettings.h"
#include "UI/Theme.h"
#include "UI/Pages.h"
#include "UI/Keyboard.h"
#include "UI/Bars.h"
#include "UI/ModDrawer.h"
#include "UI/PresetBrowser.h"

// Panel background with a fine grain and a faint star field; hosts the current page
class Stage : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    static constexpr int height = 564;
    static constexpr int pageTop = 12, pageLeft = 18;

private:
    juce::Image texture, stars;
};

// The window: top bar, panel pages, keyboard, bottom bar, modulation drawer.
// Laid out at a fixed logical size (1200 x 776) and scaled as a whole.
class ResoOpusEditor : public juce::AudioProcessorEditor,
                     public juce::DragAndDropContainer,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit ResoOpusEditor (ResoOpusProcessor&);
    ~ResoOpusEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showPage (int index);
    int getPage() const { return page; }
    void setModDrawer (bool open, int selectSlot = -1);
    void setPresetBrowser (bool open);
    bool isPresetBrowserOpen() const { return browser.isVisible(); }
    bool isModDrawerOpen() const { return drawer.isVisible(); }
    void setScalePercent (int percent);
    Page* getPageComponent (int i) { return pages[i]; }
    DronePage* getDronePage() { return dynamic_cast<DronePage*> (pages[0]); }
    TopBar& getTopBar() { return top; }

    static constexpr int logicalWidth = 1200, logicalHeight = 776;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateTooltips();

    ResoOpusProcessor& proc;
    juce::SharedResourcePointer<GlobalSettings> settings;
    ResoLookAndFeel lnf;

    juce::Component content;
    TopBar top;
    Stage stage;
    juce::OwnedArray<Page> pages;
    KeyboardPanel keyboard;
    BottomBar bottom;
    ModDrawer drawer;
    PresetBrowser browser;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    juce::ComponentBoundsConstrainer constrainer;

    int page = 0, tick = 0;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResoOpusEditor)
};

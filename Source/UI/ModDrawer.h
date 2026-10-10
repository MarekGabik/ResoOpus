#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

class ResoOpusProcessor;

// Modulation summary + editor (left drawer): list of all routings, details of the selected one
// (source, destination, amount, bipolar, controller VCA, function).
class ModDrawer : public juce::Component
{
public:
    explicit ModDrawer (ResoOpusProcessor&);
    ~ModDrawer() override;

    void refresh();
    void select (int slot);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int width = 330;

private:
    struct Row { int slot, src, dst; float amt; bool on; };
    juce::Rectangle<int> listArea() const;
    juce::Rectangle<int> rowRect (int i) const;
    void rebuildDetail();
    void showAddMenu();

    ResoOpusProcessor& proc;
    std::vector<Row> rows;
    int selected = -1, scroll = 0;

    juce::TextButton addButton { "+ Add" }, clearButton { "Clear disabled" }, closeButton { "Close" };
    juce::ComboBox srcBox, dstBox, ctlBox, fnBox;
    juce::ToggleButton onToggle { "Enabled" }, biToggle { "Bipolar" };
    juce::Slider amtSlider, ctlSlider, fnSlider;
    std::unique_ptr<juce::ComboBoxParameterAttachment> srcAtt, ctlAtt, fnAtt;
    std::unique_ptr<juce::SliderParameterAttachment> amtAtt, ctlAmtAtt, fnAmtAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> onAtt, biAtt;
    juce::Array<int> destCodes;
};

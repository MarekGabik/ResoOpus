#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <map>
#include <vector>

class ResoOpusProcessor;

// Modulation matrix: one row per destination, one column per source, the cell holds the depth.
// Drag a cell up / down to set the depth (an empty cell creates the routing), double-click removes it,
// right-click: enabled, bipolar, controller (a second source that scales it), function. "+ ADD DESTINATION" adds a row.
class ModMatrixView : public juce::Component, public juce::SettableTooltipClient, private juce::AudioProcessorListener
{
public:
    explicit ModMatrixView (ResoOpusProcessor&);
    ~ModMatrixView() override;

    void refresh();                    // re-reads the slots (editor timer)
    void highlightSource (int source); // column highlight (-1 = none)
    void highlightSlot (int slot);     // row highlight and scroll to it

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int numCols = 10;   // all sources except None

private:
    struct Cell { int slot = -1; float amt = 0.0f; bool on = true, bi = true; int ctl = 0, fn = 0; };
    // the sources of this page first (LFO 1, LFO 2, RANDOM, MOD ENV), then LEAD ENV and the controllers
    static int sourceOf (int col)
    {
        static const int order[numCols] { 6, 7, 8, 10, 9, 1, 2, 3, 4, 5 };   // Mod::Source values
        return order[col < 0 ? 0 : (col >= numCols ? numCols - 1 : col)];
    }
    // the last knob the user touched (any parameter gesture): offered first in "+ ADD DESTINATION"
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    std::atomic<int> lastTouched { 0 };
    int headerH() const { return 40; }
    int rowH() const { return 26; }
    int nameW() const { return 230; }
    float colW() const;
    int visibleRows() const;
    juce::Rectangle<float> cellRect (int row, int col) const;
    juce::Rectangle<float> nameRect (int row) const;
    juce::Rectangle<float> addRect() const;
    bool hit (juce::Point<float>, int& row, int& col) const;   // col -1 = name column
    const Cell* cellAt (int row, int col) const;
    void showCellMenu (int row, int col);
    void showAddMenu();
    juce::String displayName (int code) const;
public:
    juce::PopupMenu buildAddMenu();    // "+ ADD DESTINATION": every destination once, grouped like the plug-in
private:
    void removeRow (int row);

    ResoOpusProcessor& proc;
    std::vector<int> rows;                       // destination codes
    std::vector<int> pending;                    // added rows without a routing yet
    std::map<std::pair<int, int>, Cell> cells;   // (destination, source)
    int scroll = 0, hoverRow = -1, hoverCol = -1, dragRow = -1, dragCol = -1, hiSource = -1, hiSlot = -1;
    float dragStart = 0.0f;
    juce::String signature;
};

#pragma once

#include "Theme.h"
#include <map>
#include <vector>

class ResoOpusProcessor;

// Modulation matrix: one row per destination, one column per source, the cell holds the depth.
// Drag a cell up / down to set the depth (an empty cell creates the routing), double-click removes it,
// right-click: enabled, bipolar, controller (a second source that scales it), function. "+ ADD DESTINATION" adds a row.
class ModMatrixView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModMatrixView (ResoOpusProcessor&);

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

    static constexpr int numCols = 10;   // sources Velocity .. Mod Env

private:
    struct Cell { int slot = -1; float amt = 0.0f; bool on = true, bi = true; int ctl = 0, fn = 0; };
    static int sourceOf (int col) { return col + 1; }
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
    void removeRow (int row);

    ResoOpusProcessor& proc;
    std::vector<int> rows;                       // destination codes
    std::vector<int> pending;                    // added rows without a routing yet
    std::map<std::pair<int, int>, Cell> cells;   // (destination, source)
    int scroll = 0, hoverRow = -1, hoverCol = -1, dragRow = -1, dragCol = -1, hiSource = -1, hiSlot = -1;
    float dragStart = 0.0f;
    juce::String signature;
};

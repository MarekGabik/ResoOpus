#pragma once

#include "Theme.h"
#include "State/PresetManager.h"

class ResoOpusProcessor;

// Preset browser overlay (opened from the preset name in the top bar): categories on the left, filters,
// search, Save / Save as, the list with favourite stars and options, a preview of the current preset on the right.
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser (ResoOpusProcessor&);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

    std::function<void()> onClose;

    // used by the rows
    void rowClicked (int row, const juce::MouseEvent&, juce::Rectangle<int> starArea, juce::Rectangle<int> optionsArea);
    void rowDoubleClicked (int row);
    const Preset* presetForRow (int row) const;
    bool isCurrent (int row) const;
    PresetManager& getManager() { return presets; }

    static constexpr int panelWidth = 900, panelHeight = 500;
    juce::Rectangle<int> panelBounds() const;

private:
    enum class Filter { all, factory, user, favourites };

    int getNumRows() override { return visible.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void rebuild();
    void setFilter (Filter f);
    void showFileMenu();
    void showOptionsMenu (int presetIndex, juce::Point<int> screenPos);
    void showSaveAsDialog();
    void showEditInfoDialog (int presetIndex);
    void confirmDelete (int presetIndex);
    int previewIndex() const;
    void close() { if (onClose) onClose(); }

    PresetManager& presets;
    juce::SharedResourcePointer<GlobalSettings> settings;

    juce::Array<int> visible;      // indices into presets.getPresets()
    juce::String category;         // empty = all
    Filter filter = Filter::all;
    bool ignoreSelection = false;

    struct IconButton : public juce::Button
    {
        IconButton (const juce::String& name, juce::Path p) : juce::Button (name), icon (std::move (p)) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        juce::Path icon;
    };

    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::TextButton filterAll { "All" }, filterFactory { "Factory" }, filterUser { "User" }, filterFav { "Favourites" };
    juce::TextEditor search;
    IconButton fileMenu { "Preset files", Icons::menu() };
    IconButton closeButton { "Close", Icons::close() };
    juce::TextButton saveButton { "Save" }, saveAsButton { "Save as" };
    juce::ListBox list;
    juce::TextButton editInfoButton { "Edit info" }, deleteButton { "Delete" };
    juce::Rectangle<int> previewArea;

    std::unique_ptr<juce::FileChooser> chooser;
};

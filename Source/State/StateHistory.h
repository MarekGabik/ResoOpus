#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Undo/redo and A/B comparison. Parameter values are polled on the message thread and a
// new undo step is committed once a change has settled (mouse released).
// Each of the A and B slots keeps its own undo history.
class StateHistory : private juce::Timer, public juce::ChangeBroadcaster
{
public:
    explicit StateHistory (juce::AudioProcessor& processor);
    ~StateHistory() override;

    bool canUndo() const { return ! undoStack[active].empty(); }
    bool canRedo() const { return ! redoStack[active].empty(); }
    void undo();
    void redo();

    int getActiveSlot() const { return active; }
    void switchTo (int slot);
    void copyToOther();

    // Call after a bulk state change (preset load, state restore) to avoid a bogus undo step
    void resync();

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    using Snapshot = std::vector<float>;

    void timerCallback() override;
    Snapshot capture() const;
    void apply (const Snapshot& s);

    juce::Array<juce::RangedAudioParameter*> params;
    Snapshot current;
    std::vector<Snapshot> undoStack[2], redoStack[2];
    Snapshot slots[2];
    int active = 0;
    static constexpr size_t maxSteps = 64;
};

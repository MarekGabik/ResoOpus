#include "StateHistory.h"

using namespace juce;

StateHistory::StateHistory (AudioProcessor& processor)
{
    for (auto* p : processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            params.add (rp);

    current = capture();
    slots[0] = slots[1] = current;
    startTimerHz (8);
}

StateHistory::~StateHistory()
{
    stopTimer();
}

StateHistory::Snapshot StateHistory::capture() const
{
    Snapshot s;
    s.reserve ((size_t) params.size());
    for (auto* p : params)
        s.push_back (p->getValue());
    return s;
}

void StateHistory::apply (const Snapshot& s)
{
    if (s.size() != (size_t) params.size())
        return;

    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params.getUnchecked (i);
        if (std::abs (p->getValue() - s[(size_t) i]) > 1.0e-6f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (s[(size_t) i]);
            p->endChangeGesture();
        }
    }

    current = s;
}

void StateHistory::timerCallback()
{
    if (ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
        return;

    auto now = capture();
    if (now == current)
        return;

    undoStack[active].push_back (current);
    if (undoStack[active].size() > maxSteps)
        undoStack[active].erase (undoStack[active].begin());

    redoStack[active].clear();
    current = std::move (now);
    sendChangeMessage();
}

void StateHistory::undo()
{
    timerCallback(); // commit pending change first
    if (! canUndo())
        return;

    redoStack[active].push_back (current);
    auto s = undoStack[active].back();
    undoStack[active].pop_back();
    apply (s);
    sendChangeMessage();
}

void StateHistory::redo()
{
    if (! canRedo())
        return;

    undoStack[active].push_back (current);
    auto s = redoStack[active].back();
    redoStack[active].pop_back();
    apply (s);
    sendChangeMessage();
}

void StateHistory::switchTo (int slot)
{
    slot = jlimit (0, 1, slot);
    if (slot == active)
        return;

    timerCallback();
    slots[active] = capture();
    active = slot;
    apply (slots[active]);
    sendChangeMessage();
}

void StateHistory::copyToOther()
{
    slots[1 - active] = capture();
    sendChangeMessage();
}

void StateHistory::resync()
{
    current = capture();
}

ValueTree StateHistory::toValueTree() const
{
    ValueTree t ("AB");
    t.setProperty ("active", active, nullptr);

    for (int s = 0; s < 2; ++s)
    {
        auto snap = s == active ? capture() : slots[s];
        MemoryBlock mb (snap.data(), snap.size() * sizeof (float));
        t.setProperty ("slot" + String (s), mb.toBase64Encoding(), nullptr);
    }
    return t;
}

void StateHistory::fromValueTree (const ValueTree& t)
{
    if (! t.isValid())
        return;

    for (int s = 0; s < 2; ++s)
    {
        MemoryBlock mb;
        if (mb.fromBase64Encoding (t.getProperty ("slot" + String (s)).toString())
             && mb.getSize() == (size_t) params.size() * sizeof (float))
        {
            slots[s].assign ((const float*) mb.getData(), (const float*) mb.getData() + params.size());
        }
    }

    active = jlimit (0, 1, (int) t.getProperty ("active", 0));
    undoStack[0].clear(); undoStack[1].clear();
    redoStack[0].clear(); redoStack[1].clear();
    resync();
    sendChangeMessage();
}

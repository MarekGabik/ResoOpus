#include "ModDrawer.h"
#include "PluginProcessor.h"

using namespace juce;

namespace
{
constexpr int headerH = 64, rowH = 30, detailH = 318;

void styleSlider (Slider& s)
{
    s.setSliderStyle (Slider::LinearHorizontal);
    s.setTextBoxStyle (Slider::TextBoxRight, false, 64, 20);
    s.setColour (Slider::trackColourId, Palette::curve);
    s.setColour (Slider::backgroundColourId, Colour (0xff26272d));
    s.setColour (Slider::thumbColourId, Palette::text);
    s.setColour (Slider::textBoxTextColourId, Palette::text);
    s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
}
}

ModDrawer::ModDrawer (ResoOpusProcessor& p) : proc (p)
{
    for (auto* b : { &addButton, &clearButton, &closeButton })
    {
        addAndMakeVisible (b);
        b->setColour (TextButton::buttonColourId, Palette::button);
    }
    addButton.onClick = [this] { showAddMenu(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    clearButton.onClick = [this]
    {
        for (auto& r : rows) if (! r.on) proc.removeModulation (r.slot);
        refresh();
    };

    const auto& names = Mod::sourceNames();
    for (int i = 0; i < names.size(); ++i) { srcBox.addItem (names[i], i + 1); ctlBox.addItem (names[i], i + 1); }
    const auto& fns = Mod::functionNames();
    for (int i = 0; i < fns.size(); ++i) fnBox.addItem (fns[i], i + 1);
    destCodes = Mod::allDestinations();
    for (int c : destCodes) dstBox.addItem (Mod::destName (c), c + 1);
    dstBox.onChange = [this]
    {
        if (selected >= 0 && dstBox.getSelectedId() > 0)
            proc.setParamReal (Params::modID (selected, "dst"), (float) (dstBox.getSelectedId() - 1));
    };

    for (auto* c : { &srcBox, &dstBox, &ctlBox, &fnBox }) addChildComponent (c);
    for (auto* t : { &onToggle, &biToggle })
    {
        addChildComponent (t);
        t->setColour (ToggleButton::textColourId, Palette::label);
        t->setColour (ToggleButton::tickColourId, Palette::curve);
        t->setColour (ToggleButton::tickDisabledColourId, Palette::textFaint);
    }
    for (auto* s : { &amtSlider, &ctlSlider, &fnSlider })
    {
        styleSlider (*s);
        addChildComponent (s);
    }
    setInterceptsMouseClicks (true, true);
}

ModDrawer::~ModDrawer()
{
    srcAtt.reset(); ctlAtt.reset(); fnAtt.reset(); amtAtt.reset(); ctlAmtAtt.reset(); fnAmtAtt.reset(); onAtt.reset(); biAtt.reset();
}

Rectangle<int> ModDrawer::listArea() const
{
    return { 0, headerH, getWidth(), getHeight() - headerH - (selected >= 0 ? detailH : 0) };
}

Rectangle<int> ModDrawer::rowRect (int i) const
{
    const auto a = listArea();
    return { 8, a.getY() + (i - scroll) * rowH, getWidth() - 16, rowH - 2 };
}

void ModDrawer::refresh()
{
    std::vector<Row> now;
    for (int s = 0; s < Params::numModSlots; ++s)
    {
        const int src = (int) proc.getParamReal (Params::modID (s, "src"));
        const int dst = (int) proc.getParamReal (Params::modID (s, "dst"));
        if (src <= 0 || dst <= 0) continue;
        now.push_back ({ s, src, dst, proc.getParamReal (Params::modID (s, "amt")), proc.getParamReal (Params::modID (s, "on")) > 0.5f });
    }
    bool changed = now.size() != rows.size();
    for (size_t i = 0; ! changed && i < now.size(); ++i)
        changed = now[i].slot != rows[i].slot || now[i].src != rows[i].src || now[i].dst != rows[i].dst
                  || std::abs (now[i].amt - rows[i].amt) > 0.001f || now[i].on != rows[i].on;
    if (! changed) return;
    rows = now;
    bool stillThere = false;
    for (auto& r : rows) stillThere = stillThere || r.slot == selected;
    if (selected >= 0 && ! stillThere) select (-1);
    if (selected >= 0 && dstBox.getSelectedId() != (int) proc.getParamReal (Params::modID (selected, "dst")) + 1)
        dstBox.setSelectedId ((int) proc.getParamReal (Params::modID (selected, "dst")) + 1, dontSendNotification);
    repaint();
}

void ModDrawer::select (int slot)
{
    selected = slot;
    rebuildDetail();
    resized();
    repaint();
}

void ModDrawer::rebuildDetail()
{
    srcAtt.reset(); ctlAtt.reset(); fnAtt.reset(); amtAtt.reset(); ctlAmtAtt.reset(); fnAmtAtt.reset(); onAtt.reset(); biAtt.reset();
    const bool vis = selected >= 0;
    for (Component* c : std::initializer_list<Component*> { &srcBox, &dstBox, &ctlBox, &fnBox, &onToggle, &biToggle, &amtSlider, &ctlSlider, &fnSlider })
        c->setVisible (vis);
    if (! vis) return;

    auto P = [this] (const char* id) { return proc.param (Params::modID (selected, id)); };
    srcAtt = std::make_unique<ComboBoxParameterAttachment> (*P ("src"), srcBox);
    ctlAtt = std::make_unique<ComboBoxParameterAttachment> (*P ("ctl"), ctlBox);
    fnAtt = std::make_unique<ComboBoxParameterAttachment> (*P ("fn"), fnBox);
    amtAtt = std::make_unique<SliderParameterAttachment> (*P ("amt"), amtSlider);
    ctlAmtAtt = std::make_unique<SliderParameterAttachment> (*P ("ctlamt"), ctlSlider);
    fnAmtAtt = std::make_unique<SliderParameterAttachment> (*P ("fnamt"), fnSlider);
    onAtt = std::make_unique<ButtonParameterAttachment> (*P ("on"), onToggle);
    biAtt = std::make_unique<ButtonParameterAttachment> (*P ("bi"), biToggle);
    dstBox.setSelectedId ((int) proc.getParamReal (Params::modID (selected, "dst")) + 1, dontSendNotification);
    const auto col = Mod::sourceColour ((int) proc.getParamReal (Params::modID (selected, "src")));
    amtSlider.setColour (Slider::trackColourId, col);
}

void ModDrawer::resized()
{
    addButton.setBounds (14, 34, 64, 22);
    clearButton.setBounds (84, 34, 108, 22);
    closeButton.setBounds (getWidth() - 70, 34, 56, 22);
    if (selected < 0) return;

    auto d = getLocalBounds().removeFromBottom (detailH).reduced (14, 10);
    d.removeFromTop (30);
    auto row = [&d] (int h = 24) { auto r = d.removeFromTop (h); d.removeFromTop (6); return r; };
    auto lab = 92;
    srcBox.setBounds (row().withTrimmedLeft (lab));
    dstBox.setBounds (row().withTrimmedLeft (lab));
    auto t = row();
    onToggle.setBounds (t.removeFromLeft (t.getWidth() / 2));
    biToggle.setBounds (t);
    amtSlider.setBounds (row().withTrimmedLeft (lab));
    ctlBox.setBounds (row().withTrimmedLeft (lab));
    ctlSlider.setBounds (row().withTrimmedLeft (lab));
    fnBox.setBounds (row().withTrimmedLeft (lab));
    fnSlider.setBounds (row().withTrimmedLeft (lab));
}

void ModDrawer::paint (Graphics& g)
{
    g.setColour (Colour (0xf70c0d10));
    g.fillRect (getLocalBounds());
    g.setColour (Palette::panelEdge);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    g.setColour (Palette::text);
    g.setFont (Fonts::bold (15.0f));
    g.drawText ("Modulation", 14, 6, 200, 24, Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (String ((int) rows.size()) + " / " + String (Params::numModSlots), getWidth() - 120, 6, 106, 24, Justification::centredRight, false);

    const auto la = listArea();
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (la);
        if (rows.empty())
        {
            g.setColour (Palette::textDim);
            g.setFont (Fonts::regular (12.5f));
            g.drawFittedText ("No modulation yet.\n\nDrag the arrow from an LFO, envelope or random header (CNTRL pages) onto any knob, "
                              "right-click a knob > Modulate with, or use + Add.",
                              la.reduced (16, 12).withHeight (120), Justification::topLeft, 8);
        }
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& r = rows[(size_t) i];
            auto rr = rowRect (i).toFloat();
            if (r.slot == selected)
            {
                g.setColour (Colour (0xff1d1e24));
                g.fillRoundedRectangle (rr, 5.0f);
            }
            const auto col = Mod::sourceColour (r.src);
            const auto dot = Rectangle<float> (11.0f, 11.0f).withCentre ({ rr.getX() + 12.0f, rr.getCentreY() });
            g.setColour (col.withAlpha (r.on ? 1.0f : 0.4f));
            if (r.on) g.fillEllipse (dot); else g.drawEllipse (dot, 2.0f);
            g.setColour (r.on ? Palette::text : Palette::textFaint);
            g.setFont (Fonts::regular (12.5f));
            g.drawText (Mod::sourceNames()[r.src] + String (CharPointer_UTF8 (" \xe2\x86\x92 ")) + Mod::destName (r.dst),
                        rr.withTrimmedLeft (26.0f).withTrimmedRight (76.0f), Justification::centredLeft, true);
            g.setFont (Fonts::mono (11.0f));
            g.setColour (col.withAlpha (r.on ? 1.0f : 0.4f));
            g.drawText ((r.amt >= 0 ? "+" : "") + String (roundToInt (r.amt * 100.0f)) + " %", rr.withTrimmedRight (24.0f), Justification::centredRight, false);
            Icons::draw (g, Icons::close(), Rectangle<float> (10.0f, 10.0f).withCentre ({ rr.getRight() - 10.0f, rr.getCentreY() }), Palette::textDim, 1.4f);
        }
    }

    if (selected >= 0)
    {
        auto d = getLocalBounds().removeFromBottom (detailH);
        g.setColour (Colour (0xff141519));
        g.fillRect (d);
        const auto col = Mod::sourceColour ((int) proc.getParamReal (Params::modID (selected, "src")));
        g.setColour (col);
        g.fillRect (d.removeFromTop (2));
        g.setColour (Palette::text);
        g.setFont (Fonts::bold (13.0f));
        g.drawText ("Slot " + String (selected + 1), d.getX() + 14, d.getY() + 8, 200, 20, Justification::centredLeft, false);

        auto inner = d.reduced (14, 10);
        inner.removeFromTop (30);
        g.setFont (Fonts::regular (12.0f));
        g.setColour (Palette::label);
        for (auto* name : { "Source", "Destination", "", "Amount", "Controller", "Ctrl amount", "Function", "Fn amount" })
        {
            auto r = inner.removeFromTop (24);
            inner.removeFromTop (6);
            g.drawText (name, r, Justification::centredLeft, false);
        }
    }
}

void ModDrawer::mouseDown (const MouseEvent& e)
{
    const auto la = listArea();
    if (! la.contains (e.getPosition())) return;
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto rr = rowRect (i);
        if (! rr.contains (e.getPosition())) continue;
        const auto& r = rows[(size_t) i];
        if (e.x > rr.getRight() - 22) { proc.removeModulation (r.slot); refresh(); return; }
        if (e.x < rr.getX() + 24) { proc.setParamReal (Params::modID (r.slot, "on"), r.on ? 0.0f : 1.0f); refresh(); return; }
        select (r.slot == selected ? -1 : r.slot);
        return;
    }
}

void ModDrawer::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const int visible = listArea().getHeight() / rowH;
    scroll = jlimit (0, std::max (0, (int) rows.size() - visible), scroll + (w.deltaY < 0 ? 1 : -1));
    repaint();
}

void ModDrawer::showAddMenu()
{
    PopupMenu src;
    for (int s = 1; s < Mod::numSources; ++s) src.addItem (s, Mod::sourceNames()[s]);

    src.showMenuAsync (PopupMenu::Options().withTargetComponent (&addButton), [sp = SafePointer<ModDrawer> (this)] (int s)
    {
        if (sp == nullptr || s <= 0) return;
        PopupMenu dst;
        PopupMenu groups[Mod::globalBlock];
        for (int c : sp->destCodes) groups[c / Mod::blockSize - 1].addItem (c, Mod::destName (c));
        for (int b = 1; b <= Mod::globalBlock; ++b)
            dst.addSubMenu (Mod::blockName (b), groups[b - 1]);
        dst.showMenuAsync (PopupMenu::Options().withTargetComponent (&sp->addButton), [sp, s] (int d)
        {
            if (sp == nullptr || d <= 0) return;
            const int slot = sp->proc.addModulation (s, Mod::destParamID (d), 0.25f);
            sp->refresh();
            if (slot >= 0) sp->select (slot);
        });
    });
}

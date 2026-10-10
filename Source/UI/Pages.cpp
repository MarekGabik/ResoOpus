#include "Pages.h"
#include "PluginProcessor.h"

using namespace juce;

//==============================================================================
Knob* Page::knob (const String& id, const String& label, Colour c, Knob::Style s)
{
    auto* k = new Knob (proc, id, label, c, s);
    owned.add (k);
    knobs.add (k);
    return k;
}

ModulePanel* Page::panel (const String& title, Colour c, Rectangle<int> b, int source, const String& right)
{
    auto* p = new ModulePanel (title, c, source, right);
    owned.add (p);
    p->setBounds (b);
    addAndMakeVisible (p);
    return p;
}

void Page::refreshLive()
{
    for (auto* k : knobs)
        if (k->isVisible()) k->refreshLive();
}

void Page::setModEditCallback (std::function<void (int)> cb)
{
    for (auto* k : knobs) k->onEditModulation = cb;
}

Knob* Page::findKnob (const String& id) const
{
    for (auto* k : knobs) if (k->getParamID() == id) return k;
    return nullptr;
}

//==============================================================================
SelectorPill::SelectorPill (const String& l, std::function<String()> t, std::function<void (SelectorPill&)> c)
    : label (l), getText (std::move (t)), click (std::move (c))
{
    setSize (140, 44);
    shown = getText();
}

void SelectorPill::refresh()
{
    const auto t = getText();
    if (t != shown) { shown = t; repaint(); }
}

void SelectorPill::paint (Graphics& g)
{
    g.setColour (Palette::label);
    g.setFont (Fonts::bold (10.5f).withExtraKerningFactor (0.05f));
    g.drawText (label, 0, 0, getWidth(), 13, Justification::centredLeft, false);
    auto box = Rectangle<float> (0.0f, 17.0f, (float) getWidth(), 26.0f);
    g.setColour (isMouseOver() ? Palette::buttonHover : Palette::button);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (Colour (0xff2a2b31));
    g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (Fonts::regular (12.5f));
    g.drawText (shown, box.withTrimmedLeft (10.0f).withTrimmedRight (22.0f), Justification::centredLeft, true);
    Path arrow;
    const float ax = box.getRight() - 15.0f, ay = box.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::textDim);
    g.fillPath (arrow);
}

void SelectorPill::mouseDown (const MouseEvent&) { if (click) click (*this); }

//==============================================================================
LedDot::LedDot (ResoOpusProcessor& p, const String& id)
{
    param = p.param (id);
    jassert (param != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
    if (auto* d = Params::findDef (id)) setTooltip (d->tip);
    setSize (12, 12);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void LedDot::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.5f);
    if (on)
    {
        g.setColour (Palette::curve.withAlpha (0.3f));
        g.fillEllipse (r.expanded (2.0f));
        g.setColour (Palette::curve);
    }
    else
        g.setColour (Colour (0xff2d2e35));
    g.fillEllipse (r);
}

void LedDot::mouseDown (const MouseEvent&) { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

//==============================================================================
void GravityView::refresh()
{
    bool any = false, changed = false;
    for (int v = 0; v < 8; ++v)
    {
        const float l = proc.liveLevel[v].load();
        const int t = roundToInt (proc.getParamReal (Params::voiceID (v, "type")));
        if (std::abs (l - levels[v]) > 0.01f || t != types[v]) { levels[v] = l; types[v] = t; changed = true; }
        any = any || l > 0.01f;
    }
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    if (any) phase += 0.18 * std::min (0.1, now - lastTime);   // slow orbit while something sounds
    lastTime = now;
    if (any || changed) repaint();
}

void GravityView::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre().translated (0.0f, -6.0f);
    const float R = 92.0f;
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 1.0f);
    g.setColour (Colours::white.withAlpha (0.035f));
    g.drawEllipse (Rectangle<float> (R * 1.3f, R * 1.3f).withCentre (c), 1.0f);

    // centre: what this panel will become
    g.setColour (Palette::textFaint);
    g.setFont (Fonts::bold (11.0f).withExtraKerningFactor (0.12f));
    g.drawText ("GRAVITY", Rectangle<float> (200.0f, 16.0f).withCentre (c.translated (0.0f, -8.0f)), Justification::centred, false);
    g.setFont (Fonts::regular (11.0f));
    g.drawText ("knob arrives in phase 3", Rectangle<float> (200.0f, 16.0f).withCentre (c.translated (0.0f, 10.0f)), Justification::centred, false);

    for (int v = 0; v < 8; ++v)
    {
        const double a = phase + v * MathConstants<double>::twoPi / 8.0;
        const Point<float> p (c.x + R * (float) std::cos (a), c.y + R * (float) std::sin (a));
        const auto col = Palette::voiceType (types[v]);
        const float lv = jlimit (0.0f, 1.0f, levels[v] * 1.4f);
        const float r = 4.0f + 2.0f * lv;
        if (lv > 0.02f)
        {
            g.setColour (col.withAlpha (0.25f * lv));
            g.fillEllipse (Rectangle<float> (r * 4.0f, r * 4.0f).withCentre (p));
        }
        g.setColour (lv > 0.02f ? col.withAlpha (0.4f + 0.6f * lv) : Colour (0xff2b2c33));
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
        g.setColour (col.withAlpha (0.8f));
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 1.2f);
    }
}

//==============================================================================
namespace
{
Knob::Style range (String lo, String hi, bool bipolar = false, bool small = false)
{
    Knob::Style s;
    s.lo = lo; s.hi = hi; s.bipolar = bipolar; s.small = small;
    return s;
}
Knob::Style smallStyle()
{
    Knob::Style s;
    s.small = true;
    return s;
}
}

int DronePage::voiceType (int v) const { return roundToInt (proc.getParamReal (Params::voiceID (v, "type"))); }

DronePage::DronePage (ResoOpusProcessor& p) : Page (p)
{
    const int gw = 422, gh = 352, gap = 10;

    // GROUP A / B: HOLD and PITCH, below them the four voice strips of the group
    const int groupH = 112, stripY = groupH + gap, stripH = gh - stripY;
    for (int g = 0; g < 2; ++g)
    {
        const int gx = g == 0 ? 0 : width - gw;
        auto* gp = new GroupPanel (*this, g, String (g == 0 ? "GROUP A" : "GROUP B") + String (CharPointer_UTF8 (" \xc2\xb7 ")) + (g == 0 ? "1-4" : "5-8"));
        owned.add (gp);
        gp->setBounds (gx, 0, gw, groupH);
        addAndMakeVisible (gp);

        auto* hold = knob (Params::groupID (g, "hold"), "HOLD", Palette::group, smallStyle());
        auto* pitch = knob (Params::groupID (g, "pitch"), "PITCH", Palette::group, smallStyle());
        gp->addAndMakeVisible (hold);
        gp->addAndMakeVisible (pitch);
        hold->setTopLeftPosition (gw / 2 - 96, ModulePanel::headerHeight + 2);
        pitch->setTopLeftPosition (gw / 2 + 16, ModulePanel::headerHeight + 2);

        const int sw = (gw - 3 * 6) / 4;
        for (int k = 0; k < 4; ++k)
        {
            const int v = g * 4 + k;
            auto* col = new Column (*this, v);
            owned.add (col);
            col->setBounds (gx + k * (sw + 6), stripY, sw, stripH);
            addAndMakeVisible (col);
            col->tune = knob (Params::voiceID (v, "tune"), "TUNE", Palette::voiceType (voiceType (v)), smallStyle());
            col->tide = knob (Params::voiceID (v, "tide"), "TIDE", Palette::choir, smallStyle());
            col->vol = new VolBar (proc, v);
            owned.add (col->vol);
            col->addAndMakeVisible (col->tune);
            col->addAndMakeVisible (col->tide);
            col->addAndMakeVisible (col->vol);
            col->tune->setTopLeftPosition ((sw - col->tune->getWidth()) / 2, ModulePanel::headerHeight + 6);
            col->tide->setTopLeftPosition ((sw - col->tide->getWidth()) / 2, ModulePanel::headerHeight + 92);
            col->vol->setBounds (8, stripH - 34, sw - 16, 28);
            columns[v] = col;
        }
    }

    // GRAVITY (phase 1: orbit of the voices)
    auto* gpanel = panel ("GRAVITY", Palette::gravity, { gw + gap, 0, width - 2 * (gw + gap), gh });
    gravity = new GravityView (proc);
    owned.add (gravity);
    gravity->setBounds (0, ModulePanel::headerHeight + 2, gpanel->getWidth(), gh - ModulePanel::headerHeight - 2);
    gpanel->addAndMakeVisible (gravity);

    // detail of the selected voice
    detailPanel = panel ("VOICE 1", Palette::choir, { 0, gh + gap, width, height - gh - gap });
    for (int v = 0; v < 8; ++v)
    {
        auto& d = details[v];
        auto add = [&d, this] (Component* c, int x, int y, bool choirOnly)
        {
            detailPanel->addChildComponent (c);
            c->setTopLeftPosition (x, y);
            d.all.add (c);
            if (choirOnly) d.choirOnly.add (c);
        };
        const String V = Params::voiceID (v, "");
        d.type = new LedRadio (proc, V + "type", "TYPE", { "CHOIR", "SIREN", "PULSAR" });
        owned.add (d.type);
        add (d.type, 16, 40, false);

        d.chord = new LedRadio ("CHORD", { "UNISON", "OCTAVES", "FIFTHS", "MAJOR", "MINOR" },
            [this, v]
            {
                float iv[5];
                for (int i = 0; i < 5; ++i) iv[i] = proc.getParamReal (Params::voiceID (v, "int" + String (i + 1)));
                return Params::matchChord (iv);   // -1 = own intervals (no LED lit)
            },
            [this, v] (int c) { proc.applyChord (v, c); });
        d.chord->setTooltip ("Fills the five oscillator intervals with a chord in one click; fine-tune them afterwards by ear");
        owned.add (d.chord);
        add (d.chord, 136, 40, true);

        for (int i = 0; i < 5; ++i)
        {
            auto* k = knob (V + "int" + String (i + 1), "OSC " + String (i + 1), Palette::choir, smallStyle());
            add (k, 264 + i * 80, 60, true);
            auto* led = new LedDot (proc, V + "on" + String (i + 1));
            owned.add (led);
            add (led, 264 + i * 80 + 12, 60, true);
        }
        auto* div = new Component();
        owned.add (div);
        div->setSize (1, 110);
        add (div, 674, 46, false);

        const int x0 = 688, dx = 94, y = 53;
        add (knob (V + "tide", "TIDE", Palette::choir, range ("0", "MAX")), x0, y, true);
        add (knob (V + "attack", "ATTACK", Palette::choir, range ("1ms", "30s")), x0 + dx, y, false);
        add (knob (V + "release", "RELEASE", Palette::choir, range ("1ms", "30s")), x0 + 2 * dx, y, false);
        add (knob (V + "volume", "VOLUME", Palette::choir, range ("-inf", "+6")), x0 + 3 * dx, y, false);
        add (knob (V + "pan", "PAN", Palette::choir, range ("L", "R", true)), x0 + 4 * dx, y, false);
    }

    phaseNote = new Label ({}, "This voice type gets its engine in phase 2. TUNE, envelope, VOLUME and PAN already work.");
    owned.add (phaseNote);
    phaseNote->setFont (Fonts::regular (12.5f));
    phaseNote->setColour (Label::textColourId, Palette::textDim);
    phaseNote->setJustificationType (Justification::centred);
    phaseNote->setBounds (264, 70, 400, 60);
    detailPanel->addChildComponent (phaseNote);

    selectVoice ((int) proc.uiState.getProperty ("selVoice", 0));
}

void DronePage::selectVoice (int v)
{
    selected = jlimit (0, 7, v);
    proc.uiState.setProperty ("selVoice", selected, nullptr);
    for (int i = 0; i < 8; ++i) shownType[i] = -1;   // force a layout refresh
    refreshLive();
}

void DronePage::GroupPanel::mouseDown (const MouseEvent& e)
{
    if (e.y < ModulePanel::headerHeight && e.x > getWidth() - 140)
        owner.showFillMenu (group, this);
}

void DronePage::showFillMenu (int group, Component* target)
{
    PopupMenu m, roots;
    const int curRoot = roundToInt (proc.getParamReal ("snap_root"));
    m.addSectionHeader ("FILL: spread the four voices over a scale (1-3-5-7)");
    for (int s = 0; s < Params::scaleNames().size(); ++s) m.addItem (100 + s, Params::scaleNames()[s]);
    for (int r = 0; r < 12; ++r) roots.addItem (200 + r, Params::findDef ("snap_root")->choices[r], true, r == curRoot);
    m.addSeparator();
    m.addSubMenu ("Root: " + Params::findDef ("snap_root")->choices[curRoot], roots);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this, group] (int r)
    {
        if (r >= 200) proc.setParamReal ("snap_root", (float) (r - 200));
        else if (r >= 100) proc.fillGroup (group, r - 100, roundToInt (proc.getParamReal ("snap_root")));
    });
}

void DronePage::Column::paintOverChildren (Graphics& g)
{
    const auto col = Palette::voiceType (jmax (0, type));
    if (selected)
    {
        g.setColour (col.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 4.0f);
        g.setColour (col);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 1.6f);
    }
    if (type != VP::Choir)
    {
        g.setColour (Palette::textFaint);
        g.setFont (Fonts::regular (10.5f));
        g.drawFittedText ("engine in\nphase 2", Rectangle<int> (0, ModulePanel::headerHeight + 104, getWidth(), 40), Justification::centred, 2);
    }
}

//==============================================================================
DronePage::VolBar::VolBar (ResoOpusProcessor& p, int v) : proc (p)
{
    vol = proc.param (Params::voiceID (v, "volume"));
    pan = proc.param (Params::voiceID (v, "pan"));
    setTooltip ("Volume of this voice: drag left / right (Shift = fine), double-click = default. PAN is in the voice detail below");
    setMouseCursor (MouseCursor::LeftRightResizeCursor);
}

bool DronePage::VolBar::refresh (float level)
{
    const float v = vol->getValue(), pn = pan->convertFrom0to1 (pan->getValue());
    if (std::abs (v - shownVol) > 0.002f || std::abs (pn - shownPan) > 0.004f || std::abs (level - shownLevel) > 0.02f)
    {
        shownVol = v; shownPan = pn; shownLevel = level;
        repaint();
        return true;
    }
    return false;
}

void DronePage::VolBar::paint (Graphics& g)
{
    const float w = (float) getWidth();
    g.setColour (Palette::label);
    g.setFont (Fonts::bold (10.0f).withExtraKerningFactor (0.05f));
    g.drawText ("VOL", 0, 0, 40, 12, Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (Fonts::mono (10.0f));
    const int pp = roundToInt (shownPan * 100.0f);
    g.drawText (pp == 0 ? String ("C") : (pp < 0 ? "L " + String (-pp) : "R " + String (pp)), 0, 0, (int) w, 12, Justification::centredRight, false);
    g.setColour (Colour (0xff24252b));
    g.fillRoundedRectangle (0.0f, 16.0f, w, 5.0f, 2.5f);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (0.0f, 16.0f, w * jlimit (0.0f, 1.0f, shownVol), 5.0f, 2.5f);
    g.setColour (Colour (0xff1b1c21));
    g.fillRoundedRectangle (0.0f, 24.0f, w, 3.0f, 1.5f);
    if (shownLevel > 0.005f)
    {
        g.setGradientFill (ColourGradient (Palette::green, 0.0f, 0.0f, Palette::curve, w, 0.0f, false));
        g.fillRoundedRectangle (0.0f, 24.0f, w * jlimit (0.0f, 1.0f, shownLevel * shownVol * 1.1f), 3.0f, 1.5f);
    }
}

void DronePage::VolBar::mouseDown (const MouseEvent&) { vol->beginChangeGesture(); }

void DronePage::VolBar::mouseDrag (const MouseEvent& e)
{
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float delta = (float) (e.getDistanceFromDragStartX() - (int) getProperties().getWithDefault ("lastX", 0)) / (float) getWidth() * fine;
    getProperties().set ("lastX", e.getDistanceFromDragStartX());
    vol->setValueNotifyingHost (jlimit (0.0f, 1.0f, vol->getValue() + delta));
}

void DronePage::VolBar::mouseUp (const MouseEvent&)
{
    getProperties().set ("lastX", 0);
    vol->endChangeGesture();
}

void DronePage::VolBar::mouseDoubleClick (const MouseEvent&)
{
    vol->beginChangeGesture();
    vol->setValueNotifyingHost (vol->getDefaultValue());
    vol->endChangeGesture();
}

void DronePage::refreshLive()
{
    Page::refreshLive();
    for (int v = 0; v < 8; ++v)
    {
        auto* c = columns[v];
        const int t = voiceType (v);
        const float lv = proc.liveLevel[v].load();
        const bool sel = v == selected;
        if (t != c->type || sel != c->selected)
        {
            c->type = t; c->selected = sel;
            const auto col = Palette::voiceType (t);
            c->setTitleAndColour (Params::findDef ("v1_type")->choices[t].toUpperCase(), col);
            c->vol->colour = col;
            c->tide->setVisible (t == VP::Choir);
            c->repaint();
        }
        c->setLiveValue (jlimit (0.0f, 1.0f, lv * 1.5f));
        c->vol->refresh (lv);
    }
    gravity->refresh();

    // detail: only the selected voice, choir-only controls depend on the type
    const int t = voiceType (selected);
    if (shownType[selected] != t)
    {
        for (int v = 0; v < 8; ++v)
        {
            for (auto* c : details[v].all) c->setVisible (false);
            shownType[v] = -1;
        }
        for (auto* c : details[selected].all)
            c->setVisible (t == VP::Choir || ! details[selected].choirOnly.contains (c));
        phaseNote->setVisible (t != VP::Choir);
        shownType[selected] = t;
        const auto col = Palette::voiceType (t);
        detailPanel->setTitleAndColour ("VOICE " + String (selected + 1) + String (CharPointer_UTF8 (" \xc2\xb7 ")) + Params::findDef ("v1_type")->choices[t].toUpperCase(), col);
    }
    details[selected].chord->refresh();
}

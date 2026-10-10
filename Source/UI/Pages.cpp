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
    for (int v = 0; v < Params::numVoices; ++v)
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

    for (int v = 0; v < Params::numVoices; ++v)
    {
        const double a = phase + v * MathConstants<double>::twoPi / (double) Params::numVoices;
        const Point<float> p (c.x + R * (float) std::cos (a), c.y + R * (float) std::sin (a));
        const auto col = Palette::voiceType (types[v]);
        const float lv = jlimit (0.0f, 1.0f, levels[v] * 1.4f);
        const float r = 5.0f + 2.5f * lv;
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
[[maybe_unused]] Knob::Style smallStyle()
{
    Knob::Style s;
    s.small = true;
    return s;
}
}

int DronePage::voiceType (int v) const { return roundToInt (proc.getParamReal (Params::voiceID (v, "type"))); }

DronePage::DronePage (ResoOpusProcessor& p) : Page (p)
{
    const int gw = 422, gh = 352, gap = 10, sw = (gw - 6) / 2;

    // voice strips: pair 1-2 left of GRAVITY, pair 3-4 right of it
    for (int v = 0; v < Params::numVoices; ++v)
    {
        const int x = v < 2 ? v * (sw + 6) : width - gw + (v - 2) * (sw + 6);
        auto* col = new Column (*this, v);
        owned.add (col);
        col->setBounds (x, 0, sw, gh);
        addAndMakeVisible (col);
        const String V = Params::voiceID (v, "");
        auto put = [col] (Component* c, int cx, int y) { col->addAndMakeVisible (c); c->setTopLeftPosition (cx - c->getWidth() / 2, y); col->controls.add (c); };
        const int c1 = sw / 4 + 2, c2 = sw * 3 / 4 - 2;
        put (knob (V + "tune", "TUNE", Palette::voiceType (voiceType (v)), range ("C0", "C8")), c1, 34);
        col->character[1] = knob (V + "tide", "TIDE", Palette::choir, range ("0", "MAX"));
        col->character[2] = knob (V + "siren_fm", "FM", Palette::siren, range ("0", "100"));
        col->character[3] = knob (V + "sharp", "SHARP", Palette::pulsar, range ("SOFT", "SHARP"));
        for (int t = 1; t < 4; ++t) put (col->character[t], c2, 34);
        put (knob (V + "hold", "HOLD", Palette::group, range ("0", "100")), c1, 146);
        put (knob (V + "pan", "PAN", Palette::group, range ("L", "R", true)), c2, 146);
        col->vol = new VolBar (proc, v);
        owned.add (col->vol);
        col->addAndMakeVisible (col->vol);
        col->vol->setBounds (12, gh - 44, sw - 24, 28);
        col->controls.add (col->vol);
        col->trace = new LevelTrace();
        owned.add (col->trace);
        col->addAndMakeVisible (col->trace);
        col->trace->setBounds (12, 252, sw - 24, 48);
        col->controls.add (col->trace);
        columns[v] = col;
    }

    // GRAVITY (phase 1-2: orbit of the voices)
    auto* gpanel = panel ("GRAVITY", Palette::gravity, { gw + gap, 0, width - 2 * (gw + gap), gh });
    gravity = new GravityView (proc);
    owned.add (gravity);
    gravity->setBounds (0, ModulePanel::headerHeight + 2, gpanel->getWidth(), gh - ModulePanel::headerHeight - 2);
    gpanel->addAndMakeVisible (gravity);

    // MASTER: transpose all, output volume, FILL
    const int mw = 300, dh = height - gh - gap;
    auto* master = panel ("MASTER", Palette::master, { width - mw, gh + gap, mw, dh });
    auto* pitch = knob ("pitch", "PITCH", Palette::master, range ("-24", "+24", true));
    auto* vol = knob ("master_vol", "VOLUME", Palette::master, range ("-inf", "+12"));
    master->addAndMakeVisible (pitch);
    master->addAndMakeVisible (vol);
    pitch->setTopLeftPosition (14, 50);
    vol->setTopLeftPosition (106, 50);
    auto* fill = new SelectorPill ("FILL", [] { return String ("Scale..."); }, [this] (SelectorPill& pill) { showFillMenu (&pill); });
    fill->setTooltip ("FILL: spreads the four voices over a scale (degrees 1-3-5-7), then fine-tune them by ear");
    owned.add (fill);
    fill->setSize (84, 44);
    master->addAndMakeVisible (fill);
    fill->setTopLeftPosition (204, 76);

    // detail of the selected voice
    detailPanel = panel ("VOICE 1", Palette::choir, { 0, gh + gap, width - mw - gap, dh });
    for (int v = 0; v < Params::numVoices; ++v)
    {
        auto& d = details[v];
        auto add = [&d, this] (Component* c, int x, int y, int type)
        {
            detailPanel->addChildComponent (c);
            c->setTopLeftPosition (x, y);
            d.all.add (c);
            d.byType[type].add (c);
        };
        const String V = Params::voiceID (v, "");
        auto* type = new LedRadio (proc, V + "type", "TYPE", { "OFF", "CHOIR", "SIREN", "PULSAR" });
        owned.add (type);
        detailPanel->addChildComponent (type);
        type->setTopLeftPosition (16, 36);
        d.all.add (type);

        // CHOIR: chord and the five intervals
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
        add (d.chord, 134, 36, VP::Choir);
        for (int i = 0; i < 5; ++i)
        {
            auto* k = knob (V + "int" + String (i + 1), "OSC " + String (i + 1), Palette::choir, smallStyle());
            add (k, 250 + i * 76, 52, VP::Choir);
            auto* led = new LedDot (proc, V + "on" + String (i + 1));
            owned.add (led);
            add (led, 250 + i * 76 + 12, 52, VP::Choir);
        }
        // SIREN
        const char* sirenIds[] = { "siren_rate", "siren_am", "siren_div", "siren_noise", "siren_wave" };
        const char* sirenNames[] = { "RATE", "AM", "DIVIDER", "NOISE", "WAVE" };
        const char* sirenLo[] = { ".05", "0", "OFF", "0", "SQR" };
        const char* sirenHi[] = { "300", "100", "1:16", "100", "TRI" };
        for (int i = 0; i < 5; ++i)
            add (knob (V + sirenIds[i], sirenNames[i], Palette::siren, range (sirenLo[i], sirenHi[i])), 134 + i * 100, 44, VP::Siren);
        // PULSAR
        const char* pulsarIds[] = { "bloom", "twin", "twin_mix", "sub", "vibrato" };
        const char* pulsarNames[] = { "BLOOM", "TWIN", "BLEND", "SUB", "VIBRATO" };
        const char* pulsarLo[] = { "0", "-12", "0", "0", "0" };
        const char* pulsarHi[] = { "100", "+24", "100", "100", "50c" };
        for (int i = 0; i < 5; ++i)
            add (knob (V + pulsarIds[i], pulsarNames[i], Palette::pulsar, range (pulsarLo[i], pulsarHi[i])), 134 + i * 100, 44, VP::Pulsar);

        // envelope: every type that sounds
        auto* div = new Component();
        owned.add (div);
        div->setSize (1, 110);
        for (int t : { VP::Choir, VP::Siren, VP::Pulsar }) d.byType[t].add (div);
        detailPanel->addChildComponent (div);
        div->setTopLeftPosition (646, 40);
        d.all.add (div);
        for (auto* k : { knob (V + "attack", "ATTACK", Palette::group, range ("1ms", "30s")), knob (V + "release", "RELEASE", Palette::group, range ("1ms", "30s")) })
        {
            detailPanel->addChildComponent (k);
            d.all.add (k);
            for (int t : { VP::Choir, VP::Siren, VP::Pulsar }) d.byType[t].add (k);
        }
        d.all[d.all.size() - 2]->setTopLeftPosition (656, 44);
        d.all[d.all.size() - 1]->setTopLeftPosition (752, 44);
    }

    offNote = new Label ({}, "This voice is off. Pick CHOIR, SIREN or PULSAR to switch it on.");
    owned.add (offNote);
    offNote->setFont (Fonts::regular (12.5f));
    offNote->setColour (Label::textColourId, Palette::textDim);
    offNote->setJustificationType (Justification::centred);
    offNote->setBounds (150, 60, 600, 50);
    detailPanel->addChildComponent (offNote);

    selectVoice ((int) proc.uiState.getProperty ("selVoice", 0));
}

void DronePage::selectVoice (int v)
{
    selected = jlimit (0, Params::numVoices - 1, v);
    proc.uiState.setProperty ("selVoice", selected, nullptr);
    for (auto& t : shownType) t = -1;   // force a layout refresh
    refreshLive();
}

void DronePage::showFillMenu (Component* target)
{
    PopupMenu m, roots;
    const int curRoot = roundToInt (proc.getParamReal ("snap_root"));
    m.addSectionHeader ("FILL: spread the four voices over a scale (1-3-5-7)");
    for (int s = 0; s < Params::scaleNames().size(); ++s) m.addItem (100 + s, Params::scaleNames()[s]);
    for (int r = 0; r < 12; ++r) roots.addItem (200 + r, Params::findDef ("snap_root")->choices[r], true, r == curRoot);
    m.addSeparator();
    m.addSubMenu ("Root: " + Params::findDef ("snap_root")->choices[curRoot], roots);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (r >= 200) proc.setParamReal ("snap_root", (float) (r - 200));
        else if (r >= 100) proc.fillVoices (r - 100, roundToInt (proc.getParamReal ("snap_root")));
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
}

//==============================================================================
DronePage::VolBar::VolBar (ResoOpusProcessor& p, int v) : proc (p)
{
    vol = proc.param (Params::voiceID (v, "volume"));
    pan = proc.param (Params::voiceID (v, "pan"));
    setTooltip ("Volume of this voice: drag left / right (Shift = fine), double-click = default");
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
    g.drawText (Params::format (*Params::findDef ("v1_volume"), vol->convertFrom0to1 (shownVol)), 0, 0, (int) w, 12, Justification::centredRight, false);
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

void DronePage::LevelTrace::push (float level, float h)
{
    // repaint only while something changes (no redraw for a silent, steady voice)
    const float prev = history[(head + size - 1) % size];
    const bool nowFlat = std::abs (level - prev) < 0.002f && std::abs (h - hold) < 0.002f;
    history[head] = level;
    head = (head + 1) % size;
    hold = h;
    if (! (nowFlat && flat)) repaint();
    flat = nowFlat;
}

void DronePage::LevelTrace::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff111216));
    g.fillRoundedRectangle (r, 4.0f);
    auto y = [&r] (float v) { return r.getBottom() - 3.0f - jlimit (0.0f, 1.0f, v) * (r.getHeight() - 6.0f); };
    Path line, fill;
    for (int i = 0; i < size; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (size - 1);
        const float v = y (history[(head + i) % size]);
        if (i == 0) { line.startNewSubPath (x, v); fill.startNewSubPath (x, r.getBottom()); fill.lineTo (x, v); }
        else { line.lineTo (x, v); fill.lineTo (x, v); }
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (colour.withAlpha (0.28f), 0.0f, r.getY(), colour.withAlpha (0.02f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (line, PathStrokeType (1.4f));
    if (hold > 0.001f)
    {
        const float hy = y (hold);
        const float dashes[] = { 3.0f, 3.0f };
        g.setColour (Palette::label.withAlpha (0.55f));
        g.drawDashedLine (Line<float> (r.getX() + 2.0f, hy, r.getRight() - 2.0f, hy), dashes, 2, 1.0f);
    }
}

void DronePage::refreshLive()
{
    Page::refreshLive();
    for (int v = 0; v < Params::numVoices; ++v)
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
            for (int k = 1; k < 4; ++k) c->character[k]->setVisible (k == t || (t == VP::Off && k == VP::Choir));
            for (auto* ctl : c->controls) ctl->setAlpha (t == VP::Off ? 0.35f : 1.0f);
            for (int k = 1; k < 4; ++k) c->character[k]->setAlpha (t == VP::Off ? 0.35f : 1.0f);
            c->repaint();
        }
        c->setLiveValue (t == VP::Off ? 0.0f : jlimit (0.0f, 1.0f, lv * 1.5f));
        c->vol->refresh (lv);
        c->trace->colour = Palette::voiceType (t);
        c->trace->push (t == VP::Off ? 0.0f : lv, t == VP::Off ? 0.0f : proc.getParamReal (Params::voiceID (v, "hold")) / 100.0f);
    }
    gravity->refresh();

    // detail: only the selected voice; the type decides which controls show
    const int t = voiceType (selected);
    if (shownType[selected] != t)
    {
        for (int v = 0; v < Params::numVoices; ++v)
        {
            for (auto* c : details[v].all) c->setVisible (false);
            shownType[v] = -1;
        }
        auto& d = details[selected];
        d.all[0]->setVisible (true);   // TYPE
        for (auto* c : d.byType[t]) c->setVisible (true);
        offNote->setVisible (t == VP::Off);
        shownType[selected] = t;
        detailPanel->setTitleAndColour ("VOICE " + String (selected + 1) + String (CharPointer_UTF8 (" \xc2\xb7 ")) + Params::findDef ("v1_type")->choices[t].toUpperCase(),
                                        Palette::voiceType (t));
    }
    details[selected].chord->refresh();
}

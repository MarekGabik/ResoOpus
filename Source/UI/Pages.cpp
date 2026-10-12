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

MiniToggle::MiniToggle (ResoOpusProcessor& p, const String& id, const String& t) : text (t)
{
    param = p.param (id);
    jassert (param != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
    if (auto* d = Params::findDef (id)) setTooltip (d->tip);
    setSize (GlyphArrangement::getStringWidthInt (Fonts::bold (9.0f), text) + 14, 16);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void MiniToggle::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (on ? Palette::curve : Colour (0xff1c1d22));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? Palette::curve.brighter (0.2f) : Colour (0xff3a3b42));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? Palette::plateText : Palette::textDim);
    g.setFont (Fonts::bold (9.0f));
    g.drawText (text, r, Justification::centred, false);
}

void MiniToggle::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
GravityView::GravityView (ResoOpusProcessor& p) : proc (p)
{
    param = proc.param ("gravity");
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { value = param->convertTo0to1 (v); repaint(); });
    attachment->sendInitialUpdate();
    setTooltip (String (Params::findDef ("gravity")->tip) + "\n(drag a modulation source here, right-click for PATH)");
    setMouseCursor (MouseCursor::UpDownResizeCursor);
}

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
    const float lg = proc.liveGravity.load();
    if (std::abs (lg - live) > 0.002f) { live = lg; changed = true; }
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    const double dt = std::min (0.1, now - lastTime);
    lastTime = now;
    if (any)
    {
        phase += (0.12 + 0.5 * live * live) * dt;   // the orbit speeds up with GRAVITY
        jitter += dt;
    }
    buildPairs();
    if (any || changed) repaint();
}

Rectangle<float> GravityView::pairBox (int pair) const
{
    const float w = 52.0f, h = 40.0f;
    return { pair == 0 ? 6.0f : (float) getWidth() - w - 6.0f, 4.0f, w, h };
}

// a voice against its partner: a slowly turning ellipse in STILL, trembling in ORBIT, torn apart in TURBULENCE
void GravityView::buildPairs()
{
    const int N = ResoOpusProcessor::scopeSize, mask = N - 1, len = 700;
    const int end = proc.scopeWrite.load (std::memory_order_acquire);
    for (int pr = 0; pr < 2; ++pr)
    {
        auto& path = pairs[pr];
        path.clear();
        const int a = pr * 2, b = a + 1;
        if (proc.liveLevel[a].load() + proc.liveLevel[b].load() < 0.002f) continue;
        const auto box = pairBox (pr).reduced (3.0f);
        const float* xa = proc.scope[a];
        const float* xb = proc.scope[b];
        for (int i = 0; i < len; ++i)
        {
            const int k = (end - len + i) & mask;
            const float x = box.getCentreX() + jlimit (-1.0f, 1.0f, xa[k] * 1.8f) * box.getWidth() * 0.5f;
            const float y = box.getCentreY() - jlimit (-1.0f, 1.0f, xb[k] * 1.8f) * box.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
    }
}

void GravityView::paint (Graphics& g)
{
    const auto c = centre();
    const float R = 58.0f;
    static constexpr float a0 = -2.35619449f, sweep = 4.71238898f;   // -135 deg .. +135 deg (static: lambdas use them without capture, MSVC)
    auto ang = [] (float v01) { return a0 + v01 * sweep; };
    auto at = [&c] (float r, float a) { return Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };
    const Colour zoneCol[] = { Palette::zoneStill, Palette::zoneOrbit, Palette::zoneTurb, Palette::zoneCollapse };
    const float edges[] = { 0.0f, 0.25f, 0.55f, 0.85f, 1.0f };
    const int zone = Params::gravityZoneIndex (live);

    if (dropHover)
    {
        g.setColour (Palette::curve.withAlpha (0.15f));
        g.fillEllipse (Rectangle<float> (R * 2.0f + 70.0f, R * 2.0f + 70.0f).withCentre (c));
    }
    // the two pairs as X / Y figures in the top corners
    for (int pr = 0; pr < 2; ++pr)
    {
        const auto box = pairBox (pr);
        g.setColour (Colour (0xff111216));
        g.fillRoundedRectangle (box, 4.0f);
        const auto col = Palette::voiceType (types[pr * 2] != VP::Off ? types[pr * 2] : types[pr * 2 + 1]);
        if (! pairs[pr].isEmpty())
        {
            g.saveState();
            g.reduceClipRegion (box.toNearestInt());
            g.setColour (col.withAlpha (0.75f));
            g.strokePath (pairs[pr], PathStrokeType (1.0f));
            g.restoreState();
        }
        g.setColour (Palette::textFaint);
        g.setFont (Fonts::bold (8.5f));
        g.drawText (pr == 0 ? "1-2" : "3-4", box.reduced (3.0f, 1.0f), Justification::bottomRight, false);
    }
    // zone arcs (dim) + labels
    for (int z = 0; z < 4; ++z)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, R + 30.0f, R + 30.0f, 0.0f, ang (edges[z]) + 0.02f, ang (edges[z + 1]) - 0.02f, true);
        g.setColour (zoneCol[z].withAlpha (0.22f));
        g.strokePath (arc, PathStrokeType (5.0f));
        static const char* names[] = { "STILL", "ORBIT", "TURBULENCE", "COLLAPSE" };
        const auto lp = at (R + 58.0f, ang ((edges[z] + edges[z + 1]) * 0.5f));
        g.setColour (z == zone ? zoneCol[z] : Palette::textFaint);
        g.setFont (Fonts::bold (10.0f).withExtraKerningFactor (0.06f));
        g.drawText (names[z], Rectangle<float> (90.0f, 14.0f).withCentre (lp), Justification::centred, false);
    }
    // value arc coloured by zone
    for (int z = 0; z < 4; ++z)
    {
        if (value <= edges[z]) break;
        Path arc;
        arc.addCentredArc (c.x, c.y, R + 30.0f, R + 30.0f, 0.0f, ang (edges[z]) + 0.02f, ang (std::min (value, edges[z + 1])) - 0.005f, true);
        g.setColour (zoneCol[z].withAlpha (0.18f));
        g.strokePath (arc, PathStrokeType (14.0f));
        g.setColour (zoneCol[z]);
        g.strokePath (arc, PathStrokeType (5.0f));
    }
    // ticks
    for (int i = 0; i <= 20; ++i)
    {
        const float a = ang ((float) i / 20.0f);
        g.setColour (Colours::white.withAlpha (i % 5 ? 0.18f : 0.45f));
        g.drawLine (Line<float> (at (R + 21.0f, a), at (R + (i % 5 ? 25.0f : 26.5f), a)), 1.1f);
    }
    // modulation ring: from the knob to the live value
    if (std::abs (live - value) > 0.004f)
    {
        Path ring;
        ring.addCentredArc (c.x, c.y, R + 38.0f, R + 38.0f, 0.0f, ang (std::min (live, value)), ang (std::max (live, value)), true);
        g.setColour (Palette::text.withAlpha (0.9f));
        g.strokePath (ring, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (at (R + 38.0f, ang (live))));
    }

    // orbit: the voices pull towards their partner (pairs), then towards each other (ring), then scatter (collapse)
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (Rectangle<float> ((R + 13.0f) * 2.0f, (R + 13.0f) * 2.0f).withCentre (c), 1.0f);
    const float pairPull = jlimit (0.0f, 1.0f, (live - 0.2f) / 0.35f), chaos = jlimit (0.0f, 1.0f, (live - 0.8f) / 0.2f);
    // TURBULENCE shakes the orbit along one (vertical) axis; COLLAPSE scatters it in every direction
    const float shake = jlimit (0.0f, 1.0f, (live - 0.55f) / 0.3f) * (1.0f - chaos);
    for (int v = 0; v < Params::numVoices; ++v)
    {
        const double base = phase + v * MathConstants<double>::halfPi;
        const double toward = (v % 2 == 0 ? 0.55 : -0.55) * pairPull;   // partner sits a quarter turn away
        const double wob = chaos * 0.6 * std::sin (jitter * (3.1 + v * 1.7)) ;
        const float rr = R + 13.0f + chaos * 9.0f * (float) std::sin (jitter * (5.3 + v * 2.1));
        const auto p = at (rr, (float) (base + toward + wob)).translated (0.0f, shake * 8.0f * (float) std::sin (jitter * (8.0 + v * 1.3)));
        const auto col = Palette::voiceType (types[v]);
        const float lv = jlimit (0.0f, 1.0f, levels[v] * 1.4f);
        const float r = 4.5f + 2.5f * lv;
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

    // knob body: knurled skirt + brushed metal cap + pointer
    const auto body = Rectangle<float> ((R - 1.5f) * 2.0f, (R - 1.5f) * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (Colour (0xff34353c), c.x - R * 0.2f, c.y - R * 0.5f, Colour (0xff0d0e11), c.x + R * 0.4f, c.y + R, true));
    g.fillEllipse (body);
    g.setColour (Colours::black);
    g.drawEllipse (body, 1.0f);
    const float dashes[] = { 1.0f, 2.6f };
    Path skirt;
    skirt.addEllipse (body.reduced (3.0f));
    Path dashed;
    PathStrokeType (2.0f).createDashedStroke (dashed, skirt, dashes, 2);
    g.setColour (Colours::white.withAlpha (0.09f));
    g.fillPath (dashed);
    const auto cap = Rectangle<float> (R * 1.04f, R * 1.04f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xffd8d9dd), c.x - R * 0.2f, c.y - R * 0.35f, Colour (0xff3c3d43), c.x + R * 0.3f, c.y + R * 0.6f, true));
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);
    g.setColour (Colours::white);
    g.drawLine (Line<float> (at (R * 0.15f, ang (value)), at (R - 6.0f, ang (value))), 3.4f);

    // value
    g.setColour (zoneCol[Params::gravityZoneIndex (value)]);
    g.setFont (Fonts::mono (12.0f));
    g.drawText (Params::format (*Params::findDef ("gravity"), value * 100.0f), Rectangle<float> (220.0f, 16.0f).withCentre ({ c.x, c.y + R + 62.0f }),
                Justification::centred, false);
    if (editor == nullptr && std::abs (live - value) > 0.004f)
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::mono (10.5f));
        g.drawText ("live " + String (roundToInt (live * 100.0f)) + " %", Rectangle<float> (220.0f, 14.0f).withCentre ({ c.x, c.y + R + 78.0f }),
                    Justification::centred, false);
    }
}

void GravityView::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (e.mods.isCommandDown()) { attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); return; }
    dragStart = value;
    attachment->beginGesture();
}

void GravityView::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isCommandDown()) return;
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float v = jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / 260.0f * fine);
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (v));
}

void GravityView::mouseUp (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && ! e.mods.isCommandDown()) attachment->endGesture();
}

void GravityView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float step = (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f);
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (jlimit (0.0f, 1.0f, value + step)));
}

void GravityView::mouseDoubleClick (const MouseEvent&)
{
    editor = std::make_unique<TextEditor>();
    addAndMakeVisible (*editor);
    editor->setBounds (Rectangle<int> (90, 22).withCentre (centre().translated (0.0f, 58.0f + 62.0f).toInt()));
    editor->setFont (Fonts::mono (12.0f));
    editor->setJustification (Justification::centred);
    editor->setText (String (roundToInt (value * 100.0f)), false);
    editor->selectAll();
    editor->grabKeyboardFocus();
    SafePointer<GravityView> safe (this);
    editor->onReturnKey = [safe]
    {
        if (safe == nullptr) return;
        const float v = safe->editor->getText().retainCharacters ("0123456789.").getFloatValue();
        safe->attachment->setValueAsCompleteGesture (jlimit (0.0f, 100.0f, v));
        MessageManager::callAsync ([safe] { if (safe) safe->editor.reset(); });
    };
    editor->onEscapeKey = [safe] { MessageManager::callAsync ([safe] { if (safe) safe->editor.reset(); }); };
    editor->onFocusLost = editor->onEscapeKey;
}

void GravityView::showMenu()
{
    PopupMenu m, mods;
    m.addSectionHeader ("GRAVITY");
    m.addItem (1, "Reset to default");
    const int path = roundToInt (proc.getParamReal ("gravity_path"));
    PopupMenu pm;
    pm.addItem (11, "Auto - pairs, then one ring from the middle", true, path == 0);
    pm.addItem (12, "Pairs - 1-2 and 3-4 pull on each other", true, path == 1);
    pm.addItem (13, "Ring - all four in one loop", true, path == 2);
    m.addSubMenu ("Path", pm);
    for (int s = 1; s < Mod::numSources; ++s) mods.addItem (1000 + s, Mod::sourceNames()[s]);
    m.addSubMenu ("Modulate with", mods);
    for (int slot : proc.slotsForDestination ("gravity"))
        m.addItem (6000 + slot, "Remove " + Mod::sourceNames()[(int) proc.getParamReal (Params::modID (slot, "src"))]);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe = SafePointer<GravityView> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r == 1) safe->attachment->setValueAsCompleteGesture (safe->param->convertFrom0to1 (safe->param->getDefaultValue()));
        else if (r >= 11 && r <= 13) pr.setParamReal ("gravity_path", (float) (r - 11));
        else if (r >= 6000) pr.removeModulation (r - 6000);
        else if (r >= 1000) pr.addModulation (r - 1000, "gravity", 0.25f);
    });
}

bool GravityView::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("src:"); }

void GravityView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    proc.addModulation (d.description.toString().fromFirstOccurrenceOf ("src:", false, false).getIntValue(), "gravity", 0.25f);
    repaint();
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
        col->tune = knob (V + "tune", "TUNE", Palette::voiceType (voiceType (v)), range ("C0", "C8"));
        put (col->tune, c1, 34);
        col->character[1] = knob (V + "tide", "TIDE", Palette::choir, range ("0", "MAX"));
        col->character[2] = knob (V + "siren_fm", "FM", Palette::siren, range ("0", "100"));
        col->character[3] = knob (V + "sharp", "SHARP", Palette::pulsar, range ("~tri", "~square"));
        col->character[4] = knob (V + "q_index", "INDEX", Palette::quasar, range ("~sine", "~fm"));
        col->character[5] = knob (V + "n_pos", "POSITION", Palette::nebula, range ("0", "100"));
        for (int t = 1; t < 6; ++t) put (col->character[t], c2, 34);
        put (knob (V + "hold", "HOLD", Palette::group, range ("0", "100")), c1, 146);
        put (knob (V + "pan", "PAN", Palette::group, range ("L", "R", true)), c2, 146);
        col->vol = new VolBar (proc, v);
        owned.add (col->vol);
        col->addAndMakeVisible (col->vol);
        col->vol->setBounds (12, gh - 44, sw - 24, 28);
        col->controls.add (col->vol);
        col->scope = new VoiceScope (proc, v);
        owned.add (col->scope);
        col->addAndMakeVisible (col->scope);
        col->scope->setBounds (12, 250, sw - 24, 54);
        col->controls.add (col->scope);
        columns[v] = col;
    }

    // GRAVITY
    auto* gpanel = panel ("GRAVITY", Palette::gravity, { gw + gap, 0, width - 2 * (gw + gap), gh }, -1, "PATH AUTO");
    gravityPanel = gpanel;
    gpanel->onRightClick = [this] (Component& c)
    {
        PopupMenu m;
        const int cur = roundToInt (proc.getParamReal ("gravity_path"));
        m.addSectionHeader ("PATH: who pulls on whom");
        m.addItem (1, "Auto: pairs 1-2 and 3-4, then one ring", true, cur == 0);
        m.addItem (2, "Pairs: always 1-2 and 3-4", true, cur == 1);
        m.addItem (3, "Ring: always 1-2-3-4-1", true, cur == 2);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&c).withTargetScreenArea (c.localAreaToGlobal (gravityPanel->rightButtonArea().toNearestInt())),
                         [this] (int r) { if (r > 0) proc.setParamReal ("gravity_path", (float) (r - 1)); });
    };
    gravity = new GravityView (proc);
    owned.add (gravity);
    gravity->setBounds (0, ModulePanel::headerHeight + 2, gpanel->getWidth(), gh - ModulePanel::headerHeight - 2);
    gpanel->addAndMakeVisible (gravity);

    // MASTER: transpose all, DUST, output volume; FILL in the header
    const int mw = 254, dh = height - gh - gap;
    auto* master = new MasterPanel (*this);
    owned.add (master);
    masterPanel = master;
    master->setBounds (width - mw, gh + gap, mw, dh);
    addAndMakeVisible (master);
    const char* mIds[] = { "pitch", "dust", "grit", "master_vol" };
    const char* mNames[] = { "PITCH", "DUST", "GRIT", "VOLUME" };
    // the titles of every control in the detail and MASTER panels sit on one line (switches, knobs, small knobs)
    const int titleY = 42;
    for (int i = 0; i < 4; ++i)
    {
        Knob::Style st;
        st.small = true;
        st.width = 62;
        st.bipolar = i == 0;
        auto* k = knob (mIds[i], mNames[i], Palette::master, st);
        master->addAndMakeVisible (k);
        k->setTopLeftPosition (3 + i * 62, titleY);
    }

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
        auto* type = new LedRadio (proc, V + "type", "TYPE", { "OFF", "CHOIR", "SIREN", "PULSAR", "QUASAR", "NEBULA" });
        type->setIcons ({ "flat", "chord", "siren", "morph", "fm", "table" },
                        { Palette::voiceType (0), Palette::choir, Palette::siren, Palette::pulsar, Palette::quasar, Palette::nebula });
        owned.add (type);
        detailPanel->addChildComponent (type);
        type->setTopLeftPosition (16, titleY);
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
        add (d.chord, 150, titleY, VP::Choir);
        for (int i = 0; i < 5; ++i)
        {
            auto* k = knob (V + "int" + String (i + 1), "OSC " + String (i + 1), Palette::choir, smallStyle());
            add (k, 262 + i * 74, titleY, VP::Choir);
            auto* led = new LedDot (proc, V + "on" + String (i + 1));
            owned.add (led);
            add (led, 262 + i * 74 + 12, titleY, VP::Choir);
        }
        // SIREN
        const char* sirenIds[] = { "siren_rate", "siren_am", "siren_div", "siren_noise", "siren_wave" };
        const char* sirenNames[] = { "RATE", "AM", "DIVIDER", "NOISE", "WAVE" };
        const char* sirenLo[] = { ".05", "0", "OFF", "0", "~square" };
        const char* sirenHi[] = { "300", "100", "1:16", "100", "~tri" };
        for (int i = 0; i < 5; ++i)
            add (knob (V + sirenIds[i], sirenNames[i], Palette::siren, range (sirenLo[i], sirenHi[i])), 150 + i * 96, titleY, VP::Siren);
        // PULSAR
        const char* pulsarIds[] = { "sym", "bloom", "twin", "twin_mix", "sub", "vibrato" };
        const char* pulsarNames[] = { "SYMMETRY", "BLOOM", "TWIN", "BLEND", "SUB", "VIBRATO" };
        const char* pulsarLo[] = { "~pulse", "0", "-12", "0", "0", "0" };
        const char* pulsarHi[] = { "~pulsew", "100", "+24", "100", "100", "50c" };
        for (int i = 0; i < 6; ++i)
            add (knob (V + pulsarIds[i], pulsarNames[i], Palette::pulsar, range (pulsarLo[i], pulsarHi[i], i == 0, true)), 150 + i * 80, titleY, VP::Pulsar);
        // QUASAR
        const char* qIds[] = { "q_ratio", "q_fine", "q_fb", "q_track" };
        const char* qNames[] = { "RATIO", "FINE", "FEEDBACK", "TRACK" };
        const char* qLo[] = { ".5", "-50c", "~sine", "0" };
        const char* qHi[] = { "8", "+50c", "~noise", "100" };
        for (int i = 0; i < 4; ++i)
            add (knob (V + qIds[i], qNames[i], Palette::quasar, range (qLo[i], qHi[i], i == 1)), 150 + i * 96, titleY, VP::Quasar);
        // NEBULA
        auto* table = new LedRadio (proc, V + "n_table", "TABLE", { "VOCAL", "ORGAN", "GLASS" });
        owned.add (table);
        add (table, 150, titleY, VP::Nebula);
        add (knob (V + "n_scan", "SCAN", Palette::nebula, range ("-", "+", true)), 262, titleY, VP::Nebula);
        add (knob (V + "n_drift", "DRIFT", Palette::nebula, range ("0", "100")), 358, titleY, VP::Nebula);

        // every type that sounds: CHARACTER (digital - clean - analog), ATTACK, RELEASE
        auto* div = new Component();
        owned.add (div);
        div->setSize (1, 110);
        detailPanel->addChildComponent (div);
        div->setTopLeftPosition (642, 40);
        d.all.add (div);
        Component* common[] = { div, knob (V + "character", "CHARACTER", Palette::group, range ("DIGI", "ANLG", true, true)),
                                knob (V + "attack", "ATTACK", Palette::group, range ("1ms", "30s", false, true)),
                                knob (V + "release", "RELEASE", Palette::group, range ("1ms", "30s", false, true)) };
        for (int i = 1; i < 4; ++i)
        {
            detailPanel->addChildComponent (common[i]);
            d.all.add (common[i]);
            common[i]->setTopLeftPosition (648 + (i - 1) * 82, titleY);
        }
        for (auto* c : common)
            for (int t = VP::Choir; t <= VP::Nebula; ++t) d.byType[t].add (c);
    }

    offNote = new Label ({}, "This voice is off. Pick a type on the left to switch it on.");
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

bool DronePage::VolBar::refresh (float level, float hold)
{
    const float v = vol->getValue(), pn = pan->convertFrom0to1 (pan->getValue());
    if (std::abs (v - shownVol) > 0.002f || std::abs (pn - shownPan) > 0.004f || std::abs (level - shownLevel) > 0.02f || std::abs (hold - shownHold) > 0.002f)
    {
        shownVol = v; shownPan = pn; shownLevel = level; shownHold = hold;
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
    if (shownHold > 0.001f)   // HOLD: where the level rests without a touch
    {
        const float hx = w * jlimit (0.0f, 1.0f, shownHold * shownVol * 1.1f);
        g.setColour (Palette::label.withAlpha (0.8f));
        g.fillRect (hx - 0.75f, 22.0f, 1.5f, 7.0f);
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

DronePage::VoiceScope::VoiceScope (ResoOpusProcessor& p, int v) : proc (p), voice (v)
{
    setTooltip ("The signal of this voice (two periods of its note, height = level). Click: spectrum / scope");
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void DronePage::VoiceScope::mouseDown (const MouseEvent&)
{
    spectrumMode = ! spectrumMode;
    for (auto& f : frames) f.clear();
    silent = false;
    repaint();
}

void DronePage::VoiceScope::update (float level)
{
    if (level < 0.0008f)
    {
        if (silent) return;   // nothing to redraw while the voice is silent
        silent = true;
        for (auto& f : frames) f.clear();
        repaint();
        return;
    }
    silent = false;
    const int N = ResoOpusProcessor::scopeSize, mask = N - 1;
    const int end = proc.scopeWrite.load (std::memory_order_acquire);
    const float* buf = proc.scope[voice];
    const float w = (float) getWidth(), h = (float) getHeight();
    for (int i = 3; i > 0; --i) frames[i] = frames[i - 1];
    Path& p = frames[0];
    p.clear();
    if (! spectrumMode)
    {
        // two periods of the root, starting on a rising zero crossing (the picture stands still)
        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const double root = proc.liveRoot[voice].load();
        const double a4 = proc.getParamReal ("a4");
        const double hz = root > 0.0 ? a4 * std::exp2 ((root - 69.0) / 12.0) : 110.0;
        const int period = jlimit (8, 1400, (int) std::lround (sr / hz));
        const int span = 2 * period;
        int start = end - span - period;
        float best = 0.0f;
        for (int i = end - span - period; i < end - span; ++i)
        {
            const float a = buf[(i - 1) & mask], b = buf[i & mask];
            if (a < 0.0f && b >= 0.0f && b - a > best) { best = b - a; start = i; }
        }
        const float gain = 0.42f * h / 0.55f;
        for (int x = 0; x <= (int) w; ++x)
        {
            const double pos = start + (double) x / w * span;
            const int i0 = (int) std::floor (pos);
            const float f = (float) (pos - i0);
            const float v = buf[i0 & mask] + f * (buf[(i0 + 1) & mask] - buf[i0 & mask]);
            const float y = jlimit (2.0f, h - 2.0f, h * 0.5f - v * gain);
            if (x == 0) p.startNewSubPath (0.0f, y); else p.lineTo ((float) x, y);
        }
    }
    else
    {
        // spectrum of the last 2048 samples, 30 Hz - 12 kHz on a log scale, 70 dB tall
        const int M = 2048;
        for (int i = 0; i < M; ++i)
        {
            const float win = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (M - 1));
            fftData[(size_t) i] = buf[(end - M + i) & mask] * win;
        }
        std::fill (fftData.begin() + M, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const double binHz = sr / M;
        p.startNewSubPath (0.0f, h);
        for (int x = 0; x <= (int) w; x += 2)
        {
            const double f = 30.0 * std::pow (400.0, (double) x / w);
            const int b = jlimit (1, M / 2 - 1, (int) std::lround (f / binHz));
            const float mag = std::max (fftData[(size_t) b], fftData[(size_t) b + 1]) / (float) (M / 4);
            const float db = Decibels::gainToDecibels (mag, -90.0f);
            p.lineTo ((float) x, jlimit (1.0f, h, h - (db + 70.0f) / 70.0f * h));
        }
        p.lineTo (w, h);
        p.closeSubPath();
    }
    repaint();
}

void DronePage::VoiceScope::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff111216));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (getHeight() / 2, 3.0f, r.getRight() - 3.0f);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));
    if (spectrumMode)
    {
        if (! frames[0].isEmpty())
        {
            g.setColour (colour.withAlpha (0.22f));
            g.fillPath (frames[0]);
            g.setColour (colour);
            g.strokePath (frames[0], PathStrokeType (1.1f));
        }
    }
    else
    {
        const float alpha[4] = { 1.0f, 0.3f, 0.15f, 0.07f };
        for (int i = 3; i >= 0; --i)
            if (! frames[i].isEmpty())
            {
                g.setColour (colour.withAlpha (alpha[i]));
                g.strokePath (frames[i], PathStrokeType (i == 0 ? 1.5f : 1.2f, PathStrokeType::curved, PathStrokeType::rounded));
            }
    }
    g.restoreState();
    g.setColour (Palette::textFaint);
    g.setFont (Fonts::bold (8.5f));
    g.drawText (spectrumMode ? "SPECTRUM" : "SCOPE", r.reduced (5.0f, 3.0f), Justification::topRight, false);
}

void DronePage::refreshLive()
{
    Page::refreshLive();
    {
        // FILL shows the scale the voices sit on (until one of them is retuned by hand)
        static const char* shortNames[] = { "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD", "HARM MIN",
                                             "PENTA MAJ", "PENTA MIN", "WHOLE TONE", "HIRAJOSHI", "DBL HARM" };
        const int scale = jlimit (0, 11, roundToInt (proc.getParamReal ("snap_scale"))), root = jlimit (0, 11, roundToInt (proc.getParamReal ("snap_root")));
        const String fill = proc.fillMatches (scale, root) ? Params::findDef ("snap_root")->choices[root] + " " + shortNames[scale] : String ("FILL");
        if (fill != shownFill) { shownFill = fill; masterPanel->setRightText (fill); }
    }
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
            c->tune->setKnobColour (col);
            for (int k = 1; k < 6; ++k) c->character[k]->setVisible (k == t || (t == VP::Off && k == VP::Choir));
            for (auto* ctl : c->controls) ctl->setAlpha (t == VP::Off ? 0.35f : 1.0f);
            for (int k = 1; k < 6; ++k) c->character[k]->setAlpha (t == VP::Off ? 0.35f : 1.0f);
            c->repaint();
        }
        c->setLiveValue (t == VP::Off ? 0.0f : jlimit (0.0f, 1.0f, lv * 1.5f));
        const float hold = t == VP::Off ? 0.0f : proc.getParamReal (Params::voiceID (v, "hold")) / 100.0f;
        c->vol->refresh (lv, hold);
        c->scope->colour = Palette::voiceType (t);
        c->scope->update (t == VP::Off ? 0.0f : lv);
    }
    gravity->refresh();
    const int path = roundToInt (proc.getParamReal ("gravity_path"));
    if (path != shownPath)
    {
        shownPath = path;
        static const char* pathNames[] = { "PATH AUTO", "PATH PAIRS", "PATH RING" };
        gravityPanel->setRightText (pathNames[jlimit (0, 2, path)]);
    }
    // DRONE off (LEAD only): the whole page is dimmed
    const float pageAlpha = proc.getParamReal ("drone_on") > 0.5f ? 1.0f : 0.45f;
    for (auto* ch : getChildren())
        if (std::abs (ch->getAlpha() - pageAlpha) > 0.01f) ch->setAlpha (pageAlpha);

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

//==============================================================================
namespace
{
const juce::StringArray& filterModeIcons() { static const juce::StringArray i { "lp", "bp", "hp" }; return i; }
juce::Array<juce::Colour> same (juce::Colour c, int n) { juce::Array<juce::Colour> a; for (int i = 0; i < n; ++i) a.add (c); return a; }
const char* slopeText (int model)
{
    static const char* slopes[] = { "GROWL 12 dB", "LADDER 24 dB", "DIODE 24 dB", "SCREAM 12 dB", "SILK 12 dB", "COMB", "VOWEL" };
    return slopes[juce::jlimit (0, 6, model)];
}
}

FxPage::FxPage (ResoOpusProcessor& p) : Page (p)
{
    auto sm = [] (String lo, String hi, bool bi = false) { return range (lo, hi, bi, true); };
    const int rowH = 265, gap = 10, y2 = rowH + gap;

    // FILTER A | ROUTING (what the filters do, live) | FILTER B
    const int side = 360, mid = width - 2 * side - 16;
    filterPanel = panel ("FILTER A", Palette::filter, { 0, 0, side, rowH }, -1, "GROWL 12 dB");
    auto* model = new LedRadio (proc, "flt_model", "MODEL", { "GROWL", "LADDER", "DIODE", "SCREAM", "SILK", "COMB", "VOWEL" });
    model->setTooltip (Params::findDef ("flt_model")->tip);
    owned.add (model);
    auto* mode = new LedRadio (proc, "flt_mode", "MODE", { "LOW", "BAND", "HIGH" });
    mode->setIcons (filterModeIcons(), same (Palette::filter, 3));
    owned.add (mode);
    model->setColumns (2);
    filterPanel->setGrid ({ 1.0f, 1.0f, 1.0f, 1.0f }, {
        knob ("flt_cutoff", "CUTOFF", Palette::filter, sm ("20", "20k")),
        knob ("flt_res", "RESONANCE", Palette::filter, sm ("0", "10")),
        knob ("flt_drive", "DRIVE", Palette::filter, sm ("0", "10")),
        knob ("flt_spread", "SPREAD", Palette::filter, sm ("0", "100")),
        model, nullptr, mode, nullptr });
    filterPanel->setSpan (model, 2);
    filterPanel->setSpan (mode, 2);

    routingPanel = panel ("ROUTING", Palette::label, { side + 8, 0, mid, rowH }, -1, "B OFF");
    filterView = new FilterRoutingView (proc);
    owned.add (filterView);
    filterView->setSize (mid - 12, rowH - ModulePanel::headerHeight - 12);
    routingPanel->setGrid ({ 1.0f }, { filterView });

    // FILTER B: its own colour, the same as its curve in ROUTING
    filterBPanel = panel ("FILTER B", Palette::filterB, { width - side, 0, side, rowH }, -1, "SILK 12 dB");
    auto* modelB = new LedRadio (proc, "flt2_model", "MODEL", { "GROWL", "LADDER", "DIODE", "SCREAM", "SILK", "COMB", "VOWEL" });
    owned.add (modelB);
    auto* modeB = new LedRadio (proc, "flt2_mode", "MODE", { "LOW", "BAND", "HIGH" });
    modeB->setIcons (filterModeIcons(), same (Palette::filterB, 3));
    owned.add (modeB);
    auto* onB = static_cast<Component*> (owned.add (new LedButton (proc, "flt2_on", "ON")));
    auto* cutB = knob ("flt2_cutoff", "CUTOFF", Palette::filterB, sm ("20", "20k"));
    auto* resB = knob ("flt2_res", "RESONANCE", Palette::filterB, sm ("0", "10"));
    auto* drvB = knob ("flt2_drive", "DRIVE", Palette::filterB, sm ("0", "10"));
    modelB->setColumns (2);
    filterBPanel->setGrid ({ 1.0f, 1.0f, 1.0f, 1.0f }, { onB, cutB, resB, drvB, modelB, nullptr, modeB, nullptr });
    filterBPanel->setSpan (modelB, 2);
    filterBPanel->setSpan (modeB, 2);
    filterBControls = { cutB, resB, drvB, modelB, modeB };

    // ECHO
    // ECHO | MOTION | SPACE
    const int third = (width - 16) / 3;
    echoPanel = panel ("ECHO", Palette::mixer, { 0, y2, third, rowH }, -1, "DUAL");
    echoSrc = new LedRadio (proc, "echo_self", "MOD SOURCE", { "SELF", "SINE" });
    owned.add (echoSrc);
    echoKnobs[0] = knob ("echo_time1", "TIME 1", Palette::mixer, sm ("5ms", "2s"));
    echoKnobs[1] = knob ("echo_time2", "TIME 2", Palette::mixer, sm ("5ms", "2s"));
    echoKnobs[2] = knob ("echo_mod", "MOD", Palette::mixer, sm ("0", "100"));
    auto* echoMode = new LedRadio (proc, "echo_mode", "MODE", { "DUAL", "TAPE", "PING-PONG", "LO-FI", "MOD" });
    owned.add (echoMode);
    echoPanel->setGrid ({ 1.45f, 1.0f, 1.0f, 1.0f }, {
        echoMode, echoKnobs[0], echoKnobs[1], knob ("echo_fb", "FEEDBACK", Palette::mixer, sm ("0", "MAX")),
        echoSrc, echoKnobs[2], knob ("echo_mix", "MIX", Palette::mixer, sm ("DRY", "WET")),
        knob ("echo_voices", "TO VOICES", Palette::mixer, sm ("0", "100")) });
    // in tempo: the note values take the place of the TIME knobs; SYNC sits between the two
    for (int i = 0; i < 2; ++i)
    {
        echoDivs[i] = knob (i == 0 ? "echo_div1" : "echo_div2", i == 0 ? "TIME 1" : "TIME 2", Palette::mixer, sm ("1/32", "1 bar"));
        echoPanel->addAndMakeVisible (echoDivs[i]);
        echoDivs[i]->setVisible (false);
    }
    echoSync = new MiniToggle (proc, "echo_sync", "SYNC");
    owned.add (echoSync);
    echoPanel->addAndMakeVisible (echoSync);
    echoPanel->onLayout = [this]
    {
        for (int i = 0; i < 2; ++i)
            echoDivs[i]->setTopLeftPosition (echoKnobs[i]->getBounds().getCentreX() - echoDivs[i]->getWidth() / 2, echoKnobs[i]->getY());
        echoSync->setCentrePosition ((echoKnobs[0]->getBounds().getCentreX() + echoKnobs[1]->getBounds().getCentreX()) / 2, echoKnobs[0]->getY() + 48);
    };
    echoPanel->onLayout();

    // MOTION: movement in space between the filters and ECHO
    motionPanel = panel ("MOTION", Palette::motion, { third + 8, y2, third, rowH }, -1, "OFF");
    auto* motMode = new LedRadio (proc, "mot_mode", "MODE", { "ORBIT", "PHASER", "ENSEMBLE", "TREMOLO", "ROTOR" });
    owned.add (motMode);
    auto* motOn = static_cast<Component*> (owned.add (new LedButton (proc, "mot_on", "ON")));
    motSync = static_cast<Component*> (owned.add (new LedButton (proc, "mot_sync", "SYNC")));
    motRate = knob ("mot_rate", "RATE", Palette::motion, sm (".02", "10"));
    motDiv = knob ("mot_div", "DIVISION", Palette::motion, sm ("8b", "1/8"));
    motX = knob ("mot_x", "WIDTH", Palette::motion, sm ("0", "100"));
    auto* motDepth = knob ("mot_depth", "DEPTH", Palette::motion, sm ("0", "100"));
    auto* motMix = knob ("mot_mix", "MIX", Palette::motion, sm ("DRY", "WET"));
    motionPanel->setGrid ({ 1.3f, 1.0f, 1.0f, 1.0f }, { motMode, motRate, motDiv, motDepth, motOn, motSync, motX, motMix });
    motionControls = { motMode, motRate, motDiv, motDepth, motSync, motX, motMix };

    // SPACE
    spacePanel = panel ("SPACE", Palette::fx2, { 2 * (third + 8), y2, width - 2 * (third + 8), rowH }, -1, "VOID");
    auto* algo = new LedRadio (proc, "space_algo", "ALGORITHM", { "AURORA", "VOID", "RUNNER", "SATURN", "COMET" });
    algo->setTooltip (Params::findDef ("space_algo")->tip);
    owned.add (algo);
    xyz[0] = knob ("space_x", "X", Palette::fx2, sm ("0", "100"));
    xyz[1] = knob ("space_y", "Y", Palette::fx2, sm ("0", "100"));
    xyz[2] = knob ("space_z", "Z", Palette::fx2, sm ("0", "100"));
    spacePanel->setGrid ({ 1.25f, 1.0f, 1.0f }, { algo, xyz[0], xyz[1], nullptr, xyz[2], knob ("space_blend", "BLEND", Palette::fx2, sm ("DRY", "WET")) });

    refreshLive();
}

void FxPage::refreshLive()
{
    Page::refreshLive();
    const int fm = roundToInt (proc.getParamReal ("flt_model"));
    if (fm != shownModel) { shownModel = fm; filterPanel->setRightText (slopeText (fm)); }
    const int fmB = roundToInt (proc.getParamReal ("flt2_model"));
    if (fmB != shownModelB) { shownModelB = fmB; filterBPanel->setRightText (slopeText (fmB)); }
    const bool bOn = proc.getParamReal ("flt2_on") > 0.5f;
    for (auto* c : filterBControls)
        if (std::abs (c->getAlpha() - (bOn ? 1.0f : 0.4f)) > 0.01f) c->setAlpha (bOn ? 1.0f : 0.4f);
    if (isVisible()) filterView->update();
    const int r = bOn ? roundToInt (proc.getParamReal ("flt_route")) : -1;
    if (r != shownRoute)
    {
        shownRoute = r;
        routingPanel->setRightText (r < 0 ? String ("B OFF") : Params::findDef ("flt_route")->choices[jlimit (0, 3, r)].toUpperCase());
    }
    {
        const bool sync = proc.getParamReal ("echo_sync") > 0.5f;
        for (int i = 0; i < 2; ++i)
            if (echoDivs[i]->isVisible() != sync) { echoDivs[i]->setVisible (sync); echoKnobs[i]->setVisible (! sync); }
    }
    const int em = roundToInt (proc.getParamReal ("echo_mode"));
    if (em != shownEcho)
    {
        // the same knobs, named for what they do in this mode
        shownEcho = em;
        static const char* names[5][3] = { { "TIME 1", "TIME 2", "MOD" }, { "HEAD 1", "HEAD 2", "WOW" }, { "TIME L", "TIME R", "MOD" },
                                           { "TIME 1", "TIME 2", "DEGRADE" }, { "TIME 1", "TIME 2", "DEPTH" } };
        const int m = jlimit (0, 4, em);
        for (int i = 0; i < 3; ++i) echoKnobs[i]->setLabel (names[m][i]);
        for (int i = 0; i < 2; ++i) echoDivs[i]->setLabel (names[m][i]);
        echoPanel->setRightText (Params::findDef ("echo_mode")->choices[m].toUpperCase());
        static const char* info[5] = { "Two lines, left and right, feeding each other.",
                                        "A tape loop with two heads: darker, wobbling repeats.",
                                        "The repeats bounce from left to right.",
                                        "A broken digital delay: fewer bits, grainy and dark.",
                                        "Chorused repeats, wide and clean." };
        echoPanel->setFooter (info[m]);
        echoSrc->setAlpha (m == 0 || m == 2 ? 1.0f : 0.35f);   // SELF / LFO only steer DUAL and PING-PONG
    }
    {
        // MOTION: dimmed while off; RATE or DIVISION by SYNC; the ROTOR takes its speed from X; X named per mode
        const bool on = proc.getParamReal ("mot_on") > 0.5f, sync = proc.getParamReal ("mot_sync") > 0.5f;
        const int mm = jlimit (0, 4, roundToInt (proc.getParamReal ("mot_mode")));
        auto alpha = [] (Component* c, float a) { if (std::abs (c->getAlpha() - a) > 0.01f) c->setAlpha (a); };
        for (auto* c : motionControls) alpha (c, on ? 1.0f : 0.4f);
        if (on)
        {
            alpha (motRate, mm == 4 ? 0.35f : (sync ? 0.35f : 1.0f));
            alpha (motDiv, mm == 4 ? 0.35f : (sync ? 1.0f : 0.35f));
            alpha (motSync, mm == 4 ? 0.35f : 1.0f);
        }
        const int key = (on ? 10 : 0) + mm;
        if (key != shownMotion)
        {
            shownMotion = key;
            static const char* xNames[5] = { "WIDTH", "FEEDBACK", "SPREAD", "STEREO", "SPEED" };
            motX->setLabel (xNames[mm]);
            motionPanel->setRightText (on ? Params::findDef ("mot_mode")->choices[mm].toUpperCase() : String ("OFF"));
            static const char* info[5] = { "The sound circles around your head.",
                                           "Sweeping notches, left and right apart.",
                                           "The triple chorus of old string machines.",
                                           "The level pulses; STEREO lets the sides alternate.",
                                           "A rotating speaker; SPEED slow or fast, with inertia." };
            motionPanel->setFooter (on ? String (info[mm]) : String ("Switch it ON for movement in space."));
        }
    }
    const int a = roundToInt (proc.getParamReal ("space_algo"));
    if (a == shownAlgo) return;
    shownAlgo = a;
    static const char* labels[5][3] = { { "OCT UP", "OCT DOWN", "DECAY" }, { "PRE-DELAY", "WOBBLE", "DECAY" },
                                        { "FEEDBACK", "TIME", "PITCH" }, { "FREQUENCY", "STEPS", "ROOM" }, { "FEEDBACK", "TIME", "WOW" } };
    for (int i = 0; i < 3; ++i) xyz[i]->setLabel (labels[jlimit (0, 4, a)][i]);
    spacePanel->setRightText (Params::findDef ("space_algo")->choices[jlimit (0, 4, a)].toUpperCase());
    static const char* info[5] = { "Airy shimmer: soft octaves drift up or down.",
                                   "A huge dark space; DECAY at the very top freezes.",
                                   "A staircase: every repeat steps by PITCH.",
                                   "Bells and bowls; STEPS jumps by pentatonic notes.",
                                   "Tape echo: saturated, darker repeats; WOW wobbles." };
    spacePanel->setFooter (info[jlimit (0, 4, a)]);
}

//==============================================================================
ModPage::ModPage (ResoOpusProcessor& p) : Page (p), matrix (p)
{
    auto sm = [] (String lo, String hi, bool bi = false) { return range (lo, hi, bi, true); };
    const int mh = 268, pw = (width - 24) / 4;
    // MOTION: LFO 1, LFO 2, RANDOM (drag the arrow in the header onto any knob)
    const Colour lfoCol[] = { Mod::sourceColour (Mod::Lfo1), Mod::sourceColour (Mod::Lfo2), Mod::sourceColour (Mod::Random) };
    for (int l = 0; l < 2; ++l)
    {
        const String P = "lfo" + String (l + 1) + "_";
        auto* lp = panel ("LFO " + String (l + 1), lfoCol[l], { l * (pw + 8), 0, pw, mh }, Mod::Lfo1 + l);
        auto* shape = new LedRadio (proc, P + "shape", "SHAPE", { "SIN", "TRI", "UP", "DOWN", "SQR", "S&H" });
        shape->setIcons ({ "sine", "tri", "rampup", "rampdown", "square", "sh" }, same (lfoCol[l], 6));
        owned.add (shape);
        rates[l] = knob (P + "rate", "RATE", lfoCol[l], sm (".01", "40"));
        divisions[l] = knob (P + "div", "DIVISION", lfoCol[l], sm ("8b", "1/8"));
        lp->setGrid ({ 1.4f, 1.0f, 1.0f }, {
            shape, rates[l], divisions[l],
            static_cast<Component*> (owned.add (new LedButton (proc, P + "sync", "SYNC"))),
            knob (P + "phase", "PHASE", lfoCol[l], sm ("0", "360")),
            static_cast<Component*> (owned.add (new LedButton (proc, P + "retrig", "RETRIG"))) });
    }
    auto* rp = panel ("RANDOM", lfoCol[2], { 2 * (pw + 8), 0, pw, mh }, Mod::Random);
    auto* rmode = new LedRadio (proc, "rnd_mode", "MODE", { "DRIFT", "STEPS", "NOISE" });
    rmode->setIcons ({ "sine", "sh", "noise" }, same (lfoCol[2], 3));
    owned.add (rmode);
    rates[2] = knob ("rnd_rate", "RATE", lfoCol[2], sm (".01", "20"));
    divisions[2] = knob ("rnd_div", "DIVISION", lfoCol[2], sm ("8b", "1/8"));
    rp->setGrid ({ 1.3f, 1.0f, 1.0f }, {
        rmode, rates[2], divisions[2],
        static_cast<Component*> (owned.add (new LedButton (proc, "rnd_sync", "SYNC"))),
        knob ("rnd_slew", "SLEW", lfoCol[2], sm ("0", "10")), nullptr });
    // MOD ENV: an envelope started by every key (plates or LEAD)
    const auto ec = Mod::sourceColour (Mod::ModEnv);
    auto* ep = panel ("MOD ENV", ec, { 3 * (pw + 8), 0, width - 3 * (pw + 8), mh }, Mod::ModEnv);
    ep->setEnvelopeIcon (4);
    ep->setGrid ({ 1.0f, 1.0f }, {
        knob ("menv_attack", "ATTACK", ec, sm ("1ms", "30s")), knob ("menv_decay", "DECAY", ec, sm ("1ms", "30s")),
        knob ("menv_sustain", "SUSTAIN", ec, sm ("0", "100")), knob ("menv_release", "RELEASE", ec, sm ("1ms", "30s")) });
    addAndMakeVisible (matrix);
    matrix.setBounds (0, mh + 10, width, height - mh - 10);
    refreshLive();
}

void ModPage::refreshLive()
{
    Page::refreshLive();
    matrix.refresh();
    const char* syncIds[] = { "lfo1_sync", "lfo2_sync", "rnd_sync" };
    for (int i = 0; i < 3; ++i)
    {
        const bool sync = proc.getParamReal (syncIds[i]) > 0.5f;
        if (std::abs (rates[i]->getAlpha() - (sync ? 0.35f : 1.0f)) > 0.01f) rates[i]->setAlpha (sync ? 0.35f : 1.0f);
        if (std::abs (divisions[i]->getAlpha() - (sync ? 1.0f : 0.35f)) > 0.01f) divisions[i]->setAlpha (sync ? 1.0f : 0.35f);
    }
}

//==============================================================================
LeadPage::LeadPage (ResoOpusProcessor& p) : Page (p)
{
    auto sm = [] (String lo, String hi, bool bi = false) { return range (lo, hi, bi, true); };
    const int rowH = 265, gap = 10, g6 = 6, y2 = rowH + gap;
    const auto c = Palette::lead;

    auto* osc = panel ("LEAD OSC", c, { 0, 0, 330, rowH });
    Knob::Style waveSt = sm ("", "");
    waveSt.marks = { "~sine", "~tri", "~shark", "~saw", "~square" };
    osc->setGrid ({ 1.0f, 1.0f, 1.0f, 1.0f }, {
        knob ("lead_wave", "WAVE", c, waveSt),
        knob ("lead_osc2", "OSC 2", c, sm ("-12", "+12", true)),
        knob ("lead_mix", "OSC 2 LEVEL", c, sm ("0", "100")),
        knob ("lead_sym", "SYMMETRY", c, sm ("~pulse", "~pulsew", true)),
        knob ("lead_octave", "OCTAVE", c, sm ("-2", "+2", true)),
        knob ("lead_sub", "SUB", c, sm ("0", "100")),
        static_cast<Component*> (owned.add (new LedButton (proc, "lead_sync", "SYNC"))), nullptr });

    filterPanel = panel ("LEAD FILTER", c, { 330 + g6, 0, 390, rowH }, -1, "LADDER 24 dB");
    auto* fmodel = new LedRadio (proc, "lead_flt_model", "MODEL", { "GROWL", "LADDER", "DIODE", "SCREAM", "SILK", "COMB", "VOWEL" });
    owned.add (fmodel);
    auto* fmode = new LedRadio (proc, "lead_flt_mode", "MODE", { "LOW", "BAND", "HIGH" });
    fmode->setIcons (filterModeIcons(), same (c, 3));
    owned.add (fmode);
    filterPanel->setGrid ({ 1.15f, 1.15f, 1.0f, 1.0f }, {
        knob ("lead_flt_cutoff", "CUTOFF", c, sm ("20", "20k")),
        knob ("lead_flt_res", "RESONANCE", c, sm ("0", "10")),
        knob ("lead_flt_env", "ENV", c, sm ("-", "+", true)),
        knob ("lead_flt_key", "KEY", c, sm ("0", "100")),
        fmodel, nullptr, fmode, nullptr });
    fmodel->setColumns (2);
    filterPanel->setSpan (fmodel, 2);
    filterPanel->setSpan (fmode, 2);

    auto* fenv = panel ("FILTER ENV", c, { 726 + g6, 0, 212, rowH }, Mod::LeadEnv);
    fenv->setGrid ({ 1.0f, 1.0f }, {
        knob ("lead_fenv_attack", "ATTACK", c, sm ("1ms", "10s")), knob ("lead_fenv_decay", "DECAY", c, sm ("1ms", "30s")),
        knob ("lead_fenv_sustain", "SUSTAIN", c, sm ("0", "100")), knob ("lead_fenv_release", "RELEASE", c, sm ("1ms", "30s")) });

    auto* env = panel ("AMP ENV", c, { 944 + g6, 0, width - 950, rowH });
    env->setEnvelopeIcon (4);
    env->setGrid ({ 1.0f, 1.0f }, {
        knob ("lead_attack", "ATTACK", c, sm ("1ms", "10s")), knob ("lead_decay", "DECAY", c, sm ("1ms", "30s")),
        knob ("lead_sustain", "SUSTAIN", c, sm ("0", "100")), knob ("lead_release", "RELEASE", c, sm ("1ms", "30s")) });

    auto* voice = panel ("LEAD VOICE", c, { 0, y2, 760, rowH });
    auto* voices = new LedRadio (proc, "lead_voices", "VOICES", { "MONO", "LEGATO", "POLY 4", "POLY 8" });
    owned.add (voices);
    voice->setGrid ({ 1.1f, 1.0f, 1.0f, 1.0f, 1.0f }, {
        voices,
        knob ("lead_glide", "GLIDE", c, sm ("0", "2s")),
        knob ("lead_vel", "VELOCITY", c, sm ("0", "100")),
        knob ("lead_vib", "VIBRATO", c, sm ("0", "1 st")),
        knob ("lead_vib_rate", "VIB RATE", c, sm (".5", "12")),
        knob ("lead_vib_delay", "VIB DELAY", c, sm ("0", "3s")),
        knob ("lead_touch_vib", "TOUCH VIB", c, sm ("0", "100")),
        knob ("lead_level", "VOLUME", c, sm ("-inf", "+6")),
        knob ("lead_pan", "PAN", c, sm ("L", "R", true)), nullptr });

    auto* arp = panel ("ARP", c, { 760 + g6, y2, width - 766, rowH });
    auto* arpMode = new LedRadio (proc, "arp_mode", "DIRECTION", { "UP", "DOWN", "PING-PONG", "RANDOM" });
    owned.add (arpMode);
    arp->setGrid ({ 1.0f, 1.0f, 1.0f }, {
        static_cast<Component*> (owned.add (new LedButton (proc, "arp_on", "ARP"))),
        static_cast<Component*> (owned.add (new LedButton (proc, "arp_latch", "LATCH"))),
        knob ("arp_rate", "RATE", c, sm ("1/4", "1/32")),
        arpMode, nullptr, knob ("arp_octaves", "OCTAVES", c, sm ("1", "4")) });
    leadPanels = { osc, filterPanel, fenv, env, voice, arp };
    refreshLive();
}

void LeadPage::refreshLive()
{
    Page::refreshLive();
    const bool leadOn = proc.getParamReal ("lead_on") > 0.5f;
    for (auto* p : leadPanels)
        if (std::abs (p->getAlpha() - (leadOn ? 1.0f : 0.45f)) > 0.01f) p->setAlpha (leadOn ? 1.0f : 0.45f);
    const int fm = roundToInt (proc.getParamReal ("lead_flt_model"));
    if (fm != shownModel) { shownModel = fm; filterPanel->setRightText (slopeText (fm)); }
}

#include "Knob.h"
#include "PluginProcessor.h"

using namespace juce;

namespace
{
constexpr float startAngle = -135.0f * MathConstants<float>::pi / 180.0f;
constexpr float sweep = 270.0f * MathConstants<float>::pi / 180.0f;

Point<float> onCircle (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

Path arcPath (Point<float> c, float r, float a0, float a1)
{
    Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
    return p;
}
}

Knob::Knob (ResoOpusProcessor& p, const String& id, const String& lbl, Colour c, Style s)
    : proc (p), paramID (id), label (lbl), colour (c), style (std::move (s))
{
    param = proc.param (paramID);
    jassert (param != nullptr);
    isChoice = dynamic_cast<AudioParameterChoice*> (param) != nullptr;
    destCode = isChoice ? 0 : Mod::destCode (paramID);

    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v)
    {
        value = param->convertTo0to1 (v);
        repaint();
    });
    attachment->sendInitialUpdate();

    if (auto* def = Params::findDef (paramID))
    {
        String tip = String (def->tip);
        if (destCode != 0) tip << "\n(drag a modulation source here, or right-click to modulate)";
        setTooltip (tip);
    }
    setSize (Knob::preferredWidth (style), Knob::preferredHeight (style));
    setRepaintsOnMouseActivity (false);
}

Knob::~Knob() = default;

Point<float> Knob::centre() const
{
    const float top = 13.0f + (style.marks.isEmpty() ? 0.0f : 12.0f);
    return { getWidth() * 0.5f, top + (style.small ? 26.0f : 32.0f) };
}

bool Knob::hitsRing (Point<float> pos) const
{
    if (mods.isEmpty()) return false;
    const float d = pos.getDistanceFrom (centre());
    return d >= radius() + 6.0f && d <= radius() + 15.0f;
}

void Knob::refreshLive()
{
    if (destCode == 0) return;
    Array<ModInfo> now;
    for (int s : proc.slotsForDestination (paramID))
    {
        ModInfo m;
        m.slot = s;
        m.src = (int) proc.getParamReal (Params::modID (s, "src"));
        m.amt = proc.getParamReal (Params::modID (s, "amt"));
        m.on = proc.getParamReal (Params::modID (s, "on")) > 0.5f;
        m.bipolar = proc.getParamReal (Params::modID (s, "bi")) > 0.5f;
        now.add (m);
    }
    bool anyOn = false;
    for (auto& m : now) anyOn = anyOn || m.on;
    float l = anyOn ? proc.liveDest[destCode].load() : -1.0f;
    if (now != mods || std::abs (l - live) > 0.002f)
    {
        mods = now;
        live = l;
        repaint();
    }
}

String Knob::modText (const ModInfo& m) const
{
    const bool bi = Mod::isBipolar (m.src) && m.bipolar;
    return Mod::shortSourceName (m.src) + (bi ? String (CharPointer_UTF8 (" \xc2\xb1")) : String (m.amt >= 0 ? " +" : " -"))
           + String (roundToInt (std::abs (m.amt) * 100.0f)) + " %";
}

void Knob::drawWaveIcon (Graphics& g, const String& name, Rectangle<float> r, Colour c)
{
    Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom(), ym = r.getCentreY(), w = r.getWidth();
    if (name == "~sine")
    {
        p.startNewSubPath (x0, ym);
        for (int i = 1; i <= 16; ++i)
            p.lineTo (x0 + w * i / 16.0f, ym - (y1 - y0) * 0.5f * std::sin (MathConstants<float>::twoPi * i / 16.0f));
    }
    else if (name == "~tri")    { p.startNewSubPath (x0, y1); p.lineTo (r.getCentreX(), y0); p.lineTo (x1, y1); }
    else if (name == "~shark")  { p.startNewSubPath (x0, y1); p.quadraticTo (x0 + w * 0.45f, y0 + 1.0f, x0 + w * 0.62f, y0); p.lineTo (x1, y1); }
    else if (name == "~saw" || name == "~rampup") { p.startNewSubPath (x0, y1); p.lineTo (x1, y0); p.lineTo (x1, y1); }
    else if (name == "~rampdown") { p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (x1, y1); }
    else if (name == "~square") { p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (r.getCentreX(), y0); p.lineTo (r.getCentreX(), y1); p.lineTo (x1, y1); p.lineTo (x1, y0); }
    g.setColour (c);
    g.strokePath (p, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
}

void Knob::paint (Graphics& g)
{
    const auto c = centre();
    const float R = radius();
    const float a = startAngle + value * sweep;

    // label
    g.setColour (Palette::label);
    g.setFont (Fonts::bold (10.5f));
    g.drawText (label, 0, 0, getWidth(), 13, Justification::centred, false);

    if (dropHover)
    {
        g.setColour (Palette::curve.withAlpha (0.18f));
        g.fillEllipse (Rectangle<float> (R * 2.0f + 30.0f, R * 2.0f + 30.0f).withCentre (c));
    }

    // scale ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float t = startAngle + sweep * (float) i / 10.0f;
        g.setColour (Colours::white.withAlpha (i % 5 ? 0.2f : 0.45f));
        g.drawLine (Line<float> (onCircle (c, R + (i % 5 ? 4.5f : 3.0f), t), onCircle (c, R + 8.0f, t)), 1.1f);
    }

    if (style.drive)
    {
        g.setColour (Palette::red.withAlpha (0.7f));
        g.strokePath (arcPath (c, R + 10.5f, startAngle + 0.7f * sweep, startAngle + sweep), PathStrokeType (2.0f));
    }

    // track + value arc
    g.setColour (Colours::white.withAlpha (0.08f));
    g.strokePath (arcPath (c, R + 1.5f, startAngle, startAngle + sweep), PathStrokeType (2.4f));
    const float from = style.bipolar ? 0.0f : startAngle;
    if (std::abs (a - from) > 0.01f)
    {
        auto arc = arcPath (c, R + 1.5f, from, a);
        g.setColour (colour.withAlpha (0.14f));
        g.strokePath (arc, PathStrokeType (8.0f));
        g.setColour (colour);
        g.strokePath (arc, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // modulation ring(s)
    float ringR = R + 10.5f;
    for (int i = 0; i < mods.size() && i < 2; ++i)
    {
        const auto& m = mods.getReference (i);
        const auto mc = Mod::sourceColour (m.src).withAlpha (m.on ? 1.0f : 0.35f);
        const float span = m.amt * sweep;
        const bool both = Mod::isBipolar (m.src) && m.bipolar;
        float a0 = both ? a - std::abs (span) : a, a1 = a + span;
        a0 = jlimit (startAngle, startAngle + sweep, a0);
        a1 = jlimit (startAngle, startAngle + sweep, a1);
        const bool hot = ringHover || ringDragging;
        if (hot)
        {
            g.setColour (mc.withAlpha (0.18f));
            g.strokePath (arcPath (c, ringR, a0, a1), PathStrokeType (8.0f));
        }
        g.setColour (mc);
        g.strokePath (arcPath (c, ringR, a0, a1), PathStrokeType (hot ? 3.2f : 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        ringR += 4.0f;
    }
    if (live >= 0.0f && ! mods.isEmpty())
    {
        const auto p = onCircle (c, R + 10.5f, startAngle + live * sweep);
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (4.8f, 4.8f).withCentre (p));
    }

    // body: shadow, knurled skirt, metal cap, pointer
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> ((R - 1.5f) * 2.0f, (R - 1.5f) * 2.0f).withCentre (c.translated (0.0f, 2.0f)));
    {
        ColourGradient body (Colour (0xff34353c), c.x - R * 0.2f, c.y - R * 0.6f, Colour (0xff0d0e11), c.x, c.y + R, true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> ((R - 1.5f) * 2.0f, (R - 1.5f) * 2.0f).withCentre (c));
        g.setColour (Colours::black);
        g.drawEllipse (Rectangle<float> ((R - 1.5f) * 2.0f, (R - 1.5f) * 2.0f).withCentre (c), 1.0f);
    }
    g.setColour (Colours::white.withAlpha (0.09f));
    for (int i = 0; i < 36; ++i)
    {
        const float t = a + MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (Line<float> (onCircle (c, R - 4.5f, t), onCircle (c, R - 2.2f, t)), 1.0f);
    }
    {
        const float cr = R * 0.52f;
        ColourGradient cap (Colour (0xffd8d9dd), c.x - cr * 0.3f, c.y - cr * 0.5f, Colour (0xff3c3d43), c.x + cr * 0.4f, c.y + cr, true);
        cap.addColour (0.45, Colour (0xff8d8f96));
        g.setGradientFill (cap);
        g.fillEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c), 0.8f);
    }
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawLine (Line<float> (onCircle (c, R * 0.15f, a), onCircle (c, R - 3.5f, a)).withShortenedStart (0.0f), 4.0f);
    g.setColour (Colours::white);
    g.drawLine (Line<float> (onCircle (c, R * 0.15f, a), onCircle (c, R - 3.5f, a)), 2.4f);

    // scale end labels
    g.setFont (Fonts::bold (9.0f));
    g.setColour (Palette::scale);
    if (style.lo.isNotEmpty())
    {
        const auto lp = onCircle (c, R + 11.0f, startAngle);
        const auto hp = onCircle (c, R + 11.0f, startAngle + sweep);
        g.drawText (style.lo, Rectangle<float> (lp.x - 40.0f, lp.y + 3.0f, 41.0f, 11.0f), Justification::centredRight, false);
        g.drawText (style.hi, Rectangle<float> (hp.x - 1.0f, hp.y + 3.0f, 41.0f, 11.0f), Justification::centredLeft, false);
    }

    // marks around (waveforms, types)
    if (! style.marks.isEmpty())
    {
        const int n = style.marks.size();
        const int sel = isChoice ? roundToInt (value * (float) (n - 1)) : roundToInt (value * (float) (n - 1));
        for (int i = 0; i < n; ++i)
        {
            const float t = n == 1 ? 0.0f : startAngle + sweep * (float) i / (float) (n - 1);
            const auto p = onCircle (c, R + 16.0f, t);
            const auto col = i == sel ? colour : Palette::scale;
            const auto& m = style.marks[i];
            if (m.startsWithChar ('~'))
                drawWaveIcon (g, m, Rectangle<float> (10.0f, 7.0f).withCentre (p), col);
            else
            {
                g.setColour (col);
                g.drawText (m, Rectangle<float> (40.0f, 11.0f).withCentre (p.translated ((p.x - c.x) * 0.25f, 0.0f)), Justification::centred, false);
            }
        }
    }

    // value (or modulation text while hovering the ring)
    String txt = param->getCurrentValueAsText();
    Colour tc = Palette::textDim;
    if ((ringHover || ringDragging) && ! mods.isEmpty())
    {
        const auto& m = mods.getReference (0);
        for (auto& mm : mods) if (mm.slot == ringSlot) { txt = modText (mm); tc = Mod::sourceColour (mm.src); }
        if (ringSlot < 0) { txt = modText (m); tc = Mod::sourceColour (m.src); }
    }
    else if (dragging || hover)
        tc = Palette::text;
    g.setColour (tc);
    g.setFont (Fonts::mono (10.5f));
    g.drawText (txt, 0, getHeight() - 14, getWidth(), 13, Justification::centred, false);
}

void Knob::mouseEnter (const MouseEvent& e) { hover = true; mouseMove (e); repaint(); }

void Knob::mouseMove (const MouseEvent& e)
{
    const bool r = hitsRing (e.position);
    if (r != ringHover)
    {
        ringHover = r;
        if (r && ringSlot < 0 && ! mods.isEmpty()) ringSlot = mods.getReference (0).slot;
        setMouseCursor (r ? Cursors::ringDrag() : MouseCursor::NormalCursor);
        repaint();
    }
}

void Knob::mouseExit (const MouseEvent&)
{
    hover = ringHover = false;
    setMouseCursor (MouseCursor::NormalCursor);
    repaint();
}

void Knob::setNormalised (float v)
{
    v = jlimit (0.0f, 1.0f, v);
    if (isChoice)
    {
        const int n = ((AudioParameterChoice*) param)->choices.size();
        v = (float) roundToInt (v * (float) (n - 1)) / (float) (n - 1);
    }
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (v));
}

void Knob::mouseDown (const MouseEvent& e)
{
    if (editor) editor.reset();
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (e.mods.isCommandDown())
    {
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
        return;
    }

    dragStartY = e.position.y;
    if (hitsRing (e.position))
    {
        ringDragging = true;
        if (ringSlot < 0 && ! mods.isEmpty()) ringSlot = mods.getReference (0).slot;
        ringParam = proc.param (Params::modID (ringSlot, "amt"));
        if (ringParam)
        {
            dragStartValue = ringParam->getValue();
            ringParam->beginChangeGesture();
        }
    }
    else
    {
        dragging = true;
        dragStartValue = value;
        attachment->beginGesture();
    }
    e.source.enableUnboundedMouseMovement (true);
}

void Knob::mouseDrag (const MouseEvent& e)
{
    const float dy = dragStartY - e.position.y;
    const float speed = e.mods.isShiftDown() ? 1.0f / 1200.0f : 1.0f / 220.0f;
    if (ringDragging && ringParam)
    {
        ringParam->setValueNotifyingHost (jlimit (0.0f, 1.0f, dragStartValue + dy * speed * 0.5f));
        refreshLive();
        repaint();
    }
    else if (dragging)
    {
        if (isChoice)
        {
            const int n = ((AudioParameterChoice*) param)->choices.size();
            setNormalised (dragStartValue + dy / (28.0f * (float) (n - 1)) * (e.mods.isShiftDown() ? 0.3f : 1.0f));
        }
        else
            setNormalised (dragStartValue + dy * speed);
    }
}

void Knob::mouseUp (const MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement (false);
    if (ringDragging && ringParam) ringParam->endChangeGesture();
    if (dragging) attachment->endGesture();
    dragging = ringDragging = false;
    ringParam = nullptr;
    repaint();
}

void Knob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (hitsRing (e.position) && ! mods.isEmpty())
    {
        if (ringSlot < 0) ringSlot = mods.getReference (0).slot;
        if (auto* rp = proc.param (Params::modID (ringSlot, "amt")))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (jlimit (0.0f, 1.0f, rp->getValue() + dir * 0.05f));
            rp->endChangeGesture();
            refreshLive();
        }
        return;
    }
    attachment->beginGesture();
    if (isChoice)
    {
        const int n = ((AudioParameterChoice*) param)->choices.size();
        setNormalised (value + (dir > 0 ? 1.0f : -1.0f) / (float) (n - 1));
    }
    else
        setNormalised (value + dir * (e.mods.isShiftDown() ? 0.01f : 0.06f));
    attachment->endGesture();
}

void Knob::mouseDoubleClick (const MouseEvent& e)
{
    if (! hitsRing (e.position))
        showEditor();
}

void Knob::showEditor()
{
    editor = std::make_unique<TextEditor>();
    editor->setFont (Fonts::mono (12.0f));
    editor->setJustification (Justification::centred);
    editor->setColour (TextEditor::backgroundColourId, Palette::bar);
    editor->setColour (TextEditor::outlineColourId, colour);
    editor->setColour (TextEditor::focusedOutlineColourId, colour);
    editor->setText (param->getCurrentValueAsText(), false);
    editor->selectAll();
    editor->setBounds (2, getHeight() - 20, getWidth() - 4, 20);
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
    // MSVC cannot take `this` in the init-capture of a nested lambda: create the SafePointer first
    SafePointer<Knob> safe (this);
    editor->onReturnKey = [this, safe]
    {
        const auto text = editor->getText();
        const float v = param->getValueForText (text);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (v));
        MessageManager::callAsync ([safe] { if (safe) safe->editor.reset(); });
    };
    editor->onEscapeKey = [safe] { MessageManager::callAsync ([safe] { if (safe) safe->editor.reset(); }); };
    editor->onFocusLost = editor->onEscapeKey;
}

void Knob::showMenu()
{
    PopupMenu m;
    m.addSectionHeader (Params::findDef (paramID) ? String (Params::findDef (paramID)->name) : label);
    m.addItem (1, "Reset to default");
    m.addItem (2, "Enter value...");

    if (destCode != 0)
    {
        m.addSeparator();
        PopupMenu add;
        for (int s = 1; s < Mod::numSources; ++s)
            add.addItem (1000 + s, Mod::sourceNames()[s]);
        m.addSubMenu ("Modulate with", add);

        for (auto& mi : mods)
        {
            PopupMenu sm;
            sm.addItem (2000 + mi.slot, mi.on ? "Disable" : "Enable");
            sm.addItem (3000 + mi.slot, "Invert");
            if (Mod::isBipolar (mi.src)) sm.addItem (4000 + mi.slot, "Bipolar", true, mi.bipolar);
            sm.addItem (5000 + mi.slot, "Edit in modulation panel...");
            sm.addItem (6000 + mi.slot, "Remove");
            m.addSubMenu (modText (mi), sm);
        }
    }

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp = SafePointer<Knob> (this)] (int r)
    {
        if (sp == nullptr || r == 0) return;
        auto& k = *sp;
        auto setMod = [&k] (int slot, const char* id, float v) { k.proc.setParamReal (Params::modID (slot, id), v); };
        if (r == 1) k.attachment->setValueAsCompleteGesture (k.param->convertFrom0to1 (k.param->getDefaultValue()));
        else if (r == 2) k.showEditor();
        else if (r >= 1000 && r < 2000) k.proc.addModulation (r - 1000, k.paramID, 0.25f);
        else if (r >= 2000 && r < 3000) setMod (r - 2000, "on", k.proc.getParamReal (Params::modID (r - 2000, "on")) > 0.5f ? 0.0f : 1.0f);
        else if (r >= 3000 && r < 4000) setMod (r - 3000, "amt", -k.proc.getParamReal (Params::modID (r - 3000, "amt")));
        else if (r >= 4000 && r < 5000) setMod (r - 4000, "bi", k.proc.getParamReal (Params::modID (r - 4000, "bi")) > 0.5f ? 0.0f : 1.0f);
        else if (r >= 5000 && r < 6000) { if (k.onEditModulation) k.onEditModulation (r - 5000); }
        else if (r >= 6000 && r < 7000) k.proc.removeModulation (r - 6000);
        k.refreshLive();
    });
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return destCode != 0 && d.description.toString().startsWith ("src:");
}

void Knob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("src:", false, false).getIntValue();
    const int slot = proc.addModulation (src, paramID, 0.25f);
    ringSlot = slot;
    refreshLive();
    repaint();
}

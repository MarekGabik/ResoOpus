#include "Widgets.h"
#include "PluginProcessor.h"

using namespace juce;

namespace
{
void drawGlassButton (Graphics& g, Rectangle<float> r, bool on)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 5.0f);
    if (on)
    {
        g.setColour (Palette::curve.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (5.0f), 9.0f);
        ColourGradient gr (Colour (0xffffe9a6), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.3f,
                           Colour (0xffc99a2e), r.getRight(), r.getBottom(), true);
        gr.addColour (0.55, Palette::curve);
        g.setGradientFill (gr);
    }
    else
    {
        ColourGradient gr (Colour (0xff3a2d18), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.3f,
                           Colour (0xff1d160c), r.getRight(), r.getBottom(), true);
        g.setGradientFill (gr);
    }
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (on ? 0.5f : 0.08f));
    g.drawLine (r.getX() + 5.0f, r.getY() + 1.5f, r.getRight() - 5.0f, r.getY() + 1.5f, 1.0f);
}
}

//==============================================================================
LedButton::LedButton (ResoOpusProcessor& p, const String& id, const String& lbl) : label (lbl)
{
    param = p.param (id);
    jassert (param != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
    if (auto* d = Params::findDef (id)) setTooltip (d->tip);
    setSize (std::max (80, GlyphArrangement::getStringWidthInt (Fonts::bold (10.5f), label) + 10), lbl.isEmpty() ? 34 : 50);
}

void LedButton::paint (Graphics& g)
{
    float y = 2.0f;
    if (label.isNotEmpty())
    {
        g.setColour (Palette::label);
        g.setFont (Fonts::bold (10.5f));
        g.drawText (label, 0, 0, getWidth(), 13, Justification::centred, false);
        y = 19.0f;
    }
    drawGlassButton (g, Rectangle<float> (30.0f, 28.0f).withCentre ({ getWidth() * 0.5f, y + 14.0f }), on);
}

void LedButton::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
LedRadio::LedRadio (ResoOpusProcessor& p, const String& id, const String& lbl, StringArray n) : label (lbl), names (std::move (n))
{
    param = p.param (id);
    jassert (param != nullptr);
    if (auto* c = dynamic_cast<AudioParameterChoice*> (param))
    {
        numChoices = c->choices.size();
        if (names.isEmpty()) names = c->choices;
    }
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { index = roundToInt (v); repaint(); });
    attachment->sendInitialUpdate();
    if (auto* d = Params::findDef (id)) setTooltip (d->tip);
    updateSize();
}

LedRadio::LedRadio (const String& lbl, StringArray n, std::function<int()> get, std::function<void (int)> set)
    : label (lbl), names (std::move (n)), getIndex (std::move (get)), setIndex (std::move (set))
{
    numChoices = names.size();
    index = getIndex ? getIndex() : 0;
    updateSize();
}

void LedRadio::refresh()
{
    if (getIndex)
    {
        const int i = getIndex();
        if (i != index) { index = i; repaint(); }
    }
}

void LedRadio::choose (int i)
{
    if (setIndex) { setIndex (i); refresh(); }
    else attachment->setValueAsCompleteGesture ((float) i);
}

void LedRadio::setIcons (StringArray iconNames, Array<Colour> colours)
{
    icons = std::move (iconNames);
    iconColours = std::move (colours);
    updateSize();
    repaint();
}

void LedRadio::updateSize()
{
    int w = 0;
    for (auto& s : names) w = std::max (w, GlyphArrangement::getStringWidthInt (Fonts::bold (10.5f), s));
    if (! icons.isEmpty()) w += 22;
    setSize (std::max (GlyphArrangement::getStringWidthInt (Fonts::bold (10.5f), label), 30 + 10 + 12 + w) + 4,
             (label.isEmpty() ? 0 : 19) + std::max (30, names.size() * 15));
}

void LedRadio::paint (Graphics& g)
{
    float y = 0.0f;
    if (label.isNotEmpty())
    {
        g.setColour (Palette::label);
        g.setFont (Fonts::bold (10.5f));
        g.drawText (label, 0, 0, getWidth(), 13, Justification::centredLeft, false);
        y = 19.0f;
    }
    const float listH = (float) names.size() * 15.0f;
    drawGlassButton (g, Rectangle<float> (1.0f, y + std::max (0.0f, (listH - 28.0f) * 0.5f), 30.0f, 28.0f), false);

    g.setFont (Fonts::bold (10.5f));
    for (int i = 0; i < names.size(); ++i)
    {
        const float ly = y + (float) i * 15.0f;
        const bool sel = i == index;
        const auto dot = Rectangle<float> (6.0f, 6.0f).withCentre ({ 43.0f, ly + 7.5f });
        if (sel)
        {
            g.setColour (Palette::curve.withAlpha (0.35f));
            g.fillEllipse (dot.expanded (3.0f));
            g.setColour (Palette::curve);
        }
        else
            g.setColour (Colour (0xff2d2e35));
        g.fillEllipse (dot);
        int tx = 52;
        if (i < icons.size())
        {
            auto p = Icons::wave (icons[i]);
            p.applyTransform (AffineTransform::scale (15.0f, 9.0f).translated (52.0f, ly + 3.0f));
            const auto c = i < iconColours.size() ? iconColours[i] : Palette::label;
            g.setColour (sel ? c : c.withAlpha (0.45f));
            g.strokePath (p, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
            tx = 74;
        }
        g.setColour (sel ? Palette::text : Palette::textFaint.brighter (0.25f));
        g.drawText (names[i], tx, (int) ly, getWidth() - tx, 15, Justification::centredLeft, false);
    }
}

void LedRadio::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float y0 = label.isEmpty() ? 0.0f : 19.0f;
    if (e.position.x >= 36.0f)
    {
        const int i = (int) ((e.position.y - y0) / 15.0f);
        if (i >= 0 && i < numChoices) choose (i);
    }
    else
        choose ((index + 1 + numChoices) % numChoices);
}

//==============================================================================
SourceHandle::SourceHandle (int s) : source (s)
{
    setTooltip ("Drag onto a knob to modulate it with " + Mod::sourceNames()[s]);
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void SourceHandle::paint (Graphics& g)
{
    Icons::draw (g, Icons::drag(), getLocalBounds().toFloat().reduced (3.0f), Palette::plateText.withAlpha (isMouseOver() ? 1.0f : 0.7f), 1.6f);
}

void SourceHandle::mouseDrag (const MouseEvent&)
{
    if (dragging) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        Image img (Image::ARGB, 22, 22, true);
        {
            Graphics g (img);
            const auto col = Mod::sourceColour (source);
            g.setColour (col.withAlpha (0.3f));
            g.fillEllipse (1.0f, 1.0f, 20.0f, 20.0f);
            g.setColour (col);
            g.fillEllipse (6.0f, 6.0f, 10.0f, 10.0f);
        }
        c->startDragging ("src:" + String (source), this, ScaledImage (img), true);
        MessageManager::callAsync ([sp = SafePointer<SourceHandle> (this)] { if (sp) sp->dragging = false; });
    }
}

//==============================================================================
ModulePanel::ModulePanel (const String& t, Colour c, int s, const String& r) : title (t), rightText (r), colour (c), source (s)
{
    if (source > 0)
    {
        handle = std::make_unique<SourceHandle> (source);
        addAndMakeVisible (*handle);
    }
}

void ModulePanel::setGrid (std::vector<float> w, std::vector<Component*> it)
{
    weights = std::move (w);
    items = std::move (it);
    for (auto* c : items)
        if (c != nullptr) addAndMakeVisible (c);
    resized();
}

void ModulePanel::setLiveValue (float v)
{
    v = jlimit (0.0f, 1.0f, v);
    if (std::abs (v - live) > 0.04f) { live = v; repaint (0, 0, 24, headerHeight); }
}

void ModulePanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 8.0f);
    g.setColour (Palette::module);
    g.fillRoundedRectangle (r, 8.0f);

    // label plate
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), (float) headerHeight + 2.0f, 8.0f, 8.0f, true, true, false, false);
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (Palette::plateTop, 0.0f, 0.0f, Palette::plateBottom, 0.0f, (float) headerHeight, false));
        g.fillRect (r.withHeight ((float) headerHeight));
        g.setColour (colour);
        g.fillRect (r.getX(), (float) headerHeight, r.getWidth(), 2.0f);
    }
    g.setColour (Palette::plateText);
    g.setFont (Fonts::bold (11.5f).withExtraKerningFactor (0.11f));
    g.drawText (title, 0, 0, getWidth(), headerHeight, Justification::centred, false);

    // colour dot (blinks with the source for LFO / random)
    const auto dot = Rectangle<float> (8.0f, 8.0f).withCentre ({ 13.0f, headerHeight * 0.5f });
    g.setColour (Colours::black.withAlpha (0.3f));
    g.fillEllipse (dot.expanded (1.5f));
    g.setColour (colour.withAlpha (0.3f + 0.7f * live));
    g.fillEllipse (dot);

    if (rightText.isNotEmpty())
    {
        g.setColour (Colour (0xff2c2c30));
        g.setFont (Fonts::mono (10.0f).boldened());
        g.drawText (rightText, 0, 0, getWidth() - (handle ? 30 : 10), headerHeight, Justification::centredRight, false);
    }

    if (envIcon > 0)
    {
        Path p;
        const float x = (float) getWidth() - (handle ? 82.0f : 58.0f), y0 = 7.0f, y1 = 18.0f;
        if (envIcon == 4)
        {
            p.startNewSubPath (x, y1); p.lineTo (x + 6.0f, y0); p.lineTo (x + 14.0f, y0 + 5.0f);
            p.lineTo (x + 32.0f, y0 + 5.0f); p.lineTo (x + 42.0f, y1);
        }
        else
        {
            p.startNewSubPath (x, y1); p.lineTo (x + 6.0f, y1); p.lineTo (x + 12.0f, y0); p.lineTo (x + 18.0f, y0);
            p.lineTo (x + 26.0f, y0 + 5.0f); p.lineTo (x + 34.0f, y0 + 5.0f); p.lineTo (x + 42.0f, y1);
        }
        g.setColour (Colour (0xff2a2a2e));
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour (Palette::moduleEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

void ModulePanel::resized()
{
    if (handle) handle->setBounds (getWidth() - 26, 3, 20, 19);
    if (items.empty() || weights.empty()) return;

    auto body = getLocalBounds().withTrimmedTop (headerHeight + 2).reduced (6, 6);
    const int cols = (int) weights.size();
    const int rows = ((int) items.size() + cols - 1) / cols;
    float total = 0.0f;
    for (auto w : weights) total += w;

    for (size_t i = 0; i < items.size(); ++i)
    {
        auto* c = items[i];
        if (c == nullptr) continue;
        const int col = (int) i % cols, row = (int) i / cols;
        float x0 = 0.0f;
        for (int k = 0; k < col; ++k) x0 += weights[(size_t) k];
        const float cx = body.getX() + body.getWidth() * (x0 + weights[(size_t) col] * 0.5f) / total;
        const float cy = body.getY() + body.getHeight() * ((float) row + 0.5f) / (float) rows;
        c->setCentrePosition (roundToInt (cx), roundToInt (cy));
    }
}

//==============================================================================
Stack::Stack (std::vector<Component*> c, int g, bool l) : kids (std::move (c)), gap (g), left (l)
{
    int w = 0, h = 0;
    for (auto* k : kids)
    {
        addAndMakeVisible (k);
        w = std::max (w, k->getWidth());
        h += k->getHeight();
    }
    setSize (w, h + gap * ((int) kids.size() - 1));
}

void Stack::resized()
{
    int y = 0;
    for (auto* k : kids)
    {
        k->setTopLeftPosition (left ? 0 : (getWidth() - k->getWidth()) / 2, y);
        y += k->getHeight() + gap;
    }
}

//==============================================================================
void DriveMeter::setLevel (float v)
{
    if (std::abs (v - level) > 0.01f) { level = v; repaint(); }
}

void DriveMeter::paint (Graphics& g)
{
    g.setFont (Fonts::bold (10.0f));
    g.setColour (Palette::label);
    g.drawText ("DRIVE", 0, 0, getWidth(), 12, Justification::centredLeft, false);
    g.setColour (Palette::mixer);
    g.setFont (Fonts::mono (10.0f));
    g.drawText ("+" + String (level * 6.0f, 1), 0, 0, getWidth(), 12, Justification::centredRight, false);
    auto bar = Rectangle<float> (0.0f, 17.0f, (float) getWidth(), 5.0f);
    g.setColour (Colour (0xff24252b));
    g.fillRoundedRectangle (bar, 2.5f);
    if (level > 0.0f)
    {
        auto fill = bar.withWidth (std::max (3.0f, bar.getWidth() * level));
        g.setGradientFill (ColourGradient (Colour (0xffa02a6a), bar.getX(), 0.0f, Palette::mixer, bar.getRight(), 0.0f, false));
        g.fillRoundedRectangle (fill, 2.5f);
    }
}

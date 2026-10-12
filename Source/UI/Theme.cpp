#include "Theme.h"

using namespace juce;

Colour Palette::forBand (int index)
{
    // neon tones that stay distinct from each other and from the yellow result curve
    static const uint32 c[] = { 0xff38d6f0, 0xffb98bff, 0xffff74b8, 0xffa5e844, 0xffff9a4d, 0xff4aa8ff,
                                0xffe57bff, 0xff3ddba0, 0xff8f8cff, 0xffff6b81, 0xff2fd1c0, 0xff7fd4ff };
    return Colour (c[((index % 12) + 12) % 12]);
}

Colour Palette::heat (float t)
{
    static const ColourGradient map = []
    {
        ColourGradient m (Colour (0xff1c0b33), 0.0f, 0.0f, Colour (0xffff9a3c), 1.0f, 0.0f, false);
        m.addColour (0.35, Colour (0xff5a1690));
        m.addColour (0.62, Colour (0xffd6187f));
        m.addColour (0.82, Colour (0xffff3d4e));
        return m;
    }();
    return map.getColourAtPosition (jlimit (0.0, 1.0, (double) t));
}

Font Fonts::regular (float size) { return Font (FontOptions (size)); }
Font Fonts::bold (float size)    { return Font (FontOptions (size, Font::bold)); }

Font Fonts::mono (float size)
{
    static const String name = Font::getDefaultMonospacedFontName();
    return Font (FontOptions (name, size, Font::plain));
}

//==============================================================================
Colour Palette::voiceType (int t)
{
    switch (t)
    {
        case 1: return choir;
        case 2: return siren;
        case 3: return pulsar;
        case 4: return quasar;
        case 5: return nebula;
        default: return Colour (0xff6c6d76);
    }
}

namespace Icons
{
Path power()
{
    Path p;
    p.addCentredArc (0.5f, 0.55f, 0.36f, 0.36f, 0.0f, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
    p.startNewSubPath (0.5f, 0.08f);
    p.lineTo (0.5f, 0.5f);
    return p;
}

Path headphones()
{
    Path p;
    p.addCentredArc (0.5f, 0.55f, 0.38f, 0.4f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
    p.addRoundedRectangle (0.08f, 0.55f, 0.18f, 0.35f, 0.05f);
    p.addRoundedRectangle (0.74f, 0.55f, 0.18f, 0.35f, 0.05f);
    return p;
}

Path close()
{
    Path p;
    p.startNewSubPath (0.2f, 0.2f); p.lineTo (0.8f, 0.8f);
    p.startNewSubPath (0.8f, 0.2f); p.lineTo (0.2f, 0.8f);
    return p;
}

Path undo()
{
    Path p;
    p.addCentredArc (0.5f, 0.55f, 0.32f, 0.3f, 0.0f, -MathConstants<float>::halfPi * 1.3f, MathConstants<float>::halfPi * 0.9f, true);
    p.startNewSubPath (0.05f, 0.32f);
    p.lineTo (0.2f, 0.52f);
    p.lineTo (0.38f, 0.36f);
    return p;
}

Path redo()
{
    auto p = undo();
    p.applyTransform (AffineTransform::scale (-1.0f, 1.0f, 0.5f, 0.5f));
    return p;
}

Path fullScreen()
{
    Path p;
    p.startNewSubPath (0.1f, 0.38f); p.lineTo (0.1f, 0.1f); p.lineTo (0.38f, 0.1f);
    p.startNewSubPath (0.62f, 0.1f); p.lineTo (0.9f, 0.1f); p.lineTo (0.9f, 0.38f);
    p.startNewSubPath (0.9f, 0.62f); p.lineTo (0.9f, 0.9f); p.lineTo (0.62f, 0.9f);
    p.startNewSubPath (0.38f, 0.9f); p.lineTo (0.1f, 0.9f); p.lineTo (0.1f, 0.62f);
    return p;
}

Path resize()
{
    Path p;
    p.startNewSubPath (0.15f, 0.85f); p.lineTo (0.85f, 0.15f);
    p.startNewSubPath (0.5f, 0.15f);  p.lineTo (0.85f, 0.15f); p.lineTo (0.85f, 0.5f);
    p.startNewSubPath (0.15f, 0.5f);  p.lineTo (0.15f, 0.85f); p.lineTo (0.5f, 0.85f);
    return p;
}

Path scissors()
{
    Path p;
    p.addEllipse (0.12f, 0.62f, 0.26f, 0.26f);
    p.addEllipse (0.62f, 0.62f, 0.26f, 0.26f);
    p.startNewSubPath (0.33f, 0.64f); p.lineTo (0.72f, 0.08f);
    p.startNewSubPath (0.67f, 0.64f); p.lineTo (0.28f, 0.08f);
    return p;
}

Path chevron (bool right)
{
    Path p;
    p.startNewSubPath (0.35f, 0.15f);
    p.lineTo (0.7f, 0.5f);
    p.lineTo (0.35f, 0.85f);
    if (! right) p.applyTransform (AffineTransform::scale (-1.0f, 1.0f, 0.5f, 0.5f));
    return p;
}

Path drag()
{
    // "patch cable" arrow used on modulation sources
    Path p;
    p.startNewSubPath (0.08f, 0.62f);
    p.cubicTo (0.3f, 0.62f, 0.38f, 0.3f, 0.62f, 0.3f);
    p.lineTo (0.9f, 0.3f);
    p.startNewSubPath (0.72f, 0.12f); p.lineTo (0.9f, 0.3f); p.lineTo (0.72f, 0.48f);
    p.addEllipse (0.02f, 0.56f, 0.12f, 0.12f);
    return p;
}

Path star()
{
    Path p;
    p.addStar ({ 0.5f, 0.53f }, 5, 0.21f, 0.5f, 0.0f);
    return p;
}

Path pin()
{
    Path p;
    p.addEllipse (0.3f, 0.06f, 0.4f, 0.4f);
    p.startNewSubPath (0.5f, 0.46f);
    p.lineTo (0.5f, 0.95f);
    return p;
}

Path menu()
{
    Path p;
    for (float y : { 0.22f, 0.5f, 0.78f })
    {
        p.startNewSubPath (0.12f, y);
        p.lineTo (0.88f, y);
    }
    return p;
}

Path dots()
{
    Path p;
    for (float x : { 0.18f, 0.5f, 0.82f })
        p.addEllipse (x - 0.08f, 0.42f, 0.16f, 0.16f);
    return p;
}

Path user()
{
    Path p;
    p.addEllipse (0.3f, 0.06f, 0.4f, 0.4f);
    p.addCentredArc (0.5f, 0.98f, 0.4f, 0.42f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
    return p;
}

Path wave (const String& nm)
{
    const String name = nm.trimCharactersAtStart ("~");
    Path p;
    auto fn = [&p] (std::function<float (float)> f, int steps = 32)
    {
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const float y = 0.5f - 0.42f * f (x);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
    };
    const float pi2 = MathConstants<float>::twoPi;
    if (name == "sine") fn ([pi2] (float x) { return std::sin (pi2 * x); });
    else if (name == "tri") { p.startNewSubPath (0.0f, 0.5f); p.lineTo (0.25f, 0.08f); p.lineTo (0.75f, 0.92f); p.lineTo (1.0f, 0.5f); }
    else if (name == "shark") { p.startNewSubPath (0.0f, 0.92f); p.quadraticTo (0.45f, 0.1f, 0.62f, 0.08f); p.lineTo (1.0f, 0.92f); }
    else if (name == "saw" || name == "rampup") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.5f, 0.08f); p.lineTo (0.5f, 0.92f); p.lineTo (1.0f, 0.08f); }
    else if (name == "rampdown") { p.startNewSubPath (0.0f, 0.08f); p.lineTo (0.5f, 0.92f); p.lineTo (0.5f, 0.08f); p.lineTo (1.0f, 0.92f); }
    else if (name == "square") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.0f, 0.08f); p.lineTo (0.5f, 0.08f); p.lineTo (0.5f, 0.92f); p.lineTo (1.0f, 0.92f); p.lineTo (1.0f, 0.08f); }
    else if (name == "pulse") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.0f, 0.08f); p.lineTo (0.22f, 0.08f); p.lineTo (0.22f, 0.92f); p.lineTo (0.5f, 0.92f);
                                p.lineTo (0.5f, 0.08f); p.lineTo (0.72f, 0.08f); p.lineTo (0.72f, 0.92f); p.lineTo (1.0f, 0.92f); }
    else if (name == "pulsew") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.0f, 0.08f); p.lineTo (0.4f, 0.08f); p.lineTo (0.4f, 0.92f); p.lineTo (0.5f, 0.92f);
                                 p.lineTo (0.5f, 0.08f); p.lineTo (0.9f, 0.08f); p.lineTo (0.9f, 0.92f); p.lineTo (1.0f, 0.92f); }
    else if (name == "sh") { p.startNewSubPath (0.0f, 0.6f); p.lineTo (0.25f, 0.6f); p.lineTo (0.25f, 0.15f); p.lineTo (0.5f, 0.15f); p.lineTo (0.5f, 0.85f);
                             p.lineTo (0.75f, 0.85f); p.lineTo (0.75f, 0.35f); p.lineTo (1.0f, 0.35f); }
    else if (name == "noise") { const float v[] = { 0.5f, 0.2f, 0.7f, 0.35f, 0.9f, 0.15f, 0.6f, 0.45f, 0.8f, 0.3f, 0.55f };
                                for (int i = 0; i < 11; ++i) { if (i == 0) p.startNewSubPath (0.0f, v[0]); else p.lineTo ((float) i / 10.0f, v[i]); } }
    else if (name == "flat") { p.startNewSubPath (0.0f, 0.5f); p.lineTo (1.0f, 0.5f); }
    else if (name == "chord") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.5f, 0.25f); p.lineTo (0.5f, 0.92f); p.lineTo (1.0f, 0.25f);
                                p.startNewSubPath (0.0f, 0.7f); p.lineTo (0.33f, 0.08f); p.lineTo (0.33f, 0.7f); p.lineTo (0.66f, 0.08f); p.lineTo (0.66f, 0.7f); p.lineTo (1.0f, 0.08f); }
    else if (name == "siren") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.0f, 0.08f); p.lineTo (0.3f, 0.08f); p.lineTo (0.3f, 0.92f); p.lineTo (0.5f, 0.92f);
                                p.lineTo (0.5f, 0.08f); p.lineTo (0.62f, 0.08f); p.lineTo (0.62f, 0.92f); p.lineTo (0.74f, 0.92f); p.lineTo (0.74f, 0.08f);
                                p.lineTo (0.82f, 0.08f); p.lineTo (0.82f, 0.92f); p.lineTo (0.9f, 0.92f); p.lineTo (0.9f, 0.08f); p.lineTo (1.0f, 0.08f); }
    else if (name == "morph") { p.startNewSubPath (0.0f, 0.92f); p.lineTo (0.25f, 0.08f); p.lineTo (0.5f, 0.92f); p.lineTo (0.5f, 0.08f); p.lineTo (0.75f, 0.08f);
                                p.lineTo (0.75f, 0.92f); p.lineTo (1.0f, 0.92f); }
    else if (name == "fm") fn ([pi2] (float x) { return std::sin (pi2 * x + 2.2f * std::sin (3.0f * pi2 * x)); }, 64);
    else if (name == "table") { for (int k = 0; k < 3; ++k) { const float o = 0.16f * (float) k;
                                    for (int i = 0; i <= 16; ++i) { const float x = (float) i / 16.0f;
                                        const float y = 0.62f - o - 0.2f * std::sin (pi2 * x + (float) k);
                                        if (i == 0) p.startNewSubPath (x * 0.8f + o * 0.6f, y); else p.lineTo (x * 0.8f + o * 0.6f, y); } } }
    else if (name == "lp") { p.startNewSubPath (0.0f, 0.3f); p.lineTo (0.5f, 0.3f); p.quadraticTo (0.68f, 0.3f, 1.0f, 0.95f); }
    else if (name == "hp") { p.startNewSubPath (0.0f, 0.95f); p.quadraticTo (0.32f, 0.3f, 0.5f, 0.3f); p.lineTo (1.0f, 0.3f); }
    else if (name == "bp") { p.startNewSubPath (0.0f, 0.95f); p.quadraticTo (0.35f, 0.2f, 0.5f, 0.2f); p.quadraticTo (0.65f, 0.2f, 1.0f, 0.95f); }
    return p;
}

void fill (Graphics& g, const Path& unitPath, Rectangle<float> area, Colour c)
{
    auto p = unitPath;
    p.applyTransform (AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY()));
    g.setColour (c);
    g.fillPath (p);
}

void draw (Graphics& g, const Path& unitPath, Rectangle<float> area, Colour c, float strokeWidth)
{
    auto p = unitPath;
    p.applyTransform (AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY()));
    g.setColour (c);
    g.strokePath (p, PathStrokeType (strokeWidth, PathStrokeType::curved, PathStrokeType::rounded));
}
}

//==============================================================================
const MouseCursor& Cursors::ringDrag()
{
    static const MouseCursor cursor = []
    {
        constexpr float scale = 2.0f, size = 32.0f;
        Image img (Image::ARGB, (int) (size * scale), (int) (size * scale), true);
        Graphics g (img);
        g.addTransform (AffineTransform::scale (scale));
        const float cx = 16.0f;

        // arrow shaft with heads at both ends
        Path arrow;
        arrow.startNewSubPath (cx, 5.0f);
        arrow.lineTo (cx, 27.0f);
        Path heads;
        heads.addTriangle (cx, 1.5f, cx - 6.0f, 9.0f, cx + 6.0f, 9.0f);
        heads.addTriangle (cx, 30.5f, cx - 6.0f, 23.0f, cx + 6.0f, 23.0f);

        // ring arc around the middle
        Path ring;
        ring.addCentredArc (cx, 16.0f, 7.5f, 7.5f, 0.0f, 0.5f, MathConstants<float>::twoPi - 0.5f, true);

        g.setColour (Colours::black.withAlpha (0.85f));
        g.strokePath (arrow, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.strokePath (ring, PathStrokeType (4.5f));
        g.strokePath (heads, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillPath (heads);

        g.setColour (Palette::curve);
        g.strokePath (ring, PathStrokeType (2.0f));
        g.setColour (Colours::white);
        g.strokePath (arrow, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillPath (heads);
        return MouseCursor (ScaledImage (img, scale), Point<int> (16, 16));
    }();
    return cursor;
}

ResoLookAndFeel::ResoLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, Palette::bgBottom);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::backgroundColourId, Palette::button);
    setColour (Label::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Palette::bar);
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::outlineColourId, Palette::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, Palette::curve);
    setColour (TextEditor::highlightColourId, Palette::curve.withAlpha (0.35f));
    setColour (CaretComponent::caretColourId, Palette::curve);
    setColour (PopupMenu::backgroundColourId, Palette::bar);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::headerTextColourId, Palette::textDim);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::buttonHover);
    setColour (PopupMenu::highlightedTextColourId, Palette::text);
    setColour (TooltipWindow::backgroundColourId, Palette::bar);
    setColour (TooltipWindow::textColourId, Palette::text);
    setColour (TooltipWindow::outlineColourId, Palette::panelEdge);
    setColour (AlertWindow::backgroundColourId, Palette::bgTop);
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (AlertWindow::outlineColourId, Palette::panelEdge);
    setColour (TextButton::buttonColourId, Palette::button);
    setColour (TextButton::textColourOffId, Palette::text);
}

Font ResoLookAndFeel::getPopupMenuFont() { return Fonts::regular (14.0f); }
Font ResoLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

void ResoLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    g.fillAll (Palette::bar);
    g.setColour (Palette::panelEdge);
    g.drawRect (0, 0, width, height);
}

void ResoLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& background, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? background : (highlighted || down ? background.brighter (0.15f) : background);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (b.getToggleState() ? 0.0f : 0.05f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

Font ResoLookAndFeel::getTextButtonFont (TextButton&, int) { return Fonts::regular (12.0f); }

Rectangle<int> ResoLookAndFeel::getTooltipBounds (const String& tipText, Point<int> screenPos, Rectangle<int> parentArea)
{
    // lay the text out at the real wrap width, so long tips get as many lines as they need
    AttributedString s;
    s.append (tipText, Fonts::regular (13.0f), Palette::text);
    s.setWordWrap (AttributedString::byWord);
    TextLayout layout;
    layout.createLayout (s, 300.0f);
    const int w = (int) std::ceil (layout.getWidth()) + 20;
    const int h = (int) std::ceil (layout.getHeight()) + 12;
    return Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                           screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                           w, h).constrainedWithin (parentArea);
}

void ResoLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    auto r = Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (Palette::bar);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    AttributedString s;
    s.append (text, Fonts::regular (13.0f), Palette::text);
    s.setWordWrap (AttributedString::byWord);
    s.setJustification (Justification::topLeft);
    TextLayout layout;
    layout.createLayout (s, 300.0f);
    layout.draw (g, Rectangle<float> (10.0f, 6.0f, (float) width - 20.0f, (float) height - 12.0f));
}

void ResoLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? Palette::buttonHover : Palette::button);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path arrow;
    const float cx = (float) width - 11.0f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.setColour (Palette::textDim);
    g.fillPath (arrow);
}

Font ResoLookAndFeel::getComboBoxFont (ComboBox&) { return Fonts::regular (12.5f); }

void ResoLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

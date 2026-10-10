#include "PluginEditor.h"

using namespace juce;

//==============================================================================
void Stage::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Palette::stageTop, 0.0f, 0.0f, Palette::stageBottom, 0.0f, r.getHeight(), false));
    g.fillRect (r);

    // fine grain texture, drawn once
    if (texture.isNull())
    {
        texture = Image (Image::ARGB, 160, 160, true);
        Random rnd (7);
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 160; ++x)
                texture.setPixelAt (x, y, Colours::white.withAlpha (rnd.nextFloat() * 0.035f));
    }
    g.setTiledImageFill (texture, 0, 0, 1.0f);
    g.fillRect (r);

    // faint stars, drawn once
    if (stars.isNull())
    {
        stars = Image (Image::ARGB, getWidth(), getHeight(), true);
        Graphics sg (stars);
        Random rnd (11);
        for (int i = 0; i < 70; ++i)
        {
            const float x = rnd.nextFloat() * (float) getWidth(), y = rnd.nextFloat() * (float) getHeight();
            const float d = 0.4f + rnd.nextFloat() * 1.6f;
            sg.setColour (Colours::white.withAlpha (0.03f + rnd.nextFloat() * 0.12f));
            sg.fillEllipse (x, y, d, d);
        }
    }
    g.drawImageAt (stars, 0, 0);
}

//==============================================================================
ResoOpusEditor::ResoOpusEditor (ResoOpusProcessor& p)
    : AudioProcessorEditor (p), proc (p), top (p, *settings), keyboard (p), bottom (p), drawer (p)
{
    setLookAndFeel (&lnf);
    proc.editorOpen = true;

    addAndMakeVisible (content);
    content.addAndMakeVisible (top);
    content.addAndMakeVisible (stage);
    content.addAndMakeVisible (keyboard);
    content.addAndMakeVisible (bottom);
    content.addChildComponent (drawer);

    pages.add (new DronePage (p));
    pages.add (new FxPage (p));
    for (auto* pg : pages)
    {
        stage.addChildComponent (pg);
        pg->setModEditCallback ([this] (int slot) { setModDrawer (true, slot); });
    }

    top.onTab = [this] (int i) { showPage (i); };
    top.onMod = [this] { setModDrawer (! drawer.isVisible()); };
    top.isModOpen = [this] { return drawer.isVisible(); };
    top.onScale = [this] (int s) { setScalePercent (s); };
    top.getScale = [this] { return roundToInt (scale * 100.0f); };
    drawer.onClose = [this] { setModDrawer (false); };

    settings->addChangeListener (this);
    updateTooltips();

    scale = jlimit (0.5f, 2.0f, (float) (int) proc.uiState.getProperty ("scale", settings->getInt (GlobalSettings::scaling, 100)) / 100.0f);
    constrainer.setFixedAspectRatio ((double) logicalWidth / logicalHeight);
    constrainer.setSizeLimits (roundToInt (logicalWidth * 0.5f), roundToInt (logicalHeight * 0.5f), logicalWidth * 2, logicalHeight * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (roundToInt (logicalWidth * scale), roundToInt (logicalHeight * scale));

    showPage ((int) proc.uiState.getProperty ("page", 0));
    top.refresh();
    bottom.refresh();
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

ResoOpusEditor::~ResoOpusEditor()
{
    stopTimer();
    proc.editorOpen = false;
    settings->removeChangeListener (this);
    tooltips.reset();
    setLookAndFeel (nullptr);
}

void ResoOpusEditor::paint (Graphics& g)
{
    g.fillAll (Palette::bgBottom);
}

void ResoOpusEditor::resized()
{
    scale = (float) getWidth() / (float) logicalWidth;
    content.setTransform (AffineTransform::scale (scale));
    content.setBounds (0, 0, logicalWidth, logicalHeight);
    top.setBounds (0, 0, logicalWidth, TopBar::height);
    stage.setBounds (0, TopBar::height, logicalWidth, Stage::height);
    for (auto* pg : pages) pg->setBounds (Stage::pageLeft, Stage::pageTop, Page::width, Page::height);
    keyboard.setBounds (0, TopBar::height + Stage::height, logicalWidth, logicalHeight - TopBar::height - Stage::height - BottomBar::height);
    bottom.setBounds (0, logicalHeight - BottomBar::height, logicalWidth, BottomBar::height);
    drawer.setBounds (0, TopBar::height, ModDrawer::width, logicalHeight - TopBar::height);
    proc.uiState.setProperty ("scale", roundToInt (scale * 100.0f), nullptr);
}

void ResoOpusEditor::setScalePercent (int percent)
{
    settings->setInt (GlobalSettings::scaling, percent);
    setSize (roundToInt (logicalWidth * percent / 100.0f), roundToInt (logicalHeight * percent / 100.0f));
}

void ResoOpusEditor::showPage (int index)
{
    page = jlimit (0, pages.size() - 1, index);
    for (int i = 0; i < pages.size(); ++i) pages[i]->setVisible (i == page);
    top.setPage (page);
    proc.uiState.setProperty ("page", page, nullptr);
    pages[page]->refreshLive();
}

void ResoOpusEditor::setModDrawer (bool open, int slot)
{
    drawer.setVisible (open);
    if (open)
    {
        drawer.refresh();
        if (slot >= 0) drawer.select (slot);
        drawer.toFront (false);
    }
    top.repaint();
}

bool ResoOpusEditor::keyPressed (const KeyPress& key)
{
    const auto c = key.getTextCharacter();
    if (c >= '1' && c < '1' + pages.size()) { showPage (c - '1'); return true; }
    if (c == '9' || c == '0') { setModDrawer (! drawer.isVisible()); return true; }
    if (key == KeyPress ('z', ModifierKeys::commandModifier, 0)) { if (proc.history) proc.history->undo(); return true; }
    if (key == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) { if (proc.history) proc.history->redo(); return true; }
    if (key == KeyPress::escapeKey && drawer.isVisible()) { setModDrawer (false); return true; }
    return false;
}

void ResoOpusEditor::timerCallback()
{
    pages[page]->refreshLive();
    keyboard.refresh();
    if ((++tick % 3) == 0)
    {
        top.refresh();
        bottom.refresh();
        if (drawer.isVisible()) drawer.refresh();
    }
}

void ResoOpusEditor::changeListenerCallback (ChangeBroadcaster*)
{
    updateTooltips();
}

void ResoOpusEditor::updateTooltips()
{
    const bool on = settings->getBool (GlobalSettings::tooltips);
    if (on && tooltips == nullptr)
        tooltips = std::make_unique<TooltipWindow> (this, 700);
    else if (! on)
        tooltips.reset();
}

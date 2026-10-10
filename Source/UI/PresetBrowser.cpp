#include "PresetBrowser.h"
#include "PluginProcessor.h"

using namespace juce;

namespace
{
    class PresetRow : public Component
    {
    public:
        explicit PresetRow (PresetBrowser& b) : browser (b) {}

        void update (int r, bool sel) { row = r; selected = sel; repaint(); }

        Rectangle<int> starArea() const    { return getLocalBounds().removeFromRight (30); }
        Rectangle<int> optionsArea() const { return getLocalBounds().withTrimmedRight (30).removeFromRight (28); }

        void paint (Graphics& g) override
        {
            auto* p = browser.presetForRow (row);
            if (p == nullptr) return;
            SharedResourcePointer<GlobalSettings> settings;
            const bool current = browser.isCurrent (row);
            const bool hover = isMouseOver (true);

            auto bg = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            if (current)       { g.setColour (Palette::curve.withAlpha (0.16f)); g.fillRoundedRectangle (bg, 4.0f); }
            else if (selected) { g.setColour (Palette::curve.withAlpha (0.08f)); g.fillRoundedRectangle (bg, 4.0f); }
            else if (hover)    { g.setColour (Palette::buttonHover.withAlpha (0.6f)); g.fillRoundedRectangle (bg, 4.0f); }

            const bool fav = settings->isFavourite (p->key());
            const auto star = starArea().toFloat().withSizeKeepingCentre (13.0f, 13.0f);
            if (fav) Icons::fill (g, Icons::star(), star, Palette::curve);
            else if (hover) Icons::draw (g, Icons::star(), star, Palette::textDim, 1.1f);
            if (hover) Icons::fill (g, Icons::dots(), optionsArea().toFloat().withSizeKeepingCentre (14.0f, 14.0f), Palette::textDim);

            auto r = getLocalBounds().withTrimmedLeft (12).withTrimmedRight (62);
            if (settings->getDefaultPreset() == p->key())
                Icons::draw (g, Icons::pin(), r.removeFromRight (22).toFloat().withSizeKeepingCentre (12.0f, 12.0f), Palette::curve, 1.3f);
            if (! p->isFactory)
                Icons::draw (g, Icons::user(), r.removeFromRight (22).toFloat().withSizeKeepingCentre (12.0f, 12.0f), Palette::textDim, 1.3f);

            g.setColour (current ? Palette::curve : Palette::text);
            g.setFont (Fonts::regular (14.0f));
            g.drawText (p->name, r.removeFromLeft (r.getWidth() - 110), Justification::centredLeft, true);
            g.setColour (Palette::textFaint);
            g.setFont (Fonts::regular (11.5f));
            g.drawText (p->category, r, Justification::centredRight, true);
        }

        void mouseEnter (const MouseEvent&) override { repaint(); }
        void mouseExit (const MouseEvent&) override  { repaint(); }
        void mouseDown (const MouseEvent& e) override { browser.rowClicked (row, e, starArea(), optionsArea()); }
        void mouseDoubleClick (const MouseEvent&) override { browser.rowDoubleClicked (row); }

    private:
        PresetBrowser& browser;
        int row = -1;
        bool selected = false;
    };

    void styleToggle (TextButton& b)
    {
        b.setColour (TextButton::buttonColourId, Palette::button);
        b.setColour (TextButton::buttonOnColourId, Palette::curve.withAlpha (0.2f));
        b.setColour (TextButton::textColourOffId, Palette::textDim);
        b.setColour (TextButton::textColourOnId, Palette::curve);
    }
}

void PresetBrowser::IconButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (highlighted || down ? Palette::buttonHover : Palette::button);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    Icons::draw (g, icon, r.withSizeKeepingCentre (12.0f, 12.0f), highlighted ? Palette::text : Palette::textDim, 1.5f);
}

//==============================================================================
PresetBrowser::PresetBrowser (ResoOpusProcessor& p) : presets (*p.presets)
{
    setWantsKeyboardFocus (true);

    auto addCategory = [this] (const String& name, const String& value)
    {
        auto* b = categoryButtons.add (new TextButton (name));
        styleToggle (*b);
        b->onClick = [this, value] { category = value; rebuild(); };
        addAndMakeVisible (b);
    };
    addCategory ("All categories", {});
    for (auto& c : PresetManager::categories()) addCategory (c, c);

    for (auto* b : { &filterAll, &filterFactory, &filterUser, &filterFav })
    {
        styleToggle (*b);
        addAndMakeVisible (b);
    }
    filterAll.onClick     = [this] { setFilter (Filter::all); };
    filterFactory.onClick = [this] { setFilter (Filter::factory); };
    filterUser.onClick    = [this] { setFilter (Filter::user); };
    filterFav.onClick     = [this] { setFilter (Filter::favourites); };

    search.setTextToShowWhenEmpty ("Search name, category or description", Palette::textFaint);
    search.setFont (Fonts::regular (13.0f));
    search.setIndents (8, 5);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { close(); };
    search.onEscapeKey = [this] { close(); };
    addAndMakeVisible (search);

    addAndMakeVisible (fileMenu);
    fileMenu.onClick = [this] { showFileMenu(); };
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { close(); };

    for (auto* b : { &saveButton, &saveAsButton, &editInfoButton, &deleteButton })
    {
        b->setColour (TextButton::buttonColourId, Palette::button);
        b->setColour (TextButton::textColourOffId, Palette::text);
        addAndMakeVisible (b);
    }
    saveAsButton.setColour (TextButton::buttonColourId, Palette::curve.withAlpha (0.22f));
    saveAsButton.setColour (TextButton::textColourOffId, Palette::curve);
    saveButton.setTooltip ("Overwrite the current user preset");
    saveAsButton.setTooltip ("Save the current sound as a new user preset");
    saveButton.onClick = [this] { presets.saveCurrent(); };
    saveAsButton.onClick = [this] { showSaveAsDialog(); };
    editInfoButton.onClick = [this] { showEditInfoDialog (previewIndex()); };
    deleteButton.onClick = [this] { confirmDelete (previewIndex()); };

    list.setModel (this);
    list.setRowHeight (28);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (list);

    presets.addChangeListener (this);
    settings->addChangeListener (this);
    setFilter (Filter::all);
}

PresetBrowser::~PresetBrowser()
{
    presets.removeChangeListener (this);
    settings->removeChangeListener (this);
    list.setModel (nullptr);
}

Rectangle<int> PresetBrowser::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (panelWidth, panelHeight).withY (14);
}

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        presets.rescan();
        grabKeyboardFocus();
    }
}

void PresetBrowser::changeListenerCallback (ChangeBroadcaster*) { rebuild(); }

void PresetBrowser::setFilter (Filter f)
{
    filter = f;
    filterAll.setToggleState (f == Filter::all, dontSendNotification);
    filterFactory.setToggleState (f == Filter::factory, dontSendNotification);
    filterUser.setToggleState (f == Filter::user, dontSendNotification);
    filterFav.setToggleState (f == Filter::favourites, dontSendNotification);
    rebuild();
}

void PresetBrowser::rebuild()
{
    visible.clearQuick();
    const auto& all = presets.getPresets();
    const auto query = search.getText().trim().toLowerCase();
    for (int i = 0; i < all.size(); ++i)
    {
        const auto& p = all.getReference (i);
        if (category.isNotEmpty() && p.category != category) continue;
        if (filter == Filter::factory && ! p.isFactory) continue;
        if (filter == Filter::user && p.isFactory) continue;
        if (filter == Filter::favourites && ! settings->isFavourite (p.key())) continue;
        if (query.isNotEmpty() && ! p.name.toLowerCase().contains (query) && ! p.category.toLowerCase().contains (query)
            && ! p.description.toLowerCase().contains (query)) continue;
        visible.add (i);
    }
    for (int i = 0; i < categoryButtons.size(); ++i)
        categoryButtons[i]->setToggleState ((i == 0 && category.isEmpty()) || categoryButtons[i]->getButtonText() == category, dontSendNotification);

    ignoreSelection = true;
    list.updateContent();
    int currentRow = -1;
    for (int r = 0; r < visible.size(); ++r) if (isCurrent (r)) currentRow = r;
    if (currentRow >= 0) list.selectRow (currentRow); else list.deselectAllRows();
    ignoreSelection = false;

    auto* current = presets.getCurrent();
    saveButton.setEnabled (current != nullptr && ! current->isFactory);
    const int pi = previewIndex();
    const bool userPreset = pi >= 0 && ! presets.getPresets().getReference (pi).isFactory;
    editInfoButton.setVisible (userPreset);
    deleteButton.setVisible (userPreset);
    list.repaint();
    repaint();
}

const Preset* PresetBrowser::presetForRow (int row) const
{
    if (! isPositiveAndBelow (row, visible.size())) return nullptr;
    return &presets.getPresets().getReference (visible[row]);
}

bool PresetBrowser::isCurrent (int row) const
{
    auto* p = presetForRow (row);
    return p != nullptr && p->key() == presets.getCurrentKey();
}

int PresetBrowser::previewIndex() const { return presets.indexOf (presets.getCurrentKey()); }

Component* PresetBrowser::refreshComponentForRow (int row, bool selected, Component* existing)
{
    auto* r = dynamic_cast<PresetRow*> (existing);
    if (r == nullptr) { delete existing; r = new PresetRow (*this); }
    r->update (row, selected);
    return r;
}

void PresetBrowser::selectedRowsChanged (int lastRowSelected)
{
    // browsing with the arrow keys loads presets right away
    if (ignoreSelection || lastRowSelected < 0 || isCurrent (lastRowSelected)) return;
    presets.load (visible[lastRowSelected]);
}

void PresetBrowser::rowClicked (int row, const MouseEvent& e, Rectangle<int> starArea, Rectangle<int> optionsArea)
{
    auto* p = presetForRow (row);
    if (p == nullptr) return;
    if (starArea.contains (e.getPosition())) { settings->setFavourite (p->key(), ! settings->isFavourite (p->key())); return; }
    if (optionsArea.contains (e.getPosition()) || e.mods.isPopupMenu()) { showOptionsMenu (visible[row], e.getScreenPosition()); return; }
    presets.load (visible[row]);
    grabKeyboardFocus();
}

void PresetBrowser::rowDoubleClicked (int row)
{
    if (isPositiveAndBelow (row, visible.size()) && ! isCurrent (row)) presets.load (visible[row]);
    close();
}

void PresetBrowser::mouseDown (const MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition())) close();
}

bool PresetBrowser::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::escapeKey || key == KeyPress::returnKey) { close(); return true; }
    if (key == KeyPress::upKey || key == KeyPress::downKey)
    {
        if (visible.isEmpty()) return true;
        const int dir = key == KeyPress::upKey ? -1 : 1;
        int row = list.getSelectedRow();
        row = jlimit (0, visible.size() - 1, row < 0 ? 0 : row + dir);
        list.selectRow (row);
        return true;
    }
    return false;
}

//==============================================================================
void PresetBrowser::showFileMenu()
{
    PopupMenu m;
    m.addItem (1, "Open preset file...");
   #if JUCE_MAC
    m.addItem (2, "Show user presets in Finder");
   #else
    m.addItem (2, "Show user presets in File Explorer");
   #endif
    m.addItem (3, "Rescan presets");
    m.addSeparator();
    m.addItem (4, "Init (reset everything)");

    SafePointer<PresetBrowser> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&fileMenu), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1)
        {
            safe->chooser = std::make_unique<FileChooser> ("Open preset", GlobalSettings::getUserPresetFolder(), "*.ropreset");
            safe->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
            {
                if (safe != nullptr && fc.getResult().existsAsFile()) safe->presets.loadFromFile (fc.getResult());
            });
        }
        else if (r == 2) GlobalSettings::getUserPresetFolder().revealToUser();
        else if (r == 3) safe->presets.rescan();
        else if (r == 4) safe->presets.load (0);
    });
}

void PresetBrowser::showOptionsMenu (int presetIndex, Point<int> screenPos)
{
    const auto& p = presets.getPresets().getReference (presetIndex);
    const bool isDefault = settings->getDefaultPreset() == p.key();
    PopupMenu m;
    m.addItem (1, isDefault ? "Unset as default" : "Set as default (loads in new instances)");
    m.addItem (2, settings->isFavourite (p.key()) ? "Remove from favourites" : "Add to favourites");
    m.addItem (3, "Edit info", ! p.isFactory);
    m.addItem (4, "Delete preset", ! p.isFactory);

    SafePointer<PresetBrowser> safe (this);
    const auto key = p.key();
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (Rectangle<int> (1, 1).withPosition (screenPos)),
                     [safe, key, isDefault] (int r)
    {
        if (safe == nullptr) return;
        auto& st = *safe->settings;
        if (r == 1) st.setDefaultPreset (isDefault ? String() : key);
        if (r == 2) st.setFavourite (key, ! st.isFavourite (key));
        // look the preset up again: the list may have been rescanned while the menu was open
        if (r == 3) safe->showEditInfoDialog (safe->presets.indexOf (key));
        if (r == 4) safe->confirmDelete (safe->presets.indexOf (key));
    });
}

void PresetBrowser::showSaveAsDialog()
{
    auto* w = new AlertWindow ("Save preset as", "Name the preset and choose a category.", MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&getLookAndFeel());
    w->addTextEditor ("name", presets.getCurrentName(), "Name");
    w->addComboBox ("category", PresetManager::categories(), "Category");
    if (auto* current = presets.getCurrent()) w->getComboBoxComponent ("category")->setText (current->category, dontSendNotification);
    w->addTextEditor ("description", {}, "Description");
    auto* asDefault = new ToggleButton ("Load in new instances (user default)");
    auto* asFav = new ToggleButton ("Mark as favourite");
    asDefault->setSize (280, 24);
    asFav->setSize (280, 24);
    w->addCustomComponent (asDefault);
    w->addCustomComponent (asFav);
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<PresetBrowser> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w, asDefault, asFav] (int result)
    {
        std::unique_ptr<ToggleButton> d (asDefault), f (asFav);
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name");
        if (safe->presets.saveAs (name, w->getComboBoxComponent ("category")->getText(), w->getTextEditorContents ("description")))
        {
            const auto key = safe->presets.getCurrentKey();
            if (d->getToggleState()) safe->settings->setDefaultPreset (key);
            if (f->getToggleState()) safe->settings->setFavourite (key, true);
        }
    }), true);
}

void PresetBrowser::showEditInfoDialog (int presetIndex)
{
    if (! isPositiveAndBelow (presetIndex, presets.getPresets().size())) return;
    const auto& p = presets.getPresets().getReference (presetIndex);
    if (p.isFactory) return;

    auto* w = new AlertWindow ("Edit preset info", p.name, MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&getLookAndFeel());
    w->addComboBox ("category", PresetManager::categories(), "Category");
    w->getComboBoxComponent ("category")->setText (p.category, dontSendNotification);
    w->addTextEditor ("description", p.description, "Description");
    auto* asDefault = new ToggleButton ("User default");
    auto* asFav = new ToggleButton ("Favourite");
    asDefault->setToggleState (settings->getDefaultPreset() == p.key(), dontSendNotification);
    asFav->setToggleState (settings->isFavourite (p.key()), dontSendNotification);
    asDefault->setSize (280, 24);
    asFav->setSize (280, 24);
    w->addCustomComponent (asDefault);
    w->addCustomComponent (asFav);
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<PresetBrowser> safe (this);
    const auto key = p.key();
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w, asDefault, asFav, key] (int result)
    {
        std::unique_ptr<ToggleButton> d (asDefault), f (asFav);
        if (safe == nullptr || result != 1) return;
        safe->presets.updateInfo (safe->presets.indexOf (key), w->getComboBoxComponent ("category")->getText(), w->getTextEditorContents ("description"));
        auto& st = *safe->settings;
        if (d->getToggleState()) st.setDefaultPreset (key);
        else if (st.getDefaultPreset() == key) st.setDefaultPreset ({});
        st.setFavourite (key, f->getToggleState());
    }), true);
}

void PresetBrowser::confirmDelete (int presetIndex)
{
    if (! isPositiveAndBelow (presetIndex, presets.getPresets().size())) return;
    const auto& p = presets.getPresets().getReference (presetIndex);
    if (p.isFactory) return;
    SafePointer<PresetBrowser> safe (this);
    const auto key = p.key();
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete preset",
                                  "Delete \"" + p.name + "\"? The file will be removed from disk.", "Delete", "Cancel", this,
                                  ModalCallbackFunction::create ([safe, key] (int r)
                                  {
                                      if (safe != nullptr && r == 1) safe->presets.remove (safe->presets.indexOf (key));
                                  }));
}

//==============================================================================
void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.55f));
    const auto panel = panelBounds().toFloat();
    g.setColour (Colour (0xfa121317));
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

    // separators: categories | list | preview
    g.setColour (Palette::hairline);
    g.fillRect ((float) list.getX() - 11.0f, panel.getY() + 14.0f, 1.0f, panel.getHeight() - 28.0f);
    g.fillRect ((float) previewArea.getX() - 11.0f, panel.getY() + 14.0f, 1.0f, panel.getHeight() - 28.0f);

    auto r = previewArea;
    const int pi = previewIndex();
    if (pi < 0)
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (presets.getCurrentName().isNotEmpty() ? presets.getCurrentName() : String ("No preset selected"), r, Justification::centred);
        return;
    }
    const auto& p = presets.getPresets().getReference (pi);
    g.setColour (Palette::textDim);
    g.setFont (Fonts::bold (10.5f).withExtraKerningFactor (0.08f));
    g.drawText (p.category.toUpperCase() + (p.isFactory ? "  |  FACTORY" : "  |  USER"), r.removeFromTop (16), Justification::centredLeft);
    g.setColour (Palette::text);
    g.setFont (Fonts::bold (18.0f));
    g.drawText (p.name + (presets.isModified() ? " *" : ""), r.removeFromTop (28), Justification::centredLeft, true);
    r.removeFromTop (10);

    // the four voices: type, plate note, HOLD
    auto voices = r.removeFromTop (54);
    const int vw = (voices.getWidth() - 3 * 6) / 4;
    static const char* typeNames[] = { "OFF", "CHOIR", "SIREN", "PULSAR" };
    for (int v = 0; v < Params::numVoices; ++v)
    {
        auto box = voices.removeFromLeft (vw).toFloat();
        voices.removeFromLeft (6);
        const int type = roundToInt (PresetManager::valueIn (p.state, Params::voiceID (v, "type"), 0.0f));
        const auto c = Palette::voiceType (type);
        g.setColour (c.withAlpha (type == 0 ? 0.05f : 0.12f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (c.withAlpha (type == 0 ? 0.25f : 0.6f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (Fonts::bold (10.0f));
        g.drawText (typeNames[jlimit (0, 3, type)], box.removeFromTop (20.0f), Justification::centred);
        if (type != 0)
        {
            g.setColour (Palette::text);
            g.setFont (Fonts::mono (11.0f));
            g.drawText (Params::noteName (PresetManager::valueIn (p.state, Params::voiceID (v, "tune"), 60.0f)), box.removeFromTop (16.0f), Justification::centred);
            const float hold = PresetManager::valueIn (p.state, Params::voiceID (v, "hold"), 0.0f) / 100.0f;
            auto bar = box.reduced (8.0f, 5.0f).withHeight (3.0f);
            g.setColour (Palette::hairline);
            g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (c);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * hold), 1.5f);
        }
    }
    r.removeFromTop (10);

    // GRAVITY and the layers that are on
    auto gr = r.removeFromTop (22);
    const float grav = PresetManager::valueIn (p.state, "gravity", 15.0f) / 100.0f;
    const int zone = Params::gravityZoneIndex (grav);
    const Colour zoneColours[] = { Palette::zoneStill, Palette::zoneOrbit, Palette::zoneTurb, Palette::zoneCollapse };
    g.setColour (Palette::textDim);
    g.setFont (Fonts::bold (10.5f));
    g.drawText ("GRAVITY", gr.removeFromLeft (64), Justification::centredLeft);
    const auto trackArea = gr.removeFromLeft (gr.getWidth() - 96).toFloat();
    auto track = trackArea.withSizeKeepingCentre (trackArea.getWidth() - 8.0f, 4.0f);
    g.setColour (Palette::hairline);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (zoneColours[zone]);
    g.fillRoundedRectangle (track.withWidth (jmax (4.0f, track.getWidth() * grav)), 2.0f);
    g.setFont (Fonts::bold (10.5f));
    g.drawText (Params::gravityZone (grav), gr, Justification::centredRight);
    r.removeFromTop (6);

    StringArray layers;
    if (PresetManager::valueIn (p.state, "lead_on", 0.0f) > 0.5f) layers.add (PresetManager::valueIn (p.state, "arp_on", 0.0f) > 0.5f ? "LEAD + ARP" : "LEAD");
    if (PresetManager::valueIn (p.state, "echo_mix", 0.0f) > 1.0f) layers.add ("ECHO");
    if (PresetManager::valueIn (p.state, "space_blend", 0.0f) > 1.0f)
    {
        static const char* algos[] = { "AURORA", "VOID", "RUNNER", "SATURN", "COMET" };
        layers.add (algos[jlimit (0, 4, roundToInt (PresetManager::valueIn (p.state, "space_algo", 1.0f)))]);
    }
    if (PresetManager::valueIn (p.state, "flt_cutoff", 20000.0f) < 15000.0f) layers.add ("FILTER");
    int mods = 0;
    for (int s = 0; s < Params::numModSlots; ++s) if (PresetManager::valueIn (p.state, Params::modID (s, "on"), 0.0f) > 0.5f
                                                         && PresetManager::valueIn (p.state, Params::modID (s, "src"), 0.0f) > 0.5f
                                                         && PresetManager::valueIn (p.state, Params::modID (s, "dst"), 0.0f) > 0.5f) ++mods;
    if (mods > 0) layers.add (String (mods) + (mods == 1 ? " MOD" : " MODS"));
    g.setColour (Palette::textFaint);
    g.setFont (Fonts::regular (11.5f));
    g.drawText (layers.isEmpty() ? String ("DRONE ONLY") : layers.joinIntoString ("  ·  "), r.removeFromTop (18), Justification::centredLeft, true);
    r.removeFromTop (12);

    g.setColour (Palette::label);
    g.setFont (Fonts::regular (13.0f));
    r.removeFromBottom (34);
    g.drawFittedText (p.description.isNotEmpty() ? p.description : String ("No description."), r, Justification::topLeft, 12, 1.0f);
}

void PresetBrowser::resized()
{
    auto r = panelBounds().reduced (16);

    auto cats = r.removeFromLeft (140);
    for (auto* b : categoryButtons)
    {
        b->setBounds (cats.removeFromTop (28));
        cats.removeFromTop (4);
    }
    r.removeFromLeft (22);

    previewArea = r.removeFromRight (280);
    auto previewButtons = previewArea.withTrimmedTop (previewArea.getHeight() - 26);
    editInfoButton.setBounds (previewButtons.removeFromLeft (90));
    previewButtons.removeFromLeft (6);
    deleteButton.setBounds (previewButtons.removeFromLeft (80));
    r.removeFromRight (22);

    auto top = r.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (28));
    for (auto* b : { &filterAll, &filterFactory, &filterUser, &filterFav })
    {
        b->setBounds (top.removeFromLeft (b == &filterFav ? 88 : 66));
        top.removeFromLeft (4);
    }
    r.removeFromTop (8);

    auto tools = r.removeFromTop (28);
    fileMenu.setBounds (tools.removeFromLeft (28));
    tools.removeFromLeft (6);
    saveAsButton.setBounds (tools.removeFromRight (72));
    tools.removeFromRight (4);
    saveButton.setBounds (tools.removeFromRight (56));
    tools.removeFromRight (6);
    search.setBounds (tools);
    r.removeFromTop (8);

    list.setBounds (r);
}

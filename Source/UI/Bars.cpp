#include "Bars.h"
#include "PluginProcessor.h"
#include "State/FactoryPresets.h"
#include "Keyboard.h"

using namespace juce;

const StringArray& TopBar::tabNames()
{
    static const StringArray t { "DRONE", String (CharPointer_UTF8 ("LEAD \xc2\xb7 FX")) };
    return t;
}

TopBar::TopBar (ResoOpusProcessor& p, GlobalSettings& s) : proc (p), settings (s)
{
    if (proc.history) proc.history->addChangeListener (this);
}

TopBar::~TopBar()
{
    if (proc.history) proc.history->removeChangeListener (this);
}

Rectangle<float> TopBar::area (int item) const
{
    const float h = (float) height;
    auto tabW = [] (int i) { return (float) GlyphArrangement::getStringWidthInt (Fonts::bold (11.0f), tabNames()[i]) + 22.0f + (i == 1 ? 22.0f : 0.0f); };
    const int numTabs = tabNames().size();
    if (item >= Tab0 && item < Tab0 + numTabs)
    {
        float x = 104.0f;
        for (int i = 0; i < item; ++i) x += tabW (i) + 4.0f;
        return { x, 0.0f, tabW (item), h };
    }
    if (item == LeadPower) { const auto t = area (Tab0 + 1); return { t.getX() + 6.0f, 8.0f, 20.0f, 20.0f }; }
    // preset browser in the middle, edit tools (undo, redo, A/B, Copy) on the right next to Mod and Help
    const float cx = getWidth() * 0.5f;
    const float w = (float) getWidth();
    switch (item)
    {
        case Prev: return { cx - 105.0f - 26.0f, 6.0f, 20.0f, 24.0f };
        case Name: return { cx - 105.0f, 6.0f, 210.0f, 24.0f };
        case Next: return { cx + 105.0f + 6.0f, 6.0f, 20.0f, 24.0f };
        case Undo: return { w - 284.0f, 8.0f, 20.0f, 20.0f };
        case Redo: return { w - 258.0f, 8.0f, 20.0f, 20.0f };
        case AB:   return { w - 226.0f, 6.0f, 32.0f, 24.0f };
        case Copy: return { w - 190.0f, 6.0f, 38.0f, 24.0f };
        case Mod:  return { w - 142.0f, 6.0f, 42.0f, 24.0f };
        case Help: return { (float) getWidth() - 94.0f, 6.0f, 40.0f, 24.0f };
        default:   return {};
    }
}

int TopBar::itemAt (Point<float> p) const
{
    if (area (LeadPower).contains (p)) return LeadPower;
    for (int i = 0; i < tabNames().size(); ++i) if (area (i).contains (p)) return i;
    for (int i = Undo; i <= Help; ++i) if (area (i).contains (p)) return i;
    return None;
}

void TopBar::refresh()
{
    const auto n = proc.getPresetName();
    const bool lo = proc.getParamReal ("lead_on") > 0.5f;
    if (n != shownName || lo != leadOn) { shownName = n; leadOn = lo; repaint(); }
}

void TopBar::paint (Graphics& g)
{
    g.fillAll (Palette::bar);
    g.setColour (Colour (0xff16171b));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    // logo
    g.setFont (Fonts::bold (16.0f));
    g.setColour (Palette::text);
    g.drawText ("Reso", 14, 0, 60, height, Justification::centredLeft, false);
    g.setColour (Palette::curve);
    g.drawText ("Opus", 14 + GlyphArrangement::getStringWidthInt (Fonts::bold (16.0f), "Reso"), 0, 60, height, Justification::centredLeft, false);

    // tabs
    for (int i = 0; i < tabNames().size(); ++i)
    {
        auto r = area (i);
        const bool on = i == page;
        if (i == 1)
        {
            // LEAD power: gold = playing, dim = off
            const auto pr = area (LeadPower);
            if (leadOn) { g.setColour (Palette::curve.withAlpha (0.18f)); g.fillEllipse (pr.reduced (1.0f)); }
            Icons::draw (g, Icons::power(), pr.reduced (4.0f), leadOn ? Palette::curve : (hover == LeadPower ? Palette::textDim : Palette::textFaint), 1.7f);
            r = r.withTrimmedLeft (22.0f);
        }
        g.setColour (on ? Palette::text : (hover == i ? Palette::textDim : Palette::textFaint));
        g.setFont (Fonts::bold (11.0f).withExtraKerningFactor (0.06f));
        g.drawText (tabNames()[i], r, Justification::centred, false);
        if (on)
        {
            g.setColour (Palette::curve);
            g.fillRoundedRectangle (r.getX() + 11.0f, r.getBottom() - 5.0f, r.getWidth() - 22.0f, 2.0f, 1.0f);
        }
    }

    auto* h = proc.history.get();
    Icons::draw (g, Icons::undo(), area (Undo).reduced (2.0f), Palette::textDim.withAlpha (h && h->canUndo() ? (hover == Undo ? 1.0f : 0.8f) : 0.3f));
    Icons::draw (g, Icons::redo(), area (Redo).reduced (2.0f), Palette::textDim.withAlpha (h && h->canRedo() ? (hover == Redo ? 1.0f : 0.8f) : 0.3f));

    g.setFont (Fonts::bold (12.0f));
    const int slot = h ? h->getActiveSlot() : 0;
    auto abR = area (AB);
    g.setColour (slot == 0 ? Palette::text : Palette::textDim);
    g.drawText ("A", abR.withWidth (12.0f).translated (2.0f, 0.0f), Justification::centred, false);
    g.setColour (Palette::textFaint);
    g.drawText ("/", abR.withWidth (8.0f).translated (12.0f, 0.0f), Justification::centred, false);
    g.setColour (slot == 1 ? Palette::text : Palette::textDim);
    g.drawText ("B", abR.withWidth (12.0f).translated (18.0f, 0.0f), Justification::centred, false);
    g.setFont (Fonts::regular (12.0f));
    g.setColour (hover == Copy ? Palette::text : Palette::textDim);
    g.drawText ("Copy", area (Copy), Justification::centred, false);

    // preset
    Icons::draw (g, Icons::chevron (false), area (Prev).reduced (5.0f, 7.0f), hover == Prev ? Palette::text : Palette::textDim);
    Icons::draw (g, Icons::chevron (true), area (Next).reduced (5.0f, 7.0f), hover == Next ? Palette::text : Palette::textDim);
    auto nr = area (Name);
    g.setColour (hover == Name ? Palette::buttonHover : Colour (0xff15161a));
    g.fillRoundedRectangle (nr, 5.0f);
    g.setColour (Colour (0xff23242a));
    g.drawRoundedRectangle (nr.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (proc.getPresetName(), nr, Justification::centred, true);

    // Mod, Help
    const bool modOpen = isModOpen && isModOpen();
    auto mr = area (Mod);
    if (modOpen)
    {
        g.setColour (Colour (0xff2a2415));
        g.fillRoundedRectangle (mr, 4.0f);
    }
    g.setFont (Fonts::regular (12.0f));
    g.setColour (modOpen ? Palette::curve : (hover == Mod ? Palette::text : Palette::textDim));
    g.drawText ("Mod", mr, Justification::centred, false);
    g.setColour (hover == Help ? Palette::text : Palette::textDim);
    g.drawText ("Help", area (Help), Justification::centred, false);
}

void TopBar::mouseMove (const MouseEvent& e)
{
    const int h = itemAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void TopBar::mouseExit (const MouseEvent&) { hover = None; repaint(); }

String TopBar::getTooltipFor (Point<float> p) const
{
    switch (itemAt (p))
    {
        case LeadPower: return leadOn ? "LEAD is on: keys outside the plates play it - click to switch it off" : "LEAD is off - click to switch it on";
        case Undo: return "Undo (Cmd/Ctrl+Z)";
        case Redo: return "Redo (Shift+Cmd/Ctrl+Z)";
        case AB:   return "Compare two settings: click to switch between A and B";
        case Copy: return "Copy the current setting to the other slot (A to B or B to A)";
        case Name: return "Presets: factory sounds by category, save and load your own";
        case Mod:  return "Modulation panel: all routings, amounts, controllers and functions (key 9)";
        case Help: return "Help, tooltips, window size";
        default:   return {};
    }
}

void TopBar::mouseDown (const MouseEvent& e)
{
    const int it = itemAt (e.position);
    auto* h = proc.history.get();
    if (it == LeadPower) { proc.setParamReal ("lead_on", leadOn ? 0.0f : 1.0f); refresh(); return; }
    if (it >= 0 && it < tabNames().size()) { if (onTab) onTab (it); return; }
    switch (it)
    {
        case Undo: if (h) h->undo(); break;
        case Redo: if (h) h->redo(); break;
        case AB:   if (h) h->switchTo (1 - h->getActiveSlot()); break;
        case Copy: if (h) h->copyToOther(); break;
        case Prev: stepPreset (-1); break;
        case Next: stepPreset (1); break;
        case Name: showPresetMenu(); break;
        case Mod:  if (onMod) onMod(); break;
        case Help: showHelpMenu(); break;
        default: break;
    }
    repaint();
}

void TopBar::stepPreset (int dir)
{
    const int n = (int) factoryPresets().size();
    const int next = ((proc.getCurrentProgram() + dir) % n + n) % n;
    loadFactory (next);
}

void TopBar::loadFactory (int index)
{
    if (settings.getBool (GlobalSettings::silentPresets))
    {
        auto& pr = proc;
        pr.changeSilently ([&pr, index] { pr.loadFactoryPreset (index); });
    }
    else
        proc.loadFactoryPreset (index);
}

void TopBar::showPresetMenu()
{
    PopupMenu m;
    StringArray cats;
    for (auto& p : factoryPresets()) cats.addIfNotAlreadyThere (p.category);
    PopupMenu factory;
    for (auto& c : cats)
    {
        PopupMenu sub;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
            if (c == factoryPresets()[(size_t) i].category)
                sub.addItem (100 + i, factoryPresets()[(size_t) i].name, true, proc.getPresetName() == factoryPresets()[(size_t) i].name);
        factory.addSubMenu (c, sub);
    }
    m.addSubMenu ("Factory", factory);
    m.addSeparator();
    m.addItem (1, "Save preset...");
    m.addItem (2, "Load preset...");
    m.addSeparator();
    m.addItem (3, "Init (reset everything)");

    // description of the current factory preset
    for (auto& p : factoryPresets())
        if (proc.getPresetName() == p.name)
        {
            m.addSeparator();
            m.addSectionHeader (p.description);
        }

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (area (Name).toNearestInt())),
                     [sp = SafePointer<TopBar> (this)] (int r)
    {
        if (sp == nullptr || r == 0) return;
        if (r >= 100) sp->loadFactory (r - 100);
        else if (r == 1) sp->savePreset();
        else if (r == 2) sp->loadPreset();
        else if (r == 3) sp->loadFactory (0);
        sp->refresh();
    });
}

static File presetFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Gavr").getChildFile ("ResoOpus Presets");
    f.createDirectory();
    return f;
}

void TopBar::savePreset()
{
    chooser = std::make_unique<FileChooser> ("Save preset", presetFolder().getChildFile (proc.getPresetName() + ".resoopus"), "*.resoopus");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [sp = SafePointer<TopBar> (this)] (const FileChooser& fc)
    {
        if (sp == nullptr) return;
        auto f = fc.getResult();
        if (f == File()) return;
        sp->proc.setPresetName (f.getFileNameWithoutExtension());
        MemoryBlock mb;
        sp->proc.getStateInformation (mb);
        f.withFileExtension ("resoopus").replaceWithData (mb.getData(), mb.getSize());
        sp->refresh();
    });
}

void TopBar::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Load preset", presetFolder(), "*.resoopus");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [sp = SafePointer<TopBar> (this)] (const FileChooser& fc)
    {
        if (sp == nullptr) return;
        auto f = fc.getResult();
        auto mb = std::make_shared<MemoryBlock>();
        if (f.existsAsFile() && f.loadFileAsData (*mb))
        {
            auto& pr = sp->proc;
            auto apply = [&pr, mb, name = f.getFileNameWithoutExtension()]
            {
                pr.setStateInformation (mb->getData(), (int) mb->getSize());
                pr.setPresetName (name);
            };
            if (sp->settings.getBool (GlobalSettings::silentPresets)) pr.changeSilently (apply); else apply();
        }
    });
}

void TopBar::showHelpMenu()
{
    PopupMenu m;
    m.addItem (1, "Show tooltips", true, settings.getBool (GlobalSettings::tooltips));
    m.addItem (3, "Silent preset changes (fade out, clear echoes, fade in)", true, settings.getBool (GlobalSettings::silentPresets));
    PopupMenu size;
    const int cur = getScale ? getScale() : 100;
    for (int s : { 70, 80, 90, 100, 115, 130, 150 })
        size.addItem (100 + s, String (s) + " %", true, cur == s);
    m.addSubMenu ("Window size", size);
    m.addSeparator();
    m.addSectionHeader ("Plates: the white keys C3-F3 play voices 1-4; HOLD keeps them droning");
    m.addSectionHeader ("Knobs: drag, Shift = fine, double-click = type (D2+12, 3 s, L 30), Cmd-click = default");
    m.addSectionHeader ("Keys: 9 modulation panel, Cmd+Z undo");
    m.addSeparator();
    m.addItem (2, "About ResoOpus " + String (JucePlugin_VersionString) + " - Gavr", false);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (area (Help).toNearestInt())),
                     [sp = SafePointer<TopBar> (this)] (int r)
    {
        if (sp == nullptr || r == 0) return;
        if (r == 1) sp->settings.setBool (GlobalSettings::tooltips, ! sp->settings.getBool (GlobalSettings::tooltips));
        else if (r == 3) sp->settings.setBool (GlobalSettings::silentPresets, ! sp->settings.getBool (GlobalSettings::silentPresets));
        else if (r >= 100 && sp->onScale) sp->onScale (r - 100);
    });
}

//==============================================================================
BottomBar::BottomBar (ResoOpusProcessor& p) : proc (p) {}

Rectangle<float> BottomBar::area (int item) const
{
    switch (item)
    {
        case Play:     return { 10.0f, 0.0f, 120.0f, (float) height };
        case TouchSrc: return { 134.0f, 0.0f, 130.0f, (float) height };
        case SnapMode: return { 268.0f, 0.0f, 170.0f, (float) height };
        case OS:       return { (float) getWidth() - 316.0f, 0.0f, 120.0f, (float) height };
        default:       return {};
    }
}

int BottomBar::itemAt (Point<float> p) const
{
    for (int i = Play; i <= OS; ++i) if (area (i).contains (p)) return i;
    return None;
}

void BottomBar::refresh()
{
    const float c = proc.cpuLoad.load(), pk = proc.meterPeak.load();
    const auto n = proc.getPresetName();
    const int now[4] = { roundToInt (proc.getParamReal ("play_mode")), roundToInt (proc.getParamReal ("touch")),
                         roundToInt (proc.getParamReal ("snap")) * 1000 + roundToInt (proc.getParamReal ("snap_scale")) * 20 + roundToInt (proc.getParamReal ("snap_root")),
                         roundToInt (proc.getParamReal ("oversampling")) };
    bool changed = false;
    for (int i = 0; i < 4; ++i) if (now[i] != shown[i]) { shown[i] = now[i]; changed = true; }
    if (changed || std::abs (c - cpu) > 0.5f || std::abs (pk - peak) > 0.3f || n != preset)
    {
        cpu = c; peak = pk; preset = n;
        repaint();
    }
}

static String choiceText (ResoOpusProcessor& p, const char* id)
{
    if (auto* d = Params::findDef (id)) return Params::format (*d, p.getParamReal (id));
    return {};
}

void BottomBar::paint (Graphics& g)
{
    g.fillAll (Palette::bar);
    g.setColour (Colour (0xff16171b));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    g.setFont (Fonts::regular (12.0f));

    const bool plates = roundToInt (proc.getParamReal ("play_mode")) == GP::Plates;
    g.setColour (Palette::text);
    g.drawText (plates ? "Plates " + KeyboardNames::range (proc.plateNote (0), proc.plateNote (Params::numVoices - 1)) : String ("Keys"), area (Play), Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.drawText ("Touch - " + choiceText (proc, "touch"), area (TouchSrc), Justification::centredLeft, false);
    const int snap = roundToInt (proc.getParamReal ("snap"));
    g.drawText ("Snap - " + (snap == GP::SnapScale ? choiceText (proc, "snap_root") + " " + choiceText (proc, "snap_scale") : choiceText (proc, "snap")),
                area (SnapMode), Justification::centredLeft, true);

    // preset pill
    const float w = 240.0f;
    auto pill = Rectangle<float> ((getWidth() - w) * 0.5f, 3.0f, w, (float) height - 6.0f);
    g.setColour (Colour (0xff15161a));
    g.fillRoundedRectangle (pill, 5.0f);
    String cat;
    for (auto& p : factoryPresets()) if (preset == p.name) cat = p.category;
    g.setColour (Palette::curve);
    g.fillEllipse (pill.getX() + 14.0f, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (Palette::text);
    g.drawText ((cat.isNotEmpty() ? cat + " - " : String()) + preset, pill.withTrimmedLeft (26.0f).withTrimmedRight (10.0f), Justification::centred, true);

    const int os = roundToInt (proc.getParamReal ("oversampling"));
    g.setColour (Palette::textDim);
    g.drawText ("Oversampling " + String (1 << os) + "x", area (OS), Justification::centredRight, false);
    g.setFont (Fonts::mono (11.5f));
    g.setColour (Palette::text);
    g.drawText ("CPU " + String (roundToInt (cpu)) + " %", getWidth() - 186, 0, 74, height, Justification::centredRight, false);
    g.setColour (peak > -0.1f ? Palette::red : Palette::text);
    g.drawText (peak < -90.0f ? String ("-inf dB") : String (peak, 1) + " dB", getWidth() - 106, 0, 74, height, Justification::centredRight, false);
}

void BottomBar::mouseMove (const MouseEvent& e)
{
    switch (itemAt (e.position))
    {
        case Play:     setTooltip ("Plates: eight white keys play the eight voices with their own tuning. Keys: a played note retunes a voice"); break;
        case TouchSrc: setTooltip ("What makes a played voice louder than HOLD: velocity, aftertouch (pressure after the strike) or both"); break;
        case SnapMode: setTooltip ("Free: microtonal. Chromatic: voices snap to semitones. Scale: voices snap to the chosen scale"); break;
        case OS:       setTooltip ("Oversampling of the voices: 2x is the sweet spot, 4x for the cleanest high notes and pull"); break;
        default:       setTooltip ("Output peak level / CPU of this instance"); break;
    }
}

void BottomBar::mouseDown (const MouseEvent& e)
{
    auto choiceMenu = [this] (const char* id)
    {
        auto* d = Params::findDef (id);
        if (d == nullptr) return;
        PopupMenu m;
        const int cur = roundToInt (proc.getParamReal (id));
        for (int i = 0; i < d->choices.size(); ++i) m.addItem (i + 1, d->choices[i], true, cur == i);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp = SafePointer<BottomBar> (this), id] (int r)
        { if (sp && r > 0) { sp->proc.setParamReal (id, (float) (r - 1)); sp->refresh(); } });
    };
    switch (itemAt (e.position))
    {
        case Play:     proc.setParamReal ("play_mode", proc.getParamReal ("play_mode") > 0.5f ? 0.0f : 1.0f); break;
        case TouchSrc: choiceMenu ("touch"); break;
        case SnapMode:
        {
            PopupMenu m, scales, roots;
            const int snap = roundToInt (proc.getParamReal ("snap"));
            m.addItem (1, "Free (microtonal)", true, snap == 0);
            m.addItem (2, "Chromatic", true, snap == 1);
            m.addItem (3, "Scale", true, snap == 2);
            for (int i = 0; i < Params::scaleNames().size(); ++i) scales.addItem (100 + i, Params::scaleNames()[i], true, roundToInt (proc.getParamReal ("snap_scale")) == i);
            for (int i = 0; i < 12; ++i) roots.addItem (200 + i, Params::findDef ("snap_root")->choices[i], true, roundToInt (proc.getParamReal ("snap_root")) == i);
            m.addSeparator();
            m.addSubMenu ("Scale", scales);
            m.addSubMenu ("Root", roots);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp = SafePointer<BottomBar> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                if (r < 100) sp->proc.setParamReal ("snap", (float) (r - 1));
                else if (r < 200) sp->proc.setParamReal ("snap_scale", (float) (r - 100));
                else sp->proc.setParamReal ("snap_root", (float) (r - 200));
                sp->refresh();
            });
            break;
        }
        case OS:
        {
            PopupMenu m;
            const int cur = roundToInt (proc.getParamReal ("oversampling"));
            m.addItem (1, "1x - lowest CPU, some aliasing on high notes", true, cur == 0);
            m.addItem (2, "2x - recommended", true, cur == 1);
            m.addItem (3, "4x - cleanest highs and pull, about 2x CPU", true, cur == 2);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp = SafePointer<BottomBar> (this)] (int r)
            { if (sp && r > 0) { sp->proc.setParamReal ("oversampling", (float) (r - 1)); sp->repaint(); } });
            break;
        }
        default: break;
    }
    refresh();
}

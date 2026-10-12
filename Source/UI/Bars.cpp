#include "Bars.h"
#include "PluginProcessor.h"
#include "Keyboard.h"

using namespace juce;

const StringArray& TopBar::tabNames()
{
    static const StringArray t { "DRONE", "LEAD", "FX", "MOD" };
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
    auto tabW = [] (int i) { return (float) GlyphArrangement::getStringWidthInt (Fonts::bold (11.0f), tabNames()[i]) + 22.0f + (i <= 1 ? 22.0f : 0.0f) + (i == 3 ? 16.0f : 0.0f); };
    const int numTabs = tabNames().size();
    if (item >= Tab0 && item < Tab0 + numTabs)
    {
        float x = 104.0f;
        for (int i = 0; i < item; ++i) x += tabW (i) + 4.0f;
        return { x, 0.0f, tabW (item), h };
    }
    if (item == LeadPower) { const auto t = area (Tab0 + 1); return { t.getX() + 6.0f, 8.0f, 20.0f, 20.0f }; }
    if (item == DronePower) { const auto t = area (Tab0); return { t.getX() + 6.0f, 8.0f, 20.0f, 20.0f }; }
    // preset browser in the middle, edit tools (undo, redo, A/B, Copy) on the right next to Mod and Help
    const float cx = getWidth() * 0.5f;
    const float w = (float) getWidth();
    switch (item)
    {
        case Prev: return { cx - 90.0f - 24.0f, 6.0f, 20.0f, 24.0f };
        case Name: return { cx - 90.0f, 6.0f, 180.0f, 24.0f };
        case Next: return { cx + 90.0f + 4.0f, 6.0f, 20.0f, 24.0f };
        case Undo: return { w - 284.0f, 8.0f, 20.0f, 20.0f };
        case Redo: return { w - 258.0f, 8.0f, 20.0f, 20.0f };
        case AB:   return { w - 226.0f, 6.0f, 32.0f, 24.0f };
        case Copy: return { w - 190.0f, 6.0f, 38.0f, 24.0f };
        case Mod:  return {};   // the MOD page is a tab now
        case Help: return { w - 96.0f, 6.0f, 40.0f, 24.0f };
        case Tips: return { w - 52.0f, 8.0f, 20.0f, 20.0f };
        default:   return {};
    }
}

int TopBar::itemAt (Point<float> p) const
{
    if (area (LeadPower).contains (p)) return LeadPower;
    if (area (DronePower).contains (p)) return DronePower;
    for (int i = 0; i < tabNames().size(); ++i) if (area (i).contains (p)) return i;
    for (int i = Undo; i <= Tips; ++i) if (area (i).contains (p)) return i;
    return None;
}

void TopBar::refresh()
{
    const auto n = proc.getPresetName();
    const bool lo = proc.getParamReal ("lead_on") > 0.5f, dr = proc.getParamReal ("drone_on") > 0.5f;
    const bool mod = proc.presets->isModified();
    int mc = 0;
    for (int s = 0; s < Params::numModSlots; ++s)
        if (proc.getParamReal (Params::modID (s, "src")) > 0.5f && proc.getParamReal (Params::modID (s, "dst")) > 0.5f) ++mc;
    if (n != shownName || lo != leadOn || dr != droneOn || mod != shownModified || mc != modCount)
    { shownName = n; leadOn = lo; droneOn = dr; shownModified = mod; modCount = mc; repaint(); }
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
        if (i <= 1)
        {
            // DRONE / LEAD power: gold = playing, dim = off (DRONE off + LEAD on = LEAD only)
            const int item = i == 0 ? DronePower : LeadPower;
            const bool onNow = i == 0 ? droneOn : leadOn;
            const auto pr = area (item);
            if (onNow) { g.setColour (Palette::curve.withAlpha (0.18f)); g.fillEllipse (pr.reduced (1.0f)); }
            Icons::draw (g, Icons::power(), pr.reduced (4.0f), onNow ? Palette::curve : (hover == item ? Palette::textDim : Palette::textFaint), 1.7f);
            r = r.withTrimmedLeft (22.0f);
        }
        g.setColour (on ? Palette::text : (hover == i ? Palette::textDim : Palette::textFaint));
        g.setFont (Fonts::bold (11.0f).withExtraKerningFactor (0.06f));
        if (i == 3)
        {
            // MOD: the number of routings in a small gold badge
            const auto tr = r.withTrimmedRight (16.0f);
            g.drawText (tabNames()[i], tr, Justification::centred, false);
            if (modCount > 0)
            {
                const auto b = Rectangle<float> (16.0f, 14.0f).withCentre ({ tr.getRight() + 4.0f, r.getCentreY() });
                g.setColour (Palette::curve.withAlpha (on ? 1.0f : 0.7f));
                g.fillRoundedRectangle (b, 7.0f);
                g.setColour (Palette::bar);
                g.setFont (Fonts::bold (9.5f));
                g.drawText (String (modCount), b, Justification::centred, false);
            }
        }
        else
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
    const bool browserOpen = isBrowserOpen && isBrowserOpen();
    g.setColour (hover == Name || browserOpen ? Palette::buttonHover : Colour (0xff15161a));
    g.fillRoundedRectangle (nr, 5.0f);
    g.setColour (browserOpen ? Palette::curve.withAlpha (0.6f) : Colour (0xff23242a));
    g.drawRoundedRectangle (nr.reduced (0.5f), 5.0f, 1.0f);
    Icons::draw (g, Icons::menu(), Rectangle<float> (nr.getX() + 9.0f, nr.getCentreY() - 5.0f, 10.0f, 10.0f), Palette::textDim, 1.3f);
    g.setColour (Palette::text);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (proc.getPresetName() + (shownModified ? " *" : ""), nr.reduced (24.0f, 0.0f), Justification::centred, true);

    // Mod, Help
    g.setFont (Fonts::regular (12.0f));
    g.setColour (hover == Help ? Palette::text : Palette::textDim);
    g.drawText ("Help", area (Help), Justification::centred, false);

    // ? = tooltips on (gold) / off
    const bool tips = settings.getBool (GlobalSettings::tooltips, false);
    const auto tr = area (Tips).reduced (1.0f);
    if (tips) { g.setColour (Palette::curve); g.fillEllipse (tr); }
    else { g.setColour (hover == Tips ? Palette::text : Palette::textDim); g.drawEllipse (tr.reduced (0.6f), 1.2f); }
    g.setColour (tips ? Palette::bar : (hover == Tips ? Palette::text : Palette::textDim));
    g.setFont (Fonts::bold (12.0f));
    g.drawText ("?", tr.translated (0.0f, 0.5f), Justification::centred, false);
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
        case LeadPower: return leadOn ? "LEAD is on: keys outside the plates play it (with the DRONE off: every key) - click to switch it off" : "LEAD is off - click to switch it on";
        case DronePower: return droneOn ? "DRONE is on - click to switch it off (with the LEAD on: LEAD only, every key plays the LEAD)" : "DRONE is off - click to switch it on";
        case Undo: return "Undo (Cmd/Ctrl+Z)";
        case Redo: return "Redo (Shift+Cmd/Ctrl+Z)";
        case AB:   return "Compare two settings: click to switch between A and B";
        case Copy: return "Copy the current setting to the other slot (A to B or B to A)";
        case Name: return "Preset browser: categories, favourites, search, save your own";
        case Mod:  return "Modulation panel: all routings, amounts, controllers and functions (key 9)";
        case Help: return "Help, window size, preset changes";
        case Tips: return "Tooltips on / off";
        default:   return {};
    }
}

void TopBar::mouseDown (const MouseEvent& e)
{
    const int it = itemAt (e.position);
    auto* h = proc.history.get();
    if (it == LeadPower) { proc.setParamReal ("lead_on", leadOn ? 0.0f : 1.0f); refresh(); return; }
    if (it == DronePower) { proc.setParamReal ("drone_on", droneOn ? 0.0f : 1.0f); refresh(); return; }
    if (it >= 0 && it < tabNames().size()) { if (onTab) onTab (it); return; }
    switch (it)
    {
        case Undo: if (h) h->undo(); break;
        case Redo: if (h) h->redo(); break;
        case AB:   if (h) h->switchTo (1 - h->getActiveSlot()); break;
        case Copy: if (h) h->copyToOther(); break;
        case Prev: proc.presets->next (-1); break;
        case Next: proc.presets->next (1); break;
        case Name: if (onPresets) onPresets(); break;
        case Tips: settings.setBool (GlobalSettings::tooltips, ! settings.getBool (GlobalSettings::tooltips, false)); break;
        case Mod:  if (onMod) onMod(); break;
        case Help: showHelpMenu(); break;
        default: break;
    }
    repaint();
}

void TopBar::showHelpMenu()
{
    PopupMenu m;
    m.addItem (1, "Show tooltips (the ? button)", true, settings.getBool (GlobalSettings::tooltips, false));
    m.addItem (3, "Silent preset changes (fade out, clear echoes, fade in)", true, settings.getBool (GlobalSettings::silentPresets));
    PopupMenu size;
    const int cur = getScale ? getScale() : 100;
    for (int s : { 70, 80, 90, 100, 115, 130, 150 })
        size.addItem (100 + s, String (s) + " %", true, cur == s);
    m.addSubMenu ("Window size", size);
    PopupMenu tuning, bend, plates;
    const int a4 = roundToInt (proc.getParamReal ("a4"));
    for (int f : { 415, 432, 440, 442, 444 }) tuning.addItem (300 + f - 400, "A = " + String (f) + " Hz", true, a4 == f);
    const int pb = roundToInt (proc.getParamReal ("pb_range"));
    for (int r : { 0, 1, 2, 7, 12, 24 }) bend.addItem (400 + r, r == 0 ? String ("Off") : String (r) + " semitones", true, pb == r);
    const int base = roundToInt (proc.getParamReal ("plates_base"));
    for (int n : { 36, 48, 60, 72 }) plates.addItem (500 + n, "From " + Params::noteName (n), true, base == n);
    m.addSubMenu ("Tuning (reference A)", tuning);
    m.addSubMenu ("Pitch bend range", bend);
    m.addSubMenu ("Plates position", plates);
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
        if (r == 1) sp->settings.setBool (GlobalSettings::tooltips, ! sp->settings.getBool (GlobalSettings::tooltips, false));
        else if (r == 3) sp->settings.setBool (GlobalSettings::silentPresets, ! sp->settings.getBool (GlobalSettings::silentPresets));
        else if (r >= 500) sp->proc.setParamReal ("plates_base", (float) (r - 500));
        else if (r >= 400) sp->proc.setParamReal ("pb_range", (float) (r - 400));
        else if (r >= 300) sp->proc.setParamReal ("a4", (float) (r - 300 + 400));
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
        case TouchSrc: return { 136.0f, 0.0f, 136.0f, (float) height };
        case SnapMode: return { 278.0f, 0.0f, 196.0f, (float) height };
        case OS:       return { (float) getWidth() - 330.0f, 0.0f, 134.0f, (float) height };
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

    // select buttons: dark pill, label dim, value bright, a down arrow
    auto pillButton = [&g] (Rectangle<float> a, const String& label, const String& value)
    {
        auto b = a.reduced (0.0f, 4.0f);
        g.setColour (Palette::button);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
        auto t = b.reduced (8.0f, 0.0f).withTrimmedRight (12.0f);
        g.setFont (Fonts::regular (11.5f));
        if (label.isNotEmpty())
        {
            g.setColour (Palette::textDim);
            const int lw = GlyphArrangement::getStringWidthInt (Fonts::regular (11.5f), label) + 6;
            g.drawText (label, t.removeFromLeft ((float) lw), Justification::centredLeft, false);
        }
        g.setColour (Palette::text);
        g.drawText (value, t, Justification::centredLeft, true);
        Icons::draw (g, Icons::chevronDown(), Rectangle<float> (8.0f, 8.0f).withCentre ({ b.getRight() - 10.0f, b.getCentreY() }), Palette::textDim, 1.4f);
    };
    const bool plates = roundToInt (proc.getParamReal ("play_mode")) == GP::Plates;
    pillButton (area (Play), {}, plates ? "Plates " + KeyboardNames::range (proc.plateNote (0), proc.plateNote (Params::numVoices - 1)) : String ("Keys"));
    pillButton (area (TouchSrc), "Touch", choiceText (proc, "touch"));
    const int snap = roundToInt (proc.getParamReal ("snap"));
    pillButton (area (SnapMode), "Snap", snap == GP::SnapScale ? choiceText (proc, "snap_root") + " " + choiceText (proc, "snap_scale") : choiceText (proc, "snap"));

    // preset pill
    const float w = 240.0f;
    auto pill = Rectangle<float> ((getWidth() - w) * 0.5f, 3.0f, w, (float) height - 6.0f);
    g.setColour (Colour (0xff15161a));
    g.fillRoundedRectangle (pill, 5.0f);
    String cat;
    if (auto* cur = proc.presets->getCurrent()) cat = cur->category;
    g.setColour (Palette::curve);
    g.fillEllipse (pill.getX() + 14.0f, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (Palette::text);
    g.drawText ((cat.isNotEmpty() ? cat + " - " : String()) + preset, pill.withTrimmedLeft (26.0f).withTrimmedRight (10.0f), Justification::centred, true);

    const int os = roundToInt (proc.getParamReal ("oversampling"));
    pillButton (area (OS), "Oversampling", String (1 << os) + "x");
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
        case Play:     setTooltip ("Plates: four white keys play the four voices with their own tuning. Keys: a played note retunes a voice"); break;
        case TouchSrc: setTooltip ("What makes a played voice louder than HOLD: velocity, aftertouch (pressure after the strike) or both"); break;
        case SnapMode: setTooltip ("Free: microtonal. Chromatic: voices snap to semitones. Scale: voices snap to the chosen scale"); break;
        case OS:       setTooltip ("Oversampling of the voices: 2x is the sweet spot, 4x for the cleanest high notes and pull"); break;
        default:       setTooltip ("Output peak level / CPU of this instance"); break;
    }
}

void BottomBar::mouseDown (const MouseEvent& e)
{
    // MSVC: build the SafePointer outside the lambda (inside a lambda's init-capture it takes the lambda for this)
    SafePointer<BottomBar> safe (this);
    auto choiceMenu = [this, safe] (const char* id)
    {
        auto* d = Params::findDef (id);
        if (d == nullptr) return;
        PopupMenu m;
        const int cur = roundToInt (proc.getParamReal (id));
        for (int i = 0; i < d->choices.size(); ++i) m.addItem (i + 1, d->choices[i], true, cur == i);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp = safe, id] (int r)
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

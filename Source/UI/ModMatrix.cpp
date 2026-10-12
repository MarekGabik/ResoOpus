#include "ModMatrix.h"
#include "PluginProcessor.h"

using namespace juce;

ModMatrixView::ModMatrixView (ResoOpusProcessor& p) : proc (p)
{
    setTooltip ("Rows: what is modulated. Columns: the sources. Drag a cell up / down for the depth (Shift = fine), "
                "double-click to remove, right-click for bipolar, controller and function");
}

float ModMatrixView::colW() const { return ((float) getWidth() - (float) nameW() - 40.0f) / (float) numCols; }

int ModMatrixView::visibleRows() const { return std::max (1, (getHeight() - headerH() - rowH() - 8) / rowH()); }

Rectangle<float> ModMatrixView::cellRect (int row, int col) const
{
    return { (float) nameW() + (float) col * colW() + 2.0f, (float) (headerH() + (row - scroll) * rowH()) + 2.0f, colW() - 4.0f, (float) rowH() - 4.0f };
}

Rectangle<float> ModMatrixView::nameRect (int row) const
{
    return { 8.0f, (float) (headerH() + (row - scroll) * rowH()), (float) nameW() - 12.0f, (float) rowH() };
}

Rectangle<float> ModMatrixView::addRect() const
{
    const int shown = std::min ((int) rows.size() - scroll, visibleRows());
    return { 8.0f, (float) (headerH() + std::max (0, shown) * rowH()) + 4.0f, 170.0f, (float) rowH() - 4.0f };
}

bool ModMatrixView::hit (Point<float> p, int& row, int& col) const
{
    row = col = -2;
    if (p.y < (float) headerH()) return false;
    const int r = scroll + (int) ((p.y - (float) headerH()) / (float) rowH());
    if (r < scroll || r >= (int) rows.size() || r >= scroll + visibleRows()) return false;
    row = r;
    if (p.x < (float) nameW()) { col = -1; return true; }
    const int c = (int) ((p.x - (float) nameW()) / colW());
    if (c < 0 || c >= numCols) return false;
    col = c;
    return true;
}

const ModMatrixView::Cell* ModMatrixView::cellAt (int row, int col) const
{
    if (row < 0 || row >= (int) rows.size() || col < 0) return nullptr;
    auto it = cells.find ({ rows[(size_t) row], sourceOf (col) });
    return it == cells.end() ? nullptr : &it->second;
}

void ModMatrixView::refresh()
{
    std::map<std::pair<int, int>, Cell> now;
    std::vector<int> dsts;
    String sig;
    for (int s = 0; s < Params::numModSlots; ++s)
    {
        const int src = (int) proc.getParamReal (Params::modID (s, "src"));
        const int dst = (int) proc.getParamReal (Params::modID (s, "dst"));
        if (src <= 0 || dst <= 0) continue;
        Cell c;
        c.slot = s;
        c.amt = proc.getParamReal (Params::modID (s, "amt"));
        c.on = proc.getParamReal (Params::modID (s, "on")) > 0.5f;
        c.bi = proc.getParamReal (Params::modID (s, "bi")) > 0.5f;
        c.ctl = (int) proc.getParamReal (Params::modID (s, "ctl"));
        c.fn = (int) proc.getParamReal (Params::modID (s, "fn"));
        now[{ dst, src }] = c;
        if (std::find (dsts.begin(), dsts.end(), dst) == dsts.end()) dsts.push_back (dst);
        sig << s << ":" << src << ":" << dst << ":" << roundToInt (c.amt * 1000.0f) << (c.on ? "+" : "-") << (c.bi ? "b" : "u") << c.ctl << "/" << c.fn << ";";
    }
    for (int d : pending)
        if (std::find (dsts.begin(), dsts.end(), d) == dsts.end()) dsts.push_back (d);
    std::sort (dsts.begin(), dsts.end());
    for (int d : pending) sig << "p" << d;
    if (sig == signature) return;
    signature = sig;
    cells = now;
    // a pending row that got a routing is no longer pending
    pending.erase (std::remove_if (pending.begin(), pending.end(), [&now] (int d)
    {
        for (auto& kv : now) if (kv.first.first == d) return true;
        return false;
    }), pending.end());
    rows = dsts;
    scroll = jlimit (0, std::max (0, (int) rows.size() - visibleRows()), scroll);
    repaint();
}

void ModMatrixView::highlightSource (int source) { hiSource = source; repaint(); }

void ModMatrixView::highlightSlot (int slot)
{
    hiSlot = slot;
    refresh();
    if (slot >= 0)
    {
        const int dst = (int) proc.getParamReal (Params::modID (slot, "dst"));
        for (int r = 0; r < (int) rows.size(); ++r)
            if (rows[(size_t) r] == dst && (r < scroll || r >= scroll + visibleRows()))
                scroll = jlimit (0, std::max (0, (int) rows.size() - visibleRows()), r);
    }
    repaint();
}

void ModMatrixView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::module);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::moduleEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    // column headers: source name with its colour
    g.setFont (Fonts::bold (10.0f));
    for (int c = 0; c < numCols; ++c)
    {
        const int src = sourceOf (c);
        const auto col = Mod::sourceColour (src);
        auto h = Rectangle<float> ((float) nameW() + (float) c * colW(), 8.0f, colW(), (float) headerH() - 12.0f);
        if (src == hiSource || c == hoverCol)
        {
            g.setColour (col.withAlpha (src == hiSource ? 0.16f : 0.07f));
            g.fillRoundedRectangle (h.withHeight ((float) getHeight() - 14.0f).reduced (1.0f, 0.0f), 5.0f);
        }
        g.setColour (col);
        g.fillRoundedRectangle (Rectangle<float> (h.getWidth() - 16.0f, 3.0f).withCentre ({ h.getCentreX(), h.getBottom() - 2.0f }), 1.5f);
        g.setColour (src == hiSource ? Palette::text : Palette::label);
        g.drawFittedText (Mod::sourceNames()[src].toUpperCase(), h.withTrimmedBottom (6.0f).toNearestInt(), Justification::centred, 2, 0.85f);
    }
    g.setColour (Palette::label);
    g.setFont (Fonts::bold (10.5f));
    g.drawText ("DESTINATION", Rectangle<float> (14.0f, 8.0f, (float) nameW(), (float) headerH() - 12.0f), Justification::centredLeft, false);

    // rows
    for (int row = scroll; row < (int) rows.size() && row < scroll + visibleRows(); ++row)
    {
        const auto nr = nameRect (row);
        bool hiRow = false;
        for (int c = 0; c < numCols; ++c) if (auto* cell = cellAt (row, c)) hiRow = hiRow || cell->slot == hiSlot;
        if (row == hoverRow || hiRow)
        {
            g.setColour (Colours::white.withAlpha (hiRow ? 0.06f : 0.03f));
            g.fillRoundedRectangle (Rectangle<float> (4.0f, nr.getY() + 1.0f, (float) getWidth() - 8.0f, nr.getHeight() - 2.0f), 4.0f);
        }
        g.setColour (Palette::text);
        g.setFont (Fonts::regular (12.5f));
        g.drawText (Mod::destName (rows[(size_t) row]), nr.withTrimmedLeft (6.0f).withTrimmedRight (22.0f), Justification::centredLeft, true);
        if (row == hoverRow && hoverCol == -1)
            Icons::draw (g, Icons::close(), Rectangle<float> (9.0f, 9.0f).withCentre ({ nr.getRight() - 8.0f, nr.getCentreY() }), Palette::textDim, 1.4f);

        for (int c = 0; c < numCols; ++c)
        {
            const auto cr = cellRect (row, c);
            const auto col = Mod::sourceColour (sourceOf (c));
            g.setColour (Colour (0xff111216));
            g.fillRoundedRectangle (cr, 4.0f);
            if (auto* cell = cellAt (row, c))
            {
                const float a = cell->on ? 1.0f : 0.35f;
                // depth: from the centre (bipolar look) or from the left (one direction)
                auto bar = cr.reduced (3.0f, cr.getHeight() * 0.5f - 3.0f);
                g.setColour (Colour (0xff26272d));
                g.fillRoundedRectangle (bar, 2.0f);
                const float mid = cell->bi ? bar.getCentreX() : bar.getX();
                const float span = cell->bi ? bar.getWidth() * 0.5f : bar.getWidth();
                const float x = mid + cell->amt * span * (cell->bi ? 1.0f : 1.0f);
                g.setColour (col.withAlpha (a));
                g.fillRoundedRectangle (Rectangle<float> (std::min (mid, x), bar.getY(), std::abs (x - mid) + 1.0f, bar.getHeight()), 2.0f);
                g.setColour (Palette::text.withAlpha (a));
                g.setFont (Fonts::mono (9.5f));
                g.drawText ((cell->amt >= 0 ? "+" : "") + String (roundToInt (cell->amt * 100.0f)), cr.withHeight (cr.getHeight() * 0.5f).translated (0.0f, -1.0f),
                            Justification::centred, false);
                if (cell->ctl > 0 || cell->fn > 0)
                {
                    g.setColour (Mod::sourceColour (cell->ctl > 0 ? cell->ctl : 0));
                    g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre ({ cr.getRight() - 5.0f, cr.getY() + 5.0f }));
                }
                if (cell->slot == hiSlot)
                {
                    g.setColour (col);
                    g.drawRoundedRectangle (cr, 4.0f, 1.2f);
                }
            }
            else if (row == hoverRow && c == hoverCol)
            {
                g.setColour (col.withAlpha (0.5f));
                g.drawRoundedRectangle (cr.reduced (0.5f), 4.0f, 1.0f);
                g.setFont (Fonts::regular (10.0f));
                g.drawText ("drag", cr, Justification::centred, false);
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.1f));
                g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (cr.getCentre()));
            }
        }
    }

    // add a destination
    const auto ar = addRect();
    g.setColour (Palette::button);
    g.fillRoundedRectangle (ar, 5.0f);
    g.setColour (Palette::curve);
    g.setFont (Fonts::bold (10.5f));
    g.drawText ("+ ADD DESTINATION", ar, Justification::centred, false);
    if (rows.empty())
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawFittedText ("No modulation yet. Drag the coloured MOD button of a source (above) onto any knob, right-click a knob > Modulate with, "
                          "or add a destination here and drag a cell.",
                          Rectangle<int> (ar.toNearestInt().getRight() + 16, (int) ar.getY() - 4, getWidth() - ar.toNearestInt().getRight() - 30, 40),
                          Justification::centredLeft, 2);
    }
    if ((int) rows.size() > visibleRows())
    {
        g.setColour (Palette::textFaint);
        g.setFont (Fonts::mono (10.0f));
        g.drawText (String (scroll + 1) + "-" + String (std::min ((int) rows.size(), scroll + visibleRows())) + " / " + String ((int) rows.size()),
                    Rectangle<float> ((float) getWidth() - 110.0f, ar.getY(), 100.0f, ar.getHeight()), Justification::centredRight, false);
    }
}

void ModMatrixView::mouseMove (const MouseEvent& e)
{
    int r, c;
    if (! hit (e.position, r, c)) { r = -1; c = -2; }
    if (r != hoverRow || c != hoverCol) { hoverRow = r; hoverCol = c; repaint(); }
}

void ModMatrixView::mouseExit (const MouseEvent&) { hoverRow = -1; hoverCol = -2; repaint(); }

void ModMatrixView::mouseDown (const MouseEvent& e)
{
    dragRow = dragCol = -1;
    if (addRect().contains (e.position)) { showAddMenu(); return; }
    int row, col;
    if (! hit (e.position, row, col)) return;
    if (col == -1)
    {
        if (e.position.x > nameRect (row).getRight() - 18.0f) removeRow (row);
        return;
    }
    if (e.mods.isPopupMenu()) { showCellMenu (row, col); return; }
    dragRow = row;
    dragCol = col;
    const auto* cell = cellAt (row, col);
    dragStart = cell != nullptr ? cell->amt : 0.0f;
    if (cell != nullptr)
        if (auto* p = proc.param (Params::modID (cell->slot, "amt"))) p->beginChangeGesture();
}

void ModMatrixView::mouseDrag (const MouseEvent& e)
{
    if (dragRow < 0 || dragCol < 0) return;
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float v = jlimit (-1.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / 120.0f * fine);
    const auto* cell = cellAt (dragRow, dragCol);
    if (cell == nullptr)
    {
        if (std::abs (v) < 0.01f) return;
        // first drag on an empty cell creates the routing (one-directional sources start unipolar)
        const int src = sourceOf (dragCol);
        const int slot = proc.addModulation (src, Mod::destParamID (rows[(size_t) dragRow]), v);
        if (slot < 0) return;
        proc.setParamReal (Params::modID (slot, "bi"), Mod::isBipolar (src) ? 1.0f : 0.0f);
        if (auto* p = proc.param (Params::modID (slot, "amt"))) p->beginChangeGesture();
        signature.clear();
        refresh();
        return;
    }
    if (auto* p = proc.param (Params::modID (cell->slot, "amt"))) p->setValueNotifyingHost (p->convertTo0to1 (v));
    refresh();
}

void ModMatrixView::mouseUp (const MouseEvent&)
{
    if (auto* cell = cellAt (dragRow, dragCol))
        if (auto* p = proc.param (Params::modID (cell->slot, "amt"))) p->endChangeGesture();
    dragRow = dragCol = -1;
}

void ModMatrixView::mouseDoubleClick (const MouseEvent& e)
{
    int row, col;
    if (! hit (e.position, row, col) || col < 0) return;
    if (auto* cell = cellAt (row, col))
    {
        if (std::count_if (cells.begin(), cells.end(), [d = rows[(size_t) row]] (auto& kv) { return kv.first.first == d; }) == 1)
            pending.push_back (rows[(size_t) row]);   // keep the row while it is being edited
        proc.removeModulation (cell->slot);
        signature.clear();
        refresh();
    }
}

void ModMatrixView::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    scroll = jlimit (0, std::max (0, (int) rows.size() - visibleRows()), scroll + (w.deltaY < 0 ? 1 : -1));
    repaint();
}

void ModMatrixView::removeRow (int row)
{
    const int d = rows[(size_t) row];
    for (auto& kv : cells) if (kv.first.first == d) proc.removeModulation (kv.second.slot);
    pending.erase (std::remove (pending.begin(), pending.end(), d), pending.end());
    signature.clear();
    refresh();
}

void ModMatrixView::showCellMenu (int row, int col)
{
    const auto* cell = cellAt (row, col);
    const int src = sourceOf (col);
    PopupMenu m;
    m.addSectionHeader (Mod::sourceNames()[src] + String (CharPointer_UTF8 (" \xe2\x86\x92 ")) + Mod::destName (rows[(size_t) row]));
    if (cell == nullptr)
    {
        m.addItem (1, "Add (25 %)");
    }
    else
    {
        m.addItem (2, "Enabled", true, cell->on);
        m.addItem (3, "Bipolar (swings around the knob)", true, cell->bi);
        PopupMenu ctl, fn, ctlAmt, fnAmt;
        for (int s = 0; s < Mod::numSources; ++s) ctl.addItem (100 + s, s == 0 ? String ("None") : Mod::sourceNames()[s], true, cell->ctl == s);
        for (int f = 0; f < Mod::functionNames().size(); ++f) fn.addItem (200 + f, Mod::functionNames()[f], true, cell->fn == f);
        const float ca = proc.getParamReal (Params::modID (cell->slot, "ctlamt")), fa = proc.getParamReal (Params::modID (cell->slot, "fnamt"));
        for (int v : { 25, 50, 75, 100 }) ctlAmt.addItem (300 + v, String (v) + " %", true, roundToInt (ca * 100.0f) == v);
        for (int v : { 10, 25, 50, 75, 100 }) fnAmt.addItem (400 + v, String (v) + " %", true, roundToInt (fa) == v);
        m.addSubMenu ("Controller (scales it, like a VCA)", ctl);
        m.addSubMenu ("Controller amount", ctlAmt, cell->ctl > 0);
        m.addSubMenu ("Function", fn);
        m.addSubMenu ("Function amount", fnAmt, cell->fn > 0);
        m.addSeparator();
        m.addItem (4, "Remove");
    }
    const int slot = cell != nullptr ? cell->slot : -1;
    const int dst = rows[(size_t) row];
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cellRect (row, col).toNearestInt())),
                     [sp = SafePointer<ModMatrixView> (this), slot, src, dst] (int r)
    {
        if (sp == nullptr || r == 0) return;
        auto& pr = sp->proc;
        if (r == 1)
        {
            const int s = pr.addModulation (src, Mod::destParamID (dst), 0.25f);
            if (s >= 0) pr.setParamReal (Params::modID (s, "bi"), Mod::isBipolar (src) ? 1.0f : 0.0f);
        }
        else if (r == 2) pr.setParamReal (Params::modID (slot, "on"), pr.getParamReal (Params::modID (slot, "on")) > 0.5f ? 0.0f : 1.0f);
        else if (r == 3) pr.setParamReal (Params::modID (slot, "bi"), pr.getParamReal (Params::modID (slot, "bi")) > 0.5f ? 0.0f : 1.0f);
        else if (r == 4) pr.removeModulation (slot);
        else if (r >= 400) pr.setParamReal (Params::modID (slot, "fnamt"), (float) (r - 400));
        else if (r >= 300) pr.setParamReal (Params::modID (slot, "ctlamt"), (float) (r - 300) / 100.0f);
        else if (r >= 200) pr.setParamReal (Params::modID (slot, "fn"), (float) (r - 200));
        else if (r >= 100) pr.setParamReal (Params::modID (slot, "ctl"), (float) (r - 100));
        sp->signature.clear();
        sp->refresh();
    });
}

void ModMatrixView::showAddMenu()
{
    PopupMenu dst, groups[Mod::globalBlock];
    for (int c : Mod::allDestinations()) groups[c / Mod::blockSize - 1].addItem (c, Mod::destName (c), std::find (rows.begin(), rows.end(), c) == rows.end());
    for (int b = 1; b <= Mod::globalBlock; ++b) dst.addSubMenu (Mod::blockName (b), groups[b - 1]);
    dst.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (addRect().toNearestInt())), [sp = SafePointer<ModMatrixView> (this)] (int d)
    {
        if (sp == nullptr || d <= 0) return;
        sp->pending.push_back (d);
        sp->signature.clear();
        sp->refresh();
        for (int r = 0; r < (int) sp->rows.size(); ++r)
            if (sp->rows[(size_t) r] == d) sp->scroll = jlimit (0, std::max (0, (int) sp->rows.size() - sp->visibleRows()), r - sp->visibleRows() + 1);
        sp->repaint();
    });
}

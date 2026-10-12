#include "FilterView.h"
#include "PluginProcessor.h"
#include "../DSP/Effects.h"

using namespace juce;

FilterRoutingView::FilterRoutingView (ResoOpusProcessor& p)
    : proc (p), route (p, "flt_route", "ROUTING", { "SERIAL", "PARALLEL", "SPLIT", "STEREO" })
{
    route.setTooltip (Params::findDef ("flt_route")->tip);
    addAndMakeVisible (route);
    setTooltip ("What FILTER A (gold) and FILTER B (violet) do to the sound right now, modulation included. "
                "The white curve is the result of the ROUTING");
    work.assign ((size_t) fftSize * 2, 0.0f);
    update();
}

void FilterRoutingView::measure (const Setting& s, std::vector<std::complex<float>>& out)
{
    // a quiet impulse through a fresh filter: quiet enough that the models stay linear
    ::dsp::AnalogFilter f;
    f.prepare (displayRate);
    f.reset();
    const float amp = 0.01f;
    const float g = (float) std::tan (MathConstants<double>::pi * std::min ((double) s.cutoff, 0.45 * displayRate) / displayRate);
    std::fill (work.begin(), work.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        work[(size_t) i] = f.process (i == 0 ? amp : 0.0f, g, s.res, s.drive, s.mode, s.model) / amp;
    fft.performRealOnlyForwardTransform (work.data(), true);
    out.resize ((size_t) fftSize / 2 + 1);
    for (size_t k = 0; k < out.size(); ++k) out[k] = { work[2 * k], work[2 * k + 1] };
}

void FilterRoutingView::update()
{
    bool changed = false;
    const char* models[2] = { "flt_model", "flt2_model" };
    const char* modes[2] = { "flt_mode", "flt2_mode" };
    for (int i = 0; i < 2; ++i)
    {
        Setting s;
        s.cutoff = proc.liveFilter[i][0].load (std::memory_order_relaxed);
        s.res = proc.liveFilter[i][1].load (std::memory_order_relaxed);
        s.drive = proc.liveFilter[i][2].load (std::memory_order_relaxed);
        if (s.cutoff <= 0.0f)   // the audio has not run yet: the knob values
        {
            s.cutoff = proc.getParamReal (i == 0 ? "flt_cutoff" : "flt2_cutoff");
            s.res = proc.getParamReal (i == 0 ? "flt_res" : "flt2_res") / 10.0f;
            s.drive = proc.getParamReal (i == 0 ? "flt_drive" : "flt2_drive") / 10.0f;
        }
        s.model = roundToInt (proc.getParamReal (models[i]));
        s.mode = roundToInt (proc.getParamReal (modes[i]));
        if (! (s == shown[i]) || resp[i].empty()) { shown[i] = s; measure (s, resp[i]); changed = true; }
    }
    const bool on = proc.getParamReal ("flt2_on") > 0.5f;
    const int r = roundToInt (proc.getParamReal ("flt_route"));
    if (on != bOn || r != routeShown) { bOn = on; routeShown = r; changed = true; route.setAlpha (on ? 1.0f : 0.4f); }
    if (changed)
    {
        sum.resize (resp[0].size());
        for (size_t k = 0; k < sum.size(); ++k)
            sum[k] = routeShown == GP::RouteSerial ? resp[0][k] * resp[1][k] : 0.7f * (resp[0][k] + resp[1][k]);
        repaint();
    }
}

// the ROUTING switch starts on the line of MODEL / MODE in the filter panels beside it (row 2 of their grid)
static constexpr int routeTop = 120;

Rectangle<float> FilterRoutingView::graphArea() const { return getLocalBounds().toFloat().withHeight ((float) routeTop - 8.0f).reduced (4.0f, 2.0f); }
Rectangle<float> FilterRoutingView::diagramArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) routeTop + 10.0f).withHeight (74.0f).withTrimmedLeft ((float) route.getRight() + 14.0f).reduced (4.0f, 0.0f);
}

float FilterRoutingView::xFor (double hz) const
{
    const auto a = graphArea();
    return a.getX() + a.getWidth() * (float) (std::log (hz / 20.0) / std::log (1000.0));
}

float FilterRoutingView::yFor (double db) const
{
    const auto a = graphArea();
    const double top = 24.0, bottom = -36.0;
    return a.getY() + a.getHeight() * (float) ((top - jlimit (bottom, top, db)) / (top - bottom));
}

Path FilterRoutingView::curve (const std::vector<std::complex<float>>& h, bool closed) const
{
    Path p;
    if (h.empty()) return p;
    const auto a = graphArea();
    const double binHz = displayRate / fftSize;
    bool first = true;
    for (float x = 0.0f; x <= a.getWidth(); x += 1.5f)
    {
        const double hz = 20.0 * std::pow (1000.0, (double) (x / a.getWidth()));
        const double pos = hz / binHz;
        const size_t k = (size_t) pos;
        if (k + 1 >= h.size()) break;
        const double fr = pos - (double) k;
        const double m = (1.0 - fr) * std::abs (h[k]) + fr * std::abs (h[k + 1]);
        const float y = yFor (20.0 * std::log10 (m + 1.0e-9));
        if (first) { p.startNewSubPath (a.getX() + x, y); first = false; }
        else p.lineTo (a.getX() + x, y);
    }
    if (closed && ! first)
    {
        p.lineTo (p.getCurrentPosition().x, a.getBottom());
        p.lineTo (a.getX(), a.getBottom());
        p.closeSubPath();
    }
    return p;
}

void FilterRoutingView::paint (Graphics& g)
{
    const auto a = graphArea();
    g.setColour (Colour (0xff0e0f12));
    g.fillRoundedRectangle (a.expanded (2.0f), 6.0f);

    // grid: 100 Hz, 1 kHz, 10 kHz and 0 dB
    g.setFont (Fonts::mono (9.0f));
    for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const bool major = hz == 100.0 || hz == 1000.0 || hz == 10000.0;
        g.setColour (Colours::white.withAlpha (major ? 0.08f : 0.035f));
        g.drawVerticalLine (roundToInt (xFor (hz)), a.getY(), a.getBottom());
        if (major)
        {
            g.setColour (Palette::textFaint);
            g.drawText (hz >= 1000.0 ? String (roundToInt (hz / 1000.0)) + "k" : String (roundToInt (hz)),
                        Rectangle<float> (xFor (hz) + 3.0f, a.getBottom() - 13.0f, 30.0f, 12.0f), Justification::centredLeft, false);
        }
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (roundToInt (yFor (0.0)), a.getX(), a.getRight());
    g.setColour (Palette::textFaint);
    g.drawText ("0 dB", Rectangle<float> (a.getRight() - 34.0f, yFor (0.0) - 13.0f, 32.0f, 12.0f), Justification::centredRight, false);

    const Colour colA = Palette::filter, colB = Palette::filterB, colSum = Palette::text;
    g.saveState();
    g.reduceClipRegion (a.toNearestInt());
    auto draw = [&] (const std::vector<std::complex<float>>& h, Colour c, float fillAlpha, float width)
    {
        if (fillAlpha > 0.0f) { g.setColour (c.withAlpha (fillAlpha)); g.fillPath (curve (h, true)); }
        g.setColour (c);
        g.strokePath (curve (h, false), PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
    };
    StringArray legend;
    Array<Colour> legendCol;
    if (! bOn)
    {
        draw (resp[0], colA, 0.14f, 2.0f);
        legend.add ("A"); legendCol.add (colA);
        legend.add ("B off"); legendCol.add (Palette::textFaint);
    }
    else if (routeShown == GP::RouteSerial || routeShown == GP::RouteParallel)
    {
        draw (resp[0], colA.withAlpha (0.7f), 0.0f, 1.2f);
        draw (resp[1], colB.withAlpha (0.7f), 0.0f, 1.2f);
        draw (sum, colSum, 0.10f, 2.2f);
        legend.add ("A"); legendCol.add (colA);
        legend.add ("B"); legendCol.add (colB);
        legend.add (routeShown == GP::RouteSerial ? String (CharPointer_UTF8 ("A \xc3\x97 B")) : String ("A + B")); legendCol.add (colSum);
    }
    else
    {
        const bool split = routeShown == GP::RouteSplit;
        draw (resp[0], colA, 0.10f, 2.0f);
        draw (resp[1], colB, 0.10f, 2.0f);
        legend.add (split ? "A DRONE" : "A LEFT"); legendCol.add (colA);
        legend.add (split ? "B LEAD" : "B RIGHT"); legendCol.add (colB);
    }
    g.restoreState();

    g.setFont (Fonts::bold (10.0f));
    float lx = a.getX() + 8.0f;
    for (int i = 0; i < legend.size(); ++i)
    {
        g.setColour (legendCol[i]);
        g.fillRoundedRectangle (Rectangle<float> (lx, a.getY() + 10.0f, 10.0f, 3.0f), 1.5f);
        const float w = (float) GlyphArrangement::getStringWidthInt (Fonts::bold (10.0f), legend[i]);
        g.drawText (legend[i], Rectangle<float> (lx + 14.0f, a.getY() + 4.0f, w + 4.0f, 14.0f), Justification::centredLeft, false);
        lx += w + 30.0f;
    }

    drawDiagram (g, diagramArea());
}

void FilterRoutingView::drawDiagram (Graphics& g, Rectangle<float> r) const
{
    // boxes for the filters, small labels for the ends, lines with arrows for the signal
    const float bw = 34.0f, bh = 20.0f, cy = r.getCentreY();
    auto box = [&] (Point<float> c, const String& t, Colour col)
    {
        const auto b = Rectangle<float> (bw, bh).withCentre (c);
        g.setColour (col.withAlpha (0.18f));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (col);
        g.drawRoundedRectangle (b, 4.0f, 1.3f);
        g.setFont (Fonts::bold (11.0f));
        g.drawText (t, b, Justification::centred, false);
    };
    auto end = [&] (Point<float> c, const String& t, bool right)
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::bold (9.5f));
        const auto area = right ? Rectangle<float> (c.x + 5.0f, c.y - 7.0f, 46.0f, 14.0f) : Rectangle<float> (c.x - 51.0f, c.y - 7.0f, 46.0f, 14.0f);
        g.drawText (t, area, right ? Justification::centredLeft : Justification::centredRight, false);
    };
    auto wire = [&] (std::initializer_list<Point<float>> pts, bool arrow)
    {
        Path p;
        bool first = true;
        Point<float> last, prev;
        for (auto q : pts) { if (first) p.startNewSubPath (q); else p.lineTo (q); prev = last; last = q; first = false; }
        g.setColour (Palette::textFaint.brighter (0.3f));
        g.strokePath (p, PathStrokeType (1.3f));
        if (arrow)
        {
            const auto d = (last - prev);
            const float len = std::max (0.001f, d.getDistanceFromOrigin());
            const auto u = d / len, n = Point<float> (-u.y, u.x);
            Path h;
            h.addTriangle (last, last - u * 6.0f + n * 3.5f, last - u * 6.0f - n * 3.5f);
            g.fillPath (h);
        }
    };
    const float x0 = r.getX() + 44.0f, x1 = r.getRight() - 34.0f;
    const Colour colA = Palette::filter, colB = Palette::filterB;
    const float half = bw * 0.5f;
    if (! bOn)
    {
        const Point<float> a ((x0 + x1) * 0.5f, cy);
        end ({ x0, cy }, "IN", false); end ({ x1, cy }, "OUT", true);
        wire ({ { x0, cy }, { a.x - half, cy } }, true);
        wire ({ { a.x + half, cy }, { x1, cy } }, true);
        box (a, "A", colA);
        return;
    }
    const float up = cy - 17.0f, dn = cy + 17.0f;
    switch (routeShown)
    {
        case GP::RouteSerial:
        {
            const Point<float> a (x0 + (x1 - x0) * 0.33f, cy), b (x0 + (x1 - x0) * 0.67f, cy);
            end ({ x0, cy }, "IN", false); end ({ x1, cy }, "OUT", true);
            wire ({ { x0, cy }, { a.x - half, cy } }, true);
            wire ({ { a.x + half, cy }, { b.x - half, cy } }, true);
            wire ({ { b.x + half, cy }, { x1, cy } }, true);
            box (a, "A", colA); box (b, "B", colB);
            break;
        }
        case GP::RouteParallel:
        {
            const float sx = x0 + 18.0f, jx = x1 - 18.0f, mx = (x0 + x1) * 0.5f;
            end ({ x0, cy }, "IN", false); end ({ x1, cy }, "OUT", true);
            wire ({ { x0, cy }, { sx, cy } }, false);
            wire ({ { sx, cy }, { sx, up }, { mx - half, up } }, true);
            wire ({ { sx, cy }, { sx, dn }, { mx - half, dn } }, true);
            wire ({ { mx + half, up }, { jx, up }, { jx, cy } }, false);
            wire ({ { mx + half, dn }, { jx, dn }, { jx, cy } }, false);
            wire ({ { jx, cy }, { x1, cy } }, true);
            g.setColour (Palette::textDim);
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ jx, cy }));
            box ({ mx, up }, "A", colA); box ({ mx, dn }, "B", colB);
            break;
        }
        case GP::RouteSplit:
        {
            const float jx = x1 - 18.0f, mx = (x0 + x1) * 0.5f;
            end ({ x0, up }, "DRONE", false); end ({ x0, dn }, "LEAD", false); end ({ x1, cy }, "OUT", true);
            wire ({ { x0, up }, { mx - half, up } }, true);
            wire ({ { x0, dn }, { mx - half, dn } }, true);
            wire ({ { mx + half, up }, { jx, up }, { jx, cy } }, false);
            wire ({ { mx + half, dn }, { jx, dn }, { jx, cy } }, false);
            wire ({ { jx, cy }, { x1, cy } }, true);
            g.setColour (Palette::textDim);
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ jx, cy }));
            box ({ mx, up }, "A", colA); box ({ mx, dn }, "B", colB);
            break;
        }
        default:   // stereo
        {
            const float mx = (x0 + x1) * 0.5f;
            end ({ x0, up }, "L", false); end ({ x0, dn }, "R", false); end ({ x1, up }, "L", true); end ({ x1, dn }, "R", true);
            wire ({ { x0, up }, { mx - half, up } }, true);
            wire ({ { x0, dn }, { mx - half, dn } }, true);
            wire ({ { mx + half, up }, { x1, up } }, true);
            wire ({ { mx + half, dn }, { x1, dn } }, true);
            box ({ mx, up }, "A", colA); box ({ mx, dn }, "B", colB);
            break;
        }
    }
}

void FilterRoutingView::resized()
{
    route.setTopLeftPosition (6, routeTop);
}

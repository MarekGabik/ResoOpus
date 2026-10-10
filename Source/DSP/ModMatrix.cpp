#include "ModMatrix.h"
#include <cmath>
#include <algorithm>

void ModMatrix::reset()
{
    for (auto& s : state) s = State();
}

float ModMatrix::applyFunction (int fn, float x, float a, State& s, double seconds, bool bipolar)
{
    const float lo = bipolar ? -1.0f : 0.0f;
    if (! s.primed) { s.lp = s.slew = s.held = s.y = x; s.v = 0.0f; s.timer = 0.0f; s.primed = true; }

    switch (fn)
    {
        case Mod::Scale:       return x * a * 2.0f;
        case Mod::Offset:      return x + (a * 2.0f - 1.0f);
        case Mod::LowClip:     return std::max (x, lo + a * (1.0f - lo));
        case Mod::HighClip:    return std::min (x, lo + a * (1.0f - lo));
        case Mod::Exponential: return (x < 0.0f ? -1.0f : 1.0f) * std::pow (std::abs (x), 1.0f + a * 3.0f);
        case Mod::NormExponential:
        {
            const float u = bipolar ? 0.5f * (x + 1.0f) : x;   // keeps the ends where they are
            const float e = std::pow (std::clamp (u, 0.0f, 1.0f), 1.0f + a * 3.0f);
            return bipolar ? e * 2.0f - 1.0f : e;
        }
        case Mod::LowPass:
        case Mod::HighPass:
        {
            const double tau = 0.002 + (double) a * a * 2.0;
            s.lp += (float) (1.0 - std::exp (-seconds / tau)) * (x - s.lp);
            return fn == Mod::LowPass ? s.lp : x - s.lp;
        }
        case Mod::Slew:
        case Mod::LowSlew:
        case Mod::HighSlew:
        {
            const float maxStep = (float) (seconds / (0.001 + a * 2.0));   // full range per (a * 2) seconds
            float d = x - s.slew;
            const bool limitUp = fn != Mod::LowSlew, limitDown = fn != Mod::HighSlew;
            if (d > 0.0f && limitUp) d = std::min (d, maxStep);
            if (d < 0.0f && limitDown) d = std::max (d, -maxStep);
            s.slew += d;
            return s.slew;
        }
        case Mod::SampleHold:
        {
            s.timer += (float) seconds;
            if (s.timer >= 0.005f + a * 0.5f) { s.timer = 0.0f; s.held = x; }
            return s.held;
        }
        case Mod::Bounce:
        {
            // a ball falls when the signal drops and bounces off it
            const float gravity = 30.0f * (1.05f - a);
            if (x >= s.y) { s.y = x; s.v = 0.0f; }
            else
            {
                s.v -= gravity * (float) seconds;
                s.y += s.v * (float) seconds;
                if (s.y < x) { s.y = x; s.v = std::abs (s.v) < 0.2f ? 0.0f : -s.v * 0.6f; }
            }
            return s.y;
        }
        default: return x;
    }
}

void ModMatrix::process (const Slot* slots, const float* src, double seconds, float* out)
{
    for (int i = 0; i < Params::numModSlots; ++i)
    {
        const auto& sl = slots[i];
        if (! sl.active())
        {
            out[i] = 0.0f;
            state[i].primed = false;
            continue;
        }

        const bool srcBipolar = Mod::isBipolar (sl.src);
        float x = src[sl.src];
        bool bi = srcBipolar;
        if (srcBipolar && ! sl.bipolar) { x = 0.5f * (x + 1.0f); bi = false; }

        x = applyFunction (sl.fn, x, sl.fnAmt, state[i], seconds, bi);

        float vca = 1.0f;
        if (sl.ctl != Mod::None)
            vca = src[sl.ctl] * sl.ctlAmt;

        out[i] = x * vca * sl.amt;
        if (! std::isfinite (out[i])) { out[i] = 0.0f; state[i] = State(); }
    }
}

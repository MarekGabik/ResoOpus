#pragma once

#include "Params/Parameters.h"

// Modulation routings: source -> function -> controller (VCA) -> amount -> destination.
// Runs at control rate (once per chunk). Output is an offset in the destination's normalised 0..1 range.
class ModMatrix
{
public:
    struct Slot
    {
        bool on = false, bipolar = true;
        int src = 0, dst = 0, ctl = 0, fn = 0;
        float amt = 0.0f, ctlAmt = 1.0f, fnAmt = 0.5f;
        bool active() const { return on && src != Mod::None && dst != 0; }
    };

    void reset();

    // sources: Mod::numSources values. Writes one normalised offset per slot into out.
    void process (const Slot* slots, const float* sources, double seconds, float* out);

private:
    struct State
    {
        float lp = 0.0f, slew = 0.0f, held = 0.0f, timer = 0.0f, y = 0.0f, v = 0.0f;
        bool primed = false;
    };
    float applyFunction (int fn, float x, float amount, State& s, double seconds, bool bipolar);
    State state[Params::numModSlots];
};

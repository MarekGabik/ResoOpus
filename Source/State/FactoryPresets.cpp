#include "FactoryPresets.h"
#include "Params/Parameters.h"

// Values are set on top of the defaults (see Params::voiceDefault). Voice IDs: v1_..v8_, groups: g1_ (A), g2_ (B).
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", "Basics",
          "Drones on their own after loading: group A at 60 %, group B at 50 %. Play the white keys C3-C4 to swell voices 1-8, turn HOLD to set the drone level.",
          {}, {} },

        { "Low Choir", "Drones",
          "Two deep choirs a fifth apart under a soft octave choir. Turn group A HOLD for the bed, play C3 and D3 to lift the low voices, TIDE of voice 1 sinks it.",
          { { "g1_hold", 72.0f }, { "g2_hold", 0.0f },
            { "v1_tune", 33.0f }, { "v2_tune", 40.0f }, { "v2_on4", 1.0f },
            { "v1_volume", -7.0f }, { "v2_volume", -9.0f }, { "v1_attack", 6.0f }, { "v2_attack", 8.0f } }, {} },

        { "Slow Bloom", "Cinematic",
          "Nothing sounds until you play: every plate blooms over several seconds and fades for twelve. Hold chords on C3-C4 like a slow string section.",
          { { "g1_hold", 0.0f }, { "g2_hold", 0.0f },
            { "v3_type", 0.0f }, { "v4_type", 0.0f }, { "v5_type", 0.0f }, { "v8_type", 0.0f },
            { "v1_tune", 38.0f }, { "v2_tune", 45.0f }, { "v3_tune", 50.0f }, { "v4_tune", 53.0f },
            { "v5_tune", 57.0f }, { "v6_tune", 60.0f }, { "v7_tune", 62.0f }, { "v8_tune", 65.0f },
            { "v3_int2", 3.0f }, { "v3_int3", 7.0f }, { "v3_int4", 12.0f }, { "v3_int5", 15.0f },
            { "v4_int2", 0.07f }, { "v4_int3", -0.07f }, { "v4_int4", 0.12f }, { "v4_int5", -0.12f },
            { "v5_int2", 5.0f }, { "v5_int3", 7.0f }, { "v5_int4", 12.0f }, { "v5_int5", 17.0f },
            { "v8_int2", 0.07f }, { "v8_int3", -0.07f }, { "v8_int4", 12.0f }, { "v8_int5", 24.0f },
            { "v1_attack", 7.0f }, { "v2_attack", 6.0f }, { "v3_attack", 5.0f }, { "v4_attack", 5.0f },
            { "v5_attack", 4.0f }, { "v6_attack", 4.0f }, { "v7_attack", 3.5f }, { "v8_attack", 3.0f },
            { "v1_release", 12.0f }, { "v2_release", 12.0f }, { "v3_release", 11.0f }, { "v4_release", 11.0f },
            { "v5_release", 10.0f }, { "v6_release", 10.0f }, { "v7_release", 9.0f }, { "v8_release", 9.0f },
            { "touch", 2.0f } }, {} },

        { "Sinking Choir", "Drones",
          "The low choir is pulled down by TIDE until its chord growls; the upper group stays clear above it. Ride TIDE of voice 1 between 40 and 80 %.",
          { { "g1_hold", 70.0f }, { "g2_hold", 45.0f },
            { "v1_tide", 64.0f }, { "v6_tide", 30.0f }, { "v7_tide", 12.0f },
            { "v1_volume", -6.0f }, { "v1_attack", 5.0f } }, {} },

        { "Microtonal Glass", "Cinematic",
          "Cluster and unison choirs a few cents apart: slow beating like glass harmonica. Detune the OSC knobs by ear with Shift for more or less shimmer.",
          { { "g1_hold", 55.0f }, { "g2_hold", 55.0f },
            { "v1_tune", 62.0f }, { "v2_tune", 69.13f }, { "v6_tune", 74.0f }, { "v7_tune", 81.07f },
            { "v1_int2", 0.5f }, { "v1_int3", 2.0f }, { "v1_int4", 3.0f }, { "v1_int5", 5.0f }, { "v1_on5", 1.0f },
            { "v2_int2", 0.07f }, { "v2_int3", -0.07f }, { "v2_int4", 0.12f }, { "v2_int5", -0.12f }, { "v2_on4", 1.0f },
            { "v6_int2", 7.0f }, { "v6_int3", 12.0f }, { "v6_int4", 19.0f }, { "v6_int5", 24.0f },
            { "v7_int2", 0.07f }, { "v7_int3", -0.07f }, { "v7_int4", 0.12f }, { "v7_int5", -0.12f }, { "v7_on5", 1.0f },
            { "v1_volume", -12.0f }, { "v2_volume", -12.0f }, { "v6_volume", -13.0f }, { "v7_volume", -14.0f } }, {} },
    };
    return presets;
}

#include "FactoryPresets.h"
#include "Params/Parameters.h"

// Values are set on top of the defaults (see Params::voiceDefault). Voices: v1_..v4_ (pairs 1-2 and 3-4).
// Types: 0 Off, 1 Choir, 2 Siren, 3 Pulsar.
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", "Basics",
          "Drones on its own after loading: two low choirs, a pulsar and a slow siren on the plates. Play C3-F3 to swell voices 1-4, turn HOLD per voice for the drone level.",
          {}, {} },

        { "Low Choir", "Drones",
          "Two deep choirs a fifth apart. Turn HOLD for the bed, play C3 and D3 to lift them, TIDE of voice 1 sinks it into a growl.",
          { { "v1_tune", 33.0f }, { "v2_tune", 40.0f }, { "v2_on4", 1.0f }, { "v1_hold", 72.0f }, { "v2_hold", 55.0f },
            { "v3_type", 0.0f }, { "v4_type", 0.0f },
            { "v1_volume", -2.0f }, { "v2_volume", -4.0f }, { "v1_attack", 6.0f }, { "v2_attack", 8.0f } }, {} },

        { "Slow Bloom", "Cinematic",
          "Nothing sounds until you play: every plate blooms over seconds and fades for twelve. Hold chords on C3-F3 like a slow string section.",
          { { "v1_hold", 0.0f }, { "v2_hold", 0.0f }, { "v3_type", 1.0f }, { "v4_type", 1.0f },
            { "v1_tune", 38.0f }, { "v2_tune", 45.0f }, { "v3_tune", 50.0f }, { "v4_tune", 57.0f },
            { "v2_int2", 3.0f }, { "v2_int3", 7.0f }, { "v2_int4", 12.0f }, { "v2_int5", 15.0f }, { "v2_on4", 1.0f },
            { "v3_int2", 0.07f }, { "v3_int3", -0.07f }, { "v3_int4", 0.12f }, { "v3_int5", -0.12f },
            { "v4_int2", 4.0f }, { "v4_int3", 7.0f }, { "v4_int4", 12.0f }, { "v4_int5", 16.0f },
            { "v1_attack", 7.0f }, { "v2_attack", 6.0f }, { "v3_attack", 5.0f }, { "v4_attack", 4.0f },
            { "v1_release", 12.0f }, { "v2_release", 12.0f }, { "v3_release", 11.0f }, { "v4_release", 10.0f },
            { "touch", 2.0f } }, {} },

        { "Sinking Choir", "Drones",
          "The low choir is pulled down by TIDE until its chord growls; the pulsar stays clear above it. Ride TIDE of voice 1 between 40 and 80 %.",
          { { "v1_hold", 70.0f }, { "v2_hold", 40.0f }, { "v3_hold", 30.0f }, { "v1_tide", 64.0f }, { "v2_tide", 30.0f },
            { "v1_volume", -1.0f }, { "v1_attack", 5.0f }, { "v4_type", 0.0f } }, {} },

        { "Microtonal Glass", "Cinematic",
          "Cluster and unison choirs a few cents apart: slow beating like glass harmonica. Detune the OSC knobs by ear with Shift for more or less shimmer.",
          { { "v3_type", 1.0f }, { "v4_type", 1.0f },
            { "v1_hold", 55.0f }, { "v2_hold", 55.0f }, { "v3_hold", 50.0f }, { "v4_hold", 45.0f },
            { "v1_tune", 62.0f }, { "v2_tune", 69.13f }, { "v3_tune", 74.0f }, { "v4_tune", 81.07f },
            { "v1_int2", 0.5f }, { "v1_int3", 2.0f }, { "v1_int4", 3.0f }, { "v1_int5", 5.0f }, { "v1_on5", 1.0f },
            { "v2_int2", 0.07f }, { "v2_int3", -0.07f }, { "v2_int4", 0.12f }, { "v2_int5", -0.12f }, { "v2_on4", 1.0f },
            { "v3_int2", 7.0f }, { "v3_int3", 12.0f }, { "v3_int4", 19.0f }, { "v3_int5", 24.0f },
            { "v4_int2", 0.07f }, { "v4_int3", -0.07f }, { "v4_int4", 0.12f }, { "v4_int5", -0.12f },
            { "v1_volume", -8.0f }, { "v2_volume", -8.0f }, { "v3_volume", -9.0f }, { "v4_volume", -10.0f } }, {} },

        { "Night Sirens", "Textures",
          "A slow wailing siren over a low choir and a distant two-tone siren with noise. Turn RATE and FM of voices 3-4 for birds, alarms and space monsters.",
          { { "v3_type", 2.0f }, { "v4_type", 2.0f }, { "v1_hold", 55.0f }, { "v2_type", 0.0f },
            { "v3_tune", 69.0f }, { "v3_hold", 35.0f }, { "v3_siren_rate", 0.12f }, { "v3_siren_fm", 22.0f }, { "v3_siren_wave", 100.0f },
            { "v3_siren_div", 2.0f }, { "v3_volume", -9.0f }, { "v3_pan", -0.45f }, { "v3_attack", 4.0f },
            { "v4_tune", 76.0f }, { "v4_hold", 25.0f }, { "v4_siren_rate", 1.4f }, { "v4_siren_fm", 8.0f }, { "v4_siren_wave", 0.0f },
            { "v4_siren_noise", 18.0f }, { "v4_volume", -12.0f }, { "v4_pan", 0.5f }, { "v4_attack", 6.0f } }, {} },

        { "Pulsar Field", "Cinematic",
          "Four pulsars from soft to sharp: each one swells in flat and dull and blooms into pitch. Play the plates slowly; turn SHARP for edge.",
          { { "v1_type", 3.0f }, { "v2_type", 3.0f }, { "v3_type", 3.0f }, { "v4_type", 3.0f },
            { "v1_tune", 38.0f }, { "v2_tune", 45.0f }, { "v3_tune", 52.0f }, { "v4_tune", 57.0f },
            { "v1_sharp", 5.0f }, { "v2_sharp", 25.0f }, { "v3_sharp", 45.0f }, { "v4_sharp", 70.0f },
            { "v1_bloom", 70.0f }, { "v2_bloom", 70.0f }, { "v3_bloom", 60.0f }, { "v4_bloom", 50.0f },
            { "v1_hold", 50.0f }, { "v2_hold", 30.0f }, { "v3_hold", 0.0f }, { "v4_hold", 0.0f },
            { "v1_attack", 4.0f }, { "v2_attack", 5.0f }, { "v3_attack", 3.0f }, { "v4_attack", 2.5f },
            { "v1_release", 8.0f }, { "v2_release", 8.0f }, { "v3_release", 7.0f }, { "v4_release", 7.0f } }, {} },
    };
    return presets;
}

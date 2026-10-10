# Precedence: Gavr blueprint and real-time audio over generic ECC rules

The ECC C++ rules (`rules/ecc-cpp/`) and skills (`cpp-coding-standards`, `cpp-testing`) are generic C++
guidance. This project is a JUCE audio plugin built to the Gavr blueprints
(`~/Desktop/ResoOG/docs/GAVR_SYNTH_BLUEPRINT.md`, `~/Desktop/ResoQBand/docs/GAVR_PLUGIN_BLUEPRINT.md`).
Where they disagree, the blueprints and the rules below win.

## Audio thread (processBlock and everything it calls)
- No allocation (`new`, `std::vector` growth, `std::string`, `juce::String`, `std::function` capture growth),
  no locks or mutexes, no exceptions, no I/O, no logging. Preallocate in `prepareToPlay`.
- Lock-free communication is required here, not avoided: `std::atomic`, preallocated slots, lock-free FIFOs.
  ECC's "CP.100 avoid lock-free programming" does not apply to the audio thread.
- DSP state is mutated in place (filters, oscillators, buffers). ECC's "immutability / never mutate" applies
  only to non-realtime value objects (presets, parameter tables, UI models).
- Fixed-size `std::array` or preallocated `juce::HeapBlock` / `AudioBuffer` for DSP memory.
- `juce::ScopedNoDenormals`; no NaN/Inf may reach the output.

## Style
- Follow JUCE conventions as in ResoOG: `camelCase` functions and members, `PascalCase` types; not ECC's
  `underscore_style` (NL.10).
- C++17 (no concepts, no `std::expected`).
- JUCE ownership idioms are fine: `OwnedArray`, `std::unique_ptr`, `addAndMakeVisible`. Naked `new` only
  when ownership is handed over immediately (`auto* k = new Knob (...); owned.add (k);`).
- Exceptions only outside the audio thread (file loading, preset parsing), never inside it.

## Testing
- Tests live in the `<Name>Tests` TestHost app (`tools/TestHost/Main.cpp`) per the blueprint, with numeric
  DSP measurements (aliasing dB, CPU %, levels). Not GoogleTest, no line-coverage target.
- Take from ECC: sanitizers (ASan/UBSan) on a Debug test build, deterministic seeds, no sleeps in tests,
  `timing()` limits with 3x CI headroom.

## Git
- Commits keep the `Co-Authored-By` trailer (blueprint), whatever ECC's git rules say.
- Push to GitHub only when Marek asks.

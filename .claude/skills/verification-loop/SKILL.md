---
name: verification-loop
description: Verify a JUCE audio plugin before handing a step to Marek - universal build, architectures, auval, TestHost tests against the installed plugin, parameter audit, bench, sanitizers, diff review - and produce a PASS/FAIL report. Use after finishing a phase or feature, before a commit that closes a step, and before any push.
license: MIT
metadata:
  origin: Adapted from ECC verification-loop (github.com/affaan-m/ECC, MIT) for Gavr JUCE plugins
---

# Verification Loop (Gavr JUCE plugin)

Run every phase. If a phase fails, stop, fix it, and run the loop again. Never report "done" with a failing
or skipped phase without saying so explicitly.

Replace `<Name>` / `<CODE>` with the project's product name and 4-char plugin code (see CMakeLists.txt).

## Phase 1: Universal build
```bash
cmake --build build-universal --config Release 2>&1 | tail -20
```
Both `build/` (arm64 dev) and `build-universal/` copy into `~/Library/Audio/Plug-Ins`; the last build wins,
so the universal build must be the last one before handoff.

## Phase 2: Architectures
```bash
lipo -archs ~/Library/Audio/Plug-Ins/VST3/<Name>.vst3/Contents/MacOS/<Name>
lipo -archs ~/Library/Audio/Plug-Ins/Components/<Name>.component/Contents/MacOS/<Name>
```
Expect `x86_64 arm64` for both.

## Phase 3: AU validation
```bash
killall -9 AudioComponentRegistrar 2>/dev/null; auval -v aumu <CODE> Gavr 2>&1 | tail -5
```
Expect `AU VALIDATION SUCCEEDED`.

## Phase 4: Tests
```bash
./build-universal/<Name>Tests_artefacts/Release/<Name>Tests 2>&1 | tail -40
```
Includes hosting the installed VST3. Report passed / failed counts and the key measured numbers
(aliasing dB, CPU %, levels).

## Phase 5: Parameter audit and bench
```bash
./build-universal/<Name>Tests_artefacts/Release/<Name>Tests --audit 2>&1 | tail -15
./build-universal/<Name>Tests_artefacts/Release/<Name>Tests --bench 2>&1 | tail -15
```
Target: 100 % of parameters change the output; CPU within the blueprint targets.

## Phase 6: Sanitizers (when DSP or threading code changed)
UBSan build of the test app, run it, expect zero `runtime error` lines:
```bash
cmake -B build-ubsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DRESOOPUS_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build-ubsan --target ResoOpusTests && SANITIZED=1 ./build-ubsan/ResoOpusTests_artefacts/RelWithDebInfo/ResoOpusTests 2>&1 | grep -c "runtime error"
```
Known issue (2026-10, macOS Darwin 27): the AddressSanitizer runtime of Apple clang deadlocks in its own
initialisation (`InitializeShadowMemory`) before main() runs. Do not wait for it; use UBSan and report ASan as skipped.

## Phase 7: Real-time safety grep
```bash
grep -rnE "new |std::vector<.*>.*push_back|juce::String|std::string|std::mutex|lock_guard|throw |DBG\(" Source/DSP Source/PluginProcessor.cpp | head -30
```
Every hit inside audio-thread code must be justified (prepareToPlay, message thread) or fixed.

## Phase 8: Diff review
```bash
git status --short && git diff --stat
```
Look for unintended changes, renamed parameter IDs (forbidden), tables not appended at the end, leftover debug code.

## Report
```
VERIFICATION REPORT
===================
Build (universal):  PASS/FAIL
Architectures:      PASS/FAIL (x86_64 arm64)
auval:              PASS/FAIL
Tests:              PASS/FAIL (X/Y)
Audit:              X % parameters change sound
Bench:              CPU X % (target Y %)
Sanitizers:         PASS/FAIL/SKIPPED (reason)
RT-safety grep:     PASS/FAIL
Diff:               X files changed

Overall: READY / NOT READY
Windows: verified by CI / NOT verified yet
```
Then summarise for Marek in Czech: what is done, the measured numbers, what remains.

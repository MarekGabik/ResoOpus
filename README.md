# ResoOpus

Drone syntezátor značky Gavr pro filmové, ambientní a experimentální plochy. Osm ručně, mikrotonálně laděných
hlasů ve dvou skupinách hraje po vložení do Abletonu samo (HOLD). Ve středu nástroje bude knob GRAVITY, který vede
od čistého ladění k chaosu. Formáty VST3 (Mac + Windows), AU (Mac), Standalone.

Podrobnosti: [docs/SPECIFIKACE.md](docs/SPECIFIKACE.md) (architektura, roadmapa s branami), [docs/KONCEPT.md](docs/KONCEPT.md),
vzhled [design/resoopus-mockup.html](design/resoopus-mockup.html).

## Stav: fáze 1 (verze 0.1.0)

Hotovo a otestováno:
- 8 hlasů ve 2 skupinách. Hlas **CHOIR** = 5 pil laděných jako akord (CHORD: Unison, Octaves, Fifths, Major, Minor;
  intervaly se ladí po centech, každá pila jde vypnout), **TIDE** (do 50 % stahuje hlas dolů, nad 50 % se pily
  navzájem modulují a akord se rozpadá do vrčení)
- skupiny: **HOLD** (drone bez dotyku), **PITCH**, **FILL** (rozprostře 4 hlasy po stupnici)
- hraní: **Plates** (bílé klávesy C3–C4 = hlasy 1–8) a **Keys** (nota přeladí volný hlas), dynamika
  **Velocity / Aftertouch / obojí**, **Snap** Free / Chromatic / Scale, A = 415–466 Hz
- UI stránky DRONE podle mockupu, klávesnice s čísly hlasů, presety (5), undo/redo, A/B, modulační panel
- typy **SIREN** a **PULSAR** mají zatím jen ladění, obálku a mix, jejich zvuk přijde ve fázi 2;
  **GRAVITY** zatím ukazuje jen oběžnou dráhu hlasů, knob přijde ve fázi 3

Naměřeno (`ResoOpusTests`, Apple M1, 48 kHz):

| Vlastnost | Výsledek |
|---|---|
| Přesnost ladění (TUNE, intervaly, PITCH, A = 432 Hz, Snap) | nejhůř 0,016 centu |
| Aliasing pily CHOIR (1976 Hz, nejsilnější alias pod 10 kHz) | 1× −81 dB, 2× −93 dB, 4× −93 dB |
| CPU: výchozí patch 2× / 8 hlasů × 5 pil s TIDE 2× / 4× | 0,8 % / 1,4 % / 2,7 % jádra |
| Nečinnost (HOLD 0) | 0,27 % jádra |
| Výchozí patch bez MIDI | drone na −7,5 dBFS, náběh bez lupnutí |
| Všechno na maximu | měkký limiter drží −0,3 dBFS |
| Audit parametrů | 150 ze 150 mění zvuk |
| Překreslení UI ve 2× | 62 ms |

## Build (macOS)

```bash
cp -cR ~/Desktop/ResoOG/JUCE .
cmake -B build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release -DRESOOPUS_TESTS=ON
cmake --build build-universal
./build-universal/ResoOpusTests_artefacts/Release/ResoOpusTests ~/Library/Audio/Plug-Ins/VST3/ResoOpus.vst3
```

Build plugin rovnou nainstaluje do `~/Library/Audio/Plug-Ins` (VST3 i AU). Rychlý vývojový build jen pro arm64:
`-B build -DCMAKE_OSX_ARCHITECTURES=arm64`. Pozor: oba buildy instalují plugin, platí ten poslední.
Další režimy testů: `--audit` (mění každý parametr zvuk?), `--bench` (CPU). Sanitizery: `-DRESOOPUS_SANITIZE=ON`.

## Windows

Windows VST3 sestaví GitHub Actions (`.github/workflows/build.yml`) po pushi. Artefakt `ResoOpus-VST3-Windows`
se zkopíruje do `C:\Program Files\Common Files\VST3`. **Zatím neověřeno** (repozitář ještě není na GitHubu).

## Ableton Live

1. Po buildu Live → Preferences → Plug-Ins → Rescan (nebo restart Live).
2. ResoOpus je v prohlížeči pod Plug-Ins → Gavr. Přetáhni ho na MIDI stopu: hned hraje drone.
3. Bílé klávesy C3–C4 hrají hlasy 1–8, dotek je zesílí nad úroveň HOLD.

## Licence

Proprietární software Gavr. Používá JUCE 8 (licence JUCE).

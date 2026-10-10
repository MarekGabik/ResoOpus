# ResoOpus

> **📖 Příručka / Manual (CS · EN):** https://claude.ai/artifact/XsEtBkoLKuA1WsiY3c4SHR
> · v repozitáři: [docs/manual/index.html](docs/manual/index.html) (česky), [docs/manual/en.html](docs/manual/en.html) (English).
> Screeny se generují z pluginu: `ResoOpusTests --shots docs/manual/img`.

Drone syntezátor značky Gavr pro filmové, ambientní a experimentální plochy. Čtyři ručně, mikrotonálně laděné
hlasy ve dvou párech hrají po vložení do Abletonu samy (HOLD). Ve středu nástroje je knob GRAVITY, který vede
od čistého ladění k chaosu. Formáty VST3 (Mac + Windows), AU (Mac), Standalone.

Podrobnosti: [docs/SPECIFIKACE.md](docs/SPECIFIKACE.md) (architektura, roadmapa s branami), [docs/KONCEPT.md](docs/KONCEPT.md),
vzhled [design/resoopus-mockup.html](design/resoopus-mockup.html).

## Stav: fáze 5 (verze 0.1.0)

Hotovo a otestováno:
- **4 hlasy** ve dvou párech (1·2 a 3·4 kolem GRAVITY). TYPE každého hlasu: **OFF / CHOIR / SIREN / PULSAR**
  - **CHOIR**: 5 pil laděných jako akord (CHORD: Unison, Octaves, Fifths, Major, Minor; intervaly po centech,
    každá pila jde vypnout), **TIDE** (do 50 % stahuje hlas dolů, nad 50 % se pily navzájem modulují a akord vrčí)
  - **SIREN**: modulátor sweepuje tón (FM až ±3 oktávy) nebo ho seká (AM), WAVE obdélník (dvoutón) ↔ trojúhelník
    (kvílení), DIVIDER přidává podtóny (1:2 až 1:16), NOISE pro vítr a příboj
  - **PULSAR**: tón od trojúhelníku po obdélník (SHARP), **BLOOM** = hlas naběhne podladěný a matný a rozkvete do výšky
- u každého hlasu **HOLD** (drone bez dotyku), PAN, VOL; **MASTER**: PITCH (transpozice všeho), VOLUME, **FILL**
- hraní: **Plates** (bílé klávesy C3–F3 = hlasy 1–4) a **Keys** (nota přeladí volný hlas), dynamika
  **Velocity / Aftertouch / obojí**, **Snap** Free / Chromatic / Scale, A = 415–466 Hz
- UI stránky DRONE a LEAD · FX, klávesnice s čísly hlasů, undo/redo, A/B, modulační panel
- **LEAD** (mono melodie: 2 oscilátory s morfem tvaru, SYNC, SUB, ADSR, glide, vibrato se zpožděním a tlakem) a **ARP** v tempu skladby;
  zapíná se ikonou u záložky LEAD · FX, hrají ho klávesy mimo plates
- **42 presetů** v kategoriích Basics, Drones, Cinematic, Textures, Orbits, Lead, Pulse; **prohlížeč presetů**
  (kategorie, All / Factory / User / Favourites, hledání, hvězdička, Save / Save as, výchozí preset, náhled hlasů a GRAVITY);
  vlastní presety v `Dokumenty/Gavr/ResoOpus Presets` (*.ropreset)
- nápověda (tooltipy) je vypnutá, zapíná se ikonou **?** vedle Help
- **GRAVITY** (velký knob uprostřed): STILL (čisté ladění, zázněje) → ORBIT (hlasy v páru se navzájem modulují) →
  TURBULENCE (všechny 4 v jedné smyčce + zpětná vazba výstupu) → COLLAPSE (smyčka pulzuje sama). PATH Auto / Pairs / Ring
  (pravé tlačítko), dá se modulovat (např. tlakem prstu). **DUST**: drift ladění, prosakování hlasů, šum pozadí
- stránka **FX**: **FILTER** (stereo, LP/BP/HP, DRIVE, SPREAD, modely **GROWL** špinavý 12 dB, **LADDER** tranzistorový
  24 dB, **DIODE** diodový žebřík, **SCREAM** ostrá 12 dB rezonance, **SILK** čistý 12 dB), **ECHO** (2 linky s křížovou zpětnou vazbou,
  nad 55 % samo zpívá; SELF/LFO modulace času; TO VOICES vrací echo do hlasů), **SPACE** (AURORA shimmer, VOID s FREEZE,
  RUNNER stoupající/klesající opakování, SATURN kovová kruhová modulace, COMET špinavé zpívající delay; X/Y/Z podle algoritmu)

Naměřeno (`ResoOpusTests`, Apple M1, 48 kHz):

| Vlastnost | Výsledek |
|---|---|
| Přesnost ladění (TUNE, intervaly, PITCH, A = 432 Hz, Snap, SIREN) | nejhůř 0,016 centu |
| Aliasing (1976 Hz, nejsilnější alias pod 10 kHz): pila CHOIR / obdélník SIREN | 2× −93 dB / −93 dB (1× pila −81 dB) |
| SIREN FM 50 % / AM 100 % / DIVIDER 1:3 | skok ±1,5 okt přesně / 71 dB mezi pípnutími / podtón −0,9 dB proti tónu |
| PULSAR SHARP 0 → 100 % (3. harmonická) / BLOOM | −19,6 → −10,1 dB / start −18,6 centu, ustálení −0,2 centu |
| CPU: výchozí patch 2× / 4 hlasy × 5 pil s TIDE 2× / 4× | 1,1 % / 0,8 % / 1,7 % jádra |
| Výchozí patch bez MIDI | drone na −8,6 dBFS, náběh bez lupnutí |
| Všechno na maximu | měkký limiter drží −0,3 dBFS |
| GRAVITY: hlasitost STILL → COLLAPSE / stabilita 60 s všech pásem 44,1–192 kHz | rozdíl 2,2 dB / bez NaN, špička −0,3 dBFS |
| Efekty: stabilita v extrémech / ECHO 100 % + TO VOICES + GRAVITY 90 % 30 s / VOID FREEZE | bez NaN, ≤ −0,3 dBFS / stabilní / drží ±0,5 dB |
| CPU: výchozí patch se SPACE / nečinnost | 2,7 % / 0,65 % jádra |
| FILTER: 5 modelů, LP na 3 kHz / HP na 110 Hz / BP při max. rezonanci | −31 až −64 dB / −53 až −109 dB / +0,6 až +9,8 dB proti rezonanci 0 |
| Audit parametrů | 175 ze 175 mění zvuk |
| UBSan (nedefinované chování) | 0 chyb |

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
se zkopíruje do `C:\Program Files\Common Files\VST3`. Build i všechny testy na Windows procházejí (GitHub Actions);
v Abletonu na Windows zatím nevyzkoušeno.

## Ableton Live

1. Po buildu Live → Preferences → Plug-Ins → Rescan (nebo restart Live).
2. ResoOpus je v prohlížeči pod Plug-Ins → Gavr. Přetáhni ho na MIDI stopu: hned hraje drone.
3. Bílé klávesy C3–F3 hrají hlasy 1–4, dotek je zesílí nad úroveň HOLD.

## Licence

Proprietární software Gavr. Používá JUCE 8 (licence JUCE).

# ResoOpus – specifikace a plán vývoje

> Název **ResoOpus** (schválen 10. 10. 2026; dříve pracovně ResoMoon). Logo: `Reso` bíle + `Opus` zlatě.

Podklady: `RESERSE.md` (rešerše inspirací), `KONCEPT.md` (rozhodnutí s Markem),
`podklady/reso-moon-vst3-spec.md` (Markův podklad), Gavr blueprinty (ResoOG, ResoQBand).
**Pravidlo názvů:** v produktu (UI, parametry, tooltipy, presety, manuál, README) není žádný odkaz na hardware,
který nás inspiroval. Tento dokument proto používá jen produktové názvy.

---

## 1. Shrnutí

Drone syntezátor pro filmové, ambientní a experimentální plochy. Po vložení do Abletonu **hraje sám** (HOLD).
**Čtyři ručně, mikrotonálně laděné hlasy** ve dvou párech (1·2 a 3·4). Hlasy mezi sebou spojuje
**GRAVITY**: jeden velký knob vede od čistého ladění přes vzájemné přitahování hlasů (FM) až po kolaps do
samooscilující smyčky. Stereo mix, dva agresivní filtry, dvoulinkový ECHO a prostorová sekce SPACE
s pěti algoritmy. Volitelná melodická vrstva **LEAD** (dva VCO, glide, vibrato na tlak, arpeggiator).
Šum, drift a přeslechy (**DUST**) jsou záměrná součást zvuku a dají se stáhnout na nulu.

Formáty VST3 (Win + Mac), AU (Mac), Standalone. Cíl: Ableton Live 11/12.

## 2. Stack a identita

- JUCE 8.0.10, C++17, CMake + Ninja, universal `arm64;x86_64`, macOS 10.13+.
- `COMPANY Gavr`, `PRODUCT_NAME ResoOpus`, `BUNDLE_ID cz.gavr.resoopus`, `PLUGIN_CODE RsOp` (obsazené: RsQB, RsOG, RsTm, RsT2), instrument
  (`IS_SYNTH`, `NEEDS_MIDI_INPUT`, AU `aumu`, VST3 `Instrument|Synth`), **sidechain vstup** (stereo bus pro
  FOLLOWER a vnější audio do efektů).
- Testovací aplikace `<Name>Tests`, CI GitHub Actions Windows + macOS s anotacemi chyb (z ResoOG).
- Projektová pravidla v `.claude/` (C++ pravidla, verification-loop, přednost real-time pravidel).

## 3. Architektura zvuku

```
                    ┌──────────────────────── GRAVITY (makro) ───────────────────────┐
                    ▼                                                                  │
 SKUPINA A  pár 1·2  pár 3·4         SKUPINA B  pár 5·6  pár 7·8                       │
   hlas: CHOIR | SIREN | PULSAR   (typ volitelný pro každý hlas)                       │
   každý hlas: TUNE, charakter, obálka (ATT/RLS), VOL, PAN · skupina: HOLD, PITCH     │
        │  FM síť: pár ← sousední pár / BINARY LFO / FEEDBACK / sidechain              │
        ▼                                                                              │
 STEREO MIX (4 hlasy + LEAD + sidechain audio)                                          │
        ▼                                                                              │
 FILTER L / FILTER R (12 dB SVF s nelineárními integrátory, LP/BP, DRIVE, LINK, SPREAD)│
        ▼                                                                              │
 ECHO (2 linky, křížová ZV, modulace času SELF / LFO / sidechain) ──────── FEEDBACK ───┘
        ▼
 SPACE (AURORA / VOID / RUNNER / SATURN / COMET; X, Y, Z, BLEND)
        ▼
 MASTER (hlasitost, měkký limiter na −0,3 dBFS, měřič)
```

### 3.1 Hlasy (4 sloty, výchozí 1·2 CHOIR, 3 PULSAR, 4 SIREN; TYPE OFF hlas vypne)

> Změna 10. 10. (Marek): 8 → 4 hlasy. Pro široký drone stačí (4 × CHOIR = 20 pil), UI je klidnější a uvolněné
> místo dostal panel MASTER. Skupiny zrušeny: HOLD je u každého hlasu, PITCH a FILL jsou globální (MASTER).

| Typ | Zdroj | Ovladače | Charakter |
|---|---|---|---|
| **CHOIR** | 5 pilovitých oscilátorů **laděných relativně k TUNE hlasu** (OSC 1–5 = intervaly −24 až +36 st + centy) | TUNE (posune celý akord), **CHORD** (Unison / Octaves / Fifths / Major / Minor naplní intervaly jedním klikem), 5× interval se zap/vyp, **TIDE** | sborový akord v jednom hlasu. TIDE 0–50 % transpozice dolů, 50–100 % dál dolů + vzájemná FM pil („gravitace uvnitř hlasu“), dole sub-audio praskání |
| **SIREN** | modulátor (obdélník ↔ trojúhelník, 0,05–300 Hz) + band-limited obdélníkový tón + dělič + bílý šum | TUNE, FM (exponenciální, až ±3 okt), RATE, AM, DIVIDER (1:2–1:16 podtón), NOISE, WAVE | sirény, ptáci, „monstra“, lasery, vítr, příboj |
| **PULSAR** | jeden tón trojúhelník → žraločí ploutev → pila → obdélník (ShapeOsc, BLEP/BLAMP) | TUNE, SHARP, BLOOM (obálka ohýbá výšku až o −60 centů a ubírá ostrost, dokud hlas nenaběhne) | jemný až ostrý tón, „živý“ |

Hierarchie ovladačů (zjednodušeno 10. 10. – „méně je víc“):
- **hlas:** v pásu skupiny jen TUNE a charakter (TIDE / DIVIDER / SHARP). V detailu vybraného hlasu: TYPE, CHORD,
  intervaly OSC 1–5 (zap/vyp kliknutím na LED v popisku), TIDE, ATTACK, RELEASE (1 ms – 30 s), VOLUME, PAN.
- **skupina:** HOLD (spodní mez hlasitosti), PITCH, menu FILL.
- **globál:** GRAVITY (sám, velký). DUST je na stránce SPACE v MASTER, FEEDBACK do hlasů jako TO VOICES v ECHO.
- Vypuštěno z UI: PULL, FAST (rychlé doznění = krátký RELEASE), tlačítka LIGHT (LIGHT se přiřazuje tažením
  jako každý zdroj modulace), PATH (výchozí AUTO, ruční volba v menu pravého tlačítka na GRAVITY).
- Markovy úpravy 10. 10.: hlasy v pásech s vlastním štítkem („CHOIR 1“) a VOL lištou, TYPE i CHORD jako přepínač
  s LED, CHORD jen 5 akordů: Unison, Octaves, Fifths, Major, Minor.
Dotyk (velocity / aftertouch / MPE pressure) přidává hlasitost nad HOLD a lehký dotyk dává pomalejší náběh.

Ladění: TUNE v Hz i jako nota (`A2+12`), **SNAP** Free / Chromatic / Scale (stupnice + kořen). Skupina má
PITCH (transpozice se zachováním intervalů), celý nástroj **GLOBAL TUNE** (A4 = 415–466 Hz).
**FILL** (menu v hlavičce skupiny): zvolená stupnice/akord rozprostře 4 hlasy skupiny po svých tónech, pak se
jemně doladí uchem. Běžné ladění = 8 knobů TUNE, ne 40.

### 3.2 GRAVITY (středobod)

Jedno makro (0–100 %), modulovatelné (aftertouch, LFO, AETHER).

| Zóna | Co dělá v DSP |
|---|---|
| 0–25 % STILL | FM síť vypnutá, jen jemný vzájemný drift (zázněje) |
| 25–55 % ORBIT | lineární FM mezi sousedními páry roste (modulátor ×obálka modulátoru, takže doznívající hlas „pouští“) |
| 55–85 % TURBULENCE | přechod ze 2 smyček (skupiny zvlášť) do 1 uzavřeného kruhu (3·4→5·6, 7·8→1·2), přimíchá se FEEDBACK z ECHO a SELF modulace delaye |
| 85–100 % COLLAPSE | hloubka FM za hranou samooscilace, smyčka se chová jako LFO |

Pravidla: spojitý průběh bez skoků (přechody vah lineárně v rámci kousku ≤ 32 vzorků), kompenzace hlasitosti
(tabulka zisku + měkký limiter), FM vychází z naladěných intervalů (konsonantní ladění = hudební chaos).
Ruční režim pro pokročilé (menu pravého tlačítka na GRAVITY): a nastavit zdroj FM každého páru ručně
(PAIR / LFO / FEEDBACK / SIDECHAIN / OFF) a strukturu (2 smyčky / kruh).

### 3.3 DUST
0 % = čistě digitální. Výchozí ~20 %. Ovládá dohromady: pomalý drift ladění (náhodná procházka, max ±8 centů),
přeslechy mezi hlasy (−60 až −30 dB, do výstupu i do FM vstupů), šum pozadí (růžový, max −54 dBFS),
šum a ztrátu výšek v ECHO.

### 3.4 Efekty
- **FILTER L / R:** 2pólový TPT SVF s asymetricky saturujícími integrátory (agresivní, při rezonanci neztrácí
  basy), LP / BP, CUTOFF, RES, DRIVE, **LINK** (pravý sleduje levý), **SPREAD** (rozladění L/R pro stereo šířku).
- **ECHO:** dvě linky (TIME 1, TIME 2: 5 ms – 2 s, sync volbou v menu / psaním `1/8`), MOD 1/2, **TO VOICES** (feedback do FM hlasů), FB (nad polovinou samooscilace,
  ve smyčce měkký limiter a tlumení), modulace času SELF (vlastním výstupem) / LFO / SIDECHAIN, MIX.
  Charakter čistší, s DUST jde do lo-fi.
- **SPACE:** 5 algoritmů, společné knoby X / Y / Z (modulovatelné) + BLEND, popisky X/Y/Z se mění s algoritmem:

| Algoritmus | X | Y | Z | DSP |
|---|---|---|---|---|
| AURORA | oktáva nahoru | oktáva dolů | doznění | FDN reverb (8 linek) s pitch shifterem ±12 st ve zpětné vazbě |
| VOID | předzpoždění | modulace předzpoždění | doznění až FREEZE | velký FDN, modulované linky, nekonečné doznění |
| RUNNER | zpětná vazba | čas | transpozice opakování (±12 st, REVERSE) | delay s granulárním pitch shifterem ve smyčce |
| SATURN | frekvence | rychlost S&H | prostor | ring modulátor (sinus, frekvence S&H krokovaná) + malý reverb |
| COMET | zpětná vazba | čas | pitch | „špinavý“ samooscilující delay: saturace, pásmová ztráta, kolísání |

- Feedback do hlasů (FEEDBACK / TOTAL) se bere z výstupu ECHO se zpožděním ≤ 1 kousek (≤ 32 vzorků).
  Smyčka vede přes delay ≥ 5 ms, takže to zvuk nezmění. Měřeno testem stability.

### 3.5 LEAD (volitelná vrstva, vypínač u záložky)
2 VCO (morf sinus–trojúhelník–pila–obdélník + 2 morfovací tvary, OCTAVE LO/64'…1', TUNE ±1 okt, PW/SHAPE,
lineární FM, VCO 2 sync na VCO 1, SUB), ADSR s HOLD a LOOP, GLIDE, VIBRATO (rychlost, hloubka, zpoždění,
tlak → vibrato a jas), mono s prioritou poslední noty. ARP: směr (nahoru / dolů / ping-pong / random),
dělení (sync k Abletonu), oktávy, rytmický gate pattern 1–8, latch, stupnice. Jde do mixu (VOL, PAN) před filtry.

### 3.6 Modulace (stránka MOTION)
Zdroje (každý s barvou a táhlem pro drag na knob, jako ResoOG): **BINARY LFO** (FREQ A, FREQ B, OR = součet /
AND = součin obdélníků, Off / Link / Sync, výstup obdélník i „dvojitý trojúhelník“), **LFO A** (pomalé),
**LFO B** (rychlé, obě morf obdélník ↔ trojúhelník, sync), **RANDOM 1/2** (S&H ze šumu SIREN, slew),
**LIGHT** (pomalá organická křivka „světla“), **AETHER** (XY pad, mapovatelný na mod wheel / CC: X = výška
a vibrato všech hlasů, Y = hlasitost / swell), **STEPS** (5 kroků, 3/4/5 stages, gate na krok, sync),
**FOLLOWER** (obálka a gate ze sidechainu), obálky hlasů, velocity, aftertouch, MPE pressure.
Matice modulací převzatá z ResoOG (sloty: zdroj → funkce → controller → amount → cíl).

### 3.7 Hraní z MIDI
- **PLATES:** 8 bílých kláves jedné oktávy (výchozí C3–C4) = hlasy 1–8. Hlasy drží své ladění.
- **KEYS:** nota přeladí hlas (alokace: nejbližší volný hlas ve vybraných skupinách, glide přeladění), SNAP platí.
  Na klávesnici se hlasy na stejné notě zobrazí jako malé barevné tečky vedle sebe (max. 4 + „+n“). V PLATES je
  přiřazení pevné, čísla se nepřekrývají.
- **LEAD:** vrstva LEAD hraje chromaticky (rozsah kláves nastavitelný, nebo vlastní MIDI kanál).
- **TOUCH SOURCE:** Velocity / Aftertouch / Obojí / MPE. HOLD hraje bez MIDI.

## 4. UI

Logická velikost **1200 × 776**, škálování 70–150 % (jako ResoOG). Horní lišta 36 px, klávesnice 148 px,
dolní lišta 28 px. Stránky:
1. **DRONE**: skupina A (vlevo), **GRAVITY** uprostřed (velký knob ~190 px se zónami STILL / ORBIT /
   TURBULENCE / COLLAPSE, barevný oblouk modrá → fialová → oranžová → červená, 8 teček hlasů na oběžné dráze),
   skupina B (vpravo). Pod tím detail vybraného hlasu (např. 5 pil CHOIR).
2. **MOTION**: zdroje modulace.
3. **SPACE**: FILTER L/R, ECHO, SPACE, MASTER + DUST.
4. **LEAD**: VCO, obálka, voicing, ARP. **Vypínač vrstvy LEAD je přímo u záložky.**

Klávesnice v režimu PLATES označí 8 kláves čísly hlasů, hrané hlasy svítí. Barvy: CHOIR azurová `#38d6f0`,
SIREN růžová `#ff74b8`, PULSAR fialová `#b98bff`, LEAD zelená `#a5e844`, filtry zlaté, ECHO oranžové,
SPACE modré. GRAVITY má vlastní stupnici barev podle zóny. Postup: HTML mockup (`design/`), odsouhlasení,
pak JUCE UI podle mockupu a kontrola snímků.

## 5. DSP – rozhodnutí a cílové hodnoty

| Blok | Řešení | Cíl / test |
|---|---|---|
| Převzorkování | hlasy, FM síť, filtry 2× (výchozí), volba 1×/2×/4×, latence hlášena | |
| CHOIR pily | BLEP/BLAMP z ResoOG (`ShapeOsc`), FM lineární, frekvence ořezaná ≥ 0 | aliasing saw B6 2× > 85 dB |
| PULSAR | trojúhelníkové jádro s morfem do obdélníku, lineární FM průchodem nulou | aliasing při SHARP 100 % 2× > 80 dB |
| SIREN | Schmittův oscilátor (hystereze), dělič, AM/FM, šum | stabilita, bez DC (DC blokátor) |
| FM síť | po vzorcích (1vzorkové zpoždění mezi páry), modulátor × obálka | nic NaN/Inf, 60 s při GRAVITY 100 % na 44,1–192 kHz |
| GRAVITY | tabulky vah zón, spojité přechody, kompenzace zisku | rozdíl hlasitosti napříč 0–100 % ≤ 3 dB, max skok mezi vzorky ≤ 0,05 |
| Obálky | RC náběh/doznění, HOLD skupiny jako spodní mez, dotyk mění úroveň i rychlost | |
| Ladění | TUNE s rozlišením < 1 cent, SNAP přesně | odchylka < 1 cent |
| FILTER | TPT SVF, nelineární integrátory | samooscilace, ladění < 60 centů |
| ECHO | interpolace (kubická), limiter ve smyčce | stabilita při FB 100 %, výstup < +6 dBFS |
| SPACE | vlastní FDN, pitch shifter, granulární delay (žádný cizí GPL kód) | doznění měřené (RT60), FREEZE drží úroveň ±1 dB |
| Real-time | žádná alokace, zámky ani řetězce na audio vlákně, ScopedNoDenormals | |
| CPU (48 kHz, blok 256, M1) | | výchozí patch < 6 %, plná zátěž (8 CHOIR + LEAD + SPACE) 2× < 12 % |
| Úrovně | výchozí patch ~ −6 dBFS špička, žádný preset nad −1 dBFS | |
| Tiché přepínání presetů | fade-out 6 ms → reset všeho (vč. ECHO, SPACE, DC, oversamplerů) → fade-in | bez skoku > 0,05 |

## 6. Testy (TestHost, převzatá struktura z ResoOG)

Parametry (pořadí, ID, formát/parsování not a Hz), výchozí patch (hraje bez MIDI, úroveň), ladění a SNAP,
aliasing všech typů hlasů, GRAVITY (spojitost, hlasitost, stabilita všech zón na všech frekvencích),
ECHO/SPACE stabilita a doznění, PLATES/KEYS/TOUCH, LEAD (glide, priorita, arp sync), modulace, robustnost
(44,1–192 kHz, bloky 1–4096), stav, všechny presety, CPU (`--bench`), **audit 100 % parametrů mění zvuk**
(`--audit`), UI snímky, hostování nainstalovaného VST3, **test zakázaných názvů** (žádný řetězec v binárce,
presetech ani manuálu neobsahuje názvy inspirací).

## 7. Presety (v kódu, s kategoriemi a popisem „na co a co otočit“)

Kategorie: **Drones**, **Cinematic** (pady, Blade-Runner-style), **Orbits** (GRAVITY v pohybu),
**Textures** (SIREN, šum, vítr), **Collapse** (chaos), **Lead**, **Pulse** (STEPS, rytmické).
Cíl ve verzi 1.0: ~40 presetů. Recepty v manuálu: ambient, dub techno, filmový podklad, noise.

## 8. Roadmapa s branami

| Fáze | Obsah | Brána (musí projít) |
|---|---|---|
| **0** ✅ | rešerše, koncept, specifikace, **HTML mockup** | vzhled DRONE a název ResoOpus odsouhlaseny |
| **1** ✅ (Mac) | projekt, CMake, CI, převzatá infrastruktura z ResoOG, parametry, 8 hlasů CHOIR s CHORD a TIDE, HOLD, PITCH, FILL, Plates/Keys, Touch, Snap, UI stránky DRONE, 5 presetů | universal ✅, `lipo` ✅, `auval` ✅, testy proti nainstalovanému VST3 ✅, audit 150/150 ✅; **CI Windows čeká na push** |
| **2** ✅ (Mac) | 4 hlasy, TYPE OFF, MASTER (VOLUME, PITCH, FILL), engine SIREN a PULSAR, BLOOM, 7 presetů | universal ✅, auval ✅, testy + hostovaný VST3 ✅, audit 115/115 ✅, UBSan 0 ✅; aliasing SIREN 2× −93 dB |
| **3** ✅ (Mac) | GRAVITY (velký knob, pásma, PATH Auto/Pairs/Ring, modulace tlakem), FM síť po vzorcích s modulací průchodem nulou, zpětná vazba výstupu, DUST (drift, prosakování hlasů, šum) | stabilita 60 s ve všech pásmech a 44,1–192 kHz ✅ (špička −0,3 dBFS), hlasitost napříč pásmy 2,2 dB ✅, sweep bez skoků ✅, audit 134/134 ✅ |
| **4** ✅ (Mac) | stránka **FX**: FILTER (jeden stereo filtr se SPREAD místo L/R + LINK), ECHO (2 linky, SELF/LFO, TO VOICES), SPACE (AURORA, VOID + FREEZE, RUNNER, SATURN, COMET, popis algoritmu v panelu), usínání efektů v klidu | stabilita všech efektů v extrémech ✅, FREEZE drží ±0,5 dB ✅, ECHO 100 % + TO VOICES + GRAVITY 90 % 30 s ✅, audit 151/151 ✅, UBSan 0 ✅ |
| **5** | MOTION (zdroje + matice), LEAD + ARP, host sync | audit parametrů 100 % |
| **6** | kompletní UI podle mockupu, vizualizace GRAVITY, snímky | snímky všech stránek, gesta, škálování |
| **7** | presety, manuál CS/EN (Artifact), README, test zakázaných názvů | všechny presety v úrovni, verification-loop READY, Windows přes CI |

Každá fáze končí instalovatelnou verzí na Macu a commitem. Push na GitHub až na Markův pokyn (repozitář
nejdřív soukromý).

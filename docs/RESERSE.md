# Rešerše: SOMA Lyra-8 a Elta Music Solar 42 / 42F / 42N

Fáze 1 (rešerše) nového synthu Gavr. Stav k 10. 10. 2026.
Zdroje: oba **oficiální manuály celé** (Lyra-8 v1.0 2017 + v2.1 2020, Solar 42 01/2024 – lokálně v `reference/`,
v gitu ignorované), stránky výrobců, recenze Sound On Sound, specifikace prodejců. Kde se zdroje rozcházejí, platí
oficiální manuál (studentský „quick start“ Lyry, který koluje po webu, má několik chyb – např. funkci LINK).

---

## 1. Ve zkratce

| | **SOMA Lyra-8** | **Elta Music Solar 42 (F/N)** |
|---|---|---|
| Co to je | „Organismic synthesizer“ – 8hlasý analogový drone stroj, který se chová jako živý organismus | „Ambient machine“ – analogový mikrotonální drone stroj + polomodulární synth |
| Autor / země | Vlad Kreimer, SOMA Laboratory (2017) | Arseniy Tokarev, Elta Music (Solar 42: 2023/24, 42F: 2025, 42N: 2026) |
| Inspirace | Theremin, housle, indické rágy, neurobiologie (chaos, zpětné vazby) | Theremin „Harmonium“ (20.–50. léta), soundtrack *Blade Runner 2049* |
| Hlasy | 8 hlasů (4 páry, 2 skupiny), trojúhelník → obdélník | 8 hlasů: 4× „sbor“ 5 pil, 2× hlukové/sirény, 2× klasické VCO na klávesy |
| Ladění | ručně knobem, bez 1 V/okt – mikrotonální z principu | drony ručně knobem (mikrotonální), VCO přes 1 V/okt z dotykové klávesnice |
| Hraní | 8 dotykových senzorů na kůži (vodivost) + HOLD | 6 tlačítek dronů + 12 kapacitních plošek + joystick + sekvencery |
| Výstup | **mono** | **stereo** (pan na každém hlasu, 2 filtry, 2 efekty) |
| Efekty | Mod Delay (2 linky, křížová ZV, samo-modulace) → Distortion | Duální Polivoks filtr → duální „effector“ s cartridgemi (reverby, shimmer, pitch delay…) |
| Modulace | Hyper LFO, FM mezi hlasy, Total FB (výstup moduluje vše), vibrato | LFO A/B, 3 pulsery, 2× S&H, joystick, 5krokový sekvencer, arp/16krokový sekvencer, fotosenzory, envelope follower |
| Zvuk | temný, drsný, skřípavý, „sirény a křičící roboti“, ale i varhany a skleněná harmonika | obrovské vrstvené ambientní plochy, nestabilní drony, pady à la Blade Runner, mikrotonální clustery |
| Cena (orientačně) | cca 1 000–1 300 € | 42N: 2 259 € |

**Jednou větou:** Lyra je *nástroj na jeden nelineární organismus, který se hraje knoby a dotykem*,
Solar je *drone orchestr se stereo mixem a efekty, kde se jednotlivé „sbory“ zapínají tlačítky*.

---

## 2. SOMA Lyra-8

### 2.1 Filozofie (důležité pro náš design)
- Autor (neurobiolog) chtěl nástroj, který se chová jako **mozek: mnoho nelineárních smyček na hraně chaosu**.
  Moduly nejsou nové – unikátní je, **jak jsou propojené**. Obálka mění výšku tónu, výstup moduluje vstup,
  delay moduluje sám sebe.
- Ovladače nejsou „nastav a zapomeň“, ale **hrací prvky** (TUNE, PITCH, MOD, TIME 1/2, FB, FM struktura).
- Žádná kvantizace, žádné temperované ladění – „elektronické housle, které tě slyší“.
- **Šum a přeslechy jsou záměr**: na konci řetězce není VCA, části obvodu jsou stále otevřené, mezi hlasy jsou
  přeslechy („všechno modifikuje všechno a zvuk dýchá“), delay je lo-fi a při max. času šumí.
  Autor výslovně: čistota a stabilita mu zní mrtvě – chtěl „divokou, trochu nebezpečnou, živou přírodu“.
- Frekvenční charakteristika upravená pro živé hraní: **basy mírně zdvižené, výšky změkčené**.

### 2.2 Architektura a tok signálu

```
 senzor 1..8 ──► obálka hlasu 1..8 ─┐ (obálka je součástí oscilátoru: mění i výšku a tvar)
                                    ▼
 TUNE ─► HLAS 1..8 (tri ↔ square = SHARP, po párech) ◄── FM zdroj páru: sousední pár / Hyper LFO / Total FB / CV
                                    │
                         součet 8 hlasů (mono) + EXT IN
                                    ▼
                    MOD DELAY (2 linky, křížová ZV, rezonance mezi linkami,
                               modulace času: LFO / SELF / CV)
                                    ▼
                       DISTORTION (drive + mix)  ──► VOL ──► OUT (mono)
                                    │
                                    └──► TOTAL FB: výstup po zkreslení může nahradit LFO jako FM zdroj
```

### 2.3 Hlasy
- **8 hlasů**, ne VCO – spíš tónové generátory starých elektrických varhan, záměrně nelineární.
- Hierarchie ovladačů:
  - **na hlas:** `TUNE` (speciální potenciometr, kroky menší než půltón, rozsah „desítky až tisíce Hz“), senzor;
  - **na pár (12, 34, 56, 78):** `FAST` (rychlé/pomalé), `SHARP` (tvar trojúhelník → obdélník, platí i pro FM),
    `MOD` (hloubka FM), přepínač zdroje modulace (nahoru = sousední pár / střed = vypnuto / dolů = LFO CV);
  - **na skupinu (1234, 5678):** `PITCH` (transpozice se zachováním intervalů, normální poloha skoro na maximu),
    `HOLD` (minimální hlasitost = drone);
  - **globálně:** `VIBRATO` (8 nezávislých vibrát, každé jiné tempo), `TOTAL FB`, přepínač FM struktury.
- Rozsahy: hlasy 1–2 basové, 3–6 středové, 7–8 o oktávu výš než 3–6 (ale všechny se dají přeladit kamkoli).
- **Obálka:** v podstatě AR spouštěné senzorem. Lehký dotek / rychlé ťukání = pomalý náběh, menší tlak = obálka
  se otevře jen částečně (senzor je „velocity“ přes vodivost kůže). Pomalý režim: náběh i doznění v řádu
  **~10–20 s** (SoS uvádí ~20 s release), FAST: **~0,5–0,8 s**. Přepnutím FAST během doznívání se tón utne.
- **FAST + HOLD trik:** pár s FAST potřebuje vyšší HOLD, aby zazněl → pomalým otáčením HOLD naskakují hlasy
  postupně (nejdřív pomalé páry, pak rychlé). Využívají to performeři.
- **HOLD a obálka běží paralelně:** HOLD drží spodní mez hlasitosti, dotykem se přidá nad ni.

### 2.4 FM mezi hlasy (srdce Lyry)
- Každý hlas + jeho obálka = **FM operátor**. Vliv modulátoru **klesá s doznívající obálkou** (jako operátor v DX7).
- Přepínač zdroje na páru: pár 12 ↔ 34 a 56 ↔ 78 se modulují navzájem (dvě uzavřené smyčky, každá skupina sama).
- **Přepínač struktury `34>56 78>12`:** nahoře 34 moduluje 56 a 78 moduluje 12 → **všech 8 hlasů v jedné
  uzavřené FM smyčce**. LFO nebo vypnutý pár smyčku „rozpojí“.
- `MOD` na maximu = **samooscilace modulační smyčky**, smyčka se začne chovat jako LFO (extrémní, spontánní režim).
- **Čím výš je hlas naladěný, tím méně je citlivý na FM** – nejcitlivější jsou basy (= lineární FM, pevná odchylka v Hz).
- Výška hlasu závisí i na tom, **které sousední hlasy právě znějí** (protože jsou modulátory).
- `TOTAL FB` + LFO jako zdroj: výstup celého nástroje (včetně delaye a zkreslení) moduluje hlasy → **celý nástroj
  je jedna FM struktura**. `CV VOICES` vstup (0–5 V, ne 1 V/okt) nahradí LFO i Total FB externím signálem.

### 2.5 Hyper LFO
- Dvě jednoduchá LFO (`FREQ A`, `FREQ B`) s obdélníkem.
- `AND/OR`: dole = **součet** (analogový, výstup má schody/gradient), nahoře = **součin** (logické AND).
- `LINK`: jemná FM – A moduluje B.
- Moduluje vybrané páry hlasů a delay. Pro delay je navíc speciální **trojúhelník = součet dvou trojúhelníků A a B**.

### 2.6 Mod Delay
- **Dvě delay linky** s křížovou zpětnou vazbou a „rezonancí mezi linkami“; `TIME 1`, `TIME 2`, `MOD 1`, `MOD 2`.
- Modulace času: `LFO` (trojúhelník/obdélník viz výše) nebo **`SELF` – čas je modulován vlastním výstupem**
  (unikátní; ze stabilních rezonancí se stanou nestabilní). `CV DELAY` vstup (lineární vztah čas/napětí).
- `FB`: **kousek za polovinou začne delay samooscilovat** – „delay se stane synthem“, ve smyčkách stojaté vlny,
  otáčení časem je přelaďuje. Na hraně samooscilace jde delay „rozkopnout“ hraním hlasů.
- Charakter: **lo-fi, tmavý, silně filtrovaný, při dlouhých časech šumí** (podle SoS „2/3 analogový“, časy jdou
  záměrně za hranici čistoty). Typický čip v této třídě je PT2399 (30–340 ms čistě, ~1 s se ztrátou kvality) –
  přesný obvod Lyry SOMA nezveřejňuje, to je náš odhad.
- Recepty z manuálu: „reverb“ = TIME 1 a 2 kolem 12–15 h (různě), FB 10–11 h, MIX 12 h; „chorus“ = časy skoro na minimu.

### 2.7 Distortion a výstup
- **Až za delayem**, takže delay ovlivňuje zkreslení. `DRIVE`, `MIX`, `VOL`. Přidává jas a „crunch“, trhá signál.
- `EXT IN` se míchá s hlasy a jde přes delay i zkreslení → Lyra jako efekt; se SELF/TOTAL FB vnější signál
  deformuje i modulační smyčky.
- `HOLD GATE` CV (+5 V = plný HOLD).

### 2.8 Zvuk a použití
- Dark ambient, drone, experimentální, industriál, noise, soundtrack; meditativní improvizace s looperem.
- SoS: vyniká v **„křiku, dronech a mnoha příchutích chrupavé disonance“**; umí i varhanní akordy a tóny
  třpytící se jako skleněná harmonika. Velká část drsnosti = lo-fi delay + zkreslení.
- Hráčská zkušenost (SynthSeeker): „nehraj noty, hraj delay time, feedback, sharpness a modulaci“,
  mikro-pohyby knobů, nechat stroj „žít“; většinou se připojuje externí reverb.
- Slabiny: chybí jemné ladění, nutnost neustále dolaďovat, přeslechy, šum, mono, žádné MIDI.

### 2.9 Existující softwarová emulace
- **LIRA-8** (Mike Moreno, 2020) – VST3/AU/LV2 + Pure Data, „pay what you want“, neoficiální.
  Kód je veřejný, ale **nic z něj nepřebíráme** (licence, a hlavně nechceme kopii) – jen jako reference chování.

---

## 3. Elta Music Solar 42 / 42F / 42N

### 3.1 Historie verzí
| Model | Rok | Co přinesl |
|---|---|---|
| Solar 50 | ~2020 | 50 oscilátorů / 10 hlasů po 5 pilách, čistě drone; šum vypnutých hlasů |
| **Solar 42** | Superbooth 2023, prodej 2024 | 6 dronů + 2 VCO na 1 V/okt, dotyková klávesnice, dvojitý effector, tichý napájecí zdroj |
| **Solar 42F** | 2025 | sub-oscilátory ve VCO, **Band Pass + Stereo Distortion** ve filtru, LIN/EXP přepínač CV, bypassy effectoru, menší šum |
| **Solar 42N** (MK3) | 2026 | **přepínač rychlosti LFO**, **CV pro pravý filtr**, zjednodušené načítání efektů, bypassy zase pryč |

(Název „42“ = odkaz na Stopařova průvodce – „odpověď na otázku života, vesmíru a vůbec“.)

### 3.2 Architektura a tok signálu

```
 HLAS 1 (sbor 5 pil)  HLAS 2 (sbor)  HLAS 3 (siréna/šum)  HLAS 6 (sbor)  HLAS 7 (sbor)  HLAS 8 (siréna/šum)
 HLAS 4 (VCO)  HLAS 5 (VCO)  EXT IN  PIEZO PREAMP
        │  každý: VOL + PAN
        ▼
   STEREO MIX L/R ──► DUAL VCF (Polivoks L / Polivoks R) ──► DUAL EFFECTOR (cartridge L / cartridge R)
                                                                  │ BLEND, MASTER
                                                                  ▼
                                                          WET OUT L/R  (+ DRY OUT hlasů 4 a 5)
 Řídicí zdroje: LFO A/B, pulsery, 2× S&H, joystick X/Y, 5krokový sekvencer, obálky hlasů,
               dotyková klávesnice (CV, gate, pressure, arp, 16 kroků), envelope follower z piezo mikrofonu
```

Vše analogové kromě dotykové klávesnice a effectoru (polodigitální). Dva filtry i dva effectory se mírně liší
→ záměrně „živé“ stereo.

### 3.3 Drone hlasy „Classic“ (1, 2, 6, 7) – sbor 5 pil
- Každý hlas = **5 jednoduchých pilovitých generátorů bez 1 V/okt**, každý vlastní `TUNE`:
  gen. 1 **C0–G2**, 2 **B1–G3**, 3 **D3–B4**, 4 **E4–C6**, 5 **G5–D7** (dohromady ~8 oktáv; přibližně).
- **MUTE** tlačítko pro každý generátor → akordy, triády, dvojzvuky nebo jediný tón v jednom hlasu.
- **MOD** tlačítko pro každý generátor: zapne modulaci výšky z `CV MOD` vstupu **nebo z fotosenzoru**
  (okolní světlo mění ladění; zachytí i blikání úsporných žárovek). Bez MOD běží generátory stabilně.
- **VOLT** knob: transponuje všech 5 generátorů dolů; **za polovinou dráhy se generátory začnou navzájem
  FM modulovat** (stejný princip „ladění přechází v chaos“ jako u Lyry).
- Obálka **Attack/Release** (`ATT`, `RLS`) otevírá VCA hlasu; spouští ji tlačítko drone klávesnice, `GATE` vstup,
  nebo **HOLD** (trvalý drone). `env out` (−10…+10 V) jde použít na spuštění dalšího hlasu, filtr nebo efekt.

### 3.4 Drone hlasy „New“ (3, 8) – sirény a šum (podle DIY strojů Papa Srapy)
- Dva **Schmittovy klopné oscilátory**: pomalý (obdélníkový modulátor, `RATE` + `x1/x10`, má CV výstup) a
  zvukový (`PITCH` + `RANGE`, cca **C0–E7**).
- 4 režimy podle přepínačů FM/AM: stálý tón / **FM** (hloubka `FM AMT` + `DIVIDER`) / **AM** (přerušovaný tón) /
  **AM + FM**. Plus **bílý šum** (všechno doleva = čistý šum), šum jde i do S&H (taktovaného zvenku).
- Zvuky: sirény, ptačí křik, vesmírné příšery, salvy, příboj, vítr.
- Stejná AR obálka, HOLD, GATE jako u classic hlasů.

### 3.5 VCO hlasy (4, 5) – klasický synth na klávesy
- Analogové VCO na čipu **AS3340** (trojúhelníkové jádro), **6 tvarů** včetně dvou morfovacích
  (pila ↔ obrácená pila, sinus ↔ trojúhelník), `SHAPE` = PWM s CV, lineární FM s attenuátorem,
  **8polohový oktávový přepínač** (LO, 64'…1'), `TUNE` ±1 oktáva, sync (hlas 5), hlas 4 normalizovaně moduluje
  FM hlasu 5. 42F přidává **sub-oscilátory**.
- Každý má **ADSR obálku s VCA, s HOLD a LOOP** (smyčka = LFO/rytmus).
- Ovládané **dotykovou klávesnicí** (V/okt + gate normalizované na oba). Mají i suché výstupy mimo filtr/efekt.

### 3.6 Dotyková klávesnice (12 kapacitních plošek)
- Reaguje na **plochu prstu, ne sílu** (pressure výstup).
- Režimy: Single (12 plošek), **Twin** (2× 6), **Split** (2 strany s vlastním nastavením).
- Chování: klávesnice / **arpeggiator** (forward/back/ping-pong, latch, transpoziční variace x1–x3, rytmický
  gate pattern 1–8 kroků) / **16krokový sekvencer** (směry vč. random, gated/continuous).
- **Kvantizér s 19 stupnicemi** (dur/moll módy, blues, pentatoniky, japonská, gamelan, cikánská, arabská,
  flamenco, celotónová…) **nebo mikrotonální režim** (každá ploška se ladí v krocích 2,5 mV ≈ 3 centy).
- Glide/legato, vibrato (rychlost/hloubka/zpoždění, i od tlaku), pressure výstup jako pressure / ASR / AD /
  LOOP / random. Interní hodiny 10–300 BPM, 4 presety.

### 3.7 Modulace
- **LFO A (pomalé) a LFO B (rychlé)**: tvar plynule obdélník ↔ trojúhelník, **jen kladné napětí** (kvůli LED
  fotosenzorů). 42N: přepínač rychlosti. + **3 obdélníkové pulsery**, **2× Sample & Hold** (zdroj šum z hlasů 3/8).
- **Joystick** X/Y (−10…+10 V, nula uprostřed, offset knoby).
- **5krokový sekvencer** (Buchla styl): 0–5 V na krok, gate vypínač na krok, 3/4/5 kroků, interní PULSER nebo ext. clock.
- **Piezo kontaktní mikrofon** přímo v těle nástroje + preamp (až 40 dB) + **envelope follower a gate** →
  ťukání do nástroje jde do efektů nebo řídí modulace.

### 3.8 Filtry
- **Dva 12dB Polivoks filtry** (sovětské čipy UD12 / 140UD12 jako integrátory – SVF 2. řádu).
  Charakter: „špinavý a naštvaný“, **při rezonanci neztrácí basy**, integrátory se asymetricky saturují a při velkém
  vstupu/rezonanci jsou nestabilní. Každý kus trochu jiný → L/R se mírně liší (= stereo šířka).
- `FREQ`, `RESONANCE`, `CV`, **`LINK`** (CV levého ovládá oba). 42F: **Band Pass + Stereo Distortion**;
  42N: CV i pro pravý filtr.

### 3.9 Dual Effector (cartridge systém, polodigitální)
- Dva efektové procesory (L a R), každý může mít načtený jiný efekt. Společné knoby **X, Y, Z** (všechny s CV
  ±10 V), `BLEND` (mix), `MASTER`. Na výstupu effectoru není VCA → na maximu je slyšet digitální šum.
- Cartridge = 3 programy. Sada (24 stran manuálu):

| Cartridge | Programy (X / Y / Z) |
|---|---|
| CATHEDRAL – reverby | Shimmer (okt. nahoru / okt. dolů / decay) · Oct-up delay · Space reverb |
| MAGIC – pitched delays | Pitch delay · Reverse pitch delay · Bell pitch delay (feedback / delay / pitch) |
| TIME – mod delays | Delay reverb · Delay chorus · Delay vibrato |
| VIBROTREM | Tremolo · Vibrato · Chorus (depth / rate / reverb) |
| FILTER | Auto-wah · HP/LP · Notch |
| VIBE | Phaser · Flanger · Resonance flanger |
| PITCH SHIFTER | SynthTaver · Octaver · Pitch harmonizer |
| INFINITY – velký ambient | Resonance reverb · O.D.D. (oscillating dirty delay) · Resonance delay |
| STRING RINGER | Synthetic ring · Ring mod · S&H ring mod |
| SYNTEX-1 – basový synth | Vibe synth · Pulse synth · Acid synth |
| DIGITAL – crusher | Filter DAC · LFO DAC · Envelope crusher |
| GENERATOR – šumový mini synth | FM tone · Ramp · Voice |

Pozn.: „+ reverb na Z“ je u většiny programů → efekty jsou stavěné na ambient (vše má dozvuk).

### 3.10 Zvuk a použití
- „Obrovské vrstvené ambientní plochy, nestabilní analogové drony, pady à la Blade Runner, mikrotonální clustery,
  šumově-organické textury, pomalu se vyvíjející akordy, znepokojivý soundtrack.“
- Navržený tak, aby šlo udělat **celý drone/ambient track bez multitracku** (8 hlasů zapínaných tlačítky + mix + efekty).
- Divadlo, film, živá vystoupení, mikrotonální hudba.

---

## 4. Průnik – společná DNA obou strojů

To, co mají oba společné, je přirozený základ našeho pluginu:

1. **Drone jako výchozí stav, ne nota.** Hlasy běží pořád (HOLD), hraje se zapínáním, laděním a modulací –
   ne „stisknu klávesu, zazní tón“.
2. **Banka volně laděných hlasů bez temperovaného ladění.** Lyra: 8 hlasů knoby, Solar: 4 × 5 pil knoby.
   Mikrotonalita z principu, intervaly se ladí uchem.
3. **Ladění, které se zlomí v chaos.** Lyra: `MOD` → FM mezi páry až do samooscilace. Solar: `VOLT` za polovinou →
   generátory se navzájem FM modulují. Na obou je jeden knob „od harmonie k chaosu“.
4. **Pomalé AR obálky + hold + gate pro každý hlas**, spouštěné dotykem/tlačítkem, výstup obálky jako modulace.
5. **Dotyk jako dynamika:** Lyra – vodivost kůže (plocha, vlhkost), Solar – kapacitní plošky (plocha prstu).
   Obě „velocity“ jsou ve skutečnosti *plocha/tlak v čase* → u nás aftertouch / MPE pressure / velocity.
6. **Vnější „organické“ vlivy:** kůže (Lyra), světlo a kontaktní mikrofon (Solar) → v pluginu
   „dech“ / pomalý náhodný drift / envelope follower z externího audia (sidechain).
7. **Efekty jsou součást nástroje**, ne dodatek: Lyra delay+zkreslení, Solar filtr+reverby/pitch delaye.
   Obě mají **zpětnovazební delay, který umí samooscilovat**.
8. **Ruská/postsovětská lo-fi analogová estetika**: přeslechy, šum, nepřesnost, nestabilita jsou „feature“.
   Dědictví **Theremina** (Lyra ho cituje jako ideální živý synth, Solar staví na jeho „Harmoniu“).

**Kde se liší (a kde si musíme vybrat):**

| Téma | Lyra-8 | Solar 42 | Poznámka pro nás |
|---|---|---|---|
| Struktura | jeden nelineární organismus, vše se ovlivňuje | modulární, jasné bloky, patchování | Lyra = duše, Solar = rozsah |
| Stereo | mono | stereo (pan, 2 filtry, 2 efekty) | stereo chceme |
| Barva hlasu | tri↔square, FM | 5 pil (sbor), sirény/šum, VCO | dva typy hlasů? |
| Filtr | žádný (jen tónová korekce) | 2× Polivoks | |
| Melodie | ne (jen ladění) | 2 VCO + klávesnice, arp, sekvencer | v Abletonu MIDI chceme |
| Efekty | delay + dist (syrové) | reverb/shimmer/pitch (uhlazené) | obojí dává smysl |
| Ovládání | málo prvků, každý zásadní | hodně prvků, „konzole“ | Marek preferuje knoby |

---

## 5. První nápady pro plugin (k diskusi, ne finální plán)

Pracovní představa: **„drone organismus“ s 8 hlasy** – duše a smyčky z Lyry, stereo, sbory, filtr a efekty ze Solaru.

- **8 hlasů ve 4 párech / 2 skupinách** (Lyra hierarchie: hlas → pár → skupina → globál). Každý hlas má:
  `TUNE` (Hz/noty/centy, volitelně přichycení na stupnici), obálku (pomalá/rychlá, ATT/RLS), HOLD, VOL, PAN.
- **Typ hlasu** (přepínač na páru nebo hlasu):
  - *Lyra* – trojúhelník ↔ obdélník (`SHARP`), obálka ovlivňuje výšku a tvar,
  - *Choir* (Solar classic) – 5 pil s vlastním laděním a MUTE, `VOLT` → vzájemná FM,
  - *Siren* (Papa Srapa) – Schmitt oscilátor + modulátor, FM/AM, šum.
- **FM síť** jako Lyra: zdroj páru (sousední pár / Hyper LFO / Total FB / sidechain), přepínač struktury
  (2 smyčky / 1 velká smyčka), `MOD` až do samooscilace, modulace slábne s obálkou.
- **Hyper LFO** (A, B, součet/součin, LINK) + LFO A/B ze Solaru, S&H, **XY pad místo joysticku**, 5krokový sekvencer.
- **Stereo mix → 2× Polivoks-style filtr (LP/BP, link, mírně rozladěné L/R) → Mod Delay (2 linky, SELF, samooscilace)
  → Distortion → Space (reverb/shimmer v duchu effectoru)**. Pořadí delay → zkreslení zachovat (je to zvuk Lyry).
- **Hraní v Abletonu (klíčové rozhodnutí):**
  - režim *Plates*: 8 kláves (např. C3–G3 bílé) = 8 senzorů, velocity/aftertouch = „tlak prstu“, hlasy mají vlastní ladění;
  - režim *Tuned*: MIDI nota přeladí hlas/pár na tu notu (melodie), drone zbytek drží;
  - HOLD / drone bez MIDI vůbec (vložím plugin a hraje).
- **„Živost“**: řízený drift ladění, přeslechy mezi hlasy jako parametr (`BLEED`), šum delaye – vše s výchozí
  rozumnou hodnotou, ne vypnuté.
- DSP poznámky pro pozdější specifikaci: smyčky (Total FB, SELF delay, FM ring) **musí běžet po vzorcích**
  (ne po 32 blocích, jinak se změní charakter); FM a zkreslení s oversamplingem; Polivoks = SVF se saturujícími
  (slew-limited) integrátory; delay = tmavý lo-fi model (filtrace, komprese, šum, modulace hodin).
  CPU nároky nízké: max. ~40 jednoduchých oscilátorů + 2 filtry + delay.

---

## 6. Otevřené otázky pro Marka (vkus a priority)

1. **Kolik Lyry vs. kolik Solaru?** Spíš „Lyra se stereem a filtrem“, nebo „Solar s Lyřinou FM duší“?
2. **Melodické VCO hlasy** (Solar 4/5 + arpeggiator/sekvencer) – chceš, nebo čistě drone nástroj?
3. **Jak hrát z Abletonu** – 8 kláves = 8 hlasů, chromaticky, nebo obojí?
4. **Efekty** – syrový delay + zkreslení (Lyra), nebo i uhlazené reverby/shimmer (Solar)? Obojí?
5. **Míra chaosu ve výchozím stavu** – má plugin po vložení znít krásně, nebo rovnou „žít“?
6. Název – složka se jmenuje `ResoFoon`; je to finální název?

## 7. Co by se hodilo od tebe (další fáze)

- Zvukové ukázky / videa, která se ti na obou strojích líbí (a co přesně na nich).
- Manuál **Solar 42N** (máme jen Solar 42 z 01/2024; N je fyzicky blízko F/N změnám popsaným výše).
- Fotky panelů 42N, případně tvoje poznámky z hraní, pokud jsi na ně hrál.
- Žánry, ve kterých ho chceš používat (dub techno, ambient, soundtrack…?).

---

## Zdroje
- Lyra-8 User Manual v1.0 (04/2017) a v2.1 (2020), SOMA Laboratory – `reference/lyra8_manual_v2.1.pdf`,
  https://somasynths.com/wp-content/uploads/2020/11/LYRA-8_manual_Eng_V2_1.pdf
- Solar 42 User Manual (01/2024), Elta Music – `reference/solar42_manual_2024-01.pdf`,
  https://www.eltamusic.com/_files/ugd/12d408_d4c9881033524945ae276dc3559589ff.pdf
- Elta Music Solar 42N/F: https://www.eltamusic.com/solar-42f
- Sound On Sound, Paul Nagle: Soma Laboratory Lyra-8 (08/2017): https://www.soundonsound.com/reviews/soma-laboratory-lyra-8
- Schneidersladen – Solar 42N (změny MK3, cena): https://schneidersladen.de/en/elta-music-solar-42n
- Signal Sounds / Moog Audio – Solar 42F (změny F): https://signalsounds.com/elta-music-solar-42f-multi-voice-drone-instrument
- Synthtopia – Solar 42 oznámení: https://www.synthtopia.com/?p=138360
- Perfect Circuit – Solar 42N review: https://www.perfectcircuit.com/signal/elta-solar-42n-review (stránka blokuje stažení, citováno z výtahu vyhledávače)
- Gearnews – LIRA-8 (neoficiální emulace): https://www.gearnews.com/lira-8-vst-organismic-synthesizer-emulates-soma-labs-other-worldly-hardware
- Polivoks filtr (140UD12 jako integrátor, saturace): https://learningmodular.com/glossary/polivoks/ , https://cherryaudio.com/news/secrets-of-atomika
- PT2399 (typické limity lo-fi delaye): https://electricdruid.net/useful-design-equations-for-the-pt2399/

---

## 8. Porovnání s Markovým podkladem (`docs/podklady/reso-moon-vst3-spec.md`, 10. 10. 2026)

Podklad se s rešerší z oficiálních manuálů z velké části shoduje. Navíc potvrzuje Solar mixer s 10 kanály
(8 hlasů + EXT AUDIO + PREAMP, ověřeno na fotce panelu). Kontrolou proti manuálům a fotkám panelů (`reference/img/`) vyšlo:

| Bod v podkladu | Skutečnost (zdroj) | Dopad na plugin |
|---|---|---|
| Solar classic drone: „`TUNE` = celkové ladění hlasu“ | **každý z 5 generátorů má vlastní TUNE** (manuál str. 7, fotka: 5 sloupců MUTE/TUNE/MOD). Celkovou výšku hlasu posouvá `VOLT` | hlas Choir = 5 ladicích knobů + VOLT |
| VCO: „Hard sync na VCO 1, VCO 1 → FM VCO 2“ | **sync je na VCO 2** (hlas 5), VCO 1 (hlas 4) má výstup normalizovaný do FM VCO 2 (manuál str. 9, fotka: jack `sync` u VCO 2) | VCO 2 se synchronizuje na VCO 1 |
| Lyra `SHARP`: trojúhelník → „shark-fin“ → čtverec/pila | manuál: **trojúhelník → čtverec** (pila se nezmiňuje) | morf tri → square, „shark-fin“ jako mezistav je v pořádku |
| Lyra `LINK` | manuál v2.1: LINK = jemná FM, A moduluje B. **Fotka novějšího panelu má u stejného přepínače popisek `SYNC`** (šipka FREQ B → FREQ A), tzn. hardware revize nejspíš změnila funkci na synchronizaci | u nás přepínač Off / Link (FM) / Sync – obě varianty |
| „negas / transitron oscilátory“ (Lyra i Solar) | v manuálech není. Lyra: „jako tónové generátory starých elektrických varhan“, Solar: „jednoduché pilovité generátory“ | neověřeno, nehraje roli: modelujeme chování, ne součástky |
| Lyra delay = „PT2399“ | neověřeno, byl to můj odhad podle třídy zařízení. Manuál ani SoS čip neuvádí | jen charakter: tmavý, lo-fi, šum při dlouhých časech |
| Solar: „2 LFO“ | manuál: **5 LFO generátorů** = LFO A + LFO B + pomalé modulátory Papa Srapa hlasů (RATE ×1/×10, mají CV out) a PULSER | nemění koncept |
| Papa Srapa „5 režimů“ | manuál: 4 režimy (tón / FM / AM / AM+FM) + nastavení na čistý šum | stejné v praxi |
| Polivoks LP/BP/Distortion | BP a Stereo Distortion má **až 42F**, původní 42 (fotka) jen LP | uděláme LP / BP + drive |

### Rozložení panelů (z fotek)
- **Lyra-8:** nahoře tři moduly (Hyper LFO / Mod Delay / Distortion). Uprostřed 2× HOLD, 2× PITCH, mezi nimi
  TOTAL FB, VIBRATO a přepínač struktury. Dole 4 rámečky párů (MOD + SHARP + 2× TUNE + FAST) a pod nimi 8 senzorů.
  Nad každým MOD je přepínač zdroje s popiskem sousedního páru (`34`, `12`, `78`, `56`) a `LFO FB CV`.
  **Hierarchie se čte shora dolů: globál → skupina → pár → hlas.** To je dobrá předloha pro naše rozložení.
- **Solar 42:** 4 classic drony v rozích horní řady, uprostřed Dual Effector a pod ním filtry L/R. Střední řada:
  Papa Srapa hlasy na krajích, VCO 1/2 s EG vedle nich, uprostřed 10kanálový mixer (PAN nahoře, VOL dole).
  Spodní řada: LFO A, joystick, 5krokový sekvencer, preamp, envelope follower, LFO B. Pod tím dotyková
  klávesnice a vpravo 6 tlačítek DRONE VOICES. **Mixer a efekty jsou ve středu a hlasy kolem nich.**

# Technická specifikace a architektonický rozbor synthesizérů SOMA Laboratory Lyra-8 a ELTA Music Solar 42 pro vývoj VST3 pluginu (Ableton Live)

## Úvod a účel dokumentu
Tento dokument slouží jako kompletní, hloubkový technický a architektonický podklad pro návrh a vývoj softwarového VST3 pluginu (určeného primárně pro Ableton Live a moderní DAW) s pracovním názvem **ResoMoon VST3**. Plugin je přímo inspirován dvěma legendárními analogovými a semi-modulárními drone synthesizéry: **SOMA Laboratory Lyra-8** (autorem je Vlad Kreimer) a **ELTA Music Solar 42 / 42F / 42N** (autorem je Arseniy Tokarev).

Dokument detailně popisuje vnitřní strukturu, hlasovou architekturu, modulační obvody, efektové jednotky i ovládací prvky obou nástrojů, analyzuje jejich společné rysy a zásadní rozdíly a předkládá konceptuální návrh pro jejich syntézu do podoby VST3 pluginu.

---

## 1. Detailní popis SOMA Laboratory Lyra-8

### 1.1 Filosofie a koncept "Organismic" synthesis
Lyra-8 je koncipována jako „organismický“ analogový synthesizér. Slovo *organismic* vyjadřuje, že jednotlivé moduly nástroje spolu vzájemně interragují ne-lineárním způsobem, čímž chování synthesizéru připomíná živý organismus nebo reálný akustický nástroj (např. housle či čelo).
* **Ne-linearita a nelineární obvody:** Zvukové generátory nejsou standardní subtractive VCOs s lineární či logaritmickou závislostí na řídicím napětí (1V/oct). Místo toho využívají nelineární obvody připomínající tónové generátory starých elektrických orgánů (využívající tzv. negas oscilátory / ne-linear transitron obvody).
* **Absence koncového VCA (Bleed & Chaos):** Na konci signálového řetězce není klasický VCA, který by při uvolnění klávesy zvuk absolutně umlčel. Obvody jsou neustále v otevřeném/respirujícím stavu, což způsobuje přirozený přeslech (crosstalk), jemný šum a vzájemnou modulační interferenci mezi oscilátory.

### 1.2 Hlasová architektura (Voice Architecture)
Lyra-8 disponuje **8 zvukovými generátory (hlasy)**.
* **Struktura a dělení hlasů:**
  * 8 hlasů je rozděleno do 4 párů: `(1-2)`, `(3-4)`, `(5-6)`, `(7-8)`.
  * 4 páry jsou dále sdruženy do 2 skupin po 4 hlasech: `1234` (levá skupina) a `5678` (pravá skupina).
  * Tato hierarchie tvoří stromovou strukturu pro řízení i FM modulaci.
* **Frekvenční rozsahy hlasů:**
  * *Hlasy 1 a 2:* Basové rozsahy (nízké frekvence).
  * *Hlasy 3 až 6:* Střední frekvenční pásmo.
  * *Hlasy 7 a 8:* Vysoké frekvence (cca dvakrát vyšší rozsah než 3–6).
* **Ovládací prvky hlasové sekce:**
  * `TUNE` (8× potenciometr): Ne-kvantizované plynulé ladění každého hlasu zvlášť od desítek Hz po jednotky kHz. Speciální potenciometry umožňují mikrotonální dolaďování v rozsahu menším než půltón.
  * `PITCH` (2× potenciometr): Globální transpozice celé skupiny 4 hlasů (pro skupinu 1234 a 5678). Zachovává relativní intervaly mezi hlasy ve skupině.
  * `SHARP` (4× potenciometr): Plynulá změna vlnového průběhu pro každý pár hlasů z jemného trojúhelníkového tvaru (triangle) přes zakřivený tvar ("shark-fin") až po ostrý čtverec / pilu (square wave).
  * `HOLD` (2× potenciometr): Nastavení minimálního offsetu obálky pro skupinu 4 hlasů. Umožňuje trvalou znělost hlasů v režimu dronu bez nutnosti držet senzory.
  * `FAST` (4× páčkový přepínač): Mění čas uvolnění (release) pro daný pár hlasů. V poloze dolů (FAST ON) se obálka zavírá rychle a dotykové senzory vyžadují větší tlak/plochu prstu pro spuštění.
* **Dotykové senzory (Sensors 1–8):**
  * Každý hlas má dvojici kovových kontaktů. Zvuk se spouští propojením kontaktů vodivostí lidského těla (kůže).
  * Rychlost náběhu (attack) a hlasitost závisí na ploše dotyku, tlaku a vlhkosti kůže. Sérií rychlých lehkých doteků lze docílit pomalého náběhu.

### 1.3 Modulace a FM matice (FM Synthesis & Cross-Modulation)
Hlasy v Lyra-8 mohou fungovat buď v režimu 8-hlasých elektrických orgánů, nebo jako samostatné FM operátory.
* `MOD` (4× potenciometr): Nastavení hloubky modulace pro každý pár hlasů.
* **Přepínače zdroje FM modulace (4× třípolohový přepínač):**
  * *Střední poloha (OFF):* Modulace pro pár je vypnuta.
  * *Horní poloha (Voice Pairs):* Křížová modulace sousedními páry. Pár 3-4 moduluje 1-2, pár 1-2 moduluje 3-4, pár 7-8 moduluje 5-6, pár 5-6 moduluje 7-8.
  * *Dolní poloha (LFO / FB / CV):* Zdroj modulace se odvíjí od globálních přepínačů a externích vstupů.
* `34>56 78>12` (FM Structure switch): Při přepnutí nahoru se propojí obě skupiny do jedné uzavřené FM smyčky (pár 3-4 moduluje 5-6 a pár 7-8 moduluje 12).
* `TOTAL FB` (Total Feedback switch): Nahradí LFO signál celkovým zvukovým výstupem synthesizéru (odebraným až za sekcí distortion). Všechny obálky, delay i distortion se tak stávají součástí jediné obří FM smyčky.
* `VIBRATO`: Přepínač aktivující vibrato pro všech 8 hlasů. Nástroj obsahuje 8 samostatných, mírně odlišných generátorů vibrata.

### 1.4 Generátor modulací: Hyper LFO
Hyper LFO je složitý nízkofrekvenční generátor syntetizovaný ze dvou jednoduchých čtvercových LFO.
* `FREQ A` a `FREQ B`: Nastavení frekvence dvou interních LFO operátorů.
* `AND / OR` přepínač:
  * *OR (dolní poloha):* Analogové sčítání průběhů FREQ A a FREQ B (vzniká stupňovitý průběh).
  * *AND (horní poloha):* Logické násobení čtvercových průběhů FREQ A a FREQ B.
* `LINK`: Zapíná jemnou FM modulaci mezi LFO operátory (FREQ A moduluje FREQ B).

### 1.5 Efektová sekce: Mod Delay & Distortion
* **MOD DELAY:**
  * Obsahuje 2 nezávislé delay linky (`TIME 1`, `TIME 2`) s křížovou zpětnou vazbou (`FB`) umožňující rezonanci a samo-oscilaci.
  * `SELF / LFO` přepínač: V poloze `SELF` je čas zpoždění modulován vlastním výstupním signálem delaye (vytváří unikátní pitch-shifting a fázové dekonstrukce). V poloze `LFO` je modulován Hyper LFO generátorem.
  * `TRIANGLE / SQUARE` přepínač: Volba modulačního průběhu z LFO pro delay (Triangle je součet dvou trojúhelníků FREQ A+B; Square pochází z AND algebry).
  * `MIX`: Poměr mezi čistým a zpracovaným signálem.
* **DISTORTION (Overdrive):**
  * Zapojen na úplném konci řetězce (AŽ ZA delayem), takže delay přímo ovlivňuje chování distortionu.
  * Ovládání: `DRIVE` (míra zkreslení) a `MIX` (dry/wet poměr).

### 1.6 Vstupy / Výstupy a CV rozhraní
* `OUT`: Mono vyvážený 6.3mm TRS/TS výstup.
* `EXT IN`: Audio vstup pro zpracování vnějších signálů skrze Mod Delay a Distortion.
* `HOLD GATE`: Vstup pro napěťové řízení funkce HOLD (+5V plně otevírá VCA).
* `CV DELAY`: Unipolární vstup (0 až +5V) pro lineární řízení času zpoždění.
* `CV VOICES`: Unipolární vstup (0 až +5V) pro řízení výšky tónu hlasů s aktivovaným LFO CV. Nejedná se o 1V/oct standard, ale o modulativní ne-lineární vstup.

---

## 2. Detailní popis ELTA Music Solar 42 (Solar 42, 42F, 42N)

### 2.1 Filosofie a koncepce
ELTA Music Solar 42 (a jeho verze 42F a 42N) od Arsenije Tokareva je mikrotonální ambientní drone stanice a semi-modulární samostatný synthesizér. Je přímým evolučním nástupcem modelu Solar 50. Koncepce byla inspirována časnými elektronickými nástroji z 20.–50. let 20. století (např. *Harmonium* Leva Theremina) a sci-fi filmovými soundtracky (*Blade Runner 2049*).

Solar 42 kombinuje analogové dronové oscilátory, analogové VCO s trojúhelníkovým jádrem, digitální dotykový ovladač/sekvencer, Buchla-style sekvencer, stereo Polivoks filtry a cartridge efektor.

### 2.2 Hlasová architektura (8 Hlasů ve 3 kategoriích)
Solar 42 disponuje **8 samostatnými hlasovými bloky**, které se dělí do tří odlišných syntetických koncepcí:

#### Skupina A: 4× Classic Drone Voices (Voices 1, 2, 6, 7)
* Každý z těchto 4 hlasů obsahuje **5 samostatných pilovitých oscilátorů** (negas oscilátory realizované na tranzistorech, bez V/Oct řízení). Celkem 20 pilovitých generátorů.
* Generátory jsou odstupňovány podle frekvence: 1. oscilátor (C0–G2), 2. (B1–G3), 3. (D3–B4), 4. (E4–C6), 5. (G5–D7). Pokrývají rozsah cca 8 oktáv.
* **Ovládací prvky Classic Drone hlasu:**
  * `TUNE`: Potenciometr celkového ladění hlasu.
  * `VOLT`: Transponuje všech 5 oscilátorů směrem dolů. Po překročení poloviny dráhy začnou oscilátory mezi sebou vytvářet divokou FM modulaci.
  * `MUTE 1–5` (5× tlačítko): Zapínání/vypínání jednotlivých z 5 oscilátorů (umožňuje skládat akordy a dvojhlasy).
  * `MOD 1–5` (5× tlačítko): Aktivace modulace pro jednotlivé oscilátory z fotodetektoru nebo CV MOD vstupu.
  * `PHOTO-SENSITIVE DETECTOR` (SENSORS): Světlo-citlivé oko na panelu. Změna okolního osvětlení přímo mění frekvenci oscilátorů s aktivním MOD.
  * `ATT` / `RLS`: Nastavení Attack a Release obálky pro daný hlas.
  * `GATE` tlačítko / `HOLD`: Spuštění hlasu.

#### Skupina B: 2× New Drone Voices / Papa Srapa Noise Generators (Voices 3, 8)
* Navrženo podle DIY šumových generátorů ruského elektro-šamana Papa Srapa. Určeno pro reprodukci sirén, ptačích výkřiků, vesmírných příšer, výstřelů, mořského příboje a větru.
* **Architektura:** Obsahuje 2 Schmitt Trigger oscilátory (jeden nízkofrekvenční jako čtvercový modulátor, druhý v audio rozsahu C0–E7) + generátor bílého šumu (White Noise).
* **5 provozních režimů (kombinace přepínačů FM a AM):**
  1. *Continuous Drone:* Čistý tón řízený `PITCH` a `RANGE`.
  2. *FM Drone:* Frekvenční modulace (přepínač FM ON). Rychlost řídí `RATE` a `X1/X10`, hloubku `FM AMOUNT` a `DIVIDER`.
  3. *AM Drone:* Amplitudová modulace / přerušovaný tón (přepínač AM ON).
  4. *FM + AM Drone:* Kombinovaná přerušovaná frekvenční modulace.
  5. *Clean White Noise:* Čistý bílý šum (FM i AM OFF, PITCH na 0, NOISE na MAX).
* **Sample & Hold (S&H):** Bílý šum z hlasů 3 a 8 je interně přiveden do integrovaného Sample & Hold generátoru, který při přivedení hodinového signálu (`CLOCK`) generuje náhodná řídicí napětí.

#### Skupina C: 2× Volt/Octave VCO Voices (Voices 4, 5 / VCO 1 & VCO 2)
* Dva plnohodnotné analogové oscilátory s trojúhelníkovým jádrem postavené na čipu AS3340.
* **6 vlnových průběhů:** Sine, Triangle, Saw, Square, Morphing Saw (saw to inverted saw), Morphing Sine (sine to triangle).
* **Výbava a ovládání:**
  * Plná 1V/oct kompatibilita pro přesné hraní melodií.
  * `OCTAVE` (8-polohový přepínač: LO, 64', 32', 16', 8', 4', 2', 1').
  * `TUNE`: Plynulé dolaďování v rozsahu 1 oktávy.
  * `SHAPE` / `PW`: Šířka impulsu čtverce a morphing průběhů + CV vstup s attenuátorem.
  * `FM`: Lineární FM vstup s attenuátorem (přepínatelný LIN/EXP u verzí F/N).
  * `SYNC`: Hard Sync vstup na VCO 1. Vnitřní normalizace: výstup z VCO 1 je normalizován do FM vstupu VCO 2.
  * `EG VCA 1 & 2`: Plnohodnotná ADSR obálka pro každý VCO hlas s režimy `HOLD` a `LOOP` (převod obálky na opakovaný LFO).

### 2.3 Dotykový ovladač / Klávesnice (Touch Sensitive Controller)
* 12 kapacitních dotykových plošek s vestavěným mikrokontrolérem a OLED displejem.
* **Měření tlaku/plochy (Pressure):** Výstup napětí `PRESSURE` je přímo úměrný ploše kůže přiložené na plosku.
* **Provozní režimy klávesnice (`BEHAVIOUR`):**
  * `SINGLE`: 12 kláves působí jako jeden ovladač.
  * `TWIN`: Rozdělení na dva nezávislé 6-klávesové ovladače se společnými parametry.
  * `SPLIT`: Dva zcela nezávislé ovladače s vlastními parametry (např. vlevo arpeggiátor, vpravo klávesnice).
* **Módy hry:** `Touch Keyboard`, `Arpeggiator` (směry: forward, backward, ping-pong, random; rhythm step gate, variation x1–x3), `16-step Sequencer` (krokový sekvencer s editorem bran, not, legato a portamento).
* **Kvantizátor (Quantizer):** Integrovaný kvantizátor s přednastavenými stupnicemi (Ionian, Dorian, Pentatonic, Flamenco, Gamelan, Gypsy, Microtonal atd.). Při vypnutí kvantizace funguje jako mikrotonální ovladač.

### 2.4 Modulační a řídicí bloky (CV Sources)
* **5-Stage Sequential Voltage Source (Buchla-style Sequencer):** 5 kroků s potenciometry (0–5V), interním hodinovým generátorem `PULSER`, přepínači bran (`GATE 1–5`) a nastavitelnou délkou (3, 4, 5 kroků).
* **Joystick:** 2-osý páčkový ovladač s aretací polohy. Obsahuje offsetové potenciometry pro osy X a Y s výstupním rozsahem -10V až +10V (0V ve středu).
* **LFO A & LFO B:** Dva LFO s plynule měnitelným průběhem od čtverce po trojúhelník (`WAVE`). LFO A je pomalý, LFO B rychlý. Generují kladné napětí (0 až +10V) optimalizované pro fotosenzory.
* **Preamp pro kontaktní mikrofon (Piezo) + Envelope Follower:** Integrovaný piezo snímač pod čelním panelem (reaguje na poklep, škrábání na šasi nástroje). Zesilovač s gainem až +40dB + obálkový sledovač s výstupem CV obálky a Gate +8V.

### 2.5 Mixáž, Polivoks Filtry a Cartridge Efektor
* **Stereo Mixer:** 10 kanálů (8 hlasů + Preamp + Ext Audio). Každý kanál má vlastní potenciometr hlasitosti (`VOL`) a panoramatu (`PAN`).
* **Dual Polivoks Filter:**
  * Dvojitý analogový 12dB/oct filtr využívající sovětské integrované obvody UD12 (nebo rozptýlené operační zesilovače).
  * Vyznačuje se agresivním, špinavým charakterem; při zvýšení rezonance neztrácí basové frekvence.
  * Ovládání: `FREQ` (cut-off), `RESONANCE`, `CV` vstup, tlačítko `LINK` (propojení levého a pravého kanálu pod jeden ovladač Cutoff) a přepínání Low-Pass / Band-Pass / Distortion mode.
* **Dual Cartridge Effector:**
  * Dvojitý efektový procesor využívající fyzické výměnné cartridge (stejný systém jako pedál ELTA Console).
  * Nezávislé nahrávání efektů do levého a pravého kanálu. 3 efekty na kartu, řízení parametrů X, Y, Z pomocí CV (-10V až +10V).
  * Dostupné cartridge: Cathedral, Magic, Time, Vibrotrem, Filter, Vibe, Pitch Shifter, Infinity, String Ringer, Syntex-1, Digital, Generator.

---

## 3. Srovnávací analýza — Společné rysy a příbuzné koncepty

I když Lyra-8 a Solar 42 pocházejí od různých vývojářů (SOMA vs ELTA Music), sdílejí hlubokou konceptuální spřízněnost:

1. **Drone & Ambient Zaměření:** Oba nástroje byly navrženy pro plynulé, meditativní soundscapy, filmovou hudbu, mikrotonální kompozice a drony bez nutnosti používat klasické mřížkové sekvencery.
2. **8-Hlasá Architektura:** Oba synthesizéry staví na základu 8 samostatných hlasových bloků/generátorů.
3. **Taktilní Dotykové Rozhraní:** Oba nástroje opouštějí tradiční plastové piánové klávesy ve prospěch dotykových plošek/kontaktů reagujících na lidské tělo nebo plochu prstu.
4. **Důraz na Nelinearitu a FM/Cross-Modulaci:** Jak v Lyra-8 (FM matice, Total Feedback), tak v Solar 42 (VOLT knob FM na drone hlasech, Papa Srapa FM/AM) hraje klíčovou roli vzájemná modulace oscilátorů produkující organické chaosy.
5. **Vestavěný Zkreslovací a Efektový Řetězec:** Oba nástroje spoléhají na to, že efektová jednotka (Mod Delay + Distortion u Lyra-8; Polivoks Filter + Cartridge Effector u Solar 42) není pouze externím doplňkem, ale primárním modulativním prvkem zvuku.
6. **Zpracování Externího Audia (Ext In):** Oba synthesizéry umožňují přivést vnější zvukový signál (kytara, bicí automaty) a prohánět jej skrze interní filtry, modulační smyčky a efekty.

---

## 4. Rozdílová analýza — Zásadní odlišnosti

| Parametr / Vlastnost | SOMA Laboratory Lyra-8 | ELTA Music Solar 42 (42F / 42N) |
| :--- | :--- | :--- |
| **Typ hlasové architektury** | Homogenní (8 identických orgánových negas hlasů odstupňovaných rozsahem). | Heterogenní (4× Classic 5-saw Drones, 2× Papa Srapa Noise, 2× AS3340 VCOs). |
| **Pevné ladění / 1V/oct** | Nemá 1V/oct. Čistě ne-kvantizované manuální ladění uchem. | Hybridní: 6 drone hlasů je ne-kvantizovaných, 2 VCO hlasy mají plnou 1V/oct kompatibilitu. |
| **Dotyková klávesnice** | Čistě analogové kontakty (měří elektrický odpor kůže těla). | Kapacitní plošky s mikrokontrolérem, OLED displejem, tlakem (plocha) a kvantizátorem. |
| **Sekvencer & Arpeggiator** | Nemá žádný digitální sekvencer/arpeggiator. | Má vestavěný 16-step sekvencer/arp na klávesnici + Buchla 5-stage analogový sekvencer. |
| **VCF (Filtr)** | **NEMÁ klasický VCF filtr.** Zvuk se tvaruje pomocí `SHARP` waveshapingu a delaye. | **Obsahuje stereo 12dB Polivoks VCF filtr** s rezonancí, LPF/BPF a distortionem. |
| **Efektový procesor** | Analogovo-digitální Mod Delay (PT2399 lo-fi) s křížovou rezonancí + analogový Overdrive. | Dual Cartridge Effector (výměnné karty s DSP programy, např. Shimmer, Reverb, Pitch). |
| **Organické / Senzorické prvky**| Žádné světelné senzory ani piezo. Pouze odpor kůže na dotykových ploškách. | Světelné fotosenzory na 4 drone hlasech + vestavěný Piezo snímač pod šasi. |
| **Panorámování a Mixáž** | Mono výstup. Všechny hlasy se interně sčítají do jednoho kanálu. | Stereo výstup s plným mixážním pultem (`VOL` a `PAN` pro každý z 8 hlasů zvlášť). |
| **Modulační LFO** | Hyper LFO (kombinování 2 čtverců přes algebru AND/OR + LINK). | LFO A (pomalý) + LFO B (rychlý) s plynulým přechodem mezi čtvercem a trojúhelníkem. |
| **Paměť a Presety** | Žádné presety ani digitální paměť. | Klávesnice umožňuje uložit 4 uživatelské presety (skál, arp nastavení, kroků). |

---

## 5. VST3 Plugin Architektura & Návrh Implementace v Ableton Live ("ResoMoon VST3")

S ohledem na specifikaci má být tento `.md` soubor podkladem pro návrh VST3 pluginu určeného pro Ableton Live. Níže je předložen kompletní architektonický návrh pluginu **ResoMoon VST3**, který fúzuje silné stránky obou synthesizérů.

### 5.1 Hybridní modulární koncepce "ResoMoon"
Plugin spojuje ne-lineární organoidní chování a FM strukturu Lyra-8 s různorodostí hlasů, filtrem Polivoks, sekvencery a cartridge efektorem Solar 42.

```
[VOICE ENGINE (8 Hlasů)]
 ├── 2× Organoid FM Pairs (Lyra-8 style Negas / Organs with SHARP & FAST)
 ├── 2× Solar Classic Drones (5-saw Stack with VOLT FM & Mute buttons)
 ├── 2× Papa Srapa Noise / Noise Generator (Schmitt Trigger FM/AM/S&H)
 └── 2× AS3340 Precise VCOs (Morphing Waves, 1V/oct, Hard Sync, ADSR)
         │
         ▼
[STEREO VOICE MIXER] ──► (Individual VOL, PAN, Mute & Direct Dry Routing)
         │
         ▼
[DUAL POLIVOKS VCF] ──► (12dB LPF/BPF, Link Mode, Drive & CV Cutoff)
         │
         ▼
[DUAL EFFECT ENGINE]
 ├── Path A: SOMA Mod Delay (Self-Modulation, Cross-FB & Overdrive)
 └── Path B: Dual Cartridge DSP (Shimmer, Pitch Delay, Space Reverb)
         │
         ▼
[MASTER STEREO OUTPUT & ANALOG DRIFT ENGINE]
```

### 5.2 Klíčové moduly a parametry VST3 rozhraní

#### 1. Voice Matrix (Sekce hlasů)
* Umožňuje přepínat architekturu jednotlivých 8 hlasových slotů mezi režimy:
  * `SOMA Voice` (Lyra-8 negas orgán s ovladači `TUNE`, `SHARP`, `HOLD`, `FAST`).
  * `Solar 5-Saw Drone` (stack 5 oscilátorů s ovladači `VOLT`, `MUTE 1-5`, `MOD 1-5`).
  * `Papa Srapa Noise` (AM/FM/Noise režimy s ovladači `PITCH`, `RATE`, `DIVIDER`, `FM AMT`).
  * `AS3340 VCO` (přesný 1V/oct oscilátor s morphingem vln, octave switchem a ADSR obálkou).

#### 2. Organic Modulation Engine
* **Hyper LFO Subsystem:** Implementace algoritmu AND/OR algebry se sčítáním a násobením čtverců + LINK FM.
* **Env Follower & Piezo Emulator:** Emulace piezo poklepu na šasi a obálkového sledovače pro modulaci parametrů z interního či externího audio vstupu (Ableton Sidechain input).
* **Virtual Light / Photo Sensor:** Vizuální ovladač intenzity osvětlení (popř. mapovatelný na LFO / Expression) ovlivňující fotocitlivé oscilátory.

#### 3. Control Interface & Touch Grid (MPE / Ableton Push Integration)
* **Touch Grid (12-Plate Controller):** Plná podpora **MPE (MIDI Polyphonic Expression)** v Ableton Live 11/12.
  * *Pitch Bend / Glide:* Plynulý microtonální glide mezi ploskami.
  * *Pressure (Aftertouch):* Mapováno na plochu dotyku (emulace skin contact z Lyra-8 i kapacitního tlaku ze Solar 42).
* **Buchla 5-Stage & 16-Step Arp Sequencer:** Integrovaný krokový sekvencer s podporou synchro s hodinovým signálem Abletonu (Host Sync).

#### 4. Filter & Effect Routing
* **Dual Polivoks VCF:** DSP emulace nelineárního chování sovětského čipu UD12. Zapínatelný `LINK` pro stereo filtraci.
* **Dual Effect Block (SOMA Delay + Cartridges):**
  * Možnost sériového nebo paralelního řazení SOMA Mod Delay (s rezonancí PT2399) a vybrané DSP cartridge (např. Shimmer Reverb / Pitch Shift).

#### 5. Analog Drift & Bleed Control (Unikátní VST3 prvek)
* Jelikož v DAW je digitální ticho standardem, VST3 plugin obsahuje dedikovaný ovladač `ANALOG BLEED / CHAOS`:
  * *0 %:* Čistý digitální provoz bez šumu a přeslechů.
  * *100 %:* Věrná emulace analogové ne-linearity, přeslechů mezi hlasy (crosstalk), teplotního driftu oscilátorů a respirace šumu v pozadí.

---

## 6. Závěr
Tento dokument popisuje všechny klíčové systémové složky synthesizérů SOMA Lyra-8 a ELTA Solar 42 na základě oficiálních manuálů a video návodů. Dokument představuje kompletní technický podklad pro vývojáře VST3 pluginu **ResoMoon**, který přináší tyto legendární koncepce do prostředí Ableton Live.

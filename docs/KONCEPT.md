# Koncept (pracovní, před finální specifikací)

Stav: 10. 10. 2026, po rešerši (`RESERSE.md`) a první zpětné vazbě od Marka. Není to finální plán.

## Rozhodnuto
- **Drone nástroj.** Zvukově blíž Solaru 42 (filmový, „cinematic“, drone), ale s duší Lyry-8.
- Zachovat společnou DNA obou strojů:
  1. Výchozí stav je drone, ne stisknutá nota.
  2. Hlasy se ladí ručně a mikrotonálně.
  3. Jeden knob vede od ladění k chaosu přes FM mezi hlasy.
  4. Pomalé obálky a HOLD.
  5. Dotyk funguje jako dynamika.
  6. Efekty se zpětnou vazbou jsou součástí nástroje.
  7. Lo-fi estetika, šum je „feature“.
  8. Odkaz na Theremina.
- **Efekty uhlazené** (reverb, shimmer, mod delay), ne syrové zkreslení Lyry.
- Přepínatelná **dynamika**: velocity / aftertouch (tlak) / obojí.
- Přepínatelné **hraní**: 8 kláves = 8 hlasů, nebo chromaticky/ve stupnici.
- Název začíná `Reso` a skrytě odkazuje na vesmírný drone (pracovně **ResoMoon**). Složka zatím `~/ResoFoon`.

## „Solar s duší Lyry“: co to znamená
**Tělo ze Solaru:**
- 8 drone hlasů ve stereu (VOL + PAN).
- Hlavní typ hlasu je **sbor 5 pil** s laděním a MUTE pro každou pilu.
- Typ **Siren / Noise** jako druhá barva.
- Dva filtry v duchu Polivoksu (L/R mírně rozladěné).
- Efektová sekce: reverb, shimmer, pitch delay.

**Středobod nástroje: GRAVITY** (pracovní název, Markův návrh 10. 10.)
Jeden velký knob uprostřed panelu, makro od ladění k chaosu. Zóny:
- **0–25 % Still:** čisté ladění, jen pomalé zázněje mezi hlasy.
- **25–55 % Orbit:** jemná vzájemná FM mezi páry. Hlasy se „přitahují“, přibývají postranní tóny a tón se chvěje.
- **55–85 % Turbulence:** 2 smyčky se spojí do jedné. Přidá se zpětná vazba z efektů a self-modulace delaye.
- **85–100 % Collapse:** smyčka samoosciluje a modulace se chová jako LFO (Lyra na maximu).

Vzhled (Markův požadavek: větší než ostatní):
- Hlavní knob přibližně 2× větší než běžné knoby (běžný 96 × 98 px, GRAVITY ~190 px), uprostřed panelu mezi skupinami hlasů.
- Kolem stupnice 4 zóny s popisky STILL / ORBIT / TURBULENCE / COLLAPSE.
- Vlastní barva.
- Vnější prstenec ukazuje živou hodnotu včetně modulace (tlak, LFO), jak to známe z ResoOG.
- Uvnitř nebo kolem knobu jemná živá vizualizace: 8 teček (hlasy) na oběžné dráze. Ve Still klidně krouží, s rostoucí GRAVITY se přitahují a v Collapse chaoticky víří. Velikost knobu je tím obhájená: knob zároveň ukazuje stav nástroje.

Pravidla:
- Plynulý průběh bez skoků.
- Hlasitost kompenzovaná.
- Modulátor slábne s obálkou.
- Modulovatelný: tlak/aftertouch, LFO, AETHER.
- Lokální GRAVITY na skupině pro jemné doladění.
- Klíčová vlastnost: FM vychází z naladěných intervalů hlasů. Když jsou hlasy naladěné konsonantně, i chaos zní hudebně.

**Duše z Lyry** (co hlasy propojuje a dělá z nich organismus):
- **GRAVITY** viz výše.
- **Obálka mění i barvu a výšku:** při náběhu se hlas mírně rozjasní a „usadí“ do ladění.
- **Feedback z efektů zpět do hlasů:** Total FB, decentně, na knobu.
- **Mod delay se 2 linkami a SELF modulací.** Je čistší než u Lyry. Na hraně samooscilace „zpívá“.
- **DUST / LIFE:** drift ladění, přeslechy mezi hlasy, šum pásky a delaye. Ve výchozím stavu je mírně zapnutý.
- **Theremin:** XY pad / mod wheel „AETHER“ ovládá výšku (drift, vibrato) a hlasitost všech hlasů najednou jako ruce u antén.

## Architektura (návrh po Markově podkladu `podklady/reso-moon-vst3-spec.md`)
Markův podklad navrhuje 8 hlasových slotů s přepínatelným typem. To sedí s naším konceptem. Úprava:
melodické VCO nebudou mezi drony, ale jako **samostatná vrstva LEAD**.

```
DRONE (8 slotů, 4 páry, 2 skupiny – Lyra hierarchie)        LEAD (vypínatelná vrstva, vypínač u záložky)
 typ slotu: CHOIR (5 pil) / PULSAR (tri↔square) /       2× VCO (morf vln, sync VCO2←VCO1, FM),
            SIREN (tón/FM/AM/šum)                ADSR s LOOP, glide, zpožděné vibrato,
 výchozí rozložení podle Solaru:                             volitelný arpeggiator / 16 kroků
   1, 2, 6, 7 = CHOIR · 3, 8 = SIREN · 4, 5 = PULSAR
        │  FM síť mezi páry ◄── GRAVITY (+ Total FB z efektů)
        ▼
 STEREO MIXER (VOL, PAN pro 8 hlasů + LEAD + EXT/sidechain)
        ▼
 2× FILTR (agresivní 12 dB SVF) (LP/BP, drive, LINK, L/R mírně rozladěné)
        ▼
 MOD DELAY (2 linky, křížová ZV, SELF/LFO) ──► SPACE (reverb / shimmer / pitch delay) ──► OUT
        ▲
 DUST: drift, přeslechy, šum (0 % = čistě digitální, výchozí mírně)
```

- Modulace: BINARY LFO (A, B, OR/AND, Off/Link/Sync), LFO A (pomalé) a LFO B (rychlé), 2× S&H, XY pad
  (joystick, Theremin AETHER), 5krokový sekvencer synchronizovaný s Abletonem, envelope follower ze sidechainu.
- „Fotosenzor“ ze Solaru: zdroj modulace **LIGHT** (pomalá organická náhodná křivka), dá se přiřadit na MOD
  jednotlivých pil v CHOIR.
- Zkreslení Lyry jen jako jemný DRIVE ve filtru. Efekty zůstávají uhlazené.
- **DUST** (přeslechy, drift, šum) je něco jiného než **GRAVITY** (chaos přes FM). Proto dva oddělené ovladače.

## Názvy v produktu (Markovo pravidlo 10. 10.)
**V pluginu, UI, presetech, manuálu ani textech na webu nesmí být odkaz na původní synthy** (Lyra, SOMA, Solar,
Elta, Papa Srapa, Polivoks, názvy cartridgí, Hyper LFO…). Původní názvy patří jen do interních poznámek (rešerše).

| Interně (inspirace) | V produktu |
|---|---|
| hlas Lyra (tri↔square) | **PULSAR** (dříve ORGAN – zněl moc jako varhany) |
| Solar classic drone (5 pil) | **CHOIR** |
| Papa Srapa | **SIREN** |
| VOLT | **TIDE** (gravitace Měsíce uvnitř hlasu ↔ GRAVITY mezi hlasy) |
| Hyper LFO | **BINARY LFO** (dvě LFO jako dvojhvězda) |
| Polivoks filtr | **FILTER** |
| Cathedral / Infinity / Magic / String Ringer / O.D.D. | **AURORA / VOID / RUNNER / SATURN / COMET** |
| fotosenzor | **LIGHT** |
| joystick / Theremin | **AETHER** |

## Hlasy a efekty – detail (po Markových podkladech 10. 10.)
Podklady k VOLT a DIVIDER jsou interpretace (manuál má k oběma jen jednu větu). Kde se s manuálem rozcházejí,
platí manuál. Zbytek bereme jako zvukový směr a doladíme ho podle ucha.

**CHOIR – knob TIDE** (pracovně dříve VOLT)
- 0–50 %: plynulá transpozice všech 5 pil dolů (akord zůstává čistý).
- 50–100 %: dál dolů a **pily se začnou modulovat navzájem** uvnitř hlasu. Akord se rozpadne do hutného vrčení
  s kovovými postranními tóny a zázněji („dýchá“).
- Úplně dole sub-audio: periody pily se mění v praskání a pulzy (rytmická textura).
- Rozsah transpozice manuál neuvádí → naše volba, výchozí návrh až −5 oktáv (ověříme poslechem).
- Vztah ke GRAVITY: **TIDE = gravitace uvnitř hlasu** (mezi 5 pilami), **GRAVITY = gravitace mezi hlasy** (páry).
  Dvě úrovně téhož principu.

**SIREN** (interně dříve „Papa Srapa“)
- Přepínače FM/AM dávají 4 režimy: tón / FM / AM (přerušovaný) / AM+FM. Šum: všechny knoby doleva, NOISE na max
  (podle manuálu; Markův podklad uvádí DIVIDER a MOD doprava, to manuál nepotvrzuje).
- RATE + ×1/×10 = rychlost modulátoru. FM AMT = hloubka.
- **DIVIDER (naše interpretace):** dělič frekvence (sub-harmonické poměry 1:1 až 1:16) a zároveň škálování
  odchylky. Výsledek: od pomalých sirén přes rytmické pulzy po „monstra“ a laserové výstřely.
- Šum z hlasu napájí S&H.

**SPACE – 5 algoritmů s odlišným charakterem** (vybráno z 36 programů Solar cartridgí)
Ovládání: knoby **X / Y / Z** (s modulací) + **BLEND**. Každý algoritmus má jinou roli:

| Algoritmus | Inspirace | X / Y / Z | Charakter |
|---|---|---|---|
| AURORA | Cathedral – Shimmer | oktáva nahoru / oktáva dolů / doznění | světlý, éterický, „andělský“ pad |
| VOID | Infinity – Resonance reverb | předzpoždění / modulace předzpoždění / doznění (až nekonečné) | obrovský temný prostor, zamrznutí |
| RUNNER | Magic – Pitch / Reverse / Bell pitch delay | zpětná vazba / čas / transpozice opakování | melodické padající/stoupající ozvěny, Blade Runner |
| SATURN | String Ringer – Ring / S&H ring mod | frekvence / rychlost / prostor | kovový, mimozemský, sci-fi |
| COMET | Infinity – O.D.D. (oscillating dirty delay) | zpětná vazba / čas / pitch | lo-fi, samooscilující, nejvíc „Lyra“ |

Volitelně později (jako Solar): v režimu DUAL jiný algoritmus vlevo a vpravo.

## Ladění bez otravy (Marek 10. 10.: 8 × 5 oscilátorů je moc na ruční ladění)
- Inspirace: 8 hlasů × 1 oscilátor (Lyra), 4 hlasy × 5 oscilátorů (Solar). U nás 4× CHOIR = 20 oscilátorů.
- CHOIR oscilátory jsou **intervaly relativně k TUNE hlasu** + **CHORD** jedním klikem.
- **FILL** ve skupině rozprostře hlasy po stupnici/akordu.
- Výsledek: běžně 8 knobů TUNE, jemné ladění jen tam, kde chci.

## Hraní z MIDI
- **Režim Plates:** 8 bílých kláves jedné oktávy (C–C) = 8 hlasů. Hlasy si drží vlastní ruční ladění, klávesa je jen „prst na senzoru“. Oktávu půjde posunout.
- **Režim Keys:** zahraná nota přeladí volný hlas (nebo pár) na danou notu (chromaticky nebo ve stupnici, s mikrotonálním posunem). Slouží jako usnadnění.
- **Přichycení TUNE knobů:** Free (mikrotonálně) / Chromatic / Scale.
- **HOLD:** nástroj hraje i bez MIDI.
- **Dynamika (Touch source):**
  - Velocity: síla úhozu nastaví úroveň a náběh.
  - Aftertouch: tlak po stisku přidává hlasitost nad HOLD a otevírá barvu. Obdoba prstu na senzoru Lyry.
  - Obojí.
  - MPE / poly aftertouch: tlak zvlášť pro každou klávesu.

## Otevřené
- Charakter melodického hlasu (návrh: monofonní „Theremin/CS-80“ lead s glide a zpožděným vibratem, volitelný jemný arpeggiator se stupnicí) a jestli vůbec.
- Data a zvukové ukázky od Marka.
- Finální název a PLUGIN_CODE.

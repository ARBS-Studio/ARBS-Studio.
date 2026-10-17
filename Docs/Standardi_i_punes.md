# Standardi i punës ARBS Studio në Archicad

Ky dokument përshkruan mënyrën e operimit dhe të emërtimit që krijon add-on-i **ARBS Studio** në Archicad 28/29/30, sipas legjislacionit të Republikës së Kosovës. Të gjitha vlerat janë në skedarët `Data/*.json` dhe mund të ndryshohen pa prekur kodin.

## 1. Fazat e dokumentimit

| Kodi | Faza | Qëllimi (baza ligjore) | Shkalla kryesore |
|---|---|---|---|
| **PK** | Projekti Konceptual | Kërkesa për **kushte ndërtimore** (Kat. I dhe II: UA MMPH Nr. 06/2017; Kat. III: MMPHI) | 1:200 / 1:500 |
| **PI** | Projekti Ideor | **Leje ndërtimore për Kategorinë I** (Shtojca 2) – 3 kopje fizike + 1 digjitale | 1:100 |
| **PKR** | Projekti Kryesor | **Leje ndërtimore për Kategorinë II** (Shtojca 3) dhe realizimi i ndërtimit (edhe për Kat. I) | 1:100 / 1:50 |
| **PZ** | Projekti Zbatues | Realizimi në kantier, mbikëqyrja dhe ditari i ndërtimit (UA Nr. 15/2013); nuk është dokument për leje | 1:50 / 1:20 / 1:5 |

Kategorizimi i ndërtimeve bëhet sipas UA (MMPH) Nr. 04/2017. Inspektimet e fazave dhe certifikata e përdorimit rregullohen me UA (MMPH) Nr. 05/2017.

**Përmbajtja minimale e Projektit Ideor (Kategoria I):**
- situacioni i gjerë 1:500/1:1000 dhe situacioni i parcelës 1:200/1:500;
- bazat e bodrumit, të përdhesës dhe të kateve 1:100;
- prerjet dhe pamjet 1:100, perspektiva e jashtme;
- plani i kyçjes në ujësjellës/kanalizim dhe plani i kyçjes në rrjetin elektrik 1:100;
- përshkrimi teknik, paramasa dhe parallogaria;
- ortofoto, situacioni 50 m përreth dhe kuotat në KosovaREF01;
- masat e efiçiencës së energjisë dhe të mbrojtjes nga zjarri;
- licencat dhe deklarata e projektuesve.

## 2. Emërtimi

| Çka | Formati | Shembull |
|---|---|---|
| Fleta (Layout ID) | `<FAZA>-<DISIPLINA><NR>` | `PI-A05` |
| Emri i fletës | `<Emri> – <Kati> <Shkalla>` | `Baza – Përdhesa 1:100` |
| Skedari i eksportuar | `<NrProj>_<FAZA>_<DISIPLINA><NR>_<Emri>_R<rev>` | `2026-014_PI_A05_Baza-e-perdheses_R0.pdf` |
| Pamja në View Map | `<FAZA> / <Lloji> / <Emri> / 1:<shkalla>` | `PKR / Prerja / A-A / 1:50` |
| Kombinimi i shtresave | `<NR> <FAZA> – <Përshkrimi>` | `02 PI – Projekti Ideor` |
| Shtresa | `<GRUPI>-<NR> <Përshkrimi>` | `A-01 Muret konstruktive` |
| Kompozita | `<KODI> <Përshkrimi>` | `MJ-01 Mur i jashtëm – bllok qeramik 25 + EPS 10` |
| Materiali (Building Material) | `ARBS · <Emri>` | `ARBS · Polistiren i ekspanduar EPS-F (fasadë)` |

**Disiplinat:** A Arkitektura · K Konstruksioni · H Hidroinstalimet · E Elektroinstalimet · M Instalimet makinerike · Z Mbrojtja nga zjarri · F Fizika ndërtimore · G Gjeodezia · T Dokumentacioni tekstual.

**Kodet e dokumenteve tekstuale:** T01 Përshkrimi teknik · T05 Plani dinamik · F01 Fizika e ndërtesës · F02 Insolimi · A20 Skema e dyerve · A21 Skema e dritareve · A22 Skema e inventarit · A23 Tabela e sipërfaqeve.

## 3. Shtresat (48) dhe kombinimet (12)

**Grupet e shtresave:**
- **A** Arkitektura (A-01 … A-16)
- **K** Konstruksioni (K-01 … K-05)
- **S** Situacioni (S-01 … S-07)
- **I** Instalimet (I-01 … I-04)
- **D** Dokumentimi: kuota, tekste, etiketa, boshte (D-01 … D-10)
- **P** Prezantimi (P-01 … P-03)
- **X** Ndihmëse, nuk printohen (X-01 … X-03)

Lista e plotë është në `Data/layers.json`.

**Kombinimet:** 00 Modeli · 01 PK · 02 PI · 03 PKR bazat · 03.1 PKR prerjet/pamjet · 04 PZ · 05 Konstruksioni · 06 Situacioni · 07 Instalimet · 08 Insolimi · 09 Prezantim 3D · 10 Gjendja ekzistuese/prishjet.

## 4. View Map dhe Layout Book

- **View Map:** krijohet një dosje për çdo fazë (`PI - Projekti Ideor` …). Brenda saj janë nëndosjet *Bazat, Prerjet, Pamjet, Detajet, 3D dhe perspektiva, Skemat dhe listat, Situacioni, Insolimi*. Katet, prerjet dhe pamjet klonohen nga Project Map me kombinimin dhe shkallën e fazës (PK 1:200, PI 1:100, PKR 1:100/1:50, PZ 1:50).
- **Layout Book:** për çdo fazë krijohet një subset (prefiksi `PI-`), brenda tij një subset për disiplinë (prefiksi `A`, numërim `01`) dhe fletat nga `Data/phases.json`. Fletat «për çdo kat» krijohen për secilin kat të projektit. Masterat janë `ARBS A4/A3/A2/A1 horizontale`, me buzë 20 mm majtas për lidhje.

## 5. Kompozitat e materialeve

Shtresat renditen: mure **nga jashtë → brenda**; pllaka dhe kulme **nga lart → poshtë**. U llogaritet sipas SK EN ISO 6946.

U<sub>max</sub> sipas **Rregullores (MMPH) Nr. 04/18** (W/m²K):

| Elementi | U<sub>max</sub> |
|---|---|
| Mur i jashtëm | 0,35 |
| Mur ose dysheme në kontakt me tokën | 0,50 |
| Dysheme ndaj ajrit të jashtëm | 0,30 |
| Kulm i pjerrët ose i rrafshët | 0,30 |
| Pllakë ndaj hapësirës së pangrohur | 0,50 |
| Pllakë mes kateve të ngrohura | 0,90 |
| Mur mes banesave | 0,80 |
| Elementet e qelqit | 1,60 |

| Kodi | Kompozita | d (cm) | U (W/m²K) | U<sub>max</sub> |
|---|---|---|---|---|
| MJ-01 | Mur i jashtëm – bllok qeramik 25 + EPS 10 | 38,0 | 0,289 | ✔ ≤ 0,35 |
| MJ-02 | Mur i jashtëm – bllok qeramik 25 + lesh guri 10 | 38,0 | 0,283 | ✔ ≤ 0,35 |
| MJ-03 | Mur i jashtëm – beton qelizor 30 + EPS 5 | 37,5 | 0,236 | ✔ ≤ 0,35 |
| MJ-04 | Mur i jashtëm – beton i armuar 20 + lesh guri 12 | 34,5 | 0,284 | ✔ ≤ 0,35 |
| MJ-05 | Mur i jashtëm – fasadë e ventiluar (bllok 25 + lesh guri 12) | 44,1 | 0,241 | ✔ ≤ 0,35 |
| MT-01 | Mur bodrumi në kontakt me tokën – b/a 25 + XPS 8 | 36,1 | 0,377 | ✔ ≤ 0,50 |
| TH-03 | Mur themeli – b/a 40 + XPS 6 | 47,6 | 0,475 | ✔ ≤ 0,50 |
| MB-01 | Mur ndarës mes banesave – bllok 20 + lesh mineral 5 + GK | 29,0 | 0,434 | ✔ ≤ 0,80 |
| MN-01 | Mur ndarës i brendshëm – bllok 12 | 15,0 | 1,782 | pa kërkesë |
| MN-02 | Mur ndarës gipskartoni 12,5 cm (2+2 GK) | 12,5 | 0,384 | pa kërkesë |
| MN-03 | Mur ndaj shkallëve/hapësirës së pangrohur – bllok 20 + EPS 5 | 28,0 | 0,474 | rekomandim ≤ 0,50 |
| DY-01 | Dysheme mbi tokë – pllakë b/a 15 + EPS 5 + XPS 5 | 57,4 | 0,299 | ✔ ≤ 0,50 |
| DY-02 | Pllakë ndërkatëshe – b/a 18 me dysheme lundruese | 29,1 | 0,768 | ✔ ≤ 0,90 |
| DY-03 | Pllakë mbi bodrum/garazh të pangrohur – b/a 20 + lesh guri 8 | 38,1 | 0,289 | ✔ ≤ 0,50 |
| DY-04 | Pllakë mbi ajër të jashtëm (erker/portik) – b/a 20 + lesh guri 10 | 41,6 | 0,238 | ✔ ≤ 0,30 |
| DY-05 | Ballkon / terasë e hapur – b/a 18 | 24,8 | 2,144 | pa kërkesë |
| DY-06 | Tavan nën nënkulm të pangrohur – b/a 18 + EPS 12 | 36,0 | 0,265 | ✔ ≤ 0,30 |
| TH-01 | Pllakë themeli (radier) b/a 40 + XPS 10 | 60,8 | 0,297 | ✔ ≤ 0,50 |
| TH-02 | Pllakë themeli (radier) b/a 50 pa termoizolim | 80,8 | 1,769 | pa kërkesë |
| KR-01 | Kulm i rrafshët i pashkelshëm (klasik) – EPS 14 | 47,5 | 0,214 | ✔ ≤ 0,30 |
| KR-02 | Kulm i rrafshët i shkelshëm (terasë, kulm i përmbysur) – XPS 16 | 51,5 | 0,189 | ✔ ≤ 0,30 |
| KR-03 | Kulm i gjelbër ekstensiv – EPS 16 | 56,3 | 0,186 | ✔ ≤ 0,30 |
| KP-01 | Kulm i pjerrët i ngrohur (nënkulm i banuar) – lesh mineral 16+6 | 32,7 | 0,152 | ✔ ≤ 0,30 |
| KP-02 | Kulm i pjerrët i ftohtë – mbulesë mbi nënkulm të pangrohur | 10,5 | 1,669 | pa kërkesë |
| KP-03 | Kulm me panel sanduiç PIR 10 cm | 10,0 | 0,213 | ✔ ≤ 0,30 |

Të 47 materialet kanë vlera projektuese tipike për λ, ρ, c dhe μ (SK EN ISO 10456, fletët teknike të prodhuesve). Projektuesi i verifikon me produktin e zgjedhur. Te KP-01, U nuk përfshin efektin e mahive; për shtresa johomogjene bëhet llogaritja e detajuar sipas ISO 6946.

## 6. Dokumentet që gjenerohen

| Dokumenti | Formati | Burimi i të dhënave |
|---|---|---|
| Përshkrimi teknik (12 kapituj) | Word `.doc` | Projekti, profili i zyrës, zonat, kompozitat, hapjet |
| Fizika e ndërtesës (U + Glaser me diagram) | HTML (PDF) + Word | Kompozitat e përdorura në model dhe sipërfaqet e tyre |
| Skema e insolimit (diagrami polar, orët sipas orientimit, hijet) | HTML (PDF) | Komuna ose koordinatat, veriu i projektit, lartësia |
| Skemat e dyerve dhe të dritareve | HTML + CSV | Dyert/dritaret: tipi, B×H, pragu/parapeti, katet |
| Skema e inventarit, tabela e sipërfaqeve | HTML + CSV | Objektet, ndriçimi, zonat |
| Plani dinamik (Gantt) | HTML + CSV | Shablloni i punimeve, BTS, katet, kalendari i Kosovës |

Dokumentet ruhen në `Dokumentet/ARBS Studio/<Nr> - <Emri i projektit>/`. Skedarët HTML printohen në PDF nga shfletuesi. Skedarët `.doc` hapen dhe redaktohen në Word, kurse CSV në Excel.

## 7. Plani dinamik dhe kalendari

- Java e punës është e hënë – e premte; e shtuna mund të aktivizohet si ditë pune.
- Festat zyrtare sipas **Ligjit Nr. 03/L-064**: 1 dhe 2 janar, 7 janar, 17 shkurt, 9 prill, 1 maj, 9 maj, 25 dhjetor, e hëna e Pashkëve Katolike dhe Ortodokse, Fitër Bajrami dhe Kurban Bajrami.
- Kur festa bie të shtunën ose të dielën, dita e lirë zhvendoset në ditën e parë të punës (p.sh. 9 maj 2026 → e hënë 11 maj). Nëse edhe ajo ditë është festë, zhvendoset në ditën pasuese (p.sh. 1 maj 2027 → e martë 4 maj, sepse 3 maji është e hëna e Pashkëve Ortodokse). Këtë rast ligji nuk e sqaron, prandaj verifikojeni me kalendarin zyrtar të vitit.
- Datat e Bajrameve pas vitit 2026 janë **parashikime** dhe shënohen si të tilla në dokument. Konfirmojini me vendimin e Qeverisë dhe përditësoni `Data/holidays.json`.
- Punët e ndjeshme ndaj motit (betonime, fasada, hidroizolime, kulmi) mund të përjashtohen nga 15.12 deri më 28.02. Pushimet kolektive shtohen si periudha jo-pune.
- Llogaritja CPM përdor varësi FS/SS me vonesë dhe nxjerr rezervën dhe rrugën kritike.

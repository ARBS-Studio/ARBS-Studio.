# ARBS Studio – Add-On për Archicad 28, 29 dhe 30

Toolbar (paletë) brenda Archicad-it për punën e projektimit sipas legjislacionit të Republikës së Kosovës, me ndërfaqe në shqip.

| Moduli | Çka bën |
|---|---|
| **Projekti** | Profili i zyrës (ruhet për të gjitha projektet) dhe të dhënat e projektit (ruhen brenda `.pln`), që shkruhen edhe në *Project Info*. |
| **Template** | Krijon 48 shtresa, 12 kombinime, View Map dhe Layout Book për fazat **PK / PI / PKR / PZ**, me fleta dhe masterë A4–A1. |
| **Kompozitat** | Krijon 47 materiale (λ, ρ, c) dhe 25 kompozita për mure, dysheme, kulme të rrafshëta e të pjerrëta dhe themele, me kontrollin U sipas Rregullores MMPH 04/18. |
| **Përshkrimi teknik** | Gjeneron dokument Word me 12 kapituj, të plotësuar nga modeli: zonat, kompozitat dhe hapjet. |
| **Paramasa** | Paramasa dhe parallogaria nga sasitë e modelit (kompozitat ARBS sipas shtresave, kolonat, trarët, zonat, dyert, dritaret) × çmimet orientuese të tregut në Kosovë (`Data/prices.json`, të ndryshueshme për zyrën). Del si HTML, Word, paramasë për ofertim pa çmime dhe CSV, me rekapitulim, të paparashikuara dhe TVSH 18 %. |
| **Projekte me blloqe** | Kategoria, destinimi dhe nën-destinimi sipas ligjit. Për banim kolektiv, mikst dhe afarizëm: blloqet dhe hyrjet nga zonat «Blloku B1» / «Hyrja H1», njësitë nga zonat «Banesa» / «Lokal» ose të përcaktuara me dorë (tipe dhe vendosje sipas kateve). Specifikimi i njësive, paramasa, përshkrimi teknik dhe plani dinamik dalin për çdo bllok veç e veç dhe me rekapitulim total. |
| **Fizika** | Gjeneron elaboratin e fizikës ndërtimore: U, U<sub>max</sub>, Glaser me diagram dhe sipërfaqet nga modeli. |
| **Insolimi** | Vendos lokacionin në Archicad (38 komuna) dhe gjeneron diagramin e diellit, orët e insolimit sipas orientimit dhe hijet. |
| **Skemat** | Gjeneron skemat e dyerve, të dritareve, të inventarit dhe tabelën e sipërfaqeve (HTML + CSV për Excel). |
| **Plani dinamik** | Gantt me CPM; zbret fundjavat dhe festat zyrtare (Ligji 03/L-064, me zhvendosjen nga fundjava) dhe mund të ndalojë punët e ndjeshme ndaj motit në dimër. |

Standardi i plotë i punës dhe i emërtimit është në [Docs/Standardi_i_punes.md](Docs/Standardi_i_punes.md).

## Struktura

```
ARBS-Studio/
├─ Src/
│  ├─ AddOnMain.cpp            pika hyrëse: menyja «ARBS Studio» dhe paleta
│  ├─ AC/                      shtresa Archicad (API 28/29)
│  │  ├─ ArbsPalette.*         paleta me Browser (HTML/JS) ↔ C++
│  │  ├─ Commands.*            ruteri i komandave JSON nga paleta
│  │  ├─ TemplateSetup.*       shtresat, kombinimet, View Map, Layout Book
│  │  ├─ CompositeBuilder.*    Building Materials dhe Composites
│  │  ├─ ModelReader.*         zonat, dyert/dritaret, objektet, kompozitat e përdorura
│  │  ├─ ProjectStore.*        profili (preferencat), projekti (ModulData), Project Info
│  │  └─ ArbsUtils.*           UTF-8, skedarët, dosja e daljes, hapja e dokumenteve
│  ├─ Core/                    bërthama C++17 pa varësi (testohet pa Archicad)
│  │  ├─ KosovoCalendar.*      festat, Pashkët, Bajramet, zhvendosja nga fundjava
│  │  ├─ DynamicPlan.*         CPM (FS/SS, vonesa, rezerva, rruga kritike)
│  │  ├─ BuildingPhysics.*     U sipas ISO 6946, Glaser (60/90 ditë)
│  │  ├─ Solar.*               pozicioni i diellit (NOAA), CET/CEST, hijet
│  │  ├─ Reports.*             dokumentet në shqip (HTML, Word, CSV, SVG)
│  │  ├─ Catalog.* · Json.* · Date.* · Embedded.hpp
│  └─ UI/arbs_palette.html     ndërfaqja e paletës (pa internet, pa CDN)
├─ Data/                       të dhënat e Kosovës (JSON), ngulitur në add-on
├─ Resources/logo_arbs.png     logoja (edhe në dokumente)
├─ RINT/ RFIX/ RFIX.win/       burimet e Archicad-it (menyja, paleta, ikona, MDID)
├─ Installer/                  setup i vetëm: Windows (Inno Setup) dhe macOS (.pkg)
├─ Tests/                      testet e bërthamës dhe dokumentet shembull
├─ cmake/EmbedFiles.cmake      ngulit Data/UI/logo në binar
├─ Tools/                      submoduli GRAPHISOFT/archicad-addon-cmake-tools
└─ .github/workflows/build.yml ndërtimi automatik (AC 28/29/30 + instaluesit)
```

Add-on-i është **një skedar i vetëm** (`ARBS_Studio.apx` / `.bundle`). Të dhënat, ndërfaqja dhe logoja janë brenda tij. Nuk kërkohet asnjë program shtesë: dokumentet hapen me Word, Excel ose shfletuesin që ekzistojnë në kompjuter.

## Ndërtimi

### Mënyra më e lehtë: GitHub Actions
1. Ngarkoni repozitorin në GitHub (privat).
2. *Actions* ndërton automatikisht add-on-in për Archicad 28, 29 dhe 30, në Windows dhe macOS. DevKit-i shkarkohet automatikisht.
3. Rezultati: `ARBS_Studio_Setup_<ver>.exe` dhe `ARBS_Studio_Setup_<ver>.pkg`. Me tag `v0.1.0` publikohen edhe si *Release*.

### Lokalisht (Windows)
Kërkohen Visual Studio 2022 (toolset v142 për AC28, v143 për AC29+), CMake ≥ 3.19 dhe Python 3.10+.
```
git submodule update --init
python Tools/BuildAddOn.py --configFile config.json --acVersion 28 29 30 --buildConfig Release --package
iscc /DAppVersion=0.1.0 /DBinDir=..\..\dist Installer\Windows\ARBS_Studio.iss
```
(Para `iscc`, kopjoni `Build/Package/ARBS_Studio/<v>/INT/Release/ARBS_Studio.apx` në `dist/AC<v>/`.)

### Testet e bërthamës (pa Archicad)
```
cmake -S Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
./build-tests/arbs_samples Data dalja/     # gjeneron dokumentet shembull
```

## MDID – identifikuesi i Graphisoft (i nevojshëm për përdorim real)
Archicad i ngarkon add-on-et me MDID `1/1` **vetëm në modalitetin DEMO**. Për përdorim në zyrë:
1. Regjistrohuni falas në <https://archicadapi.graphisoft.com> → *Profile › Add-Ons* dhe merrni **Developer ID** dhe **Local ID**.
2. Vendosini te `Src/ResourceIds.hpp` (`ARBS_MDID_DEVELOPER`, `ARBS_MDID_LOCAL`) ose si sekrete në GitHub (`ARBS_MDID_DEVELOPER`, `ARBS_MDID_LOCAL`).

## Instalimi dhe përdorimi
1. Mbyllni Archicad-in dhe ekzekutoni `ARBS_Studio_Setup_<ver>.exe`. Instaluesi gjen Archicad 28/29/30 dhe e vendos add-on-in te `Add-Ons\ARBS Studio\`.
2. Hapni Archicad-in dhe zgjidhni menynë **ARBS Studio › Paleta ARBS Studio**.
3. Rrjedha e rekomanduar në një projekt të ri:
   1. **Projekti** – plotësoni profilin e zyrës dhe të dhënat e projektit.
   2. **Template** – krijoni shtresat, kombinimet, View Map dhe Layout Book.
   3. **Kompozitat** – krijoni materialet dhe kompozitat.
   4. Modeloni.
   5. Gjeneroni përshkrimin teknik, fizikën, insolimin, skemat dhe planin dinamik.
4. Për ta bërë template zyre: *Skedari › Ruaj si… › Archicad Project Template (.tpl)*.

## Statusi i verifikimit (v0.1.0)
- Bërthama: 236 kontrolle automatike kalojnë. Ato mbulojnë festat 2023–2027, Pashkët, ditët e punës, CPM, U-vlerat e të 25 kompozitave, Glaser dhe lindjen/perëndimin e diellit për Prishtinën.
- Shtresa Archicad: kontrollohet sintaksisht kundrejt header-ave zyrtarë të API DevKit **28.4001** dhe **29.3100**. Kompilimi i plotë dhe testi brenda Archicad bëhen me GitHub Actions ose me Visual Studio/Xcode.

## Hapat e ardhshëm (sipas videos referencë)
- Paramasa dhe parallogaria nga modeli (Excel) me çmimet e tregut.
- Formularët për kushte/leje ndërtimore dhe leje rrënimi, si dhe vendimet/deklaratat për inxhinierët.
- Favorites për mure, pllaka, dritare dhe zona, të lidhura me kompozitat.
- Graphic Overrides për 4 mënyrat e prezantimit (konceptual / ideor / zbatues / konstruksion).
- Etiketat dhe vulat e fletave (title block) me logon dhe të dhënat e zyrës.

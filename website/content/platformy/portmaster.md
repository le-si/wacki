---
title: "Anbernic i PortMaster"
weight: 60
params:
  icon: "🎮"
  familyKey: "handheld"
  asset: "wacki-portmaster.zip"
  sub: "Anbernic, Powkiddy, TrimUI, RGB30 i dziesiątki innych"
  tagline: "Jedna paczka PortMaster na cały ekosystem handheldów."
  color: "#b8330f"
  match: ["portmaster"]
  devices:
    - "Anbernic RG35XX (Plus / H / SP / 2024), RG34XX, RG40XX, RG28XX, RG CubeXX"
    - "Anbernic RG353x, RG503, RG552"
    - "Anbernic seria RG351 oraz oryginalny RG35XX (przez Koriki/Batocera)"
    - "Powkiddy, TrimUI, RGB30, Miyoo Flip i wiele innych"
  requirements:
    - "firmware z PortMasterem: muOS, ROCKNIX, Knulli, ArkOS, JELOS lub Batocera"
    - "archiwum wacki-portmaster.zip"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Ruch kursora", input: "gałka / krzyżak" }
    - { action: "Kliknięcie lewe", input: "A (dolny)" }
    - { action: "Kliknięcie prawe", input: "B (prawy)" }
    - { action: "Menu pauzy", input: "START" }
    - { action: "Quick-save", input: "R1" }
    - { action: "Quick-load", input: "L1" }
    - { action: "Wyjście z gry", input: "START + SELECT" }
  logPath: "`ports/Wacki/log.txt`"
---

## Instalacja

Potrzebujesz handhelda z **PortMasterem** (custom firmware: muOS, ROCKNIX,
Knulli, ArkOS, JELOS lub Batocera — stock firmware Anbernica zwykle go nie ma).

1. Wgraj `wacki-portmaster.zip` przez PortMaster („Install from zip"), albo
   wrzuć archiwum do folderu `autoinstall` PortMastera.
2. Skopiuj pliki `Dane_*.dta` z oryginalnej płyty do katalogu portu:
   ```
   /roms/ports/Wacki/data/
   ```
3. Uruchom z menu **Ports → Wacki**.

## Uwaga o przyciskach

Przyciski w pozycjach SDL: **A** = dolny, **B** = prawy. Na padach w układzie
Nintendo (większość Anbernica) są one fizycznie oznaczone jako **B** i **A** —
kieruj się pozycją, nie napisem. Gałka steruje kursorem proporcjonalnie
(delikatny wychył = precyzyjne celowanie), krzyżak — z przyspieszeniem.

<!--tech-->

## Zgodny sprzęt i architektury

Paczka działa na większości nowoczesnych handheldów Anbernic, a przy okazji na
całym ekosystemie PortMastera (Powkiddy, TrimUI, RGB30, Miyoo Flip i dziesiątki
innych urządzeń). W jednej paczce są dwie binarki:

- **aarch64** — Allwinner H700 (RG35XX Plus / H / SP / 2024, RG34XX, RG40XX,
  RG28XX, RG CubeXX), Rockchip RK3566 (RG353x, RG503) i RK3399 (RG552)
- **armhf** — seria Anbernic RG351 (RK3326) oraz oryginalny RG35XX (Actions,
  Cortex-A9) przez Koriki/Batocera

## Coś nie działa?

Wrapper `Wacki.sh` zapisuje log do `ports/Wacki/log.txt` — dołącz go do
zgłoszenia błędu wraz z wersją portu, modelem urządzenia i firmware'em.

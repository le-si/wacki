---
title: "PlayStation 2"
weight: 70
params:
  icon: "📀"
  familyKey: "console"
  asset: "wacki-ps2.zip"
  sub: "FreeMcBoot / uLaunchELF · PCSX2"
  tagline: "Tak, Wacki śmigają i na PS2 — z pada albo myszy USB."
  color: "#1d3b8b"
  match: ["ps2", "playstation"]
  devices:
    - "PlayStation 2 z homebrew (FreeMcBoot / FreeDVDBoot + uLaunchELF)"
    - "Emulator PCSX2"
  requirements:
    - "PS2 z możliwością uruchamiania homebrew, albo emulator PCSX2"
    - "pendrive FAT32 (na sprzęcie) lub HostFS (w PCSX2)"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Ruch kursora", input: "lewa gałka / mysz USB" }
    - { action: "Kliknięcie lewe", input: "✕" }
    - { action: "Kliknięcie prawe", input: "○ (kółko)" }
    - { action: "Menu pauzy", input: "START" }
  logPath: "konsola IOP / PCSX2 (HostFS)"
---

## Instalacja (na sprzęcie)

Grasz na PS2 z homebrew (FreeMcBoot + uLaunchELF) albo w emulatorze PCSX2.
Sterujesz **DualShockiem** (lewa gałka = kursor) lub **myszą USB**.

1. Rozpakuj `wacki-ps2.zip` — w środku jest folder `wacki/` z plikiem
   `wacki-ps2.elf` i podkatalogiem `data/`.
2. Skopiuj do `wacki/data/` pliki `Dane_*.dta` z oryginalnej płyty.
3. Wrzuć cały folder `wacki/` na pendrive (FAT32), tak by powstało
   `mass:/wacki/wacki-ps2.elf` oraz `mass:/wacki/data/Dane_*.dta`.
4. Uruchom `mass:/wacki/wacki-ps2.elf` przez **uLaunchELF**.

<!--tech-->

## W emulatorze PCSX2

**System → Boot ELF** i wskaż `wacki-ps2.elf` (dane przez HostFS), albo zbuduj
bootowalny obraz ISO ze źródeł (`./tools/build-ps2-iso.sh`) i odpal go przez
**Boot ISO**.

## Co siedzi pod maską

Wacki na PS2 renderuje **sprzętowo przez gsKit** (8-bitowa paleta liczona przez
GS), gra dźwięk przez **audsrv** (SFX, dialogi, muzyka, cutscenki) i zapisuje na
**kartę pamięci** (z ikoną i nazwą). Paczka `wacki-ps2.zip` zawiera bootowalny
plik ELF.

## Coś nie działa?

Na sprzęcie diagnostyka idzie przez konsolę IOP; w PCSX2 logi widać przy
uruchomieniu z HostFS. Dołącz wersję portu i sposób uruchomienia (sprzęt vs
PCSX2, USB vs ISO) do zgłoszenia błędu.

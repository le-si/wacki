---
title: "macOS"
weight: 20
params:
  icon: "🍎"
  familyKey: "pc"
  asset: "wacki-macos-arm64.tar.gz"
  sub: "Apple Silicon (M1 i nowsze)"
  tagline: "Wrzuć dane obok Wacki.app i odblokuj raz w Ustawieniach."
  color: "#e0431a"
  match: ["macos", "darwin", "osx"]
  devices:
    - "Apple Silicon — M1, M2, M3, M4…"
  requirements:
    - "Mac z procesorem Apple Silicon (M1+)"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Ruch kursora", input: "mysz / gładzik" }
    - { action: "Kliknięcie (idź / akcja)", input: "LPM" }
    - { action: "Przełącz postać", input: "Spacja / PPM" }
    - { action: "Quick-save / quick-load", input: "F5 / F9" }
    - { action: "Menu pauzy", input: "F12" }
    - { action: "Pełny ekran", input: "F11" }
    - { action: "Wyjście z gry", input: "Esc" }
  logPath: "stderr w terminalu lub `Console.app`"
---

## Instalacja

1. Pobierz `wacki-macos-arm64.tar.gz` i rozpakuj (dwuklik w Finderze).
2. Wrzuć folder `data/` z plikami `Dane_*.dta` **obok** `Wacki.app`.
   Alternatywnie: prawy klik na `Wacki.app` → **Pokaż zawartość pakietu** →
   `Contents/Resources/data/`. Wielkość liter w nazwach nie ma znaczenia.
3. Uruchom `Wacki.app` dwuklikiem — przy pierwszym razie zobacz sekcję niżej.

## Pierwsze uruchomienie na macOS

Binarki nie są podpisane certyfikatem Apple (port jest darmowy, a Apple
Developer Program kosztuje 99 $/rok), więc Gatekeeper przy pierwszym
uruchomieniu zablokuje `Wacki.app` komunikatem typu *„Apple nie może
sprawdzić, czy nie zawiera złośliwego oprogramowania"*. To **jednorazowe** —
wybierz jeden ze sposobów:

- **Przez Ustawienia systemowe** *(zalecane — bez Terminala)*:
  1. Spróbuj otworzyć `Wacki.app` dwuklikiem; pojawi się blokada — kliknij
     **Gotowe**.
  2. Otwórz **Ustawienia systemowe → Prywatność i bezpieczeństwo**, przewiń na
     sam dół do sekcji *Bezpieczeństwo*.
  3. Będzie tam wpis *„Aplikacja Wacki.app została zablokowana…"* z przyciskiem
     **Otwórz mimo to** — kliknij i potwierdź.
  4. Od teraz `Wacki.app` odpala się normalnie dwuklikiem.
- **Z Terminala** *(jedna komenda, każda wersja macOS)* — w katalogu z grą:
  ```bash
  xattr -dr com.apple.quarantine Wacki.app
  ```
- **Prawy klik** *(tylko macOS ≤ 14 Sonoma)*: prawy klik / Ctrl-klik na
  `Wacki.app` → **Otwórz** → w dialogu jeszcze raz **Otwórz**. Na macOS 15
  Sequoia Apple usunęło tę ścieżkę — użyj Ustawień powyżej.

<!--tech-->

## Coś nie działa?

Logi lecą na stderr — uruchom z terminala
`Wacki.app/Contents/MacOS/Wacki`, żeby je zobaczyć (albo zajrzyj do
`Console.app`). Dołącz je do zgłoszenia błędu wraz z wersją portu.

---
title: "Windows"
weight: 10
params:
  icon: "🪟"
  familyKey: "pc"
  asset: "wacki-windows-x86_64.zip"
  sub: "Windows 10 / 11 · 64-bit"
  tagline: "Rozpakuj, dwuklik wacki.exe i grasz."
  color: "#2b6fb0"
  match: ["windows", "win64", "win32"]
  devices:
    - "Windows 10 (64-bit)"
    - "Windows 11 (64-bit)"
  requirements:
    - "Windows 10 lub 11, 64-bit"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Ruch kursora", input: "mysz" }
    - { action: "Kliknięcie (idź / akcja)", input: "LPM" }
    - { action: "Przełącz postać", input: "Spacja / PPM" }
    - { action: "Quick-save / quick-load", input: "F5 / F9" }
    - { action: "Menu pauzy", input: "F12" }
    - { action: "Pełny ekran", input: "F11" }
    - { action: "Wyjście z gry", input: "Esc" }
  logPath: "`wacki.log` — automatycznie obok `wacki.exe`"
---

## Instalacja

1. Pobierz `wacki-windows-x86_64.zip` i rozpakuj w dowolnym folderze.
2. Skopiuj pliki `Dane_*.dta` z oryginalnej płyty do podkatalogu `data/`
   obok `wacki.exe`. Wielkość liter w nazwach nie ma znaczenia.
3. Uruchom grę dwuklikiem na `wacki.exe`.

Przy **pierwszym uruchomieniu** gra zapyta, jak chcesz grać —
*Pełny ekran*, *Okno 2×* albo *Okno 1×*. Wybór jest zapamiętany
(plik `wacki.cfg` obok gry), więc pytanie pojawia się tylko raz. W trakcie gry
**F11** przełącza pełny ekran, a rozciągnięcie okna za róg płynnie zmienia
powiększenie (gra renderuje wewnętrznie 640×480 i skaluje z zachowaniem
proporcji).

<!--tech-->

## Gdzie gra szuka plików

Gra przeszukuje po kolei (pierwszy trafiony wygrywa): zmienna `WACKI_PATH` →
katalog uruchomienia → `./data/` → katalog binarki → napędy CD i podłączone
dyski. Jeśli niczego nie znajdzie, pokaże okno wyboru folderu.

## Coś nie działa?

`wacki.exe` jest aplikacją GUI, więc logi lecą do pliku **`wacki.log`**
tworzonego automatycznie obok binarki. Dołącz go (oraz wersję portu z pierwszej
linii logu) do zgłoszenia błędu.

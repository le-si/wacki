---
title: "Linux"
weight: 30
params:
  icon: "🐧"
  familyKey: "pc"
  asset: "wacki-linux-x86_64.tar.gz"
  sub: "x86_64 · statycznie linkowane SDL2"
  tagline: "Rozpakuj i odpal ./wacki — zero zależności."
  color: "#ff7b08"
  match: ["linux"]
  devices:
    - "Dowolna dystrybucja x86_64 (glibc)"
  requirements:
    - "Linux x86_64"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Ruch kursora", input: "mysz" }
    - { action: "Kliknięcie (idź / akcja)", input: "LPM" }
    - { action: "Przełącz postać", input: "Spacja / PPM" }
    - { action: "Quick-save / quick-load", input: "F5 / F9" }
    - { action: "Menu pauzy", input: "F12" }
    - { action: "Pełny ekran", input: "F11" }
    - { action: "Wyjście z gry", input: "Esc" }
  logPath: "stderr w terminalu"
---

## Instalacja

1. Pobierz `wacki-linux-x86_64.tar.gz` i rozpakuj w dowolnym katalogu.
2. Skopiuj pliki `Dane_*.dta` z oryginalnej płyty do podkatalogu `data/`
   obok binarki. Wielkość liter w nazwach nie ma znaczenia.
3. Uruchom grę:
   ```bash
   ./wacki
   ```

SDL2 jest **statycznie wlinkowane**, więc nie trzeba doinstalowywać
`libSDL2` — binarka działa od ręki. Przy pierwszym uruchomieniu gra zapyta o
tryb wyświetlania (pełny ekran / okno); wybór trafia do `wacki.cfg` obok gry.

<!--tech-->

## Opcje uruchomienia (dla zaawansowanych)

| Flaga | Działanie |
|---|---|
| `--scale N` | okno powiększone N-krotnie (gra wewnętrznie 640×480) |
| `--scaler nearest\|linear\|best` | jakość skalowania |
| `--fullscreen` / `-f` | start w pełnym ekranie |
| `--start-stage N` | skok od razu do etapu N (1–5), pomija menu i intro |
| `WACKI_PATH=...` | ścieżka do katalogu z `Dane_*.dta` |

Przykład — okno 1280×960 ze skalowaniem liniowym:

```bash
./wacki --scale 2 --scaler linear
```

## Coś nie działa?

Logi lecą na stderr w terminalu, z którego uruchomiono grę. Dołącz je do
zgłoszenia błędu wraz z wersją portu (pierwsza linia logu).

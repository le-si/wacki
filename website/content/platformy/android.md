---
title: "Android"
weight: 40
params:
  icon: "🤖"
  familyKey: "mobile"
  asset: "wacki-android.apk"
  sub: "Telefony i tablety · Android 7+"
  tagline: "Zainstaluj APK, wskaż folder z danymi i graj dotykiem."
  color: "#3ddc84"
  match: ["android", ".apk"]
  devices:
    - "Telefony i tablety z Androidem 7.0+"
    - "Emulatory (BlueStacks i pokrewne)"
  requirements:
    - "Android 7.0 lub nowszy"
    - "zgoda na instalację spoza sklepu (sideload)"
    - "pliki Dane_*.dta z oryginalnej płyty"
  controls:
    - { action: "Kursor / kliknięcie (idź / akcja)", input: "dotknięcie ekranu" }
    - { action: "Przełącz postać", input: "dwa palce" }
    - { action: "Lewy / prawy klik (panel)", input: "przyciski na ekranie" }
    - { action: "Menu pauzy", input: "Wstecz" }
  logPath: "`adb logcat -s wacki`"
---

## Instalacja

1. Pobierz `wacki-android.apk` na telefon.
2. Zainstaluj go (sideload). Przy pierwszej instalacji APK Android poprosi o
   zgodę na **instalację z tego źródła** — zezwól w ustawieniach systemu.
3. Przy pierwszym uruchomieniu gra poprosi o **wskazanie folderu** z plikami
   `Dane_*.dta`. Wybierz katalog, w którym je trzymasz.

Gra czyta dane **wprost z tego miejsca** — nic nie kopiuje ani nie zajmuje
drugi raz pamięci. Zgoda na dostęp do folderu jest zapamiętana, więc wskazujesz
go tylko raz.

## Sterowanie dotykiem

Dotykasz ekranu, by przesunąć kursor i kliknąć (idź / wykonaj akcję). Obraz
jest **zawsze w poziomie**. Do przełączania postaci służy dotknięcie dwoma
palcami; w pasach letterboxu jest też półprzezroczysty panel z przyciskami
lewego i prawego kliknięcia.

<!--tech-->

## Coś nie działa?

Logi trafiają do systemowego logcata — podejrzyj je przez
`adb logcat -s wacki`. Jeśli gra „nie widzi" nowej wersji po reinstalacji,
odinstaluj starą i zainstaluj ponownie. Dołącz wersję portu i model telefonu
do zgłoszenia błędu.

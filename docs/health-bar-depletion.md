# Health bar (pasek życia) — depletion mechanism

Stan: **niedokończony research**. Bar renderuje się poprawnie na frame 0
(pełne zielone), ale mechanism ubywania życia nie został zidentyfikowany.
Ten plik zbiera wszystko co wiemy + leady na dalsze RE.

## Co działa

* Asset `pasek#N.wyc` (N = stage index, 1..4) ładowany przez enter_script
  stage'a jako entity (verb_id=101, kind=2, flags=0x0a40).
* Atlas: 24 frames, każdy 172×3 pikseli, drawX=7, drawY=403.
* Renderowany w `PaintHudOverlay` (`src/game.c`) — port walk'uje render
  list, znajduje entity z `atlas->name` zaczynającą się od `pasek`, bierze
  `entity[+0x30]` jako frame index, blituje na `(atlas->off_drawX[0],
  atlas->off_drawY[0])` = (7, 403).
* Re-paint po `panel.wyc` żeby panel HUD nie zasłaniał pasek (entity
  render maluje go wcześniej w pipeline).

## Gradient kolorów (potwierdzony z pixel-scan)

Z parsu palety:

| Frame | Dominant color | Stan       |
|-------|---------------|------------|
| 0     | #32 (zielony) | pełne życie |
| ~6    | #39 (jasnozielony) | ubytek 25% |
| ~12   | #44 (żółty)   | ubytek 50% |
| ~18   | #47 (czerwony) | ubytek 75% |
| 23    | #125 (BG)     | brak życia |

Kolor zmienia się płynnie przez pixele każdego frame'a — 24 stany od
pełnego (zielony) do pustego (background-only). Mechanism gradient: każdy
frame to coraz mniejsza ilość kolorowych pikseli, reszta = BG.

## Skrypt entity pasek#1 (`@ 0x00423528`)

Bytecode (parsed przez format entity VM = `op(1B) + dlt(1B) + arg(2B)`,
stride = dlt × 2 bajty):

```
pc=0:  05 02 00 00   op 0x05 SET_POS_FROM_FRAME  (anchor = atlas drawX/Y)
pc=2:  06 02 00 00   op 0x06 SET_FRAME 0         (full health)
pc=4:  32 02 00 00   op 0x32                     ← UNKNOWN (poza 0..0x24)
pc=6:  33 00 00 00   op 0x33, dlt=0              ← HANG (pc nie postępuje)

# Po offsecie 16 więcej bloków:
pc=8+:  1b 02 00 00   op 0x1b SET_FLAG_2 (FLAG_2 sticky bit)
        06 02 00 00   SET_FRAME 0
        0a 02 01 00   op 0x0a (?)
        1d 02 00 00   op 0x1d STOP (bVar3 = false)
        12 02 01 00   op 0x12 ADV_FRAME
        13 02 01 00   op 0x13 (?)
        33 00 00 00   END

# Bloki 4-6 robią: SET_DELAY + CALL 0x00423588 (idle subroutine)
```

**Obserwacja:** główny entry-point (pc=0) ustawia frame 0 i halts na
op 0x33 z dlt=0 (pc nie postępuje). To znaczy entity NIGDY samodzielnie
nie zmienia frame — pozostałe bloki bytecodu są reachable tylko jeśli
ktoś z zewnątrz przełączy `entity[+0x32]` (saved pc) na inny offset.

## Entity VM — zakres opcode'ów

Per-entity VM dispatchowany switch'em w `src/actor/vm.c`: opcode'y
0x00..0x24 (33 znane). Brak case'a 0x32 czy 0x33 → unknown → falls
through bez specjalnego handlingu, tylko `next_pc = pc + dlt`. Z
dlt=0 → infinite loop.

Lista znanych ops istotnych dla pasek:
* `0x05 SET_POS_FROM_FRAME` — anchor = atlas drawX/Y
* `0x06 SET_FRAME` — `entity[+0x30] = arg`, clamp do frame_count-1
* `0x1E SUBSCRIPT_CALL` — zmienia bytecode ptr, pc=0
* `0x1F/0x21 STOP_RESET/END` — stop + next_pc = 0
* `0x20 STOP_KEEP_PC` — stop, keep current pc

## Hipotezy depletion

### H1: Bezpośredni zapis przez global RunScriptInterpreter

Konkretny op w głównym VM może targetować entity by verb_id i
zapisywać do `entity[+0x30]` (frame). Miejsca w `src/vm/main.c`
które piszą `eb + 0x30`:

* wait-for-frame loops (read only)
* reset frame to 0
* read frame for query

Brak op'a który ustawia frame na arbitralny value od strony głównego VM.
Może w ogóle ich nie ma → depletion przez inny mechanizm.

### H2: Specific stage skrypty re-binduja entity bytecode_slot

Stage 2/3/4 enter_scripts mogą wywoływać op 0x0E (ATTACH_SCRIPT) na
entity id=101 z bytecode'em który zmienia frame na podstawie warunku
(timer, var). Stage 1 (maluch/klatka2/kiosk21/plac) najwyraźniej **nie
ma takiego pathu** — match z naszą obserwacją że pasek stoi na frame 0.

Sprawdzenie: dump `Wacky.scr [komnata]X` sekcje dla stage 2+ i grep
op-bytes które celują w id=101.

### H3: Event-driven trigger w specific verb-scripts

Akcje typu "atakowanie", "utopienie", "atak NPC" mogłyby mieć
hardcoded efekt zmniejszenia frame. Sprawdzenie: znaleźć takie
verb-scripts (np. śmierć), dump bytecode'u, szukać pattern który
narusza zwykle nieużywany var lub bezpośrednio entity field.

### H4: Op 0x32/0x33 są zdefiniowane gdzieś poza per-entity dispatcher'em

Może w innej funkcji obsługującej te entity'ow. Worth checking:

* entity reset path (called by op 0x1E SUBSCRIPT_CALL)
* op 0x23 handler
* DestroyEnt — może resetuje frame?

### H5: Pasek depletion NIE ISTNIEJE w oryginale

Może asset miał zaplanowane 24 stany ale game scripts nigdy go nie
animują. Bar zawsze jest na frame 0 = full green = visual filler.
Trudno potwierdzić bez gameplay'u oryginału (user widział na YT że
zmienia kolor — więc raczej istnieje).

## Sugerowany dalszy research

1. **Sprawdzić `Wacky.scr` stage 2-4 sekcje** — czy enter_script tam
   re-binduje pasek's entity script albo wywołuje op (0x0E ATTACH ?)
   z id=101.

2. **xref entity_id 101 (asset id pasek#N.wyc)** w bytecode — search byte
   pattern dla immediate `65 00` (= 101 LE) w skryptach z `Wacky.scr` +
   `Item.scr`.

3. **Sprawdzić op 0x32 + 0x33 special handling** — może są case'y w
   per-entity dispatcherze zwinięte do default'a lub są w innej
   części binarki (lookup table) której nie znalazłem.

4. **Sprawdzić game_over_code** — kto je ustawia, czy przed tym ktoś
   modyfikuje pasek frame.

5. **Sprawdzić jakie var'y rosną z czasem** w `g_script_vars[0x129]`.
   Mogą być time-based counters które pasek script czyta przez op 0x07
   IF_FRAME albo IF_VAR_GT (z global VM).

## Punkt zaczepienia w porcie (jeśli wprowadzamy ręcznie)

Plik: `src/game.c`, funkcja `PaintHudOverlay`, sekcja "Health bar
overlay" ok. linii 415. Aktualnie czyta `entity[+0x30]` po prostu.

Żeby przyciąć life manually (testowo / PORT EXTENSION):

```c
extern Entity *FindEntityByVerbId(uint16_t verb_id);
Entity *pasek = FindEntityByVerbId(101);   // verb_id=101 dla pasek#N
if (pasek) {
    *(uint16_t *)((uint8_t *)pasek + 0x30) = new_health;  // 0..23
}
```

Można bindnąć do F5/F6 keys żeby visual-debugować gradient.

## Status portu (stage 1)

✅ Asset ładowany, entity spawnowany, frame 0 widoczny przez cały stage 1.
✅ Renderowany na (7, 403) tuż nad inventory na panelu HUD.
✅ Atlas gradient potwierdzony przez pixel-scan.
🟡 Depletion: niedokończony research, nieportowany.

Jeśli stage 2+ kiedy ożywiamy port wymaga depletion, to jest miejsce
gdzie wracamy do tego pliku.

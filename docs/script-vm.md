# Wacki .scr bytecode VM — full opcode table

Dispatcher: `RunScriptInterpreter` w `src/vm/main.c`. Każda instrukcja
to `[op:u8][len:u8][operands: (len-1)*2 LE bytes]`.

Zmienne globalne:
- `g_script_vars[0x129]` — tablica int32
- `g_return_reg` (= `g_script_vars[4]`) — pojedynczy ushort dla wyniku
  RAND/MOUSE/STATE queries

Symbole używane w tabeli:
* **var[i]** — `g_script_vars[i]`
* **reg** — `g_return_reg`
* **a0,a1,a2** — pierwsze trzy uint16 operandy pod `pc+2,+4,+6`
* **this** — pierwszy argument do RunScriptInterpreter (aktualny actor/item id)
* **that** — drugi argument (drugi actor / picked-up item)
* **anim[i]** — i-ta animacja entity

Status legenda:
- ✅ — port 1:1 z oryginałem
- 🟡 — port partial / observable różnica vs oryginał, opisana w `src/vm/main.c` lub `src/stubs.c`
- ❌ — opcode dispatched ale efekt jest no-op'em
- ▫ — strukturalny marker bez własnej semantyki (consumed by other op's scan)

| Op   | Mnem              | Operands         | Effect                                                                 | Port |
|------|-------------------|------------------|------------------------------------------------------------------------|------|
| 0x00 | END               | —                | end the current block; skip until matching `]`                         | ✅   |
| 0x01 | IF_EQ             | a0:var, a1:cmp32 | branch if `var[this] == cmp`                                           | ✅   |
| 0x02 | IF_GE             | "                | branch if `var[this] >= cmp` (signed)                                  | ✅   |
| 0x03 | IF_LE             | "                | branch if `var[this] <= cmp` (signed)                                  | ✅   |
| 0x04 | IF_NE             | "                | branch if `var[this] != cmp`                                           | ✅   |
| 0x05 | IF_AND            | "                | branch if `(var[this] & cmp) == cmp`                                   | ✅   |
| 0x06 | ENDIF             | —                | close conditional                                                      | ▫    |
| 0x07 | ELSE              | —                | enter else branch                                                      | ✅   |
| 0x09 | PRINT             | text             | layout multi-line "\|"-separated text via Futura.30                    | ✅   |
| 0x0A | VAR_OR            | a0:i, a1:m32     | `var[i] \|= m`                                                         | ✅   |
| 0x0B | TIMER_SET         | a0:ent, a1:t     | `entity[a0]->delay_ticks = a1`                                         | ✅   |
| 0x0C | VAR_ANDNOT        | a0:i, a1:m32     | `var[i] &= ~m`                                                         | ✅   |
| 0x0D | VAR_SET           | a0:i, a1:v32     | `var[i] = v`                                                           | ✅   |
| 0x0E | NAME_LOOKUP       | a0:hash, a1:val  | hash → idx w bieżącej animation table aktora                            | ✅   |
| 0x0F | ATTACH_PROP_DIR   | a0:ent, a1:dir   | `entity[a0]->attached_prop = dir`                                      | ✅   |
| 0x10 | ANIM_ACTOR1       | a0:anim, a1:fr   | start animation a0 on Ebek, target frame a1                            | ✅   |
| 0x11 | ANIM_ACTOR2       | "                | same on Fjej                                                           | ✅   |
| 0x12 | ANIM_BOTH         | a0:anim, a1:fr   | run on both actors and wait for both                                   | ✅   |
| 0x13 | WAIT_FRAME        | —                | one ProcessGameFrameTick()                                             | ✅   |
| 0x14 | WAIT_MS           | a0:ms            | N ProcessGameFrameTick()s, ~ a0 ms                                     | ✅   |
| 0x15 | WAIT_ENT_IDLE     | a0:ent           | tick until entity has no walk delta                                    | ✅   |
| 0x16 | LABEL             | a0:id            | placeholder (target of 0x17 / 0x18)                                    | ▫    |
| 0x17 | GOTO              | a0:label_id      | jump to matching `LABEL`                                               | ✅   |
| 0x18 | LOOP              | a0:counter_idx, a1:max, a2:label | inc counter[a0]; if < max → GOTO label               | ✅   |
| 0x19 | DIALOG_OPT        | a0:id, a1:text*, a2:script* | enqueue dialogue choice (max 6)                           | ✅   |
| 0x1A | ASK1              | …                | dialogue tree variant 1                                                | ✅   |
| 0x1B | ASK2              | …                | dialogue tree variant 2                                                | ✅   |
| 0x1C | ASK3              | …                | dialogue tree variant 3                                                | ✅   |
| 0x1D | ASK4              | …                | dialogue tree variant 4                                                | ✅   |
| 0x1F | RESET_OBJ         | a0:obj           | reset clickable region a0                                              | ✅   |
| 0x20 | DOOR_EXIT         | a0:dir           | sync scene-change via LoadKomnataScene (T22B in-place, T35 verb-driven) | ✅   |
| 0x21 | IS_ITEM           | —                | `reg = ((this-0x29) < 0x8E) ? 2 : 1`                                   | ✅   |
| 0x22 | GET_HELD_ITEM     | —                | `reg = that`                                                           | ✅   |
| 0x23 | GET_STATE         | a0:obj           | `reg = object pickup status query (a0)`                                | ✅   |
| 0x24 | TAILCALL          | a0:script_ptr    | jump to subroutine, pc = base                                          | ✅   |
| 0x25 | CALL_SUB          | a0:script_ptr    | push pc → switch to subroutine                                         | ✅   |
| 0x26 | WAIT_ANIM_FRAME   | a0:ent, a1:frame | tick until `entity[a0]->frame_index == a1`                             | ✅   |
| 0x27 | SET_OBJ_PROP      | a0:tab, a1:idx, a2:x, a3:y | write (x,y) at hash a1 inside table a0                       | ✅   |
| 0x28 | SET_ENT_POS       | a0:ent, a1:x, a2:y | teleport entity                                                      | ✅   |
| 0x29 | GET_CUR_ROOM      | —                | `reg = g_cur_komnata`                                                  | ✅   |
| 0x2A | IS_SOUND_PLAYING  | a0:id            | `reg = (sample a0 active)`                                             | ✅   |
| 0x2B | SET_VERB          | a0:verb          | `g_dialogue_mode = a0`                                                 | ✅   |
| 0x2C | SCENE_SWITCH      | a0:mask_file*    | load new MASK, snapshot 0x44E750 → 0x44F4F0                            | 🟡   |
| 0x2D | PLAY_ANIM         | a0:file*, a1:layer | play one-shot animation on layer a1                                  | ✅   |
| 0x2E | PRELOAD_LIST_A    | a0:list*, a1:n   | prefetch animations for slot A                                         | ✅   |
| 0x2F | PRELOAD_LIST_B    | "                | …slot B                                                                | ✅   |
| 0x30 | SPAWN_ENTITY      | a0:anim*, a1:script*, … | AllocEntity + link to update list                               | ✅   |
| 0x31 | ENABLE_ENT        | a0:ent           | clear hidden bit                                                       | ✅   |
| 0x32 | DISABLE_ENT       | a0:ent           | set hidden bit                                                         | ✅   |
| 0x33 | RESET_ACTORS      | —                | both actors → idle default anim                                        | ✅   |
| 0x34 | GET_ACTOR_ID      | —                | `reg = g_active_actor + 1`                                             | ✅   |
| 0x35 | WALK_MODE1        | a0:speed         | actor walks in current direction (mode 1)                              | ✅   |
| 0x37 | WALK_MODE2        | "                | mode 2                                                                 | ✅   |
| 0x38 | WALKTO_MODE1      | a0:x, a1:y       | path-find to (x,y) with mode-1 anims                                   | ✅   |
| 0x3A | WALKTO_MODE2      | "                | with mode-2 anims                                                      | ✅   |
| 0x3B | ATTACH_PROP_TMP   | a0:ent, a1:prop  | attach prop to entity (one shot)                                       | ✅   |
| 0x3C | ATTACH_PROP_KEEP  | "                | attach persistently                                                    | ✅   |
| 0x3D | WAIT_PROP_FRAME   | a0:ent, a1:frame | tick until prop animation reaches frame                                | ✅   |
| 0x3E | HIDE              | a0:ent           | set entity flags1 bit 0x80                                             | ✅   |
| 0x3F | SHOW              | a0:ent           | clear entity flags1 bit 0x80                                           | ✅   |
| 0x40 | SET_CURSOR_SPEED  | a0:base, a1:min, a2:step | tweak perspective math constants                             | ✅   |
| 0x41 | SOUND_PLAY        | a0:src, a1:vol, u32@+8:id | positional sound queue + stereo pan via PlaySfxPanned (T36) | ✅   |
| 0x42 | SOUND_STOP        | —                | reset positional queue + stop music                                    | ✅   |
| 0x43 | FLAG_CLEAR        | —                | dead state write (zero readers w oryginale — no-op 1:1)               | ✅   |
| 0x44 | FLAG_SET          | —                | dead state write (zero readers w oryginale — no-op 1:1)               | ✅   |
| 0x45 | SAVE_TICK         | a0:slot          | SAVED_TICK[a0 & 0xF] = current tick                                    | ✅   |
| 0x46 | GET_TICK_DELTA    | a0:slot          | `reg = cur_tick - SAVED_TICK[a0]`                                      | ✅   |
| 0x47 | NUDGE_ENT         | a0:ent, a1:dx, a2:dy | move entity in animation space                                     | ✅   |
| 0x48 | FADE_FULL         | a0:mode          | snapshot palette; 0=normal 1=white 2=black 3=gray                      | ✅   |
| 0x49 | FADE_STEP         | a0:step          | apply `step` units toward snapshot, set reg = done?                    | ✅   |
| 0x4A | FADE_LOAD         | a0:mode          | install palette instantly                                              | ✅   |
| 0x4B | GET_ENT_X         | a0:ent           | `reg = entity[a0]->cur_anim_x` (or target_x if 0x3A & 2)               | ✅   |
| 0x4C | GET_ENT_Y         | a0:ent           | `reg = entity[a0]->cur_anim_y` (or target_y)                           | ✅   |
| 0x4D | VAR_ADD           | a0:i, a1:n       | `var[i] += n`                                                          | ✅   |
| 0x4E | VAR_SUB           | a0:i, a1:n       | `var[i] -= n`                                                          | ✅   |
| 0x4F | ABORT             | —                | RunScriptInterpreter returns 0                                         | ✅   |
| 0x50 | DISABLE_SUBANIM   | a0:ent, a1:idx   | flag entity.subanim[idx] off                                           | ✅   |
| 0x51 | ENABLE_SUBANIM    | "                | flag on                                                                | ✅   |
| 0x52 | BEGIN_DIALOG      | a0:ent, a1:tag, a2:n | open dialogue: DialogStackPush + load talking-head atlas (T20b/T20c) | ✅   |
| 0x53 | END_DIALOG        | a0:script*       | play [sampl] lines from Gadki.scr + DialogStackPop (T20b shipped)     | ✅   |
| 0x54 | SHOW_PICTURE      | a0:name*         | load + display .pic fullscreen, wait for LMB / timeout                 | ✅   |
| 0x55 | END_FORCE         | —                | treated identically to 0x56                                            | ✅   |
| 0x56 | EOF               | —                | end of stream                                                          | ✅   |
| 0x57 | DEBUG_LOG         | a0:text*         | append text to `__tmp.txt` (port writes to stderr)                     | 🟡   |

Operand-count `len` differs per opcode; the interpreter unconditionally
advances by `len*2` bytes when no jump is taken.

## Port shortcuts oznaczone 🟡

| Op | Co odbiega od oryginału |
|---|---|
| 0x2C | Port skipuje exit-reachability graph step + 3502 B scene snapshot — w shipped scriptach żaden caller go nie konsumuje |
| 0x57 | Log idzie do stderr zamiast `__tmp.txt` (intencjonalne — no FS spam) |

Choice picker UI w op 0x1A pozostaje deferred — stage 1 / 2 scripts
nie używają multi-choice dialogues, więc dialog linear playback
wystarcza. Jeśli stage 3+ używa, do dorobienia.

## Opcode'y oznaczone ❌

*Currently none — wszystkie opcode'y są co najmniej dispatched.*

0x43/0x44 wcześniej tu były jako "no-op bo nie zRE'd consumer". Po
sprawdzeniu xrefs w oryginale: target zmiennej ma 2 WRITE refs
(op 0x43, 0x44) i **zero READ refs** w całym binary — zmienna jest
dead state nawet w oryginale. No-op w porcie jest 1:1 correct, więc
oba ✅.

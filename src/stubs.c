/*
 * stubs.c — definitions for every global and helper that the rest of the
 * engine references through `extern` but whose full implementation we
 * deferred from the Ghidra reverse. Each stub:
 *
 *   • carries a one-line note with the original FUN_* / DAT_* address
 *   • has minimal-but-typed behaviour (no-ops, sane defaults)
 *   • exists so that the engine LINKS cleanly and the SDL build runs
 *
 * If you ever fully port the binary, replace each stub with the
 * corresponding decompiled body.
 */
#include "wacki.h"
#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* =========== globals ====================================================
 * Only the ones not owned by another module live here.  See game.c
 * (g_tick_counter, g_lmb_handled, g_stage_table, …), script.c
 * (g_active_actor, g_perspective_*) and assets.c (g_persp_band_count).
 */
uint32_t  g_entity_state[0x11C];        /* DAT_00449D28 */
uint32_t  g_scene_snapshot[0x1E];       /* DAT_00443332 */
int16_t   g_persp_profile[0x22*2];      /* DAT_0044E5F8 */

/* g_next_cd_check removed — rule #7. */

/* T22 phase A/B staging globals removed in T42:
 *   - g_pending_komnata: replaced by in-place LoadKomnataScene call
 *   - g_komnata_loaded_by_op20: outer-loop guard no longer needed
 *     after play_first_scene_demo collapsed to single play_demo_scene
 *     call (T22 phase B). */

/* Scene transitions (ScriptGoToKomnata) → src/scene/navigation.c
 * Actor walking (ActorWalkToBlocking, ActorWalkBothBlocking) →
 *     src/scene/actor_walk.c
 * Stage descriptors (BuildStageTable, LoadActorWalkAnims, the
 *     g_stage_table/g_stage_va_table/g_actor_walk_anim globals,
 *     and g_default_world_state) → src/scene/stage.c */

/* LoadKomnata — 1:1 port of FUN_00402A50 @ 0x00402A50.
 *
 *   piVar2 = *DAT_0044A19C;                         // komnata table base
 *   walk entries (14 bytes each) until index == id
 *   if not found: DAT_0044E5DC = 0; return;
 *   DAT_0044E588 = id;                              // g_cur_komnata
 *   DAT_0044E448 = entry[+4];                        // flags
 *   palette fade-out (deferred — see comment below)
 *   FUN_00405F80() + FUN_00402DB0()                  // clear lists
 *   FUN_00402990(name)                               // name-keyed init
 *   FindScriptByStageAndRoom(scripts, etap, name)    // locate Wacky.scr section
 *   FUN_00409970(scripts)                            // parse [sampl] tags
 *   load pal_NN_NN.pal                               // per-komnata palette
 *   if (flags & 1) link kind=3 walk-behind initial entity
 *   if (flags & 2) link kind=2/3/4 default entities (cursor, krazek)
 *   RunScriptInterpreter(0x26, 0x26, entry[+6])      // enter script
 *   palette fade-in
 *   ProcessGameFrameTick × 2
 *   RunScriptInterpreter(0x26, 0x26, entry[+10])     // secondary script
 *
 * Most palette / FUN_0040xxxx side-effects are deferred — what matters
 * for our port is: locate name, clear lists, run enter_script. */
extern uint32_t g_stage_va;
extern void EntityListClearAll(void);
extern void VisibleMasksReset(void);
extern void ResetFrameSfxState(void);
extern const void *xlat_binary_ptr(uint32_t addr);

const char *LoadKomnata(uint16_t id)
{
    if (id == 0 || !g_stage_va) return NULL;
    /* Stage descriptor: +0 = komnata array base VA */
    extern const void *PeLoaderRead(uint32_t va);
    const uint8_t *sd = (const uint8_t *)PeLoaderRead(g_stage_va);
    if (!sd) return NULL;
    uint32_t komnata_arr_va = (uint32_t)(sd[0] | (sd[1] << 8) |
                                         (sd[2] << 16) | (sd[3] << 24));
    const uint8_t *karr = (const uint8_t *)PeLoaderRead(komnata_arr_va);
    if (!karr) return NULL;

    /* T105 fix — walk komnata table 1:1 with FUN_00402A50 @ 0x00402A55:
     *   piVar5 = table_base;  iVar6 = 0;
     *   do {
     *       if (*piVar5 == 0 && piVar5[1] == 0) { iVar6 = 0; bail; }
     *       ++iVar6;  piVar5 += 14;
     *   } while (iVar6 < param_1);
     *   ++iVar6;                              // post-loop inc
     *   idx = param_1 - 1;                    // 0-based entry to use
     *
     * Walk scans entries 0..(param_1-1), checking each for NULL+0
     * sentinel mid-walk. If hit, abort. Otherwise use index (param_1-1).
     *
     * Earlier port: `for (i<=idx+1 && i<16; ++i)` — bounded by fixed
     * 16-entry sanity cap. Stages 2-5 with >16 rooms would silently
     * fail to find them. */
    int idx = (int)id - 1;     /* 1-based → 0-based */
    int found = 0;
    for (int i = 0; i < (int)id; ++i) {
        uint32_t name_va = (uint32_t)(karr[i*14 + 0] | (karr[i*14 + 1] << 8) |
                                      (karr[i*14 + 2] << 16) | (karr[i*14 + 3] << 24));
        uint16_t flags   = (uint16_t)(karr[i*14 + 4] | (karr[i*14 + 5] << 8));
        if (!name_va && flags == 0) {             /* terminator */
            fprintf(stderr, "[load-komnata] terminator hit at i=%d, requested id=%u\n",
                    i, id);
            return NULL;
        }
        if (i == idx) { found = 1; /* keep walking to verify no terminator before idx */ }
    }
    if (!found) {
        fprintf(stderr, "[load-komnata] id=%u not in stage table\n", id);
        return NULL;
    }
    uint32_t name_va   = (uint32_t)(karr[idx*14 + 0] | (karr[idx*14 + 1] << 8) |
                                    (karr[idx*14 + 2] << 16) | (karr[idx*14 + 3] << 24));
    uint16_t flags     = (uint16_t)(karr[idx*14 + 4] | (karr[idx*14 + 5] << 8));
    uint32_t enter_va  = (uint32_t)(karr[idx*14 + 6] | (karr[idx*14 + 7] << 8) |
                                    (karr[idx*14 + 8] << 16) | (karr[idx*14 + 9] << 24));
    uint32_t second_va = (uint32_t)(karr[idx*14 + 10] | (karr[idx*14 + 11] << 8) |
                                    (karr[idx*14 + 12] << 16) | (karr[idx*14 + 13] << 24));
    const char *name = (const char *)PeLoaderRead(name_va);

    g_cur_komnata = id;
    g_stats.total_komnata_loads++;                /* T56 */
    fprintf(stderr, "[load-komnata] %u '%s' flags=0x%04X enter=0x%08X second=0x%08X\n",
            id, name ? name : "(null)", flags, enter_va, second_va);

    /* Clear lists — port mirror of FUN_00405F80 + FUN_00402DB0. */
    EntityListClearAll();
    VisibleMasksReset();
    ResetFrameSfxState();
    /* T132 — original FUN_00402DB0 calls FUN_00410D20 (sound queue reset)
     * as part of room reset. Without this, positional sources from the
     * previous komnata leak into the new one's aggregate pan. */
    SoundQueueReset();

    /* T107 — partial port of FUN_00402DB0 (room reset). The original
     * does a consolidated clear that we previously split across multiple
     * code paths; the bits below explicitly cover the gaps so non-op-0x2C
     * komnaty (= no explicit mask asset) don't carry stale state from the
     * previous room:
     *
     * 1. Perspective band count — original FUN_00402DB0 unconditionally
     *    `DAT_0044A200 = 0` then `= 4 if (flags & 2)`. Earlier port only
     *    reset it inside ScriptCallBgMaskSetup (which is called only when
     *    the room has a mask asset). Rooms without one inherited band
     *    count from the previous komnata.
     *
     * 2. Actor walker state — clear walk-remaining (+0x4C/+0x50) and
     *    walker-busy flag (+0x3A bit 0) on both g_actor[]. Op 0x15 path
     *    plant later re-sets them. Without this, an actor mid-walk when
     *    the player exited the room would resume walking at room entry.
     *
     * 3. Actor scale_pct — original sets `+0x16 = 100` (= 1.0×). Without
     *    this, an actor whose script set their scale in a previous room
     *    keeps that scale until UpdateActorMovement re-computes from
     *    anchor Y. Brief visual glitch on room entry.
     *
     * Cursor entity link (DAT_0045147C / DAT_00451480) is skipped — see
     * T106 (deferred). Click queue head, panel verb-tab init, label
     * strings — all done at engine boot, no need to repeat. */
    extern int g_persp_band_count;
    g_persp_band_count = (flags & 2) ? 4 : 0;
    /* Reset perspective globals (1:1 with FUN_00402DB0 top):
     *   DAT_0044a198 = 0x78;   // g_cursor_speed   = 120
     *   DAT_00449878 = 4;       // g_perspective_min  = 4
     *   DAT_0044987c = 7;       // g_perspective_step = 7
     * Without this an op 0x40 SET_PERSPECTIVE call from a prior komnata's
     * action cinematic (which biases perspective so ACTIVE actor scales
     * toward 0) persists into the next komnata. First UpdateActorMovement
     * tick after scene-load recomputes +0x58 with stale globals — actor
     * visibly jumps ~5% between T107 hardcoded 100 and stale-perspective
     * computed value. */
    extern uint16_t g_cursor_speed;
    extern uint16_t g_perspective_min;
    extern uint16_t g_perspective_step;
    g_cursor_speed     = 0x78;
    g_perspective_min  = 4;
    g_perspective_step = 7;
    extern Entity *g_actor[2];
    for (int i = 0; i < 2; ++i) {
        Entity *a = g_actor[i];
        if (!a) continue;
        uint8_t *eb = (uint8_t *)a;
        *(uint32_t *)(eb + 0x4C) = 0;       /* walk_dx_remaining */
        *(uint32_t *)(eb + 0x50) = 0;       /* walk_dy_remaining */
        *(uint16_t *)(eb + 0x3A) &= (uint16_t)~5u;   /* clear bits 0+2 */
        *(uint16_t *)(eb + 0x32) = 0;       /* pc */
        *(uint16_t *)(eb + 0x3C) = 0;       /* delay countdown */
        /* scale_pct lives in +0x58 in our entity layout (T3 walker port);
         * original was +0x16 in 32-bit struct. UpdateActorMovement
         * re-computes from anchor Y, so just reset to 100. */
        *(uint16_t *)(eb + 0x58) = 100;
    }

    /* Find this komnata's section in Wacky.scr + parse [sampl] tags —
     * 1:1 with FUN_00402A50's FindScriptByStageAndRoom + FUN_00409970
     * sequence. Replaces hand-transcribed g_frame_sfx[] table for any
     * asset mentioned in the parsed [komnata]N section. */
    extern void *g_scripts_obj;
    if (g_scripts_obj && name) {
        char etap_str[2] = { (char)('0' + g_cur_etap), 0 };
        if (FindScriptByStageAndRoom(g_scripts_obj, etap_str, name)) {
            /* ScriptObj.start / .end are private to script.c; expose
             * via accessor — see ScriptObjGetSection() decl. */
            extern const uint8_t *ScriptObjGetSectionStart(void *self);
            extern const uint8_t *ScriptObjGetSectionEnd  (void *self);
            ResetDynamicSfxTable();
            const uint8_t *ss = ScriptObjGetSectionStart(g_scripts_obj);
            const uint8_t *se = ScriptObjGetSectionEnd  (g_scripts_obj);
            if (ss && se) ParseSamplTagsForKomnata(ss, se);
        }
    }

    /* Panel page-swap — 1:1 with FUN_00402A50 @ 0x00402D76 calling
     * FUN_004071F0 right before enter_script. Loads page[0]'s 6 slots
     * into the panel verb table so the room starts with the inventory
     * front page on the bar. */
    g_settings_anim_active = flags;     /* T121: full u16 from komnata
                                         * entry[+4] (was truncated to u8). */
    PanelPageSwap();

    /* Run enter_script (1:1 with `RunScriptInterpreter(0x26, 0x26, ptr)`). */
    if (enter_va) {
        const uint8_t *bc = (const uint8_t *)xlat_binary_ptr(enter_va);
        if (bc) RunScriptInterpreter(0x26, 0x26, (uint8_t *)bc);
    }

    /* TWO frame ticks between enter_va and second_va — 1:1 with
     * FUN_00402A50:
     *   RunScriptInterpreter(enter_va);
     *   palette fade-in;
     *   ProcessGameFrameTick();        // <-- this
     *   ProcessGameFrameTick();        // <-- and this
     *   RunScriptInterpreter(second_va);
     *
     * These ticks let EntityRenderAll process one-shot BG-blit entities
     * (spawn flags = 0x0060 → flag-0x40/0x20 branch in FUN_00406040
     * paints the atlas to the backbuffer + clears 0x20 + FlushFrameToPrimary).
     * Without them, second_va's `op 0x31 destroy id=6` removes the BG
     * entity from the render list BEFORE the renderer ever saw it →
     * komnata 5 (magaz3j) renders the prior scene's framebuffer because
     * its real BG (magaz3c.wyc spawned with flags=0x60) was never blitted.
     *
     * Stage-1 komnaty have second_va = 0 so this ran as no-op previously;
     * stage 2 komnata 5 is the first to actually use second_va, which is
     * why the bug surfaced only here. */
    if (second_va) {
        extern void ProcessGameFrameTick(void);
        ProcessGameFrameTick();
        ProcessGameFrameTick();
        const uint8_t *bc = (const uint8_t *)xlat_binary_ptr(second_va);
        if (bc) RunScriptInterpreter(0x26, 0x26, (uint8_t *)bc);
    }

    return name;
}

/* ActorWalkToBlocking — 1:1 with op 0x10/0x11/0x12 wait-for-walk
 * loop from Ghidra @ RunScriptInterpreter 0x00407820 case 0x10:
 *
 *   if (actor[+0x22] != tx || actor[+0x24] != ty) {
 *     DAT_0044e6a4 = idx;                  // swap active to this actor
 *     DAT_0044e5ac = 1;                    // synthesize click pending
 *     DAT_0044e5a4 = 1;                    // synthesize walker-bind flag
 *     DAT_0044e570 = -1;                   // walk-target id reset
 *     actor[+0x4C] = 0; actor[+0x50] = 0;
 *     UpdateActorMovement(tx, ty);         // binds walker via standard path
 *     DAT_0044e6a4 = saved_active;
 *     do {
 *       DAT_0044e5ac = 0; DAT_0044e5a4 = 0;
 *       ProcessGameFrameTick();
 *     } while (actor[+0x4C] != 0 || actor[+0x50] != 0 || DAT_0044e570 != -1);
 *   }
 *
 * EACH per-entity VM tick inside ProcessGameFrameTick advances the
 * walker via op 0x15/0x16's step loop — step size = walker bytecode's
 * step operand (× perspective scale). We just bind the walker through
 * BindActorWalker (which plants the path immediately, see #fix-2) and
 * pump ProcessGameFrameTick + SDL_Delay until +0x4C/+0x50 are zero.
 *
 * Previous port-only impl stepped 1 pixel per ProcessGameFrameTick
 * call with no SDL_Delay → spin loop pegged the CPU, walker traversed
 * 1000+ px/sec → actor teleported off-screen before the verb-script
 * could finish; QUIT events were also never honoured → game unkillable
 * during any verb-script walk. */
extern int  BindActorWalker(int actor_idx, int target_x, int target_y);
extern int  PlatformShouldQuit(void);

/* DAT_0044E448 — komnata flag bits (set from komnata table entry[+4]
 * inside FUN_00402A50 / LoadKomnata):
 *   bit 0 = panel visible (read by FUN_00407260 PanelHitTest)
 *   bit 1 = actors active (read by actor.c — UpdateActorMovement gate)
 *   bit 2 = link the kind=3/4 default entities (cursor + krazek)
 * Default is 2 (actors-only) so the menu/cutscene path doesn't render
 * the panel; LoadKomnata raises bit 0 for in-game rooms. */
uint16_t  g_settings_anim_active = 2;   /* DAT_0044E448 — komnata flags
                                          * (T121: u16 not u8 — high bits
                                          * 8-15 needed by
                                          * ScriptCallBgMaskSetup perspective
                                          * band count `(flags & 0xff02) << 1`). */
uint16_t  g_active_target_y = 0;        /* DAT_0044E5A8 */
uint16_t  g_selected_save_slot = 0;
int       g_cd_drive_letter_present = 1;/* DAT_00475A54 */

void     *g_dialogues_obj = NULL;
void     *g_scripts_obj   = NULL;
void     *g_items_obj     = NULL;
AnimAsset *g_panel_cursor = NULL;       /* DAT_0044E698 */
AnimAsset *g_panel_asset  = NULL;       /* DAT_00453744 — stage panel (panel.wyc) */
AnimAsset *g_items_atlas  = NULL;       /* DAT_0044E6AC — przedm.wyc icons */
Entity   *g_actor[2]      = { NULL, NULL };

/* g_hover_scene_verb — written by ClickHitTest; read by cursor-state
 * machine to pick an icon. 0x26 = no hover. */
uint16_t  g_hover_scene_verb  = 0x26;

/* Panel hit-test + panel globals (g_panel_verb_tab, g_hover_panel_verb,
 * g_panel_cursor_redirect, g_panel_cursor_redirect2) moved to
 * src/hud/panel.c. */

/* LoadItemNamesTable + ItemHoverDwellTick + per-item WAV name table
 * moved to src/hud/items.c. */


/* Inventory + panel page rotation (Inventory, ResetInventory,
 * PanelPageSwap, InventoryPage*, InventoryAddItem, InventoryRemoveItem,
 * InventoryDropItem, InventoryHasItem, InventorySetPageForItem) plus
 * the inventory-side globals (g_panel_page_idx,
 * g_panel_verb_tab_backup, g_panel_redraw) moved to
 * src/hud/inventory.c. */

/* =========== version / file helpers ===================================== */

/* FUN_0040F8D0 — GetFileVersionInfo() probe; portable build can't probe a
 * .dll version, so always claim a sufficiently new one. */
uint32_t GetDllPackedVersion(const char *dll) { (void)dll; return 0x00500004; }

/* =========== blit-row helpers used by the original Win32 BlitSprite ====
 * The portable graphics.c already inlines these, so these are placeholders
 * to satisfy any remaining `extern` refs in legacy code paths. */
void BlitColorKeyRow(uint8_t *d, const uint8_t *s, uint16_t n)
{ for (uint16_t i=0;i<n;++i) if (s[i]) d[i]=s[i]; }
void BlitTranslucentRow(uint8_t *d, const uint8_t *s, uint16_t n, const uint8_t *xlate)
{ for (uint16_t i=0;i<n;++i) if (s[i]) d[i] = xlate ? xlate[(d[i]<<8)|s[i]] : (uint8_t)((d[i]+s[i])>>1); }
void OptimiseRectList(void *src, uint16_t count, void **out, uint32_t *outc)
{ *out = src; *outc = count; }
void RestoreSurfaceIfLost(void *o) { (void)o; }
void RestoreLostSurfaceArea(int16_t x,int16_t y,int16_t w,int16_t h)
{ (void)x;(void)y;(void)w;(void)h; }

/* =========== AVI playback (no-op shims) ================================= */
typedef struct AviPlayer { int opened; } AviPlayer;
void *NewAviPlayer(void *p)         { AviPlayer *a=(AviPlayer*)p; if(a) a->opened=0; return p; }
void  DestroyAviPlayer(void *p)     { (void)p; }
void  StartAviPlayback(void *p)     { (void)p; }
int   PollAviPlayback(void *p)      { (void)p; return 0x20D; /* "done" */ }
void  StopAviPlayback(void *p)      { (void)p; }
int   OpenAviCutscene(void *p, const char *path, void *owner)
{ (void)p; (void)owner; fprintf(stderr, "[avi] open(%s) ok\n", path?path:"(null)"); return 1; }

/* =========== DirectSound version checker (no-op shims) ================== */
typedef struct DSoundVerChecker { int dummy; } DSoundVerChecker;
void  DSoundVer_Init   (DSoundVerChecker *self) { (void)self; }
int   DSoundVer_IsBad  (DSoundVerChecker *self) { (void)self; return 0; }
short DSoundVer_Confirm(DSoundVerChecker *self) { (void)self; return 4; /* IDRETRY */ }
void  DSoundVer_Free   (DSoundVerChecker *self) { (void)self; }

/* Animation resolver (FindAnimationScript, PlayActorAnimByPath)
 * moved to src/anim/resolver.c. */
void  PlayAnimation(uint16_t anim, uint16_t frame)
{ (void)anim; (void)frame; }
void  PrintTextOnScreen(uint16_t hx, uint16_t hy, const char *text)
{ (void)hx; (void)hy; if(text) fprintf(stderr, "[text] %s\n", text); }
void  PaletteFadeStep(int delta) { (void)delta; }
void  PaletteFadeInOut(uint16_t pct, const uint8_t *pal,
                       uint16_t first, uint32_t flags, void *cb)
{ (void)pct;(void)flags;(void)cb; if(pal) InstallPalette(pal, first); }
void  SetPalette(const uint8_t *pal, uint16_t first) { InstallPalette(pal, first); }

/* =========== placeholder rendering ======================================
 * If the engine is missing a background asset it would otherwise leave the
 * back-buffer untouched (black). Paint a recognisable test card and the
 * name of the file we *would* have loaded so the user sees activity. */
/* DrawPlaceholderScreen — no-op kept only as a compatibility hook.
 * (The test-card "mosaic" the early-port used has been removed at the
 * user's request: when an asset is missing the engine now just leaves the
 * back-buffer in its previous state and logs to stderr.) */
extern uint8_t *g_back_shadow;
extern uint8_t  g_palette_rgb[256*3];
/* DrawPlaceholderScreen + Screenshot helpers moved to
 * src/util/screenshot.c. */
/* UpdateAllEntities removed — was a no-op placeholder. Its responsibilities
 * are now split between EntityWalkerTick (per-entity VM ticks) and
 * EntityRenderAll (z-sorted blit), both wired into ProcessGameFrameTick. */
/* Deferred click-event queue (EnqueueClickEvent, FlushQueuedClicks)
 * moved to src/scene/click_queue.c. */
/* cd_watchdog_dispatcher REMOVED — rule #7. */

/* ------------------------------------------------------------------------- *
 * Script-VM ↔ subsystem bridges. These mirror the FUN_xxx calls that
 * RunScriptInterpreter makes per-opcode. Until the full entity / dialogue /
 * walker subsystems are ported, most of these are observable no-ops; the
 * sound ones forward to the real audio.c so PLAY_SOUND opcodes in shipped
 * scripts can actually play their WAVs out of Dane_02.dta.
 * ------------------------------------------------------------------------- */

/* Frame delta in milliseconds — read by code that genuinely wants real
 * wall-clock ms (held-item ghost interp, speech-balloon dismiss timer,
 * etc.). Recomputed every frame from the MM timer in the original; we
 * default to ~16 ms (60 fps) which matches our SDL pacing. */
uint32_t g_frame_delta_ms = 16;

/* Frame delta in 10 ms TICKS — 1:1 with DAT_0044E578 in the PE. The
 * original game arms timeSetEvent (call site at 0x00403D84) with a 10 ms
 * periodic timer whose ISR (FUN_00403E40) does `INC DAT_0044E454`. That
 * counter is sampled in FUN_004024D0 into DAT_0044E578 = (now - prev)
 * 10 ms units. EVERY in-PE site that reads DAT_0044E578 (cursor anim
 * accumulator, entity VM frame countdown +0x3C, dialog/prop timer, op
 * 0x14 WAIT_MS countdown, op 0x26/0x3D anim-frame waits, dialog choice
 * dismiss) expects this unit, not real milliseconds. Driving them with
 * g_frame_delta_ms (real ms) makes everything animate ~10× too fast.
 *
 * Updated in lockstep with g_frame_delta_ms inside the PGFT Inner /
 * EntityWalkerTick dt blocks; an accumulator carries the sub-10 ms
 * remainder so we don't drift over time at frame rates that aren't a
 * clean multiple of 10 (e.g. 16 ms @ 60 fps emits 2/1/2/1/… ticks). */
uint16_t g_frame_delta_ticks = 1;

/* WackiRand / WackiRandSeed moved to src/util/rng.c. */

/* Positional sound queue (SoundQueueReset, SoundQueueEnqueue,
 * SoundQueueMixForListener) + sound script bridges
 * (ScriptCallSoundPlay, ScriptCallSoundStop) moved to
 * src/audio/sound_queue.c. */

/* Palette fade machinery — 1:1 port of cases 0x48/0x49/0x4A.
 *
 *   case 0x48 (full fade): DAT_004549E0 = 0; DAT_00455000 = step;
 *                          load target into DAT_00451DC8;
 *                          zero DAT_00454A00 work buffer.
 *   case 0x49 (step):      if (progress < 100) {
 *                              progress += step;
 *                              FUN_004140e0(work, target, out, progress%);
 *                              FUN_00412d10(out, 0);   // install
 *                              return 0;
 *                          }
 *                          // else return previous value
 *   case 0x4A (instant?):  similar to 0x48 but without progress reset.
 *
 * FUN_004140e0 linearly interpolates each palette byte between source
 * (DAT_00454A00, snapshot of pal at fade start) and target
 * (DAT_00451DC8) by progress/100, writing result to DAT_00454D00.
 *
 * Port state mirrors:
 *   g_palette_rgb       = DAT_00454D00 / live pal (256 entries × 3 RGB)
 *   s_pal_fade_source   = DAT_00454A00 (snapshot at fade start)
 *   s_pal_fade_target   = DAT_00451DC8 (loaded by case 0x48)
 *   s_pal_fade_progress = DAT_004549E0 (0..100)
 *   s_pal_fade_step     = DAT_00455000 (per-step advance) */
/* Palette fade (ScriptCallPalLoad + ScriptCallPalFadeStep) moved to
 * src/script_bridge/palette.c. */

void ScriptCallDestroyEnt(uint16_t id, int also_unreg_asset);  /* fwd decl */

/* BG mask setup (ScriptCallBgMaskSetup) moved to
 * src/scene/bg_mask.c. */

/* Click hit-test (FindEntityByVerbId, ClickHitTest) moved to
 * src/scene/hit_test.c.
 *
 * Mask-list registration (ScriptCallRegMaskList) and the
 * VisibleMasks* compatibility stubs moved to
 * src/scene/mask_list.c. */

extern Entity *AllocEntity(uint16_t w, uint16_t h, uint16_t kind, uint16_t flags);
extern void    LinkEntityToList(Entity **head, Entity *e, int position);
extern Entity *g_render_list_head;
extern Entity *g_click_list_head;
extern void    RegisterEntityForUpdate(Entity *e, uint16_t kind, uint16_t id);
extern void   *FindUpdateRegistration(uint16_t kind, uint16_t id);  /* FUN_00405D80 */
extern const void *xlat_binary_ptr(uint32_t);

/* ------------------------------------------------------------------------- *
 * SpawnActorEntity — 1:1 with op 0x30 SPAWN code path used for Ebek/Fjej.
 *
 * Original engine pre-spawns both actors at game start with their atlas
 * (ebek.wyc / fjej.wyc) bound and verb_id = 1/2 in the click payload, so
 *   - op 0x28 SET_ENTITY_XY id=1  → FUN_00404C30(1) finds Ebek's click
 *     entity → returns its owner render entity → moves it.
 *   - op 0x28 SET_ENTITY_XY id=2  → same for Fjej.
 *   - DispatchClickEvent + verb_table searches resolve actor verb_ids.
 *
 * Returns the spawned render entity (= g_actor[idx]). Owns:
 *   - kind=2 render entity registered (kind=2, id) in update table,
 *     linked to render list
 *   - kind=4 click entity (offset+8 = 1 in click list), bound to
 *     a tiny 1-entry verb table { count=1, verb_id }, linked to
 *     click list + registered (kind=4, id) in update table          */
extern Entity *AllocEntity(uint16_t w, uint16_t h, uint16_t kind, uint16_t flags);

/* SpawnActorEntity + ScriptCallSpawnEntity moved to
 * src/scene/spawn.c. */

/* Asset / entity script bridges (ScriptCallLoadAsset, DestroyEnt,
 * EnableEnt, HideEnt, ShowEnt, WalkMode, WalkTo, AttachProp) moved
 * to src/script_bridge/entity.c. */

/* Speech balloon globals (g_speech_text/actor/tick/dismiss_ticks) and
 * text rendering (TextTranslationLutInit,
 * ScriptCallShowText, TickSpeechBalloon, ScriptCallDialogEnd, plus the
 * balloon-state globals) moved to src/text/balloon.c. */

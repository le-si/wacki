/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Mateusz Szuła
 *
 * src/scene/actor_walk.c — blocking actor-walk dispatch.
 *
 * Script opcodes 0x10 (walk Ebek), 0x11 (walk Fjej), and 0x12 (walk
 * both) drive an actor to a target position synchronously — the
 * script's VM tick blocks on the walker until the actor reaches the
 * target or the walk is interrupted by a click.
 *
 * The actual movement is set up by BindActorWalker (src/actor/
 * walker.c) — it plants Bresenham state on the entity, binds the
 * walk-anim bytecode, and arms the per-entity VM. This module
 * provides the two blocking wait loops on top of that:
 *
 *   ActorWalkToBlocking   — op 0x10 / 0x11: single-actor synchronous
 *   ActorWalkBothBlocking — op 0x12: stagger actor 0 by ~500 ms, bind
 *                          actor 1, then wait — either for the FIRST
 *                          actor to arrive (mode 0) or for both (mode != 0)
 *
 * Both leave g_script_vars[SCRIPT_VAR_INTERRUPTED_FLAG] = 1 on click
 * interruption, 0 on natural drain — the calling verb script reads
 * that to decide whether to abort or fall through to the next op. */

#include "wacki.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

extern int  BindActorWalker(int actor_idx, int target_x, int target_y);
extern void ProcessGameFrameTick(void);

/* ---- constants ---------------------------------------------------- */

/* Frame pacing target while the script blocks — same 33 ms (≈ 30 fps)
 * as the main play_demo_scene loop, so the blocking wait doesn't
 * change the visible animation speed. */
#define WALK_FRAME_DELAY_MS                 33

/* Walker safety caps. The wait loop bails after this many iterations
 * regardless of whether the walker drained — defensive against an
 * accidentally-NaN target or a stuck walker keeping the script alive
 * forever. ~1024 iterations × 33 ms ≈ 33 s, well past any normal walk. */
#define WALK_SINGLE_SAFETY_ITERS            1024
#define WALK_BOTH_SAFETY_ITERS              2048

/* op 0x12 "walk together" stagger — actor 0 walks alone for ~50 ticks
 * (≈ 500 ms at 100 Hz multimedia timer) before actor 1 starts moving,
 * giving the visible "Wacki + Fjej walk together" cue. */
#define WALK_STAGGER_TICKS                  0x32
#define WALK_STAGGER_FALLBACK_TICK_DELTA    3   /* when g_frame_delta_ticks is 0 */

/* g_script_vars index that carries the "interrupted by click" flag
 * back to the calling verb script. */
#define SCRIPT_VAR_INTERRUPTED_FLAG         4

/* op 0x12 wait-mode flag (operand a2), faithful to FUN_00407820 case
 * 0x12:
 *   a2 == 0 → return as soon as EITHER actor reaches the target. This
 *             is the go-to-exit path: the verb script runs op 0x20
 *             (GO_EXIT) next, so the komnata transition fires when the
 *             leader — actor 0, who gets the head-start — hits the edge.
 *   a2 != 0 → block until BOTH actors have arrived.
 *
 * NOTE: the port long had these inverted (0 = wait both). Because the
 * real exit-verb bytecode ships a2 = 0, that inversion forced BOTH
 * characters to walk to the edge before the room changed, instead of
 * just one. */
#define WALK_BOTH_MODE_WAIT_EITHER          0

/* op 0x12 (a2 == 0) writes which actor reached the target into the
 * per-actor flag words var[1] (actor 0 / Ebek) and var[2] (actor 1 /
 * Fjej): 0 = arrived, 1 = still walking. These alias the same words
 * walker.c uses for the frozen bit (SCRIPT_VAR_ACTOR_FLAGS_BASE), but
 * neither actor is frozen while walking together, so the original's
 * whole-word write is non-destructive. */
#define SCRIPT_VAR_ACTOR0_ARRIVED           1
#define SCRIPT_VAR_ACTOR1_ARRIVED           2

/* ---- helpers ---------------------------------------------------- */

/* Returns 1 if the actor's walker still has remaining steps (either
 * axis non-zero), 0 when it has drained. */
static int actor_walker_busy(Entity *e)
{
    uint8_t *eb = (uint8_t *)e;
    uint32_t wdx = EOFF(eb, ENT_OFF_WALKER_DX_REM, uint32_t);
    uint32_t wdy = EOFF(eb, ENT_OFF_WALKER_DY_REM, uint32_t);
    return wdx != 0 || wdy != 0;
}

/* Common epilogue for both blocking-walk variants: consume the click
 * flag, write the interrupted indicator to var[4], and clear g_lmb_
 * handled so subsequent click dispatch doesn't see a stale latch. */
static void walk_epilogue(int interrupted)
{
    g_lmb_clicked = 0;
    g_lmb_handled = 0;
    g_script_vars[SCRIPT_VAR_INTERRUPTED_FLAG] = (uint32_t)interrupted;
}

/* ---- single-actor blocking walk (op 0x10 / 0x11) ---------------- */

void ActorWalkToBlocking(int idx, int16_t tx, int16_t ty)
{
    if (idx < 0 || idx > 1 || !g_actor[idx]) return;
    Entity  *e  = g_actor[idx];
    uint8_t *eb = (uint8_t *)e;

    /* Already at target — original short-circuits before binding the
     * walker so the per-entity VM doesn't run a 0-step plant. */
    if ((int)EOFF(eb, ENT_OFF_ANCHOR_X, int16_t) == (int)tx &&
        (int)EOFF(eb, ENT_OFF_ANCHOR_Y, int16_t) == (int)ty)
    {
        return;
    }

    if (!BindActorWalker(idx, (int)tx, (int)ty)) return;

    /* Pump frames until the walker drains, the user clicks (interrupt),
     * or the platform requests quit. */
    int interrupted = 0;
    int safety      = WALK_SINGLE_SAFETY_ITERS;
    while (safety-- > 0) {
        if (!actor_walker_busy(e)) break;
        ProcessGameFrameTick();
        if (PlatformShouldQuit() || g_game_over_code) break;
        if (g_lmb_clicked) { interrupted = 1; break; }
        EnginePaceFrame(WALK_FRAME_DELAY_MS);
    }

    walk_epilogue(interrupted);
}

/* ---- both-actor blocking walk (op 0x12) ------------------------- */

/* Drain ~WALK_STAGGER_TICKS of frame-delta ticks before binding actor
 * 1's walker. The visible effect is Fjej starting a few frames after
 * Ebek so they walk side-by-side instead of in lock-step. Returns 1
 * if the user clicked mid-stagger (caller should bail with
 * interrupted=1), 0 if the stagger drained normally. */
static int run_stagger_phase(void)
{
    extern uint16_t g_frame_delta_ticks;
    int stagger_left = WALK_STAGGER_TICKS;
    while (stagger_left > 0) {
        ProcessGameFrameTick();
        if (PlatformShouldQuit() || g_game_over_code) return 0;
        if (g_lmb_clicked) return 1;

        int dt = g_frame_delta_ticks
               ? (int)g_frame_delta_ticks
               : WALK_STAGGER_FALLBACK_TICK_DELTA;
        stagger_left -= dt;
        EnginePaceFrame(WALK_FRAME_DELAY_MS);
    }
    return 0;
}

/* Returns 1 if either bound actor still has walker work to do — the
 * wait-both predicate (loop while this holds, exit once both drain). */
static int any_bound_actor_busy(const int bound[2])
{
    for (int i = 0; i < 2; ++i) {
        if (!bound[i] || !g_actor[i]) continue;
        if (actor_walker_busy(g_actor[i])) return 1;
    }
    return 0;
}

/* Returns 1 once actor `i` has reached the target. A present actor whose
 * walker has drained counts as arrived; so does one that never bound a
 * walker (already at the target). Absent actors never satisfy the wait.
 *
 * NOTE: a drained walker (+0x4C/+0x50 == 0) read right after a full
 * ProcessGameFrameTick reliably means "arrived at the FINAL target", not
 * merely "between waypoint legs": PerActorWaypointAdvanceTick re-arms the
 * walker for the next leg inside the same tick (frame_tick.c), so a
 * mid-path actor reads busy again by the time we look. This is the port's
 * equivalent of the original's pending-target sentinel
 * (DAT_0044e570 / DAT_0044e5c0 == -1), which lived in its multi-leg
 * re-pathing rather than in a separate flag we track here. */
static int actor_arrived(const int bound[2], int i)
{
    if (!g_actor[i]) return 0;
    if (!bound[i])   return 1;
    return !actor_walker_busy(g_actor[i]);
}

void ActorWalkBothBlocking(int16_t tx, int16_t ty, int mode)
{
    if (!g_actor[0] && !g_actor[1]) return;

    /* Bind actor 0 immediately; actor 1 only after the stagger drains. */
    int bound[2] = { 0, 0 };
    if (g_actor[0]) bound[0] = BindActorWalker(0, (int)tx, (int)ty);

    int interrupted = 0;
    if (run_stagger_phase()) {
        interrupted = 1;
        goto done;
    }
    if (PlatformShouldQuit() || g_game_over_code) goto done;

    if (g_actor[1]) bound[1] = BindActorWalker(1, (int)tx, (int)ty);

    int safety = WALK_BOTH_SAFETY_ITERS;
    if (mode == WALK_BOTH_MODE_WAIT_EITHER) {
        /* Go-to-exit: stop the instant EITHER actor arrives, checking
         * actor 0 (the head-start leader) first, and record which one
         * reached the target in var[1]/var[2] — mirrors FUN_00407820. */
        while (safety-- > 0) {
            ProcessGameFrameTick();
            if (PlatformShouldQuit() || g_game_over_code) break;

            if (g_actor[0]) {
                if (actor_arrived(bound, 0)) {
                    g_script_vars[SCRIPT_VAR_ACTOR0_ARRIVED] = 0;
                    break;
                }
                g_script_vars[SCRIPT_VAR_ACTOR0_ARRIVED] = 1;
            }
            if (g_actor[1]) {
                if (actor_arrived(bound, 1)) {
                    g_script_vars[SCRIPT_VAR_ACTOR1_ARRIVED] = 0;
                    break;
                }
                g_script_vars[SCRIPT_VAR_ACTOR1_ARRIVED] = 1;
            }

            if (g_lmb_clicked) { interrupted = 1; break; }
            EnginePaceFrame(WALK_FRAME_DELAY_MS);
        }
    } else {
        /* Wait until BOTH actors have drained their walkers. */
        while (safety-- > 0) {
            if (!any_bound_actor_busy(bound)) break;
            ProcessGameFrameTick();
            if (PlatformShouldQuit() || g_game_over_code) break;
            if (g_lmb_clicked) { interrupted = 1; break; }
            EnginePaceFrame(WALK_FRAME_DELAY_MS);
        }
    }

done:
    walk_epilogue(interrupted);
}

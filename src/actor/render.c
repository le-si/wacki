/* src/actor/render.c — entity z-sorted render walk + walk-behind mask.
 *
 * Once per frame, after the per-entity VM has updated every entity's
 * position/frame, EntityRenderAll:
 *
 *   1. Collects all entities into a working pointer array
 *   2. Sorts them by foot_y (lower foot = drawn first = behind)
 *   3. Walks back-to-front blitting each into the back buffer
 *
 * Special cases the walk handles inline:
 *
 *   - One-shot BG blit (flags 0x40 | 0x20): paints opaquely and clears
 *     bit 0x20 so subsequent frames skip. Used for room-background
 *     sprites in komnaty whose .pic is a 1×1 palette stub.
 *
 *   - Walk-behind mask (.msk asset, kind=0, flag_22 bit 1 clear):
 *     restores scene background pixels through the mask shape, but
 *     only over pixels we know an actor wrote. Achieves the "actor
 *     walks behind kiosk" effect without needing per-pixel depth.
 *
 *   - Perspective-scaled sprites (+0x58 ≠ 0/100): blit at scaled size,
 *     and recompute drawn position from anchor + scaled atlas hot-spot
 *     so the foot stays put across scale changes.
 *
 *   - Alpha-plane sprites (flags 0x100): use the dedicated alpha-plane
 *     blit path with tint blending.
 *
 * The actor-paint mask (s_actor_paint_mask) is a 1-byte-per-pixel
 * scratch buffer that records every pixel an actor (g_actor[0]/[1])
 * or "sky" entity wrote this frame. Walk-behind only restores BG
 * over those pixels — static foreground props with foot_y below the
 * walkable area are not affected.
 */

#include "wacki.h"
#include "internal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Entity *g_actor[2];

static int cmp_entity_y(const void *a, const void *b)
{
    Entity *ea = *(Entity *const *)a;
    Entity *eb = *(Entity *const *)b;
    /* Sort by foot_y at +0x26 — 1:1 with cmp callback LAB_00406190 in
     * the original (disassembled: `mov dx,[ecx+0x26]; mov ax,[ecx+0x26]; sub`).
     * Field +0x26 is `drawn_y + height` (set in ExecEntityScript post-exec
     * when flags & 0x200 == 0), i.e. the *bottom* of the drawn sprite =
     * foot_y. Earlier we sorted by +0x0C, which is the *drawn anchor*: for
     * FLAG_2 (foot-anchored) sprites that's the top-left of the bitmap,
     * giving wrong z-order. */
    int ay = (int)EOFF(ea, 0x26, int16_t);
    int by = (int)EOFF(eb, 0x26, int16_t);
    /* Fallback for entities whose +0x26 hasn't been computed yet
     * (flag 0x200 set, or freshly-spawned before first VM tick) —
     * approximate from anchor Y at +0x24 (the foot coord). */
    if (ay == 0) ay = (int)EOFF(ea, 0x24, int16_t);
    if (by == 0) by = (int)EOFF(eb, 0x24, int16_t);
    return ay - by;
}

/* Actor-paint scratch mask. 1 byte per backbuffer pixel; set to 1 when
 * an ACTOR (g_actor[0] / g_actor[1]) writes a non-transparent pixel.
 * Walk-behind BG-through-mask blits ONLY over these — so kioskb.msk's
 * shape covers Ebek/Fjej walking behind kiosk but doesn't repaint over
 * static entities (banan1, kiosk worker, etc.) that happen to be at
 * higher z-depth than the mask's foot_y. 1:1 conceptual match to the
 * original engine's walk-behind which only affects moving actors, not
 * baked-in scene props. */
static uint8_t *s_actor_paint_mask = NULL;
static int      s_actor_paint_sz = 0;
static void ensure_actor_paint_mask(void)
{
    int need = (int)g_screen_w * g_screen_h;
    if (need > s_actor_paint_sz) {
        free(s_actor_paint_mask);
        s_actor_paint_mask = (uint8_t *)malloc((size_t)need);
        s_actor_paint_sz = s_actor_paint_mask ? need : 0;
    }
    if (s_actor_paint_mask)
        memset(s_actor_paint_mask, 0, (size_t)s_actor_paint_sz);
}

void EntityRenderAll(Entity *head)
{
    (void)head;
    Entity *list[128];
    int n = 0;
    int total = EntityListCount(0);
    for (int i = 0; i < total && n < 128; ++i) {
        Entity *e = EntityListAt(0, i);
        if (e) list[n++] = e;
    }
    if (n > 1) qsort(list, n, sizeof(Entity *), cmp_entity_y);

    /* Reset actor-paint mask for this frame. */
    ensure_actor_paint_mask();

    /* TEMP DIAG: dump render-order when an action sprite is present
     * (atlas name contains 'kwiat'/'hustawk'/etc). Helps trace
     * "double character" z-sort issue. */
    {
        static uint32_t s_last_dump = 0;
        uint32_t now = SDL_GetTicks();
        int has_action = 0;
        for (int j = 0; j < n; ++j) {
            AnimAsset *aa = (AnimAsset *)ent_ptr_resolve(EOFF(list[j], 0x28, uint32_t));
            if (aa && (strstr(aa->name, "kwiat") || strstr(aa->name, "hustawk") ||
                       strstr(aa->name, "cool") || strstr(aa->name, "gadp"))) {
                has_action = 1;
                break;
            }
        }
        if (has_action && now - s_last_dump > 500) {
            extern Entity *g_actor[2];
            s_last_dump = now;
            fprintf(stderr, "[render-order] (action sprite present):\n");
            for (int j = 0; j < n; ++j) {
                Entity *ee = list[j];
                AnimAsset *aa = (AnimAsset *)ent_ptr_resolve(EOFF(ee, 0x28, uint32_t));
                int aidx = (ee == g_actor[0]) ? 0 : (ee == g_actor[1]) ? 1 : -1;
                fprintf(stderr, "  [%2d] +0x26=%d anchor=(%d,%d) draw=(%d,%d) wh=(%d,%d) "
                        "scale=%u flags=0x%04X atlas=%s%s\n",
                        j,
                        (int)EOFF(ee, 0x26, int16_t),
                        (int)EOFF(ee, 0x22, int16_t), (int)EOFF(ee, 0x24, int16_t),
                        (int)EOFF(ee, 0x0A, int16_t), (int)EOFF(ee, 0x0C, int16_t),
                        (int)EOFF(ee, 0x0E, int16_t), (int)EOFF(ee, 0x10, int16_t),
                        (unsigned)EOFF(ee, 0x58, uint16_t),
                        (unsigned)EOFF(ee, 8, uint16_t),
                        aa ? aa->name : "(none)",
                        aidx == 0 ? " <EBEK>" : aidx == 1 ? " <FJEJ>" : "");
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        Entity *e = list[i];
        uint16_t flags = EOFF(e, 8, uint16_t);
        /* Visibility gates — 1:1 with the original FUN_00406040 (renderer)
         * + the per-entity interpreter FUN_004012E0 logic:
         *
         *   bit 0x0001 = renderable (set by AllocEntity → FUN_00405920).
         *                Original renderer's outer-most guard is `flags & 1`.
         *                We don't set it on AllocEntity (legacy), so we just
         *                gate on the hide bits instead.
         *   bit 0x0080 = HIDDEN (op 0x3E HIDE_ENTITY sets, op 0x3F SHOW
         *                clears). EntityRenderAll skips these.
         *   bit 0x2000 = "wait-for-enable" / alpha-plane spawn. In the
         *                original, the per-entity interpreter SKIPS pixel-
         *                buffer prep when this bit is set (entity has a
         *                private pixel buffer that stays zero/transparent
         *                until the script ops fill it). Since our renderer
         *                blits from the atlas directly (not the entity's
         *                private buffer), reading would expose the asset's
         *                full first frame — visible garbage. So skip them
         *                here until the per-entity script enables them via
         *                op 0x3F or a SET_POS_FROM_FRAME path.
         *   bit 0x0040 = fading-out, bit 0x8000 = pending-free.
         */
        if (flags & 0x80)   continue;      /* hidden (op 0x3E) */
        if (flags & 0x40) {
            /* 1:1 with FUN_00406040 @ 0x004060f5: flag 0x40 has a
             * sub-branch on bit 0x20:
             *
             *   flags & 0x20 set   → ONE-SHOT BG BLIT. The original
             *                        renderer clears bit 0x20, calls
             *                        FUN_00411730 (PaintImageToBackbuffer
             *                        — opaque memcpy + dirty-rect), then
             *                        FlushFrameToPrimary. Subsequent
             *                        ticks see only bit 0x40 → skip.
             *
             *   flags & 0x20 clear → already blitted (or genuine
             *                        fade-out) → skip render.
             *
             * Used in komnaty whose entry in the stage table names a
             * stub .pic (e.g. magaz3j.pic = 1×1 palette-only, komnata 5):
             * the real background is spawned as a kind=2 atlas entity
             * via op 0x30 with flags=0x0060 (e.g. magaz3c.wyc), painted
             * once via this branch, then second_va @ 0x0042D828 destroys
             * the entity (id=6 destroy in logs). Earlier port skipped
             * unconditionally on 0x40 → BG never painted → komnata
             * showed the prior scene's framebuffer. */
            if (flags & 0x20) {
                AnimAsset *atlas = (AnimAsset *)ent_ptr_resolve(EOFF(e, 0x28, uint32_t));
                if (atlas && atlas->frame_count && atlas->pixel_ptrs) {
                    uint16_t f = EOFF(e, 0x30, uint16_t);
                    if (f >= atlas->frame_count) f = 0;
                    uint8_t *px = atlas->pixel_ptrs[f];
                    if (px) {
                        uint16_t fw = atlas->off_widths [f];
                        uint16_t fh = atlas->off_heights[f];
                        int16_t bx = (int16_t)EOFF(e, 0x0A, int16_t);
                        int16_t by = (int16_t)EOFF(e, 0x0C, int16_t);
                        PaintImageToBackbuffer(bx, by, fw, fh, px);
                        /* Clear bit 0x20 — mirror of FUN_00406040
                         * `*(ushort *)(piVar2 + 2) = uVar1 & 0xffdf;` */
                        EOFF(e, 8, uint16_t) = (uint16_t)(flags & ~0x20u);
                        FlushFrameToPrimary();
                        /* Stash atlas pixels as the scene's persistent
                         * BG — but ONLY if this sprite is BG-sized.
                         * Original FUN_00406040 doesn't discriminate
                         * (both BG sprites and HUD panel buttons use
                         * flag 0x60 → one-shot blit + persist in
                         * backbuffer); but our port takes a per-frame
                         * full-BG repaint shortcut, so we need to know
                         * which atlas to repaint. HUD buttons (id=100
                         * guziki, id=101 pasek) are 30×30 / 172×3 in
                         * the panel area (Y ≥ 400) and would overwrite
                         * the BG save. Heuristic: ≥ half play-area
                         * width AND tops out above the panel. */
                        if (fw >= 320 && (by < 200 || (by + fh) >= 380))
                            SaveSceneBgAtlas(bx, by, fw, fh, px);
                    }
                }
            }
            continue;
        }
        if (flags & 0x8000) continue;      /* pending free */
        /* NOTE: bit 0x2000 is NOT a visibility flag per the original
         * renderer (FUN_00406040 only checks bit 0x01 / 0x40 / 0xC000).
         * It's an "alpha-plane / static" flag that tells the per-entity
         * interpreter (FUN_004012E0) to skip pixel-buffer prep for
         * non-kind-3 entities. For kind-3 (RLE) entities, the original
         * always decodes — and our port blits directly from atlas so the
         * flag doesn't affect us. Earlier code skipped 0x2000 entirely;
         * that hid the HUD avatar (e1lataje id=100/101 — kind=3 with
         * 0x2000 set permanently per script) plus rob1/szczel HUD-glue.
         * Position validity (`dx <= 0 || dy <= 0`) is the natural gate:
         * entities that never get a positive position via their script
         * (e.g. unresolved SUBSCRIPT_CALL targets) skip on that check. */

        AnimAsset *atlas = (AnimAsset *)ent_ptr_resolve(EOFF(e, 0x28, uint32_t));
        /* kind=1 manual-buffer entities (speech balloons from op 0x09):
         * no atlas binding — pixels live in e->pixels with width/height
         * in e->width/e->height. Direct color-key blit (transparent=0).
         * NOTE: op 0x09 SHOW_TEXT is dead code in the shipped game (audio-
         * only dialogue), so this branch never fires in practice — kept
         * 1:1 with FUN_00406040 in case a future stage's scripts use it. */
        if (!atlas) {
            if (!e->pixels || !e->width || !e->height) continue;
            int16_t bx = (int16_t)EOFF(e, 0x0A, int16_t);
            int16_t by = (int16_t)EOFF(e, 0x0C, int16_t);
            BlitSpriteToBackbuffer((uint16_t)bx, (uint16_t)by,
                                   0, 0, e->width, e->height,
                                   e->width, e->height,
                                   e->pixels, 0);
            continue;
        }
        if (atlas->frame_count == 0) continue;
        uint16_t f = EOFF(e, 0x30, uint16_t);
        if (f >= atlas->frame_count) f = 0;
        uint8_t *px = atlas->pixel_ptrs ? atlas->pixel_ptrs[f] : NULL;
        if (!px) continue;
        uint16_t fw = atlas->off_widths [f];
        uint16_t fh = atlas->off_heights[f];
        /* Position: SET_POS_FROM_FRAME (opcode 0x05) normally writes the
         * atlas drawX/drawY[frame] into e+0xA/0xC; with the walker disabled
         * those stay at 0. Fall back to the atlas hot-spot directly — which
         * for prop atlases (drut/barstoi/pies) IS the scene-local position. */
        int16_t  dx = (int16_t)EOFF(e, 0x0A, int16_t);
        int16_t  dy = (int16_t)EOFF(e, 0x0C, int16_t);
        int       fallback_used = 0;
        if (dx == 0 && dy == 0 && atlas->off_drawX && atlas->off_drawY) {
            dx = (int16_t)atlas->off_drawX[f];
            dy = (int16_t)atlas->off_drawY[f];
            fallback_used = 1;
        }
        (void)fallback_used;
        /* Position validity:
         *   - dx,dy <= 0 → atlas hot-spot is a foot-anchor (negative because
         *     it offsets the sprite ABOVE the foot point). These entities
         *     MUST be positioned by the per-entity script (op 0x05/0x15/0x16)
         *     before they're meaningful on-screen. Without the walker, we
         *     can't synthesise a sensible position — render at (320, 380)
         *     would just plant the actor mid-screen, regardless of whether
         *     they're supposed to be there. Skip them entirely; the user
         *     will notice "missing" rather than "wrong-place" garbage.
         *   - dx,dy > 0 → scene-baked position (drut, barstoi, pies, …) —
         *     render at that hot-spot. */
        /* Off-screen skip — see game.c unified-draw comment. Generous
         * on the right edge so e1lataje (glizda) renders its full
         * leftward flight from x=700 down past x=0. */
        if ((int)dx + (int)fw <= 0) continue;
        if ((int)dy + (int)fh <= 0) continue;
        if (dx >= 640 + 200) continue;
        if (dy >= 400 + 200) continue;
        uint16_t scale = EOFF(e, 0x58, uint16_t);

        /* RLE-decode kind=3 frames into per-call scratch. */
        if (atlas->kind == 3) {
            int need = (int)fw * (int)fh;
            static uint8_t *s_scratch = NULL;
            static int      s_scratch_sz = 0;
            if (need > s_scratch_sz) {
                free(s_scratch);
                s_scratch = (uint8_t *)malloc((size_t)need);
                s_scratch_sz = s_scratch ? need : 0;
            }
            if (!s_scratch) continue;
            DepackRleFrame(px, s_scratch, need);
            px = s_scratch;
        }

        /* T-walk-behind: 1bpp packed MASK shape → BG-through-mask blit.
         * Detection: asset is .msk (kind == 0) AND flag_22 bit 1 clear
         * (= NOT 8bpp visible pixels). The mask's pixel data is a 1bpp
         * packed shape (stride = (w+7)/8 bytes per row, MSB-first).
         *
         * Effect: where the mask bit is set, copy the BG pixel at the
         * same global (x, y) onto the backbuffer. This re-covers any
         * actor pixels with the tree / building / foreground BG art that
         * the scene's .pic carries. Sort order (foot_y at +0x26) ensures
         * we run AFTER actors whose foot_y is smaller (= further from
         * camera than the mask) → actor walks BEHIND the mask region.
         *
         * Without this, walk-behind .msk entities would either not
         * render (their entity layer wasn't linked into the render list)
         * or render as 8bpp garbage (current bug — masks are 1bpp shapes
         * but blitted as opaque 8bpp). 1:1-style with the original engine:
         * FUN_00406040 iterates a single render list including walk-
         * behind masks and FUN_004120c0 does the actual blit; the mask
         * blit path consults the BG pixel buffer for source data instead
         * of asset pixels for the shape-only case.
         *
         * Skipped at scale != 100 because perspective-scaled masks need
         * additional plumbing (none of the walk-behind .msk files we've
         * inspected so far carry a scale flag, so this is a safe skip). */
        extern void  *g_scene_bg_raw;
        extern uint32_t g_scene_bg_size;
        if (atlas->kind == 0 && (atlas->flag_22 & 2) == 0 &&
            g_scene_bg_raw && g_scene_bg_size > 776 &&
            (scale == 0 || scale == 100)) {
            const uint8_t *bg = (const uint8_t *)g_scene_bg_raw;
            uint16_t bg_w = (uint16_t)(bg[4] | (bg[5] << 8));
            uint16_t bg_h = (uint16_t)(bg[6] | (bg[7] << 8));
            const uint8_t *bg_pixels = bg + 776;
            int mask_stride = (fw + 7) / 8;
            int dx_i = (int)dx, dy_i = (int)dy;
            for (int my = 0; my < (int)fh; ++my) {
                int gy = dy_i + my;
                if (gy < 0 || gy >= (int)bg_h) continue;
                if (gy >= (int)g_screen_h) break;
                const uint8_t *row_src = px + (size_t)my * mask_stride;
                const uint8_t *bg_row = bg_pixels + (size_t)gy * bg_w;
                uint8_t *dst_row = g_back_shadow
                                 + (size_t)gy * g_screen_w;
                const uint8_t *paint_row = s_actor_paint_mask
                    ? s_actor_paint_mask + (size_t)gy * g_screen_w : NULL;
                for (int mx = 0; mx < (int)fw; ++mx) {
                    int gx = dx_i + mx;
                    if (gx < 0) continue;
                    if (gx >= (int)bg_w || gx >= (int)g_screen_w) break;
                    /* MSB-first packing (matches .fld convention). */
                    uint8_t bit = (uint8_t)(row_src[mx >> 3]
                                            & (0x80 >> (mx & 7)));
                    if (!bit) continue;
                    /* Walk-behind only affects actors — skip BG restore
                     * if no actor pixel was painted here. */
                    if (paint_row && !paint_row[gx]) continue;
                    dst_row[gx] = bg_row[gx];
                }
            }
            continue;   /* skip normal blit — mask handled. */
        }
        int blit_w, blit_h;
        /* PORT SHORTCUT — perspective-scaled actor blit. Original engine
         * gated this on flag 0x400; our actors don't get that flag, but
         * we want the perspective-shrink effect (sprite shrinks as actor
         * walks into the scene depth, smaller Y). Apply scaling whenever
         * +0x58 != 0 && != 100, AND recompute drawn from anchor +
         * scaled atlas hot-spot using SAME scale as blit — so foot stays
         * at script anchor regardless of frame transitions or scale
         * changes between EntityWalkerTick (uses OLD scale) and
         * UpdateActorMovement (sets NEW scale). Without this recompute,
         * sprite position jumps when scale changes ("animation goes
         * wild during action"). */
        if (scale && scale != 100) {
            uint16_t dw = (uint16_t)((fw * scale) / 100);
            uint16_t dh = (uint16_t)((fh * scale) / 100);
            if (dw == 0) dw = 1;
            if (dh == 0) dh = 1;
            /* Timing-safe foot anchor: override post-exec's pre-computed
             * drawn pos with one based on CURRENT anchor + CURRENT scale
             * × atlas hot-spot. Only for FLAG_2 (foot-anchored) entities. */
            if (EOFF(e, 0x3A, uint16_t) & 2 &&
                atlas->off_drawX && atlas->off_drawY &&
                f < atlas->frame_count) {
                int16_t anchor_x = EOFF(e, 0x22, int16_t);
                int16_t anchor_y = EOFF(e, 0x24, int16_t);
                int16_t hx = (int16_t)atlas->off_drawX[f];
                int16_t hy = (int16_t)atlas->off_drawY[f];
                dx = (int16_t)(anchor_x + ((int32_t)hx * scale) / 100);
                dy = (int16_t)(anchor_y + ((int32_t)hy * scale) / 100);
            }
            if ((flags & 0x100) && !(flags & 0x4)) {
                BlitAlphaScaledToBackbuffer(dx, dy, fw, fh, dw, dh, px, 2);
            } else {
                BlitSpriteScaledColorKeyFlip(dx, dy, fw, fh, dw, dh, px,
                                             (flags & 0x4) ? 1 : 0);
            }
            blit_w = dw; blit_h = dh;
        } else {
            BlitSpriteToBackbuffer((uint16_t)dx, (uint16_t)dy, 0, 0,
                                   fw, fh, fw, fh, px, 0);
            blit_w = fw; blit_h = fh;
        }

        /* Walk-behind paint-mask: mark destination pixels for entities
         * that should be occluded by walk-behind masks. Two cases:
         *   1. Actors (g_actor[0/1]) — always marked. Ebek/Fjej walk
         *      behind buildings via the standard walk-behind effect.
         *   2. "Sky" entities — those whose entire sprite sits ABOVE
         *      the walkable floor (entity_bottom < walk_top). These are
         *      flying things (airplane, ufo, witch, birds) that should
         *      be occluded by tall buildings between camera and sky.
         *
         * Static foreground props (banan on dumpster, kioskar1 worker)
         * have foot_y INSIDE or BELOW the walk area — not marked, so
         * walk-behind mask doesn't erase them (Fix #30 use-case still
         * preserved without the actor-only restriction).
         *
         * Fix #43 — sky gate now requires BOTH geometric position AND
         * spawn flag 0x2000 (set on genuine sky entities like
         * samolot3/ufo via op 0x30 SPAWN; cleared on foreground props).
         * Without the flag check, foreground props that happen to be
         * positioned above the walk area (e.g. foto.wyc camera lifted
         * to (356, 18) during use-camera anim in foto3.pic) get
         * incorrectly painted over by foto3.msk's walk-behind blit —
         * the lifted camera disappears, leaving the BG-baked photo-
         * machine visible. The combined gate still excludes HUD glue
         * (e1lataje carries 0x2000 too but lives at y>300 so
         * entity_bottom_y > walk_fld_oy = 304 in stage 2). */
        int is_actor = (e == g_actor[0] || e == g_actor[1]);
        extern int16_t g_walk_fld_oy;       /* top Y of walkable area */
        int entity_bottom_y = dy + blit_h;
        int is_sky = (g_walk_fld_oy > 0 && entity_bottom_y <= (int)g_walk_fld_oy
                      && (flags & 0x2000));
        if (s_actor_paint_mask && (is_actor || is_sky)) {
            int x0 = dx, y0 = dy;
            int x1 = dx + blit_w, y1 = dy + blit_h;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > (int)g_screen_w) x1 = g_screen_w;
            if (y1 > (int)g_screen_h) y1 = g_screen_h;
            for (int yy = y0; yy < y1; ++yy) {
                uint8_t *m = s_actor_paint_mask + (size_t)yy * g_screen_w;
                for (int xx = x0; xx < x1; ++xx) {
                    m[xx] = 1;
                }
            }
        }
    }
}

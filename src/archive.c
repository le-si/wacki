/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Mateusz Szuła
 *
 * src/archive.c — Cygert "BASE_IO_CPP" archive (.dta) handling.
 *
 * .DTA archive layout:
 *
 *   +0           u32 magic = 'BASE'
 *   +4..N        payload — for each file: DtaFileHeader + PKv2-
 *                compressed bytes
 *   N..end-8     SPIS index — a PKv2-compressed array of
 *                DtaIndexEntry records
 *   end-8        u32 magic = 'SPIS'
 *   end-4        u32 spis_size = offset from EOF back to the SPIS
 *                magic
 *
 * OpenDtaArchiveFile loads + depacks the SPIS index into s_dir[].
 * LoadFileFromDta does an UPPER-cased linear name lookup and depacks
 * the matching file in-place; the caller owns the returned buffer
 * (free via xfree). */

#include "wacki.h"
#include "wacki/log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- constants ---------------------------------------------------- */

/* DTA path buffer size + a sanity ceiling on the SPIS index size so a
 * corrupt header doesn't request a multi-GB allocation. */
#define DTA_PATH_BYTES                      260
#define SPIS_MAX_SIZE_BYTES                 (32u * 1024u * 1024u)

/* Each DtaIndexEntry is 16 bytes (= 4-byte file-offset + 12-byte
 * NUL-padded name) — the depacked SPIS length divided by this gives
 * the entry count. */
#define DTA_INDEX_ENTRY_BYTES               16
#define DTA_INDEX_ENTRY_LOG2                4

/* SPIS trailer layout. The trailing u32 stores the distance from
 * EOF-4 back to the SPIS magic; ADD this to the magic offset to find
 * the start of the embedded PKv2 stream. */
#define SPIS_FOOTER_SIZE_BYTES              4
#define SPIS_HEADER_BYTES                   16     /* 4 × u32 */

/* Lower-to-upper-case ASCII fold mask (a..z → A..Z). */
#define ASCII_UPPERCASE_MASK                0xDF

/* ---- module state ----------------------------------------------- */

/* s_dir + s_dir_count are non-static so the standalone extractor in
 * tools/dta-extract.c can walk the directory after OpenDtaArchive
 * File populates it. The path buffer is module-local — only one
 * archive is mounted at a time. */
static char        s_dta_path[DTA_PATH_BYTES];
static const char *s_dta_path_p = s_dta_path;
DtaIndexEntry     *s_dir        = NULL;
int32_t            s_dir_count  = 0;

/* The mounted archive is kept OPEN across LoadFileFromDta calls. Re-opening
 * per asset means a file open per load; on slow storage (an SD card, optical
 * media) an open is far heavier than a plain seek+read, so a flood of them
 * during gameplay saturates the I/O path — stalling frames and starving the
 * audio feed (sound loops). Held handle = open once at mount, seek+read per
 * asset. (LoadFileFromDta is only ever called from the game thread, so the
 * shared handle needs no locking.) */
static CygFile    *s_dta_fp     = NULL;

static void dta_cache_clear(void);   /* defined near LoadFileFromDta */

/* ---- helpers ---------------------------------------------------- */

/* macOS / case-sensitive FS fallback: retry with the basename
 * uppercased so files written as `DANE_02.DTA` by the original
 * installer are found even when the caller passes `dane_02.dta`. */
static void uppercase_basename(char *path)
{
    size_t plen = strlen(path);
    size_t cut  = plen;
    while (cut > 0 && path[cut - 1] != '/' && path[cut - 1] != '\\') --cut;
    for (size_t i = cut; i < plen; ++i) {
        char c = path[i];
        if (c >= 'a' && c <= 'z') path[i] = (char)(c - 32);
    }
}

/* Open the .dta file, retrying once with the basename uppercased for
 * case-sensitive filesystems. */
static CygFile *open_dta_with_case_fallback(void)
{
    CygFile *f = fopen_cyg(s_dta_path_p, "rb");
    if (f) return f;
    uppercase_basename(s_dta_path);
    return fopen_cyg(s_dta_path_p, "rb");
}

/* Verify the BASE magic at file offset 0. Returns 1 on match, 0
 * otherwise (logs the failure). */
static int verify_base_magic(CygFile *f, const char *path)
{
    uint32_t magic = 0;
    fread_cyg(&magic, 1, 4, f);
    if (magic == DTA_MAGIC_BASE) return 1;
    LOG_INFO("log", "%s: bad BASE magic 0x%08X", path, magic);
    return 0;
}

/* Read the SPIS header (4×u32 starting at EOF-spis_size-8). Returns 1
 * on success; writes *out_comp + *out_unp from the embedded PKv2 size
 * fields. */
static int read_spis_header(CygFile *f, int32_t spis_size,
                            const char *path,
                            uint32_t *out_comp, uint32_t *out_unp)
{
    uint32_t spis_hdr[SPIS_HEADER_BYTES / 4] = {0};
    fseek_cyg(f, -(spis_size + (int32_t)SPIS_FOOTER_SIZE_BYTES
                              + (int32_t)SPIS_FOOTER_SIZE_BYTES), SEEK_END);
    fread_cyg(spis_hdr, 1, sizeof spis_hdr, f);

    if (spis_hdr[0] != DTA_MAGIC_SPIS) {
        LOG_INFO("log", "%s: bad SPIS magic 0x%08X at -%d", path, spis_hdr[0], spis_size + (int32_t)SPIS_FOOTER_SIZE_BYTES + 4);
        return 0;
    }
    *out_comp = spis_hdr[2];
    *out_unp  = spis_hdr[3];
    return 1;
}

/* Upper-case the 12-byte DTA lookup key in place (a..z → A..Z). */
static void dta_key_upper(char *key)
{
    for (int i = 0; i < DTA_NAME_LEN && key[i]; ++i) {
        if (key[i] >= 'a' && key[i] <= 'z') key[i] &= ASCII_UPPERCASE_MASK;
    }
}

/* Linear search for `key` in the loaded directory. Returns the
 * matching entry's file_offset, or -1 if not found. */
static int32_t dta_find_offset(const char *key)
{
    for (int32_t i = 0; i < s_dir_count; ++i) {
        if (memcmp(s_dir[i].name, key, DTA_NAME_LEN) == 0) {
            return (int32_t)s_dir[i].file_offset;
        }
    }
    return -1;
}

/* ---- public API ------------------------------------------------- */

int OpenDtaArchiveFile(const char *path)
{
    /* Drop the previously mounted archive's held handle (stage change). */
    if (s_dta_fp) { fclose_cyg(s_dta_fp); s_dta_fp = NULL; }
    dta_cache_clear();   /* a name maps to different data per archive */

    s_dta_path_p = s_dta_path;
    snprintf(s_dta_path, sizeof s_dta_path, "%s", path);
    s_dir_count = 0;

    CygFile *f = open_dta_with_case_fallback();
    if (!f) return 0;                       /* silent — caller retries */

    if (!verify_base_magic(f, path)) { fclose_cyg(f); return 0; }

    /* Distance from EOF-4 back to the SPIS magic. */
    int32_t spis_size = 0;
    fseek_cyg(f, -(int32_t)SPIS_FOOTER_SIZE_BYTES, SEEK_END);
    fread_cyg(&spis_size, 1, SPIS_FOOTER_SIZE_BYTES, f);

    uint32_t comp = 0, unp = 0;
    if (!read_spis_header(f, spis_size, path, &comp, &unp)) {
        fclose_cyg(f);
        return 0;
    }
    uint32_t cap = comp > unp ? comp : unp;
    if (!cap || cap > SPIS_MAX_SIZE_BYTES) {
        LOG_INFO("log", "%s: SPIS size %u/%u out of range", path, comp, unp);
        fclose_cyg(f);
        return 0;
    }

    /* Read the PKv2 stream (starts right after the SPIS magic word). */
    s_dir = (DtaIndexEntry *)xmalloc(cap);
    if (!s_dir) { fclose_cyg(f); return 0; }
    fseek_cyg(f, -(spis_size + (int32_t)SPIS_FOOTER_SIZE_BYTES), SEEK_END);
    fread_cyg(s_dir, 1, comp, f);
    s_dta_fp = f;                /* keep open for per-asset reads */

    DepackPkv2Buffer(s_dir, s_dir, NULL);
    s_dir_count = (int32_t)(unp >> DTA_INDEX_ENTRY_LOG2);
    LOG_TRACE("archive", "%s mounted (%d entries)", path, s_dir_count);
    return 1;
}

/* ---- small-asset RAM cache -------------------------------------- *
 *
 * The game re-loads the same small assets (sounds, anim frames) over and
 * over. On a desktop the OS file cache hides that; off slow storage (an SD
 * card, optical media) every reload is a real seek+read that stutters the
 * game. Cache the depacked bytes of small assets so repeats are served from
 * RAM. Cleared on archive (stage) change since a name can map to different
 * data in a different archive. Large assets (backgrounds) are skipped —
 * they're loaded once per scene, not in a loop, and would just evict the
 * hot small entries.
 *
 * PINNED entries (DtaPreload) bypass the size cap and are never LRU-
 * evicted — used to preload menu cinematics so clicking Maluch/bomba/film
 * plays instantly instead of stalling on a load. They're freed on level entry
 * (DtaClearPreloads, since nothing re-opens the archive in-game to clear
 * them) so they don't linger in RAM all session — plus on archive change
 * like the rest.
 *
 * ENTRIES is shared by the pinned + LRU tiers: the menu pins ~10, leaving
 * the rest for the LRU; in-game the pins are cleared so all slots are LRU. */
#define DTA_CACHE_ENTRIES   24
#define DTA_CACHE_MAX_ENTRY (64 * 1024)

typedef struct {
    char     name[DTA_NAME_LEN];   /* uppercased key; data==NULL = empty */
    void    *data;                 /* depacked copy */
    uint32_t size;
    uint32_t lru;
    int      pinned;               /* preloaded: any size, never evicted */
} DtaCacheEntry;

static DtaCacheEntry s_cache[DTA_CACHE_ENTRIES];
static uint32_t      s_cache_tick = 0;

static void dta_cache_clear(void)
{
    for (int i = 0; i < DTA_CACHE_ENTRIES; ++i) {
        if (s_cache[i].data) xfree(s_cache[i].data);
        s_cache[i].data = NULL;
        s_cache[i].name[0] = 0;
        s_cache[i].pinned = 0;
    }
}

static const void *dta_cache_get(const char *key, uint32_t *size)
{
    for (int i = 0; i < DTA_CACHE_ENTRIES; ++i) {
        if (s_cache[i].data && memcmp(s_cache[i].name, key, DTA_NAME_LEN) == 0) {
            s_cache[i].lru = ++s_cache_tick;
            *size = s_cache[i].size;
            return s_cache[i].data;
        }
    }
    return NULL;
}

static void dta_cache_put(const char *key, const void *data, uint32_t size, int pin)
{
    if (size == 0) return;
    if (!pin && size > DTA_CACHE_MAX_ENTRY) return;   /* pinned bypasses the cap */

    /* Already present? Keep its (immutable) data; just pin/refresh it. This
     * dedups when DtaPreload upgrades an entry LoadFileFromDta already
     * cached, and lets DtaPreload pin with data==NULL. */
    for (int i = 0; i < DTA_CACHE_ENTRIES; ++i) {
        if (s_cache[i].data && memcmp(s_cache[i].name, key, DTA_NAME_LEN) == 0) {
            if (pin) s_cache[i].pinned = 1;
            s_cache[i].lru = ++s_cache_tick;
            return;
        }
    }
    if (!data) return;

    /* Pick an empty slot, else the LRU non-pinned one. */
    int slot = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < DTA_CACHE_ENTRIES; ++i) {
        if (!s_cache[i].data) { slot = i; break; }
        if (!s_cache[i].pinned && s_cache[i].lru < oldest) { oldest = s_cache[i].lru; slot = i; }
    }
    if (slot < 0) return;                  /* all slots pinned — nothing to evict */

    void *copy = xmalloc(size);
    if (!copy) return;
    memcpy(copy, data, size);
    if (s_cache[slot].data) xfree(s_cache[slot].data);
    memcpy(s_cache[slot].name, key, DTA_NAME_LEN);
    s_cache[slot].data   = copy;
    s_cache[slot].size   = size;
    s_cache[slot].lru    = ++s_cache_tick;
    s_cache[slot].pinned = pin;
}

int LoadFileFromDta(const char *name, void **out_buf, uint32_t *out_size)
{
    char key[DTA_NAME_LEN];
    memset(key, 0, sizeof key);
    strncpy(key, name, sizeof key);
    dta_key_upper(key);

    /* Cache hit → hand back a fresh copy (caller owns + xfree()s it),
     * skipping the file read + depack entirely. */
    uint32_t csize = 0;
    const void *cached = dta_cache_get(key, &csize);
    if (cached) {
        void *copy = xmalloc(csize);
        if (copy) {
            memcpy(copy, cached, csize);
            *out_buf  = copy;
            *out_size = csize;
            return 1;
        }
    }

    int32_t off = dta_find_offset(key);
    if (off <= 0) {
        LOG_INFO("log", "*** Brak takiego pliku w bazie %s", name);
        return 0;
    }

    CygFile *f = s_dta_fp;       /* archive stays open across loads */
    if (!f) return 0;

    fseek_cyg(f, off, SEEK_SET);
    DtaFileHeader hdr;
    fread_cyg(&hdr, 1, sizeof hdr, f);

    uint32_t cap = hdr.compressed_size > hdr.unpacked_size
                 ? hdr.compressed_size : hdr.unpacked_size;
    void *buf = xmalloc(cap);
    if (!buf) return 0;

    /* Re-read INCLUDING the header — DepackPkv2Buffer expects the full
     * PKv2 stream (magic + sizes + payload). */
    fseek_cyg(f, off, SEEK_SET);
    fread_cyg(buf, 1, hdr.compressed_size, f);

    DepackPkv2Buffer(buf, buf, NULL);

    *out_buf  = buf;
    *out_size = hdr.unpacked_size;
    dta_cache_put(key, buf, hdr.unpacked_size, 0);   /* serve repeats from RAM */
    return 1;
}

/* ---- DtaPreload --------------------------------------------------- *
 *
 * Load an asset now and PIN it in the cache (any size, never evicted) so a
 * later LoadFileFromDta / PlayMenuMusic for it is a RAM hit, not a file
 * read. Used to warm the menu cinematics (Maluch/bomba/film animations +
 * their SFX) at menu entry so triggering them doesn't hitch on a load.
 * Idempotent; the pin is dropped on the next archive (stage) change. */
int DtaPreload(const char *name)
{
    char key[DTA_NAME_LEN];
    memset(key, 0, sizeof key);
    strncpy(key, name, sizeof key);
    dta_key_upper(key);

    uint32_t csz = 0;
    if (dta_cache_get(key, &csz)) {          /* already loaded → just pin it */
        dta_cache_put(key, NULL, csz, 1);
        return 1;
    }

    void *buf = NULL;
    uint32_t sz = 0;
    if (!LoadFileFromDta(name, &buf, &sz)) return 0;   /* reads (+ may cache) */
    dta_cache_put(key, buf, sz, 1);          /* pin (upgrades or inserts) */
    xfree(buf);
    return 1;
}

/* ---- DtaClearPreloads -------------------------------------------- *
 *
 * Free only the pinned entries, leaving the LRU cache warm. The whole game
 * runs off one mounted archive (Dane_02.dta) — nothing re-opens it after
 * boot, so the full dta_cache_clear() never fires again and the menu
 * cinematics, pinned for the title screen but useless in-game, would
 * otherwise sit in RAM the entire session. LoadStage calls this on level
 * entry to reclaim that RAM without dropping the gameplay LRU. */
void DtaClearPreloads(void)
{
    int freed = 0;
    for (int i = 0; i < DTA_CACHE_ENTRIES; ++i) {
        if (s_cache[i].data && s_cache[i].pinned) {
            xfree(s_cache[i].data);
            s_cache[i].data    = NULL;
            s_cache[i].name[0] = 0;
            s_cache[i].pinned  = 0;
            ++freed;
        }
    }
    if (freed) LOG_INFO("init", "freed %d pinned preloads on level entry", freed);
}

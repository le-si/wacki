# Wacki binary asset formats

Spec for the data files shipped on the Wacki CD: container archive
(`Dane_NN.dta`), compression (`PKv2`), and the individual asset types
(`ANIM`, `MASK`, `FILD`, `PIC`, `PAL`).

All values are little-endian, all coordinates are 16-bit, all pixel
data is 8-bit paletted (palette in `paleta.pal` or per-stage variant).

---

## 1. Container — `Dane_NN.dta`

A DTA archive holds many compressed assets in a single file with a
directory at the end. Two magic markers delimit the directory:

```
+-----------+-------------------------+
| BASE      | payload (PKv2 blobs)    |
| magic     |                         |
| dword 0   |                         |
| dword 0   |                         |
+-----------+-------------------------+
| SPIS      | directory               |
| magic     | entry[count]            |
| count     |                         |
+-----------+-------------------------+
```

### 1.1 Header (12 bytes at offset 0)
| Off | Size | Field    | Notes                       |
|----:|-----:|----------|-----------------------------|
|   0 | 4    | `magic`  | `'BASE'` = 0x45534142       |
|   4 | 4    | `compressed_size`   | size of `BASE` payload region |
|   8 | 4    | `unpacked_size`     | (informational; 0 in disk samples) |

After the header, the payload region is a sequence of PKv2-compressed
blobs concatenated back-to-back. Each entry's offset is stored in
the directory.

### 1.2 Directory (`SPIS`)
At the end of the file:

| Off | Size | Field        | Notes                              |
|----:|-----:|--------------|------------------------------------|
|   0 | 4    | `magic`      | `'SPIS'` = 0x53495053              |
|   4 | 4    | `count`      | number of directory entries (1782 in Dane_02) |
|   8 | 16×N | `entry[N]`   | filename + file offset per entry   |

Each directory entry is 16 bytes:

| Off | Size | Field        | Notes                              |
|----:|-----:|--------------|------------------------------------|
|   0 | 12   | `name`       | nul-padded ASCII (DOS 8.3-ish, e.g. `EBEK.WYC`)|
|  12 | 4    | `file_offset`| absolute byte offset of entry in archive |

The port treats names case-insensitively (case-folds at lookup time),
since macOS ISO9660 mounts can present mixed case.

### 1.3 Lookup flow (port code: `src/archive.c`)
```c
OpenDtaArchiveFile(path)  → mmap or fread the whole archive
LoadFileFromDta(name, &raw, &sz):
    walk dir, case-fold compare name
    seek file_offset, read PKv2 header, DepackPkv2Buffer → raw
```

---

## 2. Compression — `PKv2`

LZ77 + Huffman-style prefix codes. Each file entry inside a DTA
starts with a 12-byte PKv2 header:

| Off | Size | Field              |
|----:|-----:|--------------------|
|   0 | 4    | `magic` = 'PKv2'   |
|   4 | 4    | `compressed_size`  |
|   8 | 4    | `unpacked_size`    |

The compressed stream follows. Port decoder lives in `src/depack.c`
(`DepackPkv2Buffer`); standalone tool `tools/pkv2-depack.c` decompresses
a single blob.

The decoder reads two state bytes near the end of the compressed buffer
(bit-stream count + buf at offsets `eot-0x19` / `eot-0x1A`) then a u32
initial-literal length at `eot-0x1E`. It walks the stream backward in
bit terms while writing the output forward. The literal/match codes
use variable-length prefixes whose tables są zrekonstruowane z
oryginalnego dekodera.

**Port status:** byte-perfect. `tools/dta-validate.sh` weryfikuje
wszystkie 1782 wpisy z `DANE_02.DTA` przeciwko
`tools/dta-baseline.sha256` i przechodzi. Historyczne wzmianki
o "off-by-X approximations" w starszych wersjach dokumentacji
są przestarzałe.

---

## 3. ANIM atlas — `.wyc` (`ASSET_MAGIC_ANIM = 'ANIM'`)

Sprite atlases (actors, props, masks). The header is 16 bytes, then
several parallel arrays follow, then the pixel blobs:

| Off | Size | Field                       |
|----:|-----:|-----------------------------|
|   0 | 4    | `magic` = 'ANIM' (0x4D494E41) |
|   4 | 4    | (reserved)                  |
|   8 | 2    | `frame_count`               |
|  10 | 2    | (reserved)                  |
|  12 | 2    | flag_22 (alpha-plane / 8bpp click select bits) |
|  14 | 2    | offset to pixel-offset table (relative to file start) |

After the header, three uint16 arrays of length `frame_count`:
- `off_widths[i]`   — frame width in pixels
- `off_heights[i]`  — frame height in pixels
- `off_drawX[i]`    — draw-offset X (foot-anchor compensation)
- `off_drawY[i]`    — draw-offset Y

Then a `uint32_t pix_off_arr[frame_count]` of byte offsets (relative
to file start) for each frame's pixel data. The pixel data itself is
either raw 8bpp (`kind=2`) or RLE-compressed (`kind=3`).

### 3.1 RLE encoding (`kind=3`)
Header is 3 bytes:
| Off | Size | Field            | Notes |
|----:|-----:|------------------|-------|
|   0 | 1    | `fill_value`     | usually 0 = transparent |
|   1 | 1    | `marker_A`       | byte introducing run-of-fill |
|   2 | 1    | `marker_B`       | byte introducing run-of-arbitrary-value |

Stream rules:
- `byte == marker_A` → next byte = count-1; emit `count` × `fill_value`
- `byte == marker_B` → next byte = count-1; next byte = value; emit `count` × value
- otherwise → emit the byte once

Decoder: `DepackRleFrame` in `src/graphics.c`.

### 3.2 `flag_22` bits
| Bit | Meaning                                          |
|----:|--------------------------------------------------|
| 0   | alpha-plane source (use RGB12 quantization blit) |
| 1   | 8bpp click test (vs 1bpp packed)                 |

Routed by:
- entity flag 0x100 (`actor.c:1050`) → BlitAlphaScaled mode 2
- click-mask flag at entity[+0x14] (`stubs.c:1789+`)

---

## 4. MASK file — `.msk` (`ASSET_MAGIC_MASK = 'MASK'`)

Click-region masks. Same overall structure as ANIM (header + per-frame
metadata + pixel blobs) but with 1-bpp packed pixel data — `(w+7) & ~7
/ 8` bytes per row. Used for hit-testing (`ClickHitTest` switch on
flag_22 bit at entity[+0x14]).

---

## 5. FILD walkability — `.fld` (`ASSET_MAGIC_FILD = 'FILD'`)

Per-room walkability bitmap. Single frame, 1-bpp packed, sized to the
walkable region of the background. Header records origin (`ox`, `oy`)
and stride for partial-bitmap overlays.

| Off | Size | Field        | Notes                                 |
|----:|-----:|--------------|---------------------------------------|
|   0 | 4    | `magic`      | 'FILD' (0x444C4946)                   |
|   4 | 2    | `w`          | bitmap width in pixels                |
|   6 | 2    | `h`          | bitmap height                         |
|   8 | 2    | `ox`         | origin X on background                |
|  10 | 2    | `oy`         | origin Y on background                |
|  12 | 2    | `stride`     | bytes per row (`= (w+7)/8`)           |
|  14 | ...  | pixels       | 1-bpp packed, LSB = leftmost          |

Bit set → walkable. Read by `is_walkable_at(x,y)` in `src/game.c`.

---

## 6. PIC backgrounds — `.pic`

Static room backgrounds, 8-bpp paletted, full screen-size (640×480).
Format (port: `paint_rawb_pic` consumer):

| Off  | Size | Field    | Notes                            |
|-----:|-----:|----------|----------------------------------|
|    0 | 2    | `w`      | width  (typically 640)           |
|    2 | 2    | `h`      | height (typically 480)           |
|    4 | 0x304| header   | misc flags + padding             |
|  0x308 | w*h | pixels   | flat 8bpp                        |

The 0x308 prefix is treated as opaque metadata by the port (consumed
by `paint_rawb_pic` which only reads pixels from offset 0x308).

---

## 7. PAL palette — `.pal`

Flat 256×3 BGR triplets (768 bytes total). Loaded directly into
`g_palette_rgb` and applied via `InstallPalette` which also rebuilds
the RGB12 quantization LUTs for alpha-plane rendering (T7).

```
+--- byte 0 ---+--- byte 1 ---+--- byte 2 ---+ ...
| B[0]         | G[0]         | R[0]         | B[1] ...
```

The original engine occasionally fades-loads a partial palette (rows
N..N+M only); this maps to `InstallPalette(rgb, first)` where `first`
selects the starting index.

---

## 8. Futura.30 bitmap font

Custom bitmap font. Loaded once at startup (`PreloadCommonAssets`),
parsed by `ParseFutFontFile` (`src/font.c`). The format is a Big-
Endian header + per-glyph descriptor array + glyph bitmaps (1-bpp
packed for system text, color-plane for menu text).

Used by:
- `RenderTextLineToBuffer` for op 0x09 PRINT and dialog speech
- `MeasureTextLine` for layout

Glyph table is the standard Mazovia ASCII range used in Polish DOS-era
games (codes 0x80+).

---

## Reference implementations

| Format | Reader                              | Writer (tool)              |
|--------|-------------------------------------|----------------------------|
| DTA    | `src/archive.c`                     | `tools/dta-extract.c`      |
| PKv2   | `src/depack.c`                      | `tools/pkv2-depack.c`      |
| ANIM   | `src/assets.c` (`LoadAssetFromDtaBase`) | — (game-time decoder)  |
| MASK   | `src/assets.c` + `ClickHitTest`     | — |
| FILD   | `src/assets.c` + `is_walkable_at`   | — |
| PIC    | `paint_rawb_pic` in `src/game.c`    | — |
| PAL    | `InstallPalette` in `src/graphics.c`| — |
| Font   | `src/font.c`                        | — |

For binary-faithful asset round-tripping (e.g. extracting frames to
PNG, repacking), build the standalone tools:

```
make tools
./dta-extract /Volumes/WACKI_1/Dane_02.dta out/
./pkv2-depack out/EBEK.WYC ebek.raw
```

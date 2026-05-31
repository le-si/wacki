/* include/wacki.h — umbrella header for the reconstructed Wacki engine.
 *
 * The engine is built around a tiny platform layer (platform_sdl.c
 * on Mac/Linux/Win, or a Win32-DirectDraw layer that could be added
 * later). Function and field names match the RE'd binary.
 *
 * The contents are split across three sibling headers — pull this
 * umbrella in and you get all three:
 *
 *   wacki/types.h    structs, enums, format magic numbers
 *   wacki/api.h      module function declarations
 *   wacki/globals.h  extern globals + macro aliases (g_script_vars
 *                    aliases for g_game_over_code / g_completed_stages,
 *                    etc.)
 *
 * Source modules can keep writing `#include "wacki.h"` and need not
 * care about the split. Code that wants a tighter dependency surface
 * (e.g. a header that only needs the type definitions) may include
 * a specific sibling directly. */

#ifndef WACKI_H
#define WACKI_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Optional Win32 path — legacy DirectDraw/DirectSound layer kept
 * compilable for archaeology. Gated behind WACKI_WITH_WIN32; the
 * SDL2 build never sees these headers. */
#if defined(WACKI_WITH_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <ddraw.h>
#  include <dsound.h>
#  include <mmsystem.h>
#endif

#include "wacki/types.h"
#include "wacki/api.h"
#include "wacki/globals.h"

#endif /* WACKI_H */

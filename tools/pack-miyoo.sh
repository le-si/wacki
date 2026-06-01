#!/usr/bin/env bash
# Stage the Miyoo build into an OnionOS / MinUI-compatible .pak.
#
# Output layout (zipped):
#
#   Wacki.pak/
#       launch.sh        wrapper: chmods + execs the binary
#       wacki            the cross-built engine
#       data/            game assets (.dta, fonts, etc. — caller-provided)
#       Wacki.png        in-launcher icon (optional)
#
# The .pak drops under SD:/App/ on stock firmware or SD:/Apps/ under
# OnionOS — both pick it up automatically. Game data is NOT bundled
# by this script (copyright reasons) — the player drops their own
# DTA files into the pak's data/ directory.
#
# Usage:  ./tools/pack-miyoo.sh [output.zip]
#         Default output: dist/wacki-miyoo.zip

set -euo pipefail

cd "$(dirname "$0")/.."

bin=dist/wacki-miyoo
if [ ! -f "$bin" ]; then
    echo "error: $bin not built — run 'make miyoo' first." >&2
    exit 1
fi

out="${1:-dist/wacki-miyoo.zip}"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

pak="$stage/Wacki.pak"
mkdir -p "$pak/data"

cp "$bin" "$pak/wacki"
chmod +x "$pak/wacki"

# launch.sh: OnionOS sets PWD to the .pak when launching. The engine
# discovers data/ adjacent to the binary via SDL_GetBasePath, so no
# extra path wiring is needed. SDL_VIDEODRIVER is forced to mali
# because OnionOS's SDL2 build prefers the Mali GLES path; without
# the explicit hint it sometimes falls back to a broken fbdev probe.
cat > "$pak/launch.sh" <<'LAUNCH'
#!/bin/sh
cd "$(dirname "$0")"
export HOME="$(pwd)"
export SDL_VIDEODRIVER=mali
export SDL_AUDIODRIVER=alsa
./wacki >> wacki.log 2>&1
LAUNCH
chmod +x "$pak/launch.sh"

# Optional icon — drop one in assets/icons/wacki-miyoo.png to embed.
if [ -f assets/icons/wacki-miyoo.png ]; then
    cp assets/icons/wacki-miyoo.png "$pak/Wacki.png"
fi

cat > "$pak/README.txt" <<'README'
Wacki — Miyoo Mini Plus port.

Place your DTA files in:    data/
Then drop this entire Wacki.pak directory under:
    /App/        (stock firmware)
or:
    /Apps/       (OnionOS)

D-pad moves the cursor. A = LMB, B = RMB. Start = pause (F12).
Hold any d-pad direction to accelerate; release to reset.
README

# zip up — preserves permissions (-X strips macOS metadata)
mkdir -p "$(dirname "$out")"
(cd "$stage" && zip -rXq "$OLDPWD/$out" Wacki.pak)

echo "[miyoo] packed → $out"
ls -lh "$out"

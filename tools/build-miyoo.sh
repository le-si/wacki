#!/usr/bin/env bash
# Cross-build the engine for Miyoo Mini Plus (and pin-compatible
# SigmaStar SSD20x handhelds — Anbernic RG35XX, Powkiddy RGB30, etc.)
# by running the host Makefile inside a Miyoo cross-toolchain Docker
# image that ships SDL2 in the buildroot sysroot.
#
# Local usage:    ./tools/build-miyoo.sh
# CI usage:       same — invoked by `make miyoo` and the .github
#                 workflow's Miyoo leg.
#
# Override the image tag with MIYOO_DOCKER_IMAGE if you fork.
# Default image: bqcuongas/sdl2-miyoo (has buildroot toolchain at
# /opt/mmiyoo and SDL2 in its sysroot).

set -euo pipefail

IMAGE="${MIYOO_DOCKER_IMAGE:-bqcuongas/sdl2-miyoo:latest}"

cd "$(dirname "$0")/.."

if [ ! -f data/WACKI.EXE ]; then
    echo "error: data/WACKI.EXE missing — required for the embedded PE blob." >&2
    echo "       Drop the file in ./data/ before building." >&2
    exit 1
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker not installed — install Docker Desktop / docker.io first." >&2
    exit 1
fi

# Resolve the version string on the HOST, where git is installed and the
# repo is trusted. Inside the container git is either absent or refuses
# the host-owned checkout ("dubious ownership"), so a git describe there
# silently falls back to "unknown" — which is exactly what shipped on the
# menu. Pass the resolved value in via -e so the Makefile's `?=` picks it
# up instead of re-running git. (Mirrors tools/build-portmaster.sh.)
VER="$(git describe --tags --always --dirty 2>/dev/null || echo unknown)"
echo "[miyoo] version: $VER"

echo "[miyoo] using image: $IMAGE"
docker pull --platform linux/amd64 "$IMAGE"

# If ./data is a symlink (common local-dev setup that keeps the
# proprietary game data outside the source repo), Docker won't follow
# the link into the host — the symlink stays inside the mount but its
# target sits outside. Mount the resolved target as an extra volume.
extra_mounts=()
if [ -L data ]; then
    data_target="$(cd "$(dirname "$(readlink data)")" && pwd)/$(basename "$(readlink data)")"
    if [ -d "$data_target" ]; then
        extra_mounts+=(-v "$data_target:$data_target")
        echo "[miyoo] data/ is a symlink — mounting target $data_target"
    fi
fi

# Run the unchanged Makefile inside the container. Layout used:
#   /opt/mmiyoo/bin/arm-linux-gnueabihf-gcc                — compiler
#   /opt/mmiyoo/arm-buildroot-linux-gnueabihf/sysroot/...  — sysroot
#       usr/bin/sdl2-config                                 — config tool
#       usr/lib/libSDL2-2.0.so.0                            — runtime lib
docker run --rm --platform linux/amd64 \
    -e "WACKI_VERSION=$VER" \
    -v "$(pwd):/root/workspace" \
    -w /root/workspace \
    "${extra_mounts[@]}" \
    "$IMAGE" \
    bash -c '
        set -euo pipefail
        export PATH=/opt/mmiyoo/bin:/opt/mmiyoo/arm-buildroot-linux-gnueabihf/sysroot/usr/bin:$PATH
        # Wipe any host-built artefacts so the cross-build doesnt
        # link against x86_64 .o files left over from a prior make.
        rm -rf dist src/embedded_wacki_pe.c
        make TARGET=miyoo \
             CROSS_COMPILE=arm-linux-gnueabihf- \
             SDL2_CFG=/opt/mmiyoo/arm-buildroot-linux-gnueabihf/sysroot/usr/bin/sdl2-config \
             engine
    '

bin=dist/wacki-miyoo
if [ ! -f "$bin" ]; then
    echo "error: $bin not produced" >&2
    exit 1
fi

echo "[miyoo] built $bin"
file "$bin" 2>/dev/null || true
ls -lh "$bin"

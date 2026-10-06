#!/usr/bin/env bash
#
# Build a private 64-bit virglrenderer/Venus server. Nothing is installed
# system-wide; the 32-bit client is provided by Ubuntu's Mesa i386 package.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="${L4D2_VIRGL_VERSION:-virglrenderer-1.3.0}"
BRIDGE_HOME="${L4D2_VENUS_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/l4d2-venus}"
SRC="$BRIDGE_HOME/src/virglrenderer"
BUILD="$BRIDGE_HOME/build"
DEPS="$BRIDGE_HOME/deps"
DOWNLOADS="$BRIDGE_HOME/downloads"
MESON_ROOT="$DEPS/meson"
VULKAN_ROOT="$DEPS/vulkan"
GF_PATCH="$SCRIPT_DIR/patches/0001-venus-prefer-ext-calibrated-timestamps.patch"

say() { printf '[l4d2-venus] %s\n' "$*"; }
die() { printf '[l4d2-venus] ERROR: %s\n' "$*" >&2; exit 1; }

for cmd in apt-get dpkg-deb git ninja patch pkg-config python3; do
    command -v "$cmd" >/dev/null || die "missing command: $cmd"
done

mkdir -p "$DOWNLOADS" "$DEPS" "$(dirname "$SRC")"

MESON_PYTHONPATH=""
if command -v meson >/dev/null; then
    run_meson() { meson "$@"; }
else
    if [ ! -d "$MESON_ROOT/usr/lib/python3/dist-packages/mesonbuild" ]; then
        say "fetching Meson without root access"
        rm -rf "$MESON_ROOT"
        mkdir -p "$MESON_ROOT"
        (
            cd "$DOWNLOADS"
            apt-get download meson
        )
        meson_deb="$(find "$DOWNLOADS" -maxdepth 1 -name 'meson_*.deb' -print | sort | tail -1)"
        [ -n "$meson_deb" ] || die "failed to download Meson"
        dpkg-deb -x "$meson_deb" "$MESON_ROOT"
    fi
    MESON_PYTHONPATH="$MESON_ROOT/usr/lib/python3/dist-packages"
    run_meson() {
        PYTHONPATH="$MESON_PYTHONPATH${PYTHONPATH:+:$PYTHONPATH}" \
            python3 -m mesonbuild.mesonmain "$@"
    }
fi

EXTRA_PKG_CONFIG=""
if ! pkg-config --exists vulkan; then
    if [ ! -f "$VULKAN_ROOT/usr/lib/x86_64-linux-gnu/pkgconfig/vulkan.pc" ]; then
        say "fetching Vulkan headers without root access"
        rm -rf "$VULKAN_ROOT"
        mkdir -p "$VULKAN_ROOT"
        (
            cd "$DOWNLOADS"
            apt-get download libvulkan-dev
        )
        vulkan_deb="$(find "$DOWNLOADS" -maxdepth 1 -name 'libvulkan-dev_*_amd64.deb' -print | sort | tail -1)"
        [ -n "$vulkan_deb" ] || die "failed to download Vulkan headers"
        dpkg-deb -x "$vulkan_deb" "$VULKAN_ROOT"
    fi
    EXTRA_PKG_CONFIG="$VULKAN_ROOT/usr/lib/x86_64-linux-gnu/pkgconfig"
fi

for dep in epoxy libdrm gbm; do
    pkg-config --exists "$dep" || die "missing build dependency: $dep (install its -dev package)"
done

if [ ! -d "$SRC/.git" ]; then
    say "cloning $VERSION"
    git clone --depth 1 --branch "$VERSION" \
        https://gitlab.freedesktop.org/virgl/virglrenderer.git "$SRC"
else
    current="$(git -C "$SRC" describe --tags --exact-match 2>/dev/null || true)"
    [ "$current" = "$VERSION" ] ||
        die "$SRC is $current, expected $VERSION; remove it or set L4D2_VIRGL_VERSION"
fi

if patch -d "$SRC" -p1 --dry-run --forward <"$GF_PATCH" >/dev/null 2>&1; then
    say "applying gf calibrated-timestamps compatibility patch"
    patch -d "$SRC" -p1 --forward <"$GF_PATCH"
elif patch -d "$SRC" -p1 --dry-run --reverse <"$GF_PATCH" >/dev/null 2>&1; then
    say "gf calibrated-timestamps compatibility patch is already applied"
else
    die "cannot apply $GF_PATCH to $VERSION"
fi

say "configuring private 64-bit Venus server"
setup_mode=()
if [ -f "$BUILD/meson-private/coredata.dat" ]; then
    setup_mode=(--wipe)
fi
PKG_CONFIG_PATH="$EXTRA_PKG_CONFIG${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
    run_meson setup "$BUILD" "$SRC" "${setup_mode[@]}" \
        -Dvenus=true \
        -Dplatforms=egl \
        -Dvideo=false \
        -Dtests=false \
        -Dfuzzer=false \
        -Dtracing=none

say "building"
run_meson compile -C "$BUILD"

SERVER="$BUILD/vtest/virgl_test_server"
WORKER="$BUILD/server/virgl_render_server"
[ -x "$SERVER" ] || die "build did not produce $SERVER"
[ -x "$WORKER" ] || die "build did not produce $WORKER"

say "ready"
say "server: $SERVER"
say "next: $(dirname "$0")/run-l4d2-venus.sh --test"

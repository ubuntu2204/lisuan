#!/usr/bin/env bash
#
# Run a 32-bit Vulkan client through Mesa Venus and a private 64-bit server
# which loads the native gf Vulkan ICD.

set -euo pipefail

BRIDGE_HOME="${L4D2_VENUS_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/l4d2-venus}"
BUILD="$BRIDGE_HOME/build"
SERVER="$BUILD/vtest/virgl_test_server"
WORKER="$BUILD/server/virgl_render_server"
GAME="${L4D2_GAME_DIR:-$HOME/.local/share/Steam/steamapps/common/Left 4 Dead 2}"
GF_ICD="${L4D2_GF_ICD:-/usr/share/vulkan/icd.d/gf_icd.x86_64.json}"
VIRTIO_ICD="${L4D2_VIRTIO_ICD:-/usr/share/vulkan/icd.d/virtio_icd.json}"
SOCKET="${XDG_RUNTIME_DIR:-/tmp}/l4d2-venus-$UID.sock"
LOG="${XDG_STATE_HOME:-$HOME/.local/state}/l4d2-venus/server.log"
SERVER_PID=""
GAME_PID=""

say() { printf '[l4d2-venus] %s\n' "$*"; }
die() { printf '[l4d2-venus] ERROR: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage:
  run-l4d2-venus.sh --check
  run-l4d2-venus.sh --test
  run-l4d2-venus.sh [--game] [L4D2 arguments...]

--check  Check the private server, i386 Venus ICD, and gf ICD.
--test   Run a 32-bit vkcube smoke test for several seconds.
--game   Start native 32-bit L4D2 with -vulkan (the default action).
EOF
}

check_files() {
    local failed=0
    command -v setsid >/dev/null || {
        printf '[l4d2-venus] MISSING command: setsid\n' >&2
        failed=1
    }
    for file in "$SERVER" "$WORKER" "$GF_ICD" "$VIRTIO_ICD" \
                /usr/lib/i386-linux-gnu/libvulkan_virtio.so; do
        if [ -e "$file" ]; then
            say "ok: $file"
        else
            printf '[l4d2-venus] MISSING: %s\n' "$file" >&2
            failed=1
        fi
    done
    if [ ! -x "$SERVER" ]; then
        say "build first: $(dirname "$0")/build-venus-bridge.sh"
        failed=1
    fi
    return "$failed"
}

cleanup() {
    if [ -n "$GAME_PID" ] && kill -0 "$GAME_PID" 2>/dev/null; then
        kill -TERM -- "-$GAME_PID" 2>/dev/null || true
        wait "$GAME_PID" 2>/dev/null || true
    fi
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$SOCKET"
}

start_server() {
    check_files >/dev/null || die "bridge prerequisites are incomplete"
    mkdir -p "$(dirname "$LOG")"
    rm -f "$SOCKET"

    say "starting 64-bit Venus server with gf ICD"
    env \
        RENDER_SERVER_EXEC_PATH="$WORKER" \
        VK_ICD_FILENAMES="$GF_ICD" \
        "$SERVER" \
            --no-loop-or-fork \
            --no-virgl \
            --venus \
            --rendernode /dev/dri/renderD128 \
            --socket-path "$SOCKET" >"$LOG" 2>&1 &
    SERVER_PID=$!

    for _ in $(seq 1 100); do
        [ -S "$SOCKET" ] && return 0
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            cat "$LOG" >&2
            die "Venus server exited before creating its socket"
        fi
        sleep 0.05
    done
    die "timed out waiting for $SOCKET"
}

find_vkcube32() {
    local candidate
    for candidate in \
        "$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime/steam-runtime/i386/usr/bin/vkcube" \
        "$HOME/.local/share/Steam/ubuntu12_32/steam-runtime/i386/usr/bin/vkcube"; do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

run_test() {
    local cube output rc
    cube="$(find_vkcube32)" || die "no 32-bit vkcube found in the Steam runtime"
    output="$(mktemp)"
    start_server

    set +e
    timeout 8 env \
        VTEST_SOCKET_NAME="$SOCKET" \
        VN_DEBUG=vtest \
        VK_ICD_FILENAMES="$VIRTIO_ICD" \
        "$cube" >"$output" 2>&1
    rc=$?
    set -e
    cat "$output"

    if grep -q 'Virtio-GPU Venus (LISUAN 7G100' "$output" &&
       { [ "$rc" -eq 0 ] || [ "$rc" -eq 124 ]; }; then
        rm -f "$output"
        say "PASS: 32-bit client is rendered by the 64-bit gf driver"
        return 0
    fi

    rm -f "$output"
    cat "$LOG" >&2
    die "Venus smoke test failed (client status $rc)"
}

run_game() {
    local rc
    [ -x "$GAME/hl2.sh" ] || die "native L4D2 launcher not found: $GAME/hl2.sh"
    start_server
    say "starting native 32-bit L4D2 through 64-bit gf"
    (
        cd "$GAME"
        exec setsid env \
            VTEST_SOCKET_NAME="$SOCKET" \
            VN_DEBUG=vtest \
            VK_ICD_FILENAMES="$VIRTIO_ICD" \
            ./hl2.sh -game left4dead2 -vulkan "$@"
    ) &
    GAME_PID=$!

    set +e
    wait "$GAME_PID"
    rc=$?
    set -e
    GAME_PID=""
    return "$rc"
}

mode="${1:---game}"
case "$mode" in
    --check)
        check_files
        ;;
    --test)
        trap cleanup EXIT INT TERM
        run_test
        ;;
    --game)
        shift || true
        trap cleanup EXIT INT TERM
        run_game "$@"
        ;;
    --help|-h)
        usage
        ;;
    *)
        trap cleanup EXIT INT TERM
        run_game "$@"
        ;;
esac

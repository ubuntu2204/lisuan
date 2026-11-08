#!/usr/bin/env bash
#
# test-dreamina-install.sh — 离线验证即梦 AI 的图标提取、桌面入口生成与回滚。
# 用临时 HOME + 合成 PE（MZ 头 + 内嵌 1x1 PNG）跑 --desktop-only / --status /
# --uninstall，不碰真实桌面、不需要官方安装器、不需要 Proton 或 mingw。
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

INSTALLER="${REPO_ROOT}/dreamina/install-dreamina.sh"
EXTRACTOR="${REPO_ROOT}/dreamina/extract-icon.py"
TMP="${TEST_TMP}/dreamina"
FAKE_HOME="${TMP}/home"
FAKE_PREFIX="${TMP}/prefix"
DATA="${FAKE_HOME}/.local/share"
APP_DIR="${DATA}/applications"
ICON_DIR="${DATA}/icons/hicolor"
DESKTOP_FILE="${FAKE_HOME}/桌面/dreamina-wine.desktop"
APP_ENTRY="${APP_DIR}/dreamina-wine.desktop"
ICON_FILE="${ICON_DIR}/256x256/apps/dreamina.png"
INSTDIR="${FAKE_PREFIX}/drive_c/users/tester/AppData/Local/Programs/dreamina-desktop"
FAKE_EXE="${INSTDIR}/即梦 AI.exe"

mkdir -p "$FAKE_HOME/Desktop" "$FAKE_HOME/桌面" "$APP_DIR" "$INSTDIR/resources"
printf 'asar' > "$INSTDIR/resources/app.asar"

# 合成一个「PE 资源解析失败、但内嵌 PNG 可兜底扫描」的最小 exe。
python3 - "$FAKE_EXE" <<'PY'
import struct, sys, zlib

def chunk(kind, payload):
    return (struct.pack('>I', len(payload)) + kind + payload +
            struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))

# 16x16 RGBA：图标提取器只接受 8..1024 的正方形帧。
side = 16
pixel = b'\x40\x80\xc0\xff'
rows = b''.join(b'\x00' + pixel * side for _ in range(side))
png = (b'\x89PNG\r\n\x1a\n' +
       chunk(b'IHDR', struct.pack('>IIBBBBB', side, side, 8, 6, 0, 0, 0)) +
       chunk(b'IDAT', zlib.compress(rows)) +
       chunk(b'IEND', b''))
with open(sys.argv[1], 'wb') as fh:
    fh.write(b'MZ' + b'\0' * 100 + png + b'\0' * 8)
PY
t_assert_eq 0 $? "synthetic PE fixture created"

export HOME="$FAKE_HOME"
export XDG_DATA_HOME="$DATA"
export DREAMINA_PROTON="${TMP}/proton"
export DREAMINA_WINE11_PREFIX="$FAKE_PREFIX"
export DREAMINA_DESKTOP_FILE="$DESKTOP_FILE"
export DREAMINA_APP_ENTRY="$APP_ENTRY"
export DREAMINA_ICON_ROOT="${DATA}/icons"
export DREAMINA_BACKUP_DIR="${DATA}/dreamina-wine11/desktop-backups"

out="$(bash "$INSTALLER" --help 2>&1)"; rc=$?
if [[ $rc -eq 0 && "$out" == *"--desktop-only"* ]]; then
    t_ok "install-dreamina.sh --help works"
else
    t_fail "install-dreamina.sh --help (rc=$rc)"
fi

bash -n "$INSTALLER" >/dev/null 2>&1
t_assert_eq 0 $? "bash -n install-dreamina.sh"
python3 -c 'import ast, sys; ast.parse(open(sys.argv[1], encoding="utf-8").read())' \
    "$EXTRACTOR" >/dev/null 2>&1
t_assert_eq 0 $? "extract-icon.py parses"

out="$(bash "$INSTALLER" --desktop-only 2>&1)"; rc=$?
if [[ $rc -eq 0 && "$out" == *"[OK] 图标"* ]]; then
    t_ok "--desktop-only extracts icon and reports status"
else
    t_fail "--desktop-only (rc=$rc): $out"
fi

if [[ -f "$ICON_FILE" ]] && \
   [[ "$(head -c8 "$ICON_FILE" | od -An -tx1 | tr -d ' \n')" == "89504e470d0a1a0a" ]]; then
    t_ok "hicolor/256x256/apps/dreamina.png is a valid PNG"
else
    t_fail "hicolor icon missing or invalid: $ICON_FILE"
fi

for f in "$DESKTOP_FILE" "$APP_ENTRY"; do
    if [[ -f "$f" ]] && grep -qxF 'Icon=dreamina' "$f" && \
       grep -qF 'run-dreamina.sh' "$f" && ! grep -qi 'doubao' "$f"; then
        t_ok "desktop entry points to dreamina icon: $(basename "$f")"
    else
        t_fail "desktop entry invalid: $f"
    fi
    if command -v desktop-file-validate >/dev/null 2>&1; then
        desktop-file-validate "$f" >/dev/null 2>&1
        t_assert_eq 0 $? "desktop-file-validate $(basename "$f")"
    else
        t_skip "desktop-file-validate not installed"
    fi
done

out="$(bash "$INSTALLER" --status 2>&1)"; rc=$?
if [[ "$out" == *"[OK] 图标"* && "$out" == *"[OK] Icon=dreamina"* ]]; then
    t_ok "--status sees icon and themed Icon entries"
else
    t_fail "--status missing OK lines (rc=$rc): $out"
fi

out="$(bash "$INSTALLER" --uninstall 2>&1)"; rc=$?
if [[ $rc -eq 0 && ! -e "$DESKTOP_FILE" && ! -e "$APP_ENTRY" && ! -e "$ICON_FILE" ]]; then
    t_ok "--uninstall removes entries and icons"
else
    t_fail "--uninstall did not clean up (rc=$rc): $out"
fi

t_finish "test-dreamina-install.sh"

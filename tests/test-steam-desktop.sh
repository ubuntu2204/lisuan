#!/usr/bin/env bash
#
# test-steam-desktop.sh — 在临时 HOME 里验证菜单 12 会统一修复用户级菜单项、
# XDG 桌面图标与历史 ~/Desktop 残留入口，并能幂等回滚。不碰真实桌面文件。
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

H="${TEST_TMP}/home"
APP="${H}/.local/share/applications"
DESK="${H}/桌面"
LEGACY="${H}/Desktop"
mkdir -p "$APP" "$DESK" "$LEGACY" "${H}/.config"
cat > "${H}/.config/user-dirs.dirs" <<'EOF'
XDG_DESKTOP_DIR="$HOME/桌面"
EOF

BAD_EXEC='Exec=env MESA_LOADER_DRIVER_OVERRIDE=gf /usr/bin/steam -no-cef-sandbox -cef-force-64bit %U'
ENTRIES=("${APP}/steam.desktop" "${DESK}/steam.desktop" "${LEGACY}/steam.desktop")
for f in "${ENTRIES[@]}"; do
    printf '[Desktop Entry]\nName=Steam\n%s\nIcon=steam\nType=Application\n' \
        "$BAD_EXEC" > "$f"
    chmod 755 "$f"
done

HOME="$H" python3 - "$REPO_ROOT" <<'PY'
import sys
sys.path.insert(0, sys.argv[1] + '/packaging')
import lisuan_fix_core as core
assert core.install_steam_fix() == 0
PY
t_assert_eq 0 $? "install_steam_fix() succeeds in isolated HOME"

for f in "${ENTRIES[@]}"; do
    if grep -q 'GBM_BACKEND=dri' "$f" && \
       grep -q 'LIBGL_ALWAYS_SOFTWARE=1' "$f" && \
       grep -q '__GLX_VENDOR_LIBRARY_NAME=mesa' "$f" && \
       grep -q -- '-cef-disable-gpu' "$f" && \
       ! grep -q '__GLX_VENDOR_LIBRARY_NAME=gf' "$f" && \
       ! grep -q 'MESA_LOADER_DRIVER_OVERRIDE=gf' "$f" && \
       ! grep -q -- '-no-cef-sandbox' "$f" && \
       ! grep -q -- '-cef-force-64bit' "$f"; then
        t_ok "fixed entry: $(basename "$(dirname "$f")")/steam.desktop"
    else
        t_fail "entry not fixed: $f"
    fi
    [[ -f "${f}.bak" ]]
    t_assert_eq 0 $? "backup created: $(basename "$(dirname "$f")")"
done

HOME="$H" python3 - "$REPO_ROOT" <<'PY'
import sys
sys.path.insert(0, sys.argv[1] + '/packaging')
import lisuan_fix_core as core
assert core.install_steam_fix() == 0
PY
t_assert_eq 0 $? "install_steam_fix() is idempotent"

HOME="$H" python3 - "$REPO_ROOT" <<'PY'
import sys
sys.path.insert(0, sys.argv[1] + '/packaging')
import lisuan_fix_core as core
assert core.remove_steam_fix() == 0
PY
t_assert_eq 0 $? "remove_steam_fix() succeeds"

for f in "${ENTRIES[@]}"; do
    if ! grep -q 'GBM_BACKEND=dri' "$f" && \
       ! grep -q 'LIBGL_ALWAYS_SOFTWARE=1' "$f" && \
       ! grep -q '__GLX_VENDOR_LIBRARY_NAME=mesa' "$f" && \
       ! grep -q -- '-cef-disable-gpu' "$f" && \
       grep -q '^Exec=/usr/bin/steam %U$' "$f"; then
        t_ok "rollback entry: $(basename "$(dirname "$f")")/steam.desktop"
    else
        t_fail "rollback incomplete: $f"
    fi
done

t_finish "test-steam-desktop.sh"

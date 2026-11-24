#!/usr/bin/env bash
#
# test-trae-wrapper.sh — 在临时 HOME 里验证 Trae/Trae CN wrapper 的桌面项安装、
# 回滚与 --app trae-cn 透传；不修改真实桌面项，也不启动真实 IDE。
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

WRAPPER="${REPO_ROOT}/electron/use-trae.sh"
BASE="${REPO_ROOT}/electron/use-electron-gf.sh"
SELF="$(readlink -f "$BASE")"

make_home() {
  local h="$1"
  mkdir -p "${h}/.local/share/applications" "${h}/bin"
  for v in trae trae-cn; do
    cat > "${h}/bin/${v}" <<EOF
#!/bin/sh
printf '%s\n' "\$@" > "\$HOME/args-${v}"
EOF
    chmod 755 "${h}/bin/${v}"
    cat > "${h}/.local/share/applications/${v}.desktop" <<EOF
[Desktop Entry]
Name=Trae test ${v}
Exec=${h}/bin/${v} %F
Icon=${v}
Type=Application

[Desktop Action NewEmptyWindow]
Name=New Empty Window
Exec=${h}/bin/${v} --new-window %F
EOF
    chmod 755 "${h}/.local/share/applications/${v}.desktop"
  done
}

H="${TEST_TMP}/home-both"
make_home "$H"
A="${H}/.local/share/applications"

HOME="$H" TRAE_BIN="${H}/bin/trae" TRAE_CN_BIN="${H}/bin/trae-cn" \
  bash "$WRAPPER" --install-desktop >/dev/null 2>&1
t_assert_eq 0 $? "install-desktop covers both Trae variants"

for v in trae trae-cn; do
  grep -qF "Exec=${SELF} --app ${v} %U" "${A}/${v}.desktop"
  t_assert_eq 0 $? "desktop Exec points to wrapper: ${v}"
  grep -qF -- '--new-window' "${A}/${v}.desktop"
  t_assert_eq 0 $? "desktop action preserved: ${v}"
done

HOME="$H" TRAE_CN_BIN="${H}/bin/trae-cn" bash "$WRAPPER" --app trae-cn >/dev/null 2>&1
t_assert_eq 0 $? "--app trae-cn launches the CN binary"
grep -qF -- '--use-angle=swiftshader' "${H}/args-trae-cn"
t_assert_eq 0 $? "CN launch uses SwiftShader software rendering"
grep -qF -- '--ozone-platform=x11' "${H}/args-trae-cn"
t_assert_eq 0 $? "CN launch forces X11"

HOME="$H" TRAE_BIN="${H}/bin/trae" TRAE_CN_BIN="${H}/bin/trae-cn" \
  bash "$WRAPPER" --uninstall-desktop >/dev/null 2>&1
t_assert_eq 0 $? "uninstall-desktop is reversible"
for v in trae trae-cn; do
  if grep -q 'use-electron-gf.sh' "${A}/${v}.desktop"; then
    t_fail "wrapper marker removed on rollback: ${v}"
  else
    t_ok "wrapper marker removed on rollback: ${v}"
  fi
done

H2="${TEST_TMP}/home-cn-only"
make_home "$H2"
A2="${H2}/.local/share/applications"
HOME="$H2" TRAE_BIN="${H2}/bin/trae" TRAE_CN_BIN="${H2}/bin/trae-cn" \
  bash "$WRAPPER" --app trae-cn --install-desktop >/dev/null 2>&1
t_assert_eq 0 $? "explicit --app trae-cn --install-desktop succeeds"
grep -q 'use-electron-gf.sh' "${A2}/trae-cn.desktop"
t_assert_eq 0 $? "explicit app installed CN desktop entry"
if grep -q 'use-electron-gf.sh' "${A2}/trae.desktop"; then
  t_fail "explicit app must not touch the international desktop entry"
else
  t_ok "explicit app must not touch the international desktop entry"
fi

t_finish "test-trae-wrapper.sh"

#!/usr/bin/env bash
#
# test-doubao-launcher.sh — 在临时 HOME + 假 Proton + 假前缀里验证豆包启动器的
# 启动加速逻辑：64 位 loader 直连（带 C:\ 路径）、WINEDEBUG 默认值、日志轮转、
# DOUBAO_FAST 别名，以及回退路径。不启动真实 Wine / 豆包。
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

LAUNCHER="${REPO_ROOT}/doubao/run-doubao-wine11.sh"
T="${TEST_TMP}/doubao-launcher"
FAKE_PROTON="${T}/Proton Fake 11.0"          # 带空格，顺带验证引号处理
FAKE_PREFIX="${T}/prefix"
FAKE_PREFIX32="${T}/prefix32"
APP_DIR="drive_c/users/steamuser/AppData/Local/Doubao/Application/app"
CAP="${T}/capture.txt"
LOGS="${T}/logs"
mkdir -p "${FAKE_PROTON}/files/bin" "${FAKE_PROTON}/files/lib/wine/x86_64-unix" \
         "${FAKE_PREFIX}/${APP_DIR}" "${FAKE_PREFIX32}/${APP_DIR}" "${LOGS}" "${T}/home"

# 最小 PE 头：偏移 60 = e_lfanew(0x40)，0x44 = Machine（0x8664=64 位 / 0x014c=32 位）
make_pe() {  # make_pe <file> <machine:le 2 bytes printf 格式>
    { printf 'MZ'; head -c 58 /dev/zero; printf '\x40\x00\x00\x00PE\0\0'; printf "$2"; } > "$1"
}
make_pe "${FAKE_PREFIX}/${APP_DIR}/Doubao.exe" '\x64\x86'
make_pe "${FAKE_PREFIX32}/${APP_DIR}/Doubao.exe" '\x4c\x01'

cat > "${FAKE_PROTON}/files/bin/wine" <<EOF
#!/bin/sh
printf 'loader=legacy\nargv=%s\nWINEDEBUG=%s\nWINELOADERNOEXEC=%s\nWINE_D3D_CONFIG=%s\nWINEDLLOVERRIDES=%s\nVK_DRIVER_FILES=%s\n' "\$*" "\${WINEDEBUG-unset}" "\${WINELOADERNOEXEC-unset}" "\${WINE_D3D_CONFIG-unset}" "\${WINEDLLOVERRIDES-unset}" "\${VK_DRIVER_FILES-unset}" > "${CAP}"
EOF
cat > "${FAKE_PROTON}/files/lib/wine/x86_64-unix/wine-preloader" <<'EOF'
#!/bin/sh
exec "$@"
EOF
cat > "${FAKE_PROTON}/files/lib/wine/x86_64-unix/wine" <<EOF
#!/bin/sh
printf 'loader=direct\nargv=%s\nWINEDEBUG=%s\nWINELOADERNOEXEC=%s\nWINE_D3D_CONFIG=%s\nWINEDLLOVERRIDES=%s\nVK_DRIVER_FILES=%s\n' "\$*" "\${WINEDEBUG-unset}" "\${WINELOADERNOEXEC-unset}" "\${WINE_D3D_CONFIG-unset}" "\${WINEDLLOVERRIDES-unset}" "\${VK_DRIVER_FILES-unset}" > "${CAP}"
EOF
chmod 755 "${FAKE_PROTON}/files/bin/wine" \
         "${FAKE_PROTON}/files/lib/wine/x86_64-unix/wine-preloader" \
         "${FAKE_PROTON}/files/lib/wine/x86_64-unix/wine"

run_launcher() {  # run_launcher <prefix> <logfile> [ENV=VAL ...] -- [args...]
    local prefix="$1" logf="$2"; shift 2
    env -i PATH="${PATH}" HOME="${T}/home" DOUBAO_PROTON="${FAKE_PROTON}" \
        DOUBAO_WINE11_PREFIX="$prefix" DOUBAO_NO_STEAM_RUNTIME=1 DOUBAO_FONT_DISABLE=1 \
        DOUBAO_GPU=sw DOUBAO_WINE11_LOG="$logf" "$@" bash "${LAUNCHER}" extra-arg >/dev/null 2>&1
}
expect_win='C:\users\steamuser\AppData\Local\Doubao\Application\app\Doubao.exe'

# 1) 64 位 PE：直连 64 位 loader，程序路径保持 C:\ 形式，默认关 fixme
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX}" "${LOGS}/a.log"
t_assert_eq 0 $? "launcher runs for 64-bit PE"
grep -qF 'loader=direct' "${CAP}"; t_assert_eq 0 $? "64-bit PE uses direct x86_64 loader"
grep -qF "argv=${expect_win} extra-arg" "${CAP}"
t_assert_eq 0 $? "direct loader gets C:\\ path and trailing args"
grep -qF 'WINEDEBUG=fixme-all' "${CAP}"; t_assert_eq 0 $? "WINEDEBUG defaults to fixme-all"
grep -qF 'WINELOADERNOEXEC=1' "${CAP}"; t_assert_eq 0 $? "WINELOADERNOEXEC=1 avoids start.exe hop"

# 2) 外部 WINEDEBUG 优先（排障需要完整 fixme 时可用）
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX}" "${LOGS}/b.log" WINEDEBUG='fixme+all'
grep -qF 'WINEDEBUG=fixme+all' "${CAP}"; t_assert_eq 0 $? "explicit WINEDEBUG is preserved"

# 3) DOUBAO_WINE_LOADER=legacy：回退旧行为，用于对照诊断
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX}" "${LOGS}/c.log" DOUBAO_WINE_LOADER=legacy
grep -qF 'loader=legacy' "${CAP}"; t_assert_eq 0 $? "legacy override uses files/bin/wine"
grep -qF "argv=${FAKE_PREFIX}/${APP_DIR}/Doubao.exe extra-arg" "${CAP}"
t_assert_eq 0 $? "legacy path keeps unix path argument"
grep -qF 'WINELOADERNOEXEC=unset' "${CAP}"; t_assert_eq 0 $? "legacy path does not set WINELOADERNOEXEC"

# 4) 32 位 PE：不走 64 位 loader
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX32}" "${LOGS}/d.log"
grep -qF 'loader=legacy' "${CAP}"; t_assert_eq 0 $? "32-bit PE falls back to generic loader"

# 5) 日志轮转：超过阈值先转成 .1，只保留一份历史
head -c 3000 /dev/zero | tr '\0' 'x' > "${LOGS}/rot.log"
run_launcher "${FAKE_PREFIX}" "${LOGS}/rot.log" DOUBAO_LOG_MAX_BYTES=1000
t_assert_eq 0 $? "launcher runs with oversized log"
[[ -s "${LOGS}/rot.log.1" && "$(stat -c %s "${LOGS}/rot.log.1")" -eq 3000 ]]
t_assert_eq 0 $? "oversized log rotated to .1"
grep -q '^=== ' "${LOGS}/rot.log"; t_assert_eq 0 $? "fresh log gets session header"

# 6) DOUBAO_FAST=1 等价于跳过 Steam Runtime 容器（不传 DOUBAO_NO_STEAM_RUNTIME 时生效）
mkdir -p "${T}/steamcommon/SteamLinuxRuntime_sniper"
cat > "${T}/steamcommon/SteamLinuxRuntime_sniper/run-in-sniper" <<EOF
#!/bin/sh
echo called > "${T}/runtime-called"
EOF
chmod 755 "${T}/steamcommon/SteamLinuxRuntime_sniper/run-in-sniper"
rm -f "${CAP}" "${T}/runtime-called"
env -i PATH="${PATH}" HOME="${T}/home" DOUBAO_PROTON="${FAKE_PROTON}" \
    DOUBAO_WINE11_PREFIX="${FAKE_PREFIX}" STEAM_COMMON="${T}/steamcommon" DOUBAO_FAST=1 \
    DOUBAO_FONT_DISABLE=1 DOUBAO_WINE11_LOG="${LOGS}/e.log" bash "${LAUNCHER}" >/dev/null 2>&1
t_assert_eq 0 $? "launcher runs with DOUBAO_FAST=1"
[[ ! -e "${T}/runtime-called" ]]; t_assert_eq 0 $? "DOUBAO_FAST=1 skips Steam Runtime wrapper"
grep -qF 'loader=direct' "${CAP}"; t_assert_eq 0 $? "DOUBAO_FAST=1 still uses direct loader"

# 7) 不加 DOUBAO_FAST：默认仍走容器（稳健默认不变）
rm -f "${CAP}" "${T}/runtime-called"
env -i PATH="${PATH}" HOME="${T}/home" DOUBAO_PROTON="${FAKE_PROTON}" \
    DOUBAO_WINE11_PREFIX="${FAKE_PREFIX}" STEAM_COMMON="${T}/steamcommon" \
    DOUBAO_GPU=sw DOUBAO_FONT_DISABLE=1 DOUBAO_WINE11_LOG="${LOGS}/f.log" bash "${LAUNCHER}" >/dev/null 2>&1
[[ -e "${T}/runtime-called" ]]; t_assert_eq 0 $? "default path still wraps in Steam Runtime"

# 8) sw 模式：WineD3D/GL 软件渲染，d3d11 强制 builtin（不受 hw 拷入的 DXVK 影响）
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX}" "${LOGS}/g.log"
grep -qF 'WINE_D3D_CONFIG=renderer=gl' "${CAP}"; t_assert_eq 0 $? "sw keeps WINE_D3D_CONFIG=renderer=gl"
grep -qF 'WINEDLLOVERRIDES=d3d11,dxgi=b;mscoree,mshtml=' "${CAP}"
t_assert_eq 0 $? "sw forces builtin d3d11/dxgi"
grep -qF 'VK_DRIVER_FILES=unset' "${CAP}"; t_assert_eq 0 $? "sw does not set VK_DRIVER_FILES"

# 9) hw 模式：预置垫片产物跳过部署，注入 gf ICD/DXVK 环境（假 Proton 无 dxvk 目录，拷贝跳过）
FAKE_GF="${T}/gf-accel"
mkdir -p "${FAKE_GF}/lib"
touch "${FAKE_GF}/lib/libvulkan_gf_shim.so" "${FAKE_GF}/lib/osrelease_redirect.so" \
      "${FAKE_GF}/lib/libvulkan_gf.so" "${FAKE_GF}/lib/libSC.so" "${FAKE_GF}/gf_icd.json"
rm -f "${CAP}"
run_launcher "${FAKE_PREFIX}" "${LOGS}/h.log" DOUBAO_GPU=hw DOUBAO_GF_ACCEL="${FAKE_GF}"
t_assert_eq 0 $? "hw launcher runs with pre-deployed shims"
grep -qF "VK_DRIVER_FILES=${FAKE_GF}/gf_icd.json" "${CAP}"
t_assert_eq 0 $? "hw overrides VK_DRIVER_FILES (pressure-vessel precedence)"
grep -qF 'WINEDLLOVERRIDES=d3d11,dxgi=n,b;mscoree,mshtml=' "${CAP}"
t_assert_eq 0 $? "hw uses native DXVK d3d11/dxgi"
grep -qF 'WINE_D3D_CONFIG=unset' "${CAP}"; t_assert_eq 0 $? "hw unsets WINE_D3D_CONFIG"

t_finish "test-doubao-launcher.sh"

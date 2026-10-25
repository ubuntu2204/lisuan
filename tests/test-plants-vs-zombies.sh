#!/usr/bin/env bash
#
# test-plants-vs-zombies.sh — offline checks for the Plants vs. Zombies (AppID 3590) fix:
#   1. Plants-vs-Zombies/fix-plants-vs-zombies.sh and the generated helpers must be valid bash/python;
#   2. Plants-vs-Zombies/patch_launch_options.py must edit Steam's localconfig.vdf without
#      breaking the VDF structure (balanced braces), be idempotent, and leave
#      the other app's block untouched.
# No Steam, no game, no Proton needed — pure text processing on a fixture.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

PVZ_DIR="${REPO_ROOT}/Plants-vs-Zombies"
FIX="${PVZ_DIR}/fix-plants-vs-zombies.sh"
PATCHER="${PVZ_DIR}/patch_launch_options.py"
WORKDIR="${TEST_TMP}/plants-vs-zombies"
mkdir -p "$WORKDIR"

# --- 0. static syntax checks ------------------------------------------------
bash -n "$FIX" 2>/dev/null
t_assert_eq 0 $? "bash -n Plants-vs-Zombies/fix-plants-vs-zombies.sh"

# 编译到临时目录，避免在仓库里留下 __pycache__
python3 -c 'import py_compile,sys; py_compile.compile(sys.argv[1], cfile=sys.argv[2], doraise=True)' \
  "$PATCHER" "${TEST_TMP}/patch_launch_options.pyc" 2>/dev/null
t_assert_eq 0 $? "py_compile Plants-vs-Zombies/patch_launch_options.py"

# 生成的宿主启动器/Steam 入口模板必须是合法 bash（用 --status 之外的方式校验：
# 直接检查模板片段里的 heredoc 已闭合，避免真的落地文件）。
t_assert_grep "$FIX" 'plants-vs-zombies-host-run\.sh' "fix script deploys the host launcher"
t_assert_grep "$FIX" 'PROTON_USE_WOW64=1' "host launcher enables new WoW64 (32-bit -> 64-bit gf Vulkan)"
t_assert_grep "$FIX" 'PVZ_RENDERER:-gl' "host launcher defaults to the wined3d GL backend"
t_assert_grep "$FIX" 'FailureReason' "fix script reads the game's own 3D-test verdict"
t_assert_grep "$FIX" 'reset-3d-test' "fix script can pull the game out of PopCap safe mode"
t_assert_grep "$FIX" 'DBUS_SESSION_BUS_ADDRESS=' "steam entry points the session bus at the host prefix"
t_assert_grep "$FIX" 'for p in /run/host /host' "steam entry handles both host-mount layouts"
t_assert_grep "$FIX" 'PRESSURE_VESSEL_PREFIX' "steam entry also detects container by env markers"
t_assert_grep "$FIX" 'PROTON_USE_WINED3D=1' "host launcher forces wined3d (DXVK can't cover D3D8 here)"
t_assert_grep "$FIX" 'flock -n 9' "host launcher is single-instance"
t_assert_grep "$FIX" 'osrelease_redirect\.so' "low-4GB remap pool is available for the vulkan path"
t_assert_grep "$FIX" 'PVZ_SHIM' "shim path is overridable"
t_assert_grep "$FIX" 'ELF 64-bit' "shim is validated as a 64-bit object"
t_assert_grep "$FIX" 'popcapgame1\.exe' "fix script extracts the PopCap body"

# --- 1. set writes a single LaunchOptions line inside the 3590 block --------
cfg="${WORKDIR}/localconfig.vdf"
cp "${FIXTURES_DIR}/localconfig.vdf.sample" "$cfg"
EXPECT_OPT='"/home/ubuntu/.local/share/lisuan-fix/plants-vs-zombies/plants-vs-zombies-steam.sh" %command%'

python3 "$PATCHER" "$cfg" 3590 --set "$EXPECT_OPT" >/dev/null 2>&1
t_assert_eq 0 $? "patch --set exits 0"
t_assert_grep "$cfg" 'LaunchOptions' "--set inserted LaunchOptions"
t_assert_eq 1 "$(grep -c 'LaunchOptions' "$cfg")" "exactly one LaunchOptions line"

# 结构自检：LaunchOptions 落在 AppID 3590 的块内，且没有破坏 246420 的块
python3 - "$PATCHER" "$cfg" <<'PY'
import sys
sys.dont_write_bytecode = True          # 不要在仓库里留下 __pycache__
sys.path.insert(0, sys.argv[1].rsplit('/', 1)[0])
import patch_launch_options as p

text = open(sys.argv[2], encoding='utf-8').read()
apps = p.find_apps_block(text)
assert apps, "apps block missing"
ao, ac = apps
apps_block = text[ao:ac]
block = apps_block[p.find_app_block(apps_block, '3590')[0]:p.find_app_block(apps_block, '3590')[1]]
other = apps_block[p.find_app_block(apps_block, '246420')[0]:p.find_app_block(apps_block, '246420')[1]]
assert '"LaunchOptions"' in block, "LaunchOptions not inside app 3590"
assert '"LaunchOptions"' not in other, "LaunchOptions leaked into app 246420"
assert '"Playtime"' in block, "existing keys lost"
assert text.count('{') == text.count('}'), "braces unbalanced"
PY
t_assert_eq 0 $? "LaunchOptions lands inside app-3590 block only"

# --- 2. --print round-trips the value --------------------------------------
printed="$(python3 "$PATCHER" "$cfg" 3590 --print 2>/dev/null)"
t_assert_eq "$EXPECT_OPT" "$printed" "--print returns the stored value"

# --- 3. idempotency: setting twice keeps one line with the new value -------
python3 "$PATCHER" "$cfg" 3590 --set 'NEWVALUE %command%' >/dev/null 2>&1
t_assert_eq 0 $? "second --set exits 0"
t_assert_eq 1 "$(grep -c 'LaunchOptions' "$cfg")" "second --set does not duplicate the line"
t_assert_grep "$cfg" 'NEWVALUE' "second --set replaces the value"
t_assert_not_grep "$cfg" 'plants-vs-zombies-steam\.sh' "old value is gone after replace"

# --- 4. --clear removes it and keeps the file balanced ---------------------
python3 "$PATCHER" "$cfg" 3590 --clear >/dev/null 2>&1
t_assert_eq 0 $? "--clear exits 0"
t_assert_eq 0 "$(grep -c 'LaunchOptions' "$cfg")" "--clear removed LaunchOptions"
python3 - "$cfg" <<'PY'
import sys
text = open(sys.argv[1], encoding='utf-8').read()
assert text.count('{') == text.count('}'), "braces unbalanced after clear"
assert '"Playtime"' in text, "unrelated keys lost"
PY
t_assert_eq 0 $? "--clear keeps the VDF balanced"

# --- 5. unknown appid is reported, not silently written -------------------
cp "${FIXTURES_DIR}/localconfig.vdf.sample" "${WORKDIR}/other.vdf"
python3 "$PATCHER" "${WORKDIR}/other.vdf" 999999 --set 'x' >/dev/null 2>&1
t_assert_eq 2 $? "missing appid exits 2"

t_finish "test-plants-vs-zombies.sh"

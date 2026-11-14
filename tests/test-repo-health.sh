#!/usr/bin/env bash
#
# test-repo-health.sh — static checks on the repository layout that must hold
# before publishing to Gitee: required files exist, no vendor artifacts are
# accidentally tracked, scripts are executable, README references resolve.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

# --- 1. Required top-level files present -----------------------------------
# LICENSE / NOTICE / .gitignore / README.md all live at the repo root
# (REPO_ROOT here is the deliverables subtree, not a standalone repo).
REPO_TOP="${REPO_ROOT}"
for f in README.md; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "required file present: $f"
done

# --- 2. Vendor artifacts must NOT be tracked (open-source hygiene) ----------
# 直接用 git ls-files 检查“是否被跟踪”，比扫磁盘更精确：本地留存但未跟踪
# 的厂商文件（.gitignore 已排除）不算违规。
# 豆包官方在线安装器（约 7MB PE）也属厂商专有产物，2026-11-07 曾误入库，
# 现由 .gitignore 的 doubao/doubao_client_*.exe 排除，这里做回归拦截。
for pat in '*.deb' '*.ko' '*.ko.zst' '*.o' '*.o.hex' 'lisuan_proprietary*' \
           'doubao/doubao_client_*.exe'; do
  if git -C "$REPO_TOP" ls-files -z -- "$pat" 2>/dev/null | grep -qz .; then
    t_fail "vendor artifact must not be tracked: $pat"
  else
    t_ok "no vendor artifact tracked: $pat"
  fi
done

# --- 3. Scripts are executable ---------------------------------------------
# 交付子树与仓库根的所有 .sh 都必须带可执行位（否则用户直接 ./xxx.sh 会失败）
for s in "${REPO_TOP}"/*.sh; do
  [[ -f "$s" ]] || continue
  [[ -x "$s" ]]
  t_assert_eq 0 $? "script is executable: $(basename "$s")"
done
while IFS= read -r -d '' s; do
  [[ -x "$s" ]]
  t_assert_eq 0 $? "script is executable: ${s#"$REPO_ROOT"/}"
done < <(find "$REPO_ROOT" -mindepth 2 -maxdepth 3 -type f -name '*.sh' \
           ! -path '*/_work/*' ! -path '*/.git/*' ! -path '*/packaging/dist/*' -print0)

# --- 4. Patch file is non-empty and mentions the root cause -----------------
[[ -s "$PATCH_FILE" ]]
t_assert_eq 0 $? "patch file non-empty"
t_assert_grep "$PATCH_FILE" 'KERNELVER' "patch mentions KERNELVER"
t_assert_grep "$PATCH_FILE" 'uname -r' "patch mentions uname -r"

# --- 5. README references that exist in this repo ---------------------------
for f in dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch \
         dkms/fix-lisuan-gpu.sh \
         dkms/verify-lisuan-gpu.sh \
         audio/fix-lisuan-hdmi-audio.sh; do
  grep -qF -- "$f" "${REPO_TOP}/README.md"
  t_assert_eq 0 $? "README references $f"
done

# --- 6. Required top-level docs present --------------------------------------
for f in CHANGELOG.md NOTICE LICENSE; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "top-level doc present: $f"
done

# --- 7. L4D2 current resources, archive paths and menu registration -----------
for f in left4dead2/fix-l4d2.sh \
         left4dead2/setlocale-shim.c \
         left4dead2/README.md \
         left4dead2/探索记录.md \
         left4dead2/archive/README.md \
         left4dead2/archive/vkcreateimage-shim.c \
         left4dead2/vtest-forward/build-venus-bridge.sh \
         left4dead2/vtest-forward/run-l4d2-venus.sh \
         left4dead2/vtest-forward/patches/0001-venus-prefer-ext-calibrated-timestamps.patch \
         left4dead2/vtest-forward/patches/0002-venus-hide-broken-feedback-loop-layout.patch; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "L4D2 resource present: $f"
done
t_assert_grep "${REPO_TOP}/left4dead2/fix-l4d2.sh" \
  'archive/vkcreateimage-shim.c' "L4D2 optional shim uses archive path"
t_assert_grep "${REPO_TOP}/packaging/build-deb.sh" \
  'find left4dead2' "deb includes L4D2 resources"
python3 - "$REPO_TOP" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
sys.path.insert(0, str(root / "packaging"))
import lisuan_fix_core as core
assert len(core.ITEMS_BY_ID) == 29
assert core.GROUP_ORDER[-1] == "Windows 应用"
assert core.ITEMS_BY_ID[22]["root"] is False
assert core.ITEMS_BY_ID[22]["cmd"]([])[1].endswith("left4dead2/fix-l4d2.sh")
assert core.ITEMS_BY_ID[23]["root"] is False
assert core.ITEMS_BY_ID[23]["doc"] == "left4dead2/README.md"
assert core.UNDO[22]["root"] is False
assert "--uninstall" in core.UNDO[22]["cmd"]
# 植物大战僵尸 GOTY（32 位 Windows 游戏，new WoW64 + 7G100 硬件渲染）：安装/说明 + 卸载
assert core.ITEMS_BY_ID[24]["root"] is False
assert core.ITEMS_BY_ID[24]["cmd"]([])[1].endswith("Plants-vs-Zombies/fix-plants-vs-zombies.sh")
assert core.ITEMS_BY_ID[25]["doc"] == "Plants-vs-Zombies/README.md"
assert core.UNDO[24]["root"] is False
assert "--uninstall" in core.UNDO[24]["cmd"]
# Windows 应用（Wine 11）：豆包 / 即梦 AI 官方 Windows 客户端
# 安装项 + 说明项各两个；都是当前用户操作，不需要 root。
assert core.ITEMS_BY_ID[26]["root"] is False
assert core.ITEMS_BY_ID[26]["cmd"]([])[1].endswith("doubao/install-doubao.sh")
assert core.ITEMS_BY_ID[26]["group"] == "Windows 应用"
assert core.ITEMS_BY_ID[27]["doc"] == "doubao/README.md"
assert core.ITEMS_BY_ID[28]["root"] is False
assert core.ITEMS_BY_ID[28]["cmd"]([])[1].endswith("dreamina/install-dreamina.sh")
assert core.ITEMS_BY_ID[28]["group"] == "Windows 应用"
assert core.ITEMS_BY_ID[29]["doc"] == "dreamina/README.md"
PY
t_assert_eq 0 $? "L4D2 menu 22/23 and undo registration"

# --- 7b. Windows 应用（Wine 11）资源与打包 -----------------------------------
for f in doubao/install-doubao.sh \
         doubao/run-doubao-wine11.sh \
         doubao/README.md \
         dreamina/install-dreamina.sh \
         dreamina/extract-icon.py \
         dreamina/dreamina-wine.desktop \
         dreamina/run-dreamina.sh \
         dreamina/powershell-stub.c \
         dreamina/README.md; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "Wine 11 app resource present: $f"
done
t_assert_grep "${REPO_TOP}/packaging/build-deb.sh" \
  'for d in doubao dreamina' "deb includes Wine 11 app scripts"
# 即梦桌面图标回归：模板不能借用豆包图标，安装脚本必须提供体检/桌面修复/回滚入口。
t_assert_grep "${REPO_TOP}/dreamina/dreamina-wine.desktop" \
  '^Icon=dreamina$' "dreamina desktop uses themed icon"
t_assert_not_grep "${REPO_TOP}/dreamina/dreamina-wine.desktop" \
  'doubao' "dreamina desktop no longer borrows doubao icon"
for flag in --desktop-only --status --uninstall --help; do
  t_assert_grep "${REPO_TOP}/dreamina/install-dreamina.sh" \
    "$flag" "dreamina installer supports $flag"
done
t_assert_grep "${REPO_TOP}/dreamina/powershell-stub.c" \
  'DREAMINA_PS_STUB_V1' "powershell stub has unique marker"
t_assert_grep "${REPO_TOP}/dreamina/run-dreamina.sh" \
  'AppData/Local/Programs/dreamina-desktop' "launcher locates install dir"
# 官方安装器属厂商专有产物，不得入库
for pat in 'dreamina/Dreamina-Desktop-*-x64-setup.exe' 'doubao/doubao_client_*.exe' \
           'war3/4_pf_files_KKDuiZhanDownloader-*.exe'; do
  if git -C "$REPO_TOP" ls-files -z -- "$pat" 2>/dev/null | grep -qz .; then
    t_fail "vendor installer must not be tracked: $pat"
  else
    t_ok "no vendor installer tracked: $pat"
  fi
done

# --- 7b2. KK 对战平台：运行修复与图标 ---------------------------------------
t_assert_grep "${REPO_TOP}/war3/run-kk-platform.sh" \
  'VK_ICD_FILENAMES=.*lvp_icd.json' "KK launcher pins llvmpipe Vulkan ICD"
t_assert_grep "${REPO_TOP}/war3/run-kk-platform.sh" \
  "WINE_D3D_CONFIG=.*renderer=gl" "KK launcher pins wined3d GL renderer"
t_assert_grep "${REPO_TOP}/war3/run-kk-platform.sh" \
  'force-device-scale-factor=1' "KK launcher fixes WebEngine DPR"
t_assert_grep "${REPO_TOP}/war3/run-kk-platform.sh" \
  'KK_LAYOUT_LEGACY' "KK layout fix has a rollback switch"
t_assert_grep "${REPO_TOP}/war3/run-kk-platform.sh" \
  'XLIB_SKIP_ARGB_VISUALS=1' "KK launcher blocks ARGB translucent windows"
t_assert_grep "${REPO_TOP}/war3/fix-kk-desktop.sh" \
  'ICON_NAME=kkduizhan' "KK desktop fix uses extracted icon name"
t_assert_grep "${REPO_TOP}/war3/fix-kk-desktop.sh" \
  'uninstall' "KK desktop fix is reversible"
t_assert_grep "${REPO_TOP}/war3/extract-icon.py" \
  'RT_GROUP_ICON' "KK icon extractor parses PE icon resources"
# --- 7b3. Wayland 合成画面取证通道 -----------------------------------------
CAPTURE="${REPO_TOP}/.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py"
t_assert_grep "$CAPTURE" 'PipeWireStreamAdded' \
  "Wayland capture subscribes to Mutter ScreenCast stream signal"
t_assert_grep "$CAPTURE" 'RecordMonitor' \
  "Wayland capture records a Mutter monitor"
t_assert_grep "$CAPTURE" 'sess.Stop' \
  "Wayland capture stops the session"
t_assert_grep "${REPO_TOP}/.codebuddy/rules/Wayland抓图约定.md" \
  'Mutter.ScreenCast' "Wayland capture rule documents the supported channel"
if git -C "$REPO_TOP" ls-files --error-unmatch \
    '.codebuddy/rules/Wayland抓图约定.md' >/dev/null 2>&1 || \
   [ -f "${REPO_TOP}/.codebuddy/rules/Wayland抓图约定.md" ]; then
  t_ok "Wayland capture rule is present in repo"
else
  t_fail "Wayland capture rule missing"
fi

# --- 7c. Steam 系统信息闪退 / GLX 弹窗修复 ---------------------------------
t_assert_grep "${REPO_TOP}/packaging/lisuan_fix_core.py" \
  'LIBGL_ALWAYS_SOFTWARE=1' "Steam fix forces software GL"
t_assert_grep "${REPO_TOP}/packaging/lisuan_fix_core.py" \
  '\-cef-disable-gpu' "Steam fix disables CEF GPU init"
t_assert_grep "${REPO_TOP}/packaging/lisuan_fix_core.py" \
  '__GLX_VENDOR_LIBRARY_NAME=mesa' "Steam fix selects system Mesa GLX vendor"
t_assert_grep "${REPO_TOP}/steam/steam-on-lisuan.md" \
  '系统信息' "Steam docs cover system information crash"
python3 - "$REPO_TOP" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
sys.path.insert(0, str(root / "packaging"))
import lisuan_fix_core as core

bad = ("[Desktop Entry]\n"
       "Exec=env MESA_LOADER_DRIVER_OVERRIDE=gf /usr/bin/steam "
       "-no-cef-sandbox -cef-force-64bit %U\n")
assert not core.steam_desktop_fixed(bad)
new, changed = core._build_fixed_exec(bad)
assert changed
assert "GBM_BACKEND=dri" in new and "LIBGL_ALWAYS_SOFTWARE=1" in new
assert "__GLX_VENDOR_LIBRARY_NAME=mesa" in new
assert core.STEAM_FIX_FLAG in new
assert "MESA_LOADER_DRIVER_OVERRIDE=gf" not in new
assert "-no-cef-sandbox" not in new and "-cef-force-64bit" not in new
fixed = "[Desktop Entry]\nExec=" + new + "\n"
assert core.steam_desktop_fixed(fixed)
without_flag = fixed.replace(" -cef-disable-gpu", "")
assert not core.steam_desktop_fixed(without_flag)
without_glx = fixed.replace(" __GLX_VENDOR_LIBRARY_NAME=mesa", "")
assert not core.steam_desktop_fixed(without_glx)
plain = core._strip_fixed_exec(fixed)
assert "GBM_BACKEND=dri" not in plain and "LIBGL_ALWAYS_SOFTWARE=1" not in plain
assert "__GLX_VENDOR_LIBRARY_NAME=mesa" not in plain
assert core.STEAM_FIX_FLAG not in plain
assert "GBM_BACKEND=dri" in core.STEAM_DESKTOP_BODY_FIXED
assert "LIBGL_ALWAYS_SOFTWARE=1" in core.STEAM_DESKTOP_BODY_FIXED
assert "__GLX_VENDOR_LIBRARY_NAME=mesa" in core.STEAM_DESKTOP_BODY_FIXED
assert core.STEAM_FIX_FLAG in core.STEAM_DESKTOP_BODY_FIXED
assert "__GLX_VENDOR_LIBRARY_NAME=mesa" in core.STEAM_FIX_ENV
assert "__GLX_VENDOR_LIBRARY_NAME=gf" in core.STEAM_HARMFUL_PREFIXES
assert core.STEAM_DESKTOP in core.steam_desktop_files()
PY
t_assert_eq 0 $? "Steam desktop fix helpers require Mesa GLX, reject old gf injection and are reversible"

# --- 8. Driver baseline and project skill -----------------------------------
t_assert_grep "${REPO_TOP}/install.py" \
  'v262\.2\.0\.56' "install.py declares required driver baseline"
python3 - "$REPO_TOP" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
sys.path.insert(0, str(root / "packaging"))
import lisuan_fix_core as core
assert core.REQUIRED_LISUAN_DRIVER_VERSION == "262.2.0.56"
assert "v262.2.0.56" in core.__doc__
assert core._is_required_driver_version("262.2.0.56")
assert core._is_required_driver_version("262.2.0.56-1")
assert not core._is_required_driver_version("262.2.0.57")
PY
t_assert_eq 0 $? "core declares and validates driver baseline"
SKILL_REL=".codebuddy/skills/lisuan-7g100-compat/SKILL.md"
SKILL_FILE="${REPO_TOP}/${SKILL_REL}"
[[ -f "$SKILL_FILE" ]]
t_assert_eq 0 $? "project skill SKILL.md present"
t_assert_grep "$SKILL_FILE" \
  'v262\.2\.0\.56' "project skill declares driver baseline"
git -C "$REPO_TOP" check-ignore -q -- "$SKILL_REL" >/dev/null 2>&1
t_assert_eq 1 $? "project skill is not gitignored"

t_finish "test-repo-health.sh"

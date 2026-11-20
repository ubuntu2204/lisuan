# KK 大厅黑块 · 标准测试流程

> 至砺算：这份流程用于判定"大厅 web 黑块"是否真的消除。**必须走完整流程到关弹窗
> 之后**，只看活动弹窗是否渲染会得出错误结论（弹窗是独立窗口，与大厅 web 内容面
> 是两套 surface）。2026-11-17 曾因只看到"新人寻宝"弹窗显示就误判成功。

## 0. 前置

- 驱动基线 `v262.2.0.56`（`python3 .codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py`）。
- 硬件方案已就绪：`bash kk/setup-kk-hw-proton.sh --status` 三项全 OK
  （DXVK v2.3 / libSC / offscreen 补丁已打）。
- 桌面入口已指向硬件方案：`grep Exec ~/.local/share/applications/kkduizhan.desktop`
  应为 `run-kk-platform-hw.sh --keep`（缺 `--keep` 时启动器取证后会杀掉 KK，
  表现为双击图标闪一下就没了）。切回软件方案：`bash kk/fix-kk-desktop.sh`；
  再切硬件：`bash kk/fix-kk-desktop.sh --hw`。
- 抓图**只能**用合成通道（X11 抓图在 Wayland 下全黑，不作证据）：
  ```bash
  python3 .codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py -o /tmp/x.png
  ```

## 1. 启动

```bash
# 首次建独立 new WoW64 前缀（会丢登录态，需手动登录一次）；之后跳过
bash kk/run-kk-platform-hw.sh --reset
# 常规启动；--keep 保留进程便于逐步验证
bash kk/run-kk-platform-hw.sh --keep
```

等待窗口：`DISPLAY=:0 xwininfo -root -tree | grep KK`，应见
`1334x947+293+32`（主框架）、`1332x946+294+33`（内容层，wrapper 生效时应与主框架
客户区原点重合）。

## 2. 登录

- 若停在登录页 `1095x558`：手动登录一次（账号已预填 `186****8365`，验证码或密码）。
  登录后会话存在硬件前缀 `compatdata/481234/pfx`，下次自动进大厅。
- 判定进入大厅：应用日志出现 `WndLobby` / `Boardcast notify`：
  ```bash
  PFX="$HOME/.local/share/Steam/steamapps/compatdata/481234/pfx"
  grep -rl 'WndLobby' "$PFX/drive_c/Program Files (x86)/kkduizhan/log"/*/platform.log
  ```

## 3. 关掉「新人寻宝大冒险」活动弹窗（关键步骤）

进大厅后会弹出 `920x560` 的"新人寻宝大冒险"活动窗。**必须关掉它再判定**，
因为它是独立 layered 窗口，渲染路径与主大厅 web 面不同：

- **必须用 Mutter RemoteDesktop 真实指针**（`kk/ui-autotest.py` 的 `Harness.click`）：
  XTEST（`xdotool click`）在本机 XWayland 不投递 ButtonPress，`XSendEvent` 对
  Wine win32k 命中无效。
- 点之前**确认 KK 在前台**：曾误点到后台 WPS 的打印按钮（坐标 1400,245 落在
  WPS 窗口上），导致截图采到白底、误判。
- `Alt+F4` 会关掉**整个主窗口**并触发退出挽留弹窗（440x360），不是关活动弹窗；
  用它判定会得到错误的"弹窗已关"。
- 判定弹窗已关且 KK 仍存活：`xwininfo -root -tree | grep 920x560` 无输出
  **且** `pgrep -f Platform.exe` 仍有进程。
- 关掉后若出现退出挽留弹窗，用真实指针点"下次再说"取消退出，再抓图。

## 4. 判定黑块（关弹窗之后）

```bash
CAP=.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py
python3 "$CAP" -o /tmp/kk-lobby-after-close.png
python3 - <<'PY'
from PIL import Image
from collections import Counter
im=Image.open('/tmp/kk-lobby-after-close.png').convert('RGB')
print('colors', len(im.getcolors(2000000) or []))
# 大厅主内容区（窗口 293,32 1334x947，去掉左侧栏与标题）采样
pix=[im.getpixel((x,y)) for y in range(120,940,16) for x in range(460,1600,16)]
print('content top:', Counter(pix).most_common(6))
PY
```

判定标准：
- **仍有黑块**：主内容区整片 `(0,0,0)` / 近黑占压倒多数，颜色数低（历史黑块态约
  3.3 万色，纯黑采样点 2600+）。游戏列表/商城等 web 内容不可见。
- **已消除**：主内容区能看到游戏卡片、推荐海报、商城导航等 web 内容，颜色数显著
  升高（活动弹窗显示时约 18 万色；关弹窗后若仍 18 万量级且非黑为主才算真消除）。

## 5. 同时判定错位

- 内容层 X 窗应在 `(294,33)`（不是 `(0,0)`）；若内容层跑到 `(0,0)` 则 wrapper 未生效。
- 点左侧栏"魔兽争霸3"视觉中心应能选中（命中零偏移）；点偏了说明 win32k 矩形仍漂移。
- 这些由 `kk-user32-wrapper.c` + `kk-geometry-shim.c` 修，`KK_GEOMETRY_SHIM=0` 关闭。

## 6. 证据留存

每步都抓合成图（`/tmp/kk-step1-launch.png`、`step2-login.png`、
`step3-popup.png`、`step4-after-close.png`），连同
`~/.local/share/kkduizhan-wine11/poc-hw.log` 与前应用日志一起报告。

## 7. 禁止事项

- 不用 X11 抓图 / xwd / ImageGrab 判定（Wayland 下全黑，是全局限制）。
- 不因"活动弹窗显示了"就判定黑块消除（本次错误的来源）。
- 不加看门狗/自动重启/守护（见崩溃处理约定）。

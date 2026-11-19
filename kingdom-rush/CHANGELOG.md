# kingdom-rush 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.10.03 — 王国保卫战（Kingdom Rush）启动崩溃修复：容器内 GLXBadFBConfig

### 现象

Steam 点"开始游戏"后弹窗、无法进入：

```
Unable to create OpenGL window
OpenGL context creation error: Could not create GL context: GLXBadFBConfig
```

### 根因

Kingdom Rush（AppID 246420）是原生 64 位 LÖVE（SDL2 + OpenGL）游戏。Steam 默认
把它放进 pressure-vessel 容器运行，容器隐藏 `/sys/bus/pci`，砺算厂商 GLX/EGL 在无
设备发现路径下无法认领屏幕，回退的 mesa 又给不出游戏请求的 GLXFBConfig →
`GLXBadFBConfig`。宿主的厂商栈完全正常（glxinfo：LISUAN 7G100、OpenGL 4.6）。
另：宿主缺 64 位 `libopenal.so.1`（Ubuntu 24.04 默认只装 i386），LÖVE 需要它。

### 修复

新增 `kingdom-rush/fix-kingdom-rush.sh`（幂等，与 Dota Underlords 同一套容器→宿主
委托方案）：

1. `apt install libopenal1` 补齐 64 位 OpenAL；
2. 给游戏自带 `run.sh` 打补丁：检测到 `/usr/lib/pressure-vessel` 时经宿主
   `systemd-run --user` 委托到宿主直跑，阻塞至游戏退出以保持 Steam 跟踪；
3. 生成 `kr-host-run.sh` 委托包装与桌面项；
4. 登记进交互式修复索引：新增分组"王国保卫战"（菜单 [18] 安装/修复、[19] 文档），
   含环境检测、状态展示与卸载（`--uninstall`）；`build-deb.sh` 打包目录加入 `kingdom-rush`。

### 验证

宿主直跑 `run.sh`（干净环境）：窗口正常创建、无 GL/音频报错（timeout 杀，rc=124）。
`python3 install.py --detect` 游戏节显示 `[OK] Kingdom Rush 修复已安装`；
`tests/run-tests.sh` 7/7 全过（含 shellcheck / repo-health）。

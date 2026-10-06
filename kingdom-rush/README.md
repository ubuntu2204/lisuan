# Kingdom Rush（砺算 7G100 修复）

Steam AppID **246420**，原生 64 位 [LÖVE](https://love2d.org) 游戏（`liblove.so.0` + SDL2 + OpenGL）。

## 至砺算：官方实现什么可以让本游戏在容器内原生运行

> 本节写给砺算官方驱动 / 图形栈研发。按投入产出比排序；**任意一项落地，本目录对应 workaround 即可退役**。

1. **容器内 GLX/EGL 设备认领**（治本，与刀塔霸业同一道墙）：厂商 GLX vendor 在
   pressure-vessel 容器内（`/sys/bus/pci` 被隐藏）无法认领屏幕，回退的 Mesa 给
   不出游戏请求的 GLXFBConfig → `GLXBadFBConfig`。参考 Mesa：基于
   `/dev/dri/renderD*` + `drmGetDevice` 的设备发现，并把 vendor ICD 装到 GLVND
   标准路径。→ 退役：`systemd-run` 容器委托宿主直跑整套方案。
2. **GLX FBConfig 枚举完整性**：确保标准 RGBA / double-buffer /
   depth-stencil 组合齐全（SDL2 + LÖVE 请求 OpenGL 2.1 context 的常规路径），
   宿主直跑能成、容器内失败，差异点值得官方在容器内复测。
3. 注：游戏另需 64 位 `libopenal1`（Ubuntu 24.04 默认只装 i386），属发行版依赖
   与驱动无关。

## 症状

Steam 点"开始游戏"后弹窗、无法进入：

```
Unable to create OpenGL window
This program requires a graphics card and video drivers which support OpenGL 2.1 or OpenGL ES 2.
OpenGL context creation error: Could not create GL context: GLXBadFBConfig
```

## 根因

1. Steam 默认把该原生游戏放进 **pressure-vessel 容器**（SteamLinuxRuntime）运行。
   容器隐藏 `/sys/bus/pci`，砺算厂商 GLX/EGL 在容器内无法认领屏幕，回退的 mesa
   又给不出游戏请求的 GLXFBConfig → `GLXBadFBConfig`。
2. 宿主的厂商图形栈完全正常（`glxinfo`：LISUAN 7G100、OpenGL 4.6），
   在宿主直跑 `run.sh` 游戏窗口正常、音频正常。
3. 宿主还缺 **64 位 `libopenal.so.1`**（Ubuntu 24.04 默认只装 i386 版），LÖVE 需要它。

## 修复

`fix-kingdom-rush.sh`（幂等）：

- 补齐 64 位 `libopenal1:amd64`；
- 给游戏自带 `run.sh` 打补丁：**检测到容器时经宿主 systemd（`systemd-run --user`）
  委托到宿主直跑**（与 Dota Underlords 同一套方案），阻塞至游戏退出以保持 Steam 跟踪；
- 生成 `kr-host-run.sh`（容器→宿主委托包装）与桌面项「Kingdom Rush (Lisuan native)」。

## 用法

```bash
sudo ./fix-kingdom-rush.sh              # 安装/修复
./fix-kingdom-rush.sh --status          # 查看状态
./fix-kingdom-rush.sh --launch          # 宿主直跑一次（排查用）
sudo ./fix-kingdom-rush.sh --uninstall  # 还原 run.sh、移除委托脚本与桌面项
```

安装后在 Steam 里正常点"开始游戏"即可。

## 备注

- 若 Steam 更新了游戏、`run.sh` 被覆盖，重跑一次 `sudo ./fix-kingdom-rush.sh` 即可。
- 委托日志见 `/tmp/kr-hostrun.log`。

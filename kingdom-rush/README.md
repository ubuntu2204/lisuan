# Kingdom Rush（砺算 7G100 修复）

Steam AppID **246420**，原生 64 位 [LÖVE](https://love2d.org) 游戏（`liblove.so.0` + SDL2 + OpenGL）。

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

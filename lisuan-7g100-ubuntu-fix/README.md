# lisuan-7g100-ubuntu-fix

砺算(Lisuan)7G100 GPU(PCI `4c54:5006`)在 **Ubuntu 24.04** 上的一系列
修复与适配方案:内核 DKMS 驱动构建修复、Mesa DRI 驱动名修复、Steam 适配,
以及与**系统 Mesa(25.2.x)**的兼容性分析与按应用切换工具。

目标环境:Ubuntu 24.04 LTS(HWE 内核 7.0.x)、海光 CPU + 砺算 7G100、
厂商包 `lisuangpu-dkms` / `lisuangpu-lib` 262.2.0.56。

## 仓库内容

| 路径 | 说明 |
|---|---|
| `patches/0001-dkms-makefile-honor-KERNELVER.patch` | 厂商 DKMS Makefile 的 `KERNEL_VERSION`/`KERNELVER` 变量名错配修复(一行补丁) |
| `scripts/fix-lisuan-gpu.sh` | 一键应用上述补丁并重编/重装/加载 `gf.ko`(幂等、自动备份) |
| `scripts/verify-lisuan-gpu.sh` | 只读体检:lspci/lsmod/modinfo/DRM 节点 |
| `scripts/fix-lisuan-mesa-driver-name.sh` | 修复 DRM 名与用户态驱动名不一致(`lisuan` vs `gf`)导致的 DRI 加载失败 |
| `scripts/use-system-mesa.sh` | 让指定应用运行在**系统 Mesa(zink)**之上,绕开厂商 Mesa 22.0.4 分叉 |
| `scripts/verify-system-mesa.sh` | 系统 Mesa 兼容性体检(zink 回退、GBM dri 后端等) |
| `scripts/fix-steam-cef-lock.sh` / `fix-steam-cef-swrender.sh` / `fix-steam-pv-ldconfig.sh` | Steam(CEF/Proton)相关适配 |
| `docs/system-mesa-analysis.md` | 7G100 与系统 Mesa 25.2.x 的源码级兼容性分析(为什么原生加载不可行、zink 路径、已知地雷) |
| `tests/` | 离线自动化测试(补丁、脚本幂等性、语法、仓库合规) |

## 快速开始

```bash
# 1) DKMS 驱动构建/加载问题(vermagic 错配)
sudo ./scripts/fix-lisuan-gpu.sh
./scripts/verify-lisuan-gpu.sh

# 2) GL/EGL 找不到驱动(driver=(null),Steam CEF GPU 崩溃)
sudo ./scripts/fix-lisuan-mesa-driver-name.sh

# 3) 想让某个应用走系统 Mesa(25.2.x,zink)而非厂商 Mesa 22.0.4
./scripts/use-system-mesa.sh --print-env
./scripts/use-system-mesa.sh glxinfo -B
./scripts/verify-system-mesa.sh
```

## 测试

全部离线、无硬件依赖:

```bash
./tests/run-tests.sh        # 跑全部
./tests/run-tests.sh -l     # 列出
./tests/run-tests.sh -v     # 显示详细输出
```

## 许可

Apache-2.0,见 [LICENSE](LICENSE) 与 [NOTICE](NOTICE)。
本仓库不包含任何厂商二进制;厂商 `.deb`/`.ko` 等产物已被 `.gitignore` 排除。

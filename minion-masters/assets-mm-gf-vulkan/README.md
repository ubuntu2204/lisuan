# assets-mm-gf-vulkan — 随从大师交付物目录

本目录是**部署流程实际取用的地方**，与已删除的开发工作目录 `mm-patch/` 相对。

| 文件 | 用途 |
|---|---|
| `MMPatch.cs` | doorstop 直载的 IL2CPP 补丁源码（**权威版本**） |
| `bin/MMPatch.dll` | 构建产物，部署脚本取它拷进游戏目录 |
| `osrelease_redirect.c/.so` | os-release 重定向 + 低 4GB 重映射 hook |
| `libvulkan_gf_shim.c/.so` | gf ICD 协商垫片 |
| `gf_icd.json` | 垫片 ICD 描述 |

## ⚠️ MMPatch 的两个陷阱

**1. 只有本目录的 `MMPatch.cs` 是权威版。**

历史上存在第二份副本（`mm-patch/doorstop/MMPatch.cs`，2026-08-26），
该目录已于 2026-08-31 删除（git 历史仍可查）。**若误构建部署那份旧版，游戏会卡在登录**。

**2. Vivox patch 必须保持停用。**

Vivox 在 wine 下原生可用（日志可见 `VxVivox Audio` 线程与
`OnLoginStateChanged: LoggedIn`）。一旦把 `LoginAsync`/`LogoutAsync` 改掉——
无论改成空操作还是跳 trampoline——游戏都会卡在 `[ChatService] Starting LoginAsync`
等不到回调（空操作版直接退出、trampoline 版卡在「加载中」）。

排查线索：若 `BepInEx/MMPatch.log` 出现 `PATCHED 2/2 Vivox async`，
即说明 `PatchVivox()` 没被注释掉，游戏必卡在登录。该症状与显卡故障相似，易误判。

## 当前实际生效的 patch

| patch | 状态 |
|---|---|
| Vivox `LoginAsync`/`LogoutAsync` | 停用（见上） |
| `NewsItemViewModel.MoveNext` | 未匹配到目标类，日志无记录 |
| `DownloadHandlerTexture.InternalGetTexture` → null | ✅ 生效，规避图片下载的 Texture 异常 |

运行期日志写在游戏目录的 `BepInEx/MMPatch.log`，排查时先看它。

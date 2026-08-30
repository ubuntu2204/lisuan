# mm-patch — 随从大师补丁的**开发工作目录**

原位于 `~/mm_patch/`，2026-08-31 整体并入仓库以便统一管理。
**它是一个工作目录，不是交付物**——随从大师的部署流程只取
`../assets-mm-gf-vulkan/bin/MMPatch.dll` 一个文件，其余都是开发过程产物。

## 目录结构

| 目录 | 大小 | 性质 | 入库 |
|---|---|---|---|
| `doorstop/` | 128K | **自研**：`MMPatch.cs` + `mmpatch.csproj` | ✅ |
| `findtype.py` | 1K | **自研**：在 `Assembly-CSharp.dll` 里搜类型名 | ✅ |
| `typelist/` | 712K | **自研**：`read.csx` 反射列举类型 | ✅ 仅 csx |
| `plugin/` | 4.1M | **自研**：`Patcher.cs`，但属**已废弃方案** | ✅ 仅源码 |
| `metalib/` `apicheck/` | 0.9M | **自研**：两个 .NET 8 分析工程 | ✅ 仅 csproj |
| `MMSkipVivox_v1.1.0_final_working.dll` | 13K | 自研产物，但**已废弃** | ✅ |
| `tools/` | 97M | **第三方**：BepInEx 6 / Il2CppInterop / il2cpp | ❌ 见下 |
| `backup/` | 134M | **第三方**：改动前的 BepInEx 目录与 dotnet 运行时 | ❌ 见下 |

`*/bin/`、`*/obj/` 为 dotnet 构建产物，已被仓库根目录 `.gitignore` 的
`[Bb]in/` `[Oo]bj/` 规则排除。

## ⚠️ plugin/ 与 MMSkipVivox 已废弃（2026-08-26 起退役）

`plugin/Patcher.cs` 是 BepInEx 6 插件 **`MMSkipVivox` v1.1.0** 的源码
（`namespace MMSkipVivox`），`MMSkipVivox_v1.1.0_final_working.dll` 是其产物。

**退役原因**：BepInEx 6 的 `Chainloader.Start()` 在此环境**永不执行** →
插件加载不了 → Vivox 崩溃回归。详见 `../MinionMasters-攻坚全记录.md`
「已废弃方案：BepInEx 6 + `MMSkipVivox.dll`」。

**现行方案**：doorstop 直载的 `MMPatch.dll`（纯 P/Invoke，不依赖 BepInEx），
源码在 `doorstop/MMPatch.cs`，构建产物 `../assets-mm-gf-vulkan/bin/MMPatch.dll`。

> **不要照 `plugin/` 重新部署**。另注意：`doorstop/bin/MMPatch.dll` 是 2026-08-26
> 的旧构建（**启用** Vivox patch，会导致游戏卡在登录），已被 `.gitignore` 排除，
> 正确版本只有 `../assets-mm-gf-vulkan/bin/MMPatch.dll`。

## 第三方依赖（不入库，共 231MB）

按仓库根目录 `.gitignore` 政策，第三方专有/大体积依赖不入库。缺失不影响
正常部署（现行方案不依赖 BepInEx），仅在需要重新开发补丁时才用到。

| 目录 | 内容 | 获取方式 |
|---|---|---|
| `tools/` | BepInEx 6（`BepInEx-Unity.IL2CPP`）、随包分发的 Il2CppInterop/Unhollower、il2cpp 工具链；含 zip 原包与解压结果 | BepInEx 官方 Releases；具体版本见 `tools/bepinex/changelog.txt` |
| `backup/` | 改动前的 BepInEx 目录、dotnet 运行时 | 由安装过程自动生成，用于回滚；可从 `tools/` 重新解包得到 |

## 自研工具用法

```bash
python3 findtype.py                    # 在游戏程序集里搜类型名（内部路径硬编码，换机器需改）
dotnet script typelist/read.csx        # 反射列举 interop 程序集的类型/方法
dotnet run --project metalib           # 读取 IL2CPP 元数据
dotnet run --project apicheck          # 校验目标 API 是否存在
```

# gf-compat — 砺算 7G100 Vulkan 兼容层（源码）

**源码与运行时产物同处本目录**（2026-08-31 由 `~/gf-compat` 并入）。

Steam 启动选项的 `LD_PRELOAD` 指向这里的 `lib/osrelease_redirect.so`。

其中两个厂商专有二进制按仓库根目录 `.gitignore` 的政策**不入库**
（第三方专有软件），但文件需保留在本目录供运行时加载，
获取方式见文末「厂商专有二进制」。

## 部署

```bash
./deploy.sh              # 编译并部署到本目录（默认）
./deploy.sh --dry-run    # 只打印将执行的操作，不改动文件
DEST=/other/path ./deploy.sh   # 部署到别处
```

厂商二进制缺失时 `deploy.sh` 会告警并给出获取位置，不会尝试下载。

## 文件

### 生产路径在用

| 文件 | 用途 |
|---|---|
| `osrelease_redirect.c` | **LD_PRELOAD，游戏启动选项直接引用它**。① 把 `/etc/os-release` 重定向到伪造的 Ubuntu 标识，绕过 gf 驱动的发行版白名单；② 把落到 >4GB 的显存映射重定位到 2.25–4GB，让 32 位游戏经 WoW64 thunk 能拿到 CPU 可见指针 |
| `gf_icd.json` | 自定义 Vulkan ICD 清单，指向 `libvulkan_gf_shim.so` 协商垫片 |
| `os-release` | ① 所用的伪造发行版标识（400 字节） |

关于 ② 的实现：`low4` 池早期是纯 bump 分配（只增不减），对战中显存映射
反复申请/释放会把 1.75GB 池单调吃光，表现为 `reloc64 FAILED` 频出后崩溃
（实测一次运行内失败 2299 次）。现改为**空闲链表回收 + 相邻块合并 +
按块大小自适应对齐**，并改用 `MAP_FIXED_NOREPLACE`（旧代码的 `MAP_FIXED`
会静默覆盖既有映射，是堆损坏隐患）。

### 诊断探针（排查 Vulkan / DXVK 问题时用）

| 文件 | 用途 |
|---|---|
| `hook.c` | 拦截 `strcmp` / `strncmp` 并打印调用栈，用于定位"到底是谁在读 os-release" |
| `vkfix.c` | 向 DXVK 伪造 `VK_KHR_load_store_op_none`（gf 只有 EXT 版，枚举值相同） |
| `vkspy.c` | 拦截 vulkan loader 关键调用，打印失败返回值，定位 DXVK 失败点 |
| `vkprobe.py` | ctypes 直接 dlopen `libvulkan_gf.so`，枚举实例/设备扩展 |
| `vkdev.py` | 同上，侧重设备与队列族能力解析 |
| `vkdev2.py` | 构造完整 `VkApplicationInfo`，走一遍 `vkCreateInstance` |
| `drmname.py` | 用 `DRM_IOCTL_VERSION` 查 `/dev/dri` 节点注册的驱动名 |

## 厂商专有二进制（不入库）

`lib/` 下这两个文件属 `lisuangpu-lib` 包，是砺算的专有软件，
按仓库政策不入库，需向厂商/OEM 索取该包后自行放入 `lib/`：

| 文件 | 大小 | 获取位置 |
|---|---|---|
| `libSC.so` | 25.9MB | 宿主 `/usr/lib/x86_64-linux-gnu/libSC.so`；Steam 容器内为 `/run/host/usr/lib/x86_64-linux-gnu/libSC.so` |
| `libvulkan_gf.so` | 7.5MB | `lisuangpu-lib` 包装后位于 `/usr/lib/x86_64-linux-gnu/libvulkan_gf.so` |

缺失时 `deploy.sh` 会告警，游戏则拿不到 gf 驱动、无法启用硬件渲染。

> `libvulkan_gf.so` 是 gf 的用户态 Vulkan ICD；
> `libSC.so` 是砺算着色器编译器，`vkCreateDevice` 运行期会 `dlopen` 它，
> 缺它会返回 `VK_ERROR_OUT_OF_HOST_MEMORY`（详见「显卡坑 14」）。

## 相关文档

- 低 4GB 映射池、发行版白名单、ICD 垫片等来龙去脉：`../fix-minion-masters.md`
  （游戏坑 8「硬件渲染」+「历史方案与已废弃路线」章；原攻坚全记录已并入该文）
- 随从大师专属的 C# 补丁工程（MMPatch）在 `../assets-mm-gf-vulkan/`
  （其 `MMPatch.cs` 是**唯一权威版本**；原开发工作目录 `mm-patch/`
  已于 2026-08-31 删除，git 历史可查）

# L4D2 历史诊断与验证归档

本目录保存仍有复查价值的诊断源码和历史验证材料，避免与当前修复入口混放。
**除特别说明外，本目录文件不参与当前启动链，也不是 `fix-l4d2.sh` 的运行依赖。**

## 文件分类

| 文件 | 当前定位 |
|---|---|
| `vkcreateimage-shim.c` | 旧 D16 诊断 shim。仍由 `../fix-l4d2.sh` 编译为可选诊断工具，但**默认不注入**，不能作为当前 D16 修复手段；根因修复在 Venus 服务端补丁中 |
| `vk32-depth-test.c` / `vk32-scan.sh` | 历史深度格式/usage 探针。部分常量与过滤逻辑尚未和最终结论完全同步，**不要直接用于判断当前驱动能力**，使用前需先校正 |
| `vk-format-probe.py` | 64 位格式能力探针，保留用于复查早期格式位误判 |
| `vk-image-test.py` | 更早期的图像创建探针，常量未与最终修正同步，仅作历史参考 |
| `dlsym-hook-test.c` | 旧 `vkcreateimage` shim 的 dlsym 转发链验证源码 |
| `locale-mix-probe.c` | 复现 glibc 混合 locale 返回形式的历史探针。当前 locale 根因是 `steamclient.so` 覆盖 `LC_ALL`，不是本探针所模拟的直接 `LC_CTYPE=C` 路径 |
| `probe32.c` | 旧 GL 转发链探针，不属于当前原生 Vulkan/Venus 主链路 |
| `l4d2-screen.png` | 历史实机验证截图，仅作证据留存，不是安装依赖 |

## 新旧路径对照

排查记录中的原始路径不改写，统一按下表理解：

- `left4dead2/<上述源码文件>` → `left4dead2/archive/<同名文件>`
- `left4dead2/vtest-forward/probe32.c` → `left4dead2/archive/probe32.c`
- `left4dead2/.l4d2-screen.png` → `left4dead2/archive/l4d2-screen.png`

`left4dead2/.l4d2-window-test.log` 是无脚本、文档或测试引用的临时日志，已删除；
其有效结论已摘要到 `../探索记录.md` 和根目录 `CHANGELOG.md`。

当前权威操作指南是 `../README.md`，完整根因与验证记录是 `../探索记录.md`。

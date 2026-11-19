# mesa 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.09.06 — 删除 mesa/use-system-mesa.sh，配方内联进文档

### 理由

- 无菜单入口、无脚本依赖、无实际案例——纯边缘实验/应急工具；
- 实测能力上限为 **GL 3.0**（zink 受厂商 Vulkan ICD 限制），低于厂商栈的
  GL 4.6，重 GL 应用不适用；
- zink 建立在 `libvulkan_gf.so` 上，而该驱动刚发现 GlobPool0 GPF 崩溃
  bug（见同日刀塔霸业条目），地基本身不稳。

### 变更

1. 删除 `mesa/use-system-mesa.sh`（从磁盘移除，git 历史保留可找回）；
2. **环境变量配方内联**进 `mesa/system-mesa-analysis.md` §7 与 `README.md`
   （三个变量：`__GLX_VENDOR_LIBRARY_NAME=mesa`、
   `__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json`、
   `GBM_BACKEND=dri`）——文档不再依赖脚本即可复现实验；
3. README 4 处引用清理；`mesa/verify-system-mesa.sh`（体检）与
   `system-mesa-analysis.md`（菜单 8 说明）保留。

---

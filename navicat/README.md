# Navicat 18 Premium Lite（AppImage）启动闪退修复

> **与 GPU 无关**：本目录是**应用层**修复——AppImage 打包缺陷（内置输入法插件与
> 自带 Qt 版本 ABI 不兼容）导致启动即段错误，与砺算 7G100 / 图形栈没有任何关系。
> 收录进本仓库，是因为这里是本机（Ubuntu 24.04 + 砺算 7G100）全部修复方案的集合地。

## 文件

| 文件 | 说明 |
|---|---|
| `fix-navicat-appimage.sh` | 重打包 AppImage 并注入 `QT_IM_MODULE=compose`（幂等；自动备份原包为 `.bak`；`--status` / `--test` / `--restore` 齐备） |

> AppImage 本体（约 360 MB，Navicat 专有软件）**不入库**，留存于
> `~/development/navicat18-premium-lite-cs-x86_64.AppImage`（从 Navicat 官方渠道获取）。
> 本目录只有脚本与文档；换了下载路径用首个参数或 `NAVICAT_APPIMAGE=` 指定即可。

## 症状

双击（或终端运行）`navicat18-premium-lite-cs-x86_64.AppImage`，「注册」对话框
闪现不到一秒即闪退；终端可见核心转储（`timeout: 被监视的命令已核心转储`），
`gdb` 下稳定复现 `SIGSEGV`。

## 根因

AppImage 自带 **Qt 6.8.3**，但内置的 fcitx 输入法插件
`usr/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin-qt6.so`
是按 **Qt 6.5** 编译的（动态段要求 `Qt_6.5` 符号版本）。本机会话跑的是
**fcitx5**，登录环境里 `QT_IM_MODULE=fcitx` + `XMODIFIERS=@im=fcitx`：

1. Qt 按 `QT_IM_MODULE=fcitx` 加载该插件；Qt 6.8.3 **仍提供** `Qt_6.5`
   版本节点，动态链接器不报错，插件"加载成功"；
2. 但插件内部按 6.5 的 `QObjectPrivate` 等内部结构构造对象，与 6.8.3 运行时
   布局不一致 → d 指针错位 → **SIGSEGV**；
3. 崩溃时机正是「注册」对话框获得焦点的一瞬（`QGuiApplication` 焦点事件 →
   输入法插件），所以表现为"闪退"。

gdb 崩溃栈（决定性证据）：

```
#0  QObject::thread() const ()                     from libQt6Core.so.6
#1  QObject::QObject(QObjectPrivate&, QObject*)
#2  ???                                            libfcitxplatforminputcontextplugin-qt6.so
#3  ???                                            libfcitxplatforminputcontextplugin-qt6.so
#4  ???                                            libfcitxplatforminputcontextplugin-qt6.so
#5  QGuiApplicationPrivate::_q_updateFocusObject(QObject*)
#8  QWidget::setFocus(Qt::FocusReason)
#19 QDialog::exec()                                ← 「注册」对话框
```

该插件是包内**唯一**的输入法插件（没有 fcitx5 版本），系统 fcitx5 的 Qt6 插件
又是按系统 Qt 编译的，同样不能跨 Qt 版本加载——**这个 AppImage 里 fcitx 输入法
不可能可用**，唯一出路是切 Qt 内置的 `compose` 输入模块。

## 修复

重打包 AppImage：runtime ELF **原样保留**，仅把 squashfs 内的 `AppRun` 注入
`export QT_IM_MODULE=compose`，再按**原压缩器 / 块大小** `mksquashfs` 重打包
（脚本自动从超级块读取，不写死）。这样**所有启动方式**（文件管理器双击、
应用菜单、终端）都自动生效，无需包装脚本、不用改桌面项。

为什么不选别的方案：

| 方案 | 问题 |
|---|---|
| 启动前 `export QT_IM_MODULE=compose` | 只覆盖手动启动；双击 / 菜单图标仍崩 |
| 只改 `~/.local/share/applications/*.desktop` 的 Exec | 同样覆盖不到双击 AppImage 文件 |
| 删掉包内 fcitx 插件 | 也要重打包才能生效，且删文件不如切输入模块干净 |
| 改会话全局 `QT_IM_MODULE` | 会波及其它 Qt 应用（微信等）的 fcitx 输入 |

## 用法

```bash
./fix-navicat-appimage.sh                          # 修复（默认 ~/development/ 下找）
./fix-navicat-appimage.sh /path/to/x.AppImage      # 指定路径
NAVICAT_APPIMAGE=/path/x.AppImage ./fix-navicat-appimage.sh
./fix-navicat-appimage.sh --status                 # 查看状态
./fix-navicat-appimage.sh --test                   # 冒烟测试（启动 10 秒，看是否存活）
./fix-navicat-appimage.sh --restore                # 从 .bak 还原原包
./fix-navicat-appimage.sh --keep-work              # 保留解包工作目录（排查用）
```

依赖：`mksquashfs` / `unsquashfs`（`sudo apt-get install squashfs-tools`）。
全程不需要 root；工作目录临时建在 AppImage 同目录（避开 /tmp 可能是 tmpfs），
结束后自动清理。

## 验证（2026-10-14 实测）

| 项 | 结果 |
|---|---|
| 修复后在模拟双击环境（`QT_IM_MODULE=fcitx XMODIFIERS=@im=fcitx`）下启动 | 「注册」窗口正常显示（780x430，`IsViewable`），进程稳定运行 |
| 原包（`.bak`）同环境 | 必现闪退（`--test` 报 SIGABRT/SIGSEGV） |
| 修复产物与手工重打包 | 行为一致（仅注入注释文字不同） |
| 脚本幂等 | 二次执行走"已修复"分支，不重复备份、不重复打包 |
| `--restore` | 还原后 md5 与原包一致，`.bak` 被正确消费 |

## 备注

- 原包自动备份为同名 `.bak`（**仅首次**备份，保留 pristine 原件）；
  `--restore` 可随时回退。确认稳定后 `.bak` 可自行删除。
- Navicat 升级 / 重装 AppImage 后需重跑一次脚本（新包仍是同样的打包缺陷）。
- 启动日志里 `Could not find the Qt platform plugin "wayland"` 与
  `libselinux.so.1: no version information available` 是**无害噪音**
  （前者自动回退 xcb；后者是包内旧版 libselinux 与系统新版 glibc 的版本声明差异），
  不用处理。
- 本目录**未接入** `install.py` 交互菜单（应用层修复，与 GPU 无关，按需手动执行脚本）。
- 若哪天 Navicat 换了 Qt 版本或修掉这个插件，本脚本可退休：`--restore` 回退即可。

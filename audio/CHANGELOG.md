# audio 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.10.21 — 显卡 HDMI/DP 音频修好（4c54:5010 绑 snd_hda_intel，纠正旧结论）

### 修复

- **7G100 的 HDMI/DP 输出终于能出声**：新增 `audio/fix-lisuan-hdmi-audio.sh`
  （幂等），安装 `/usr/local/sbin/lisuan-hdmi-audio-bind` + `lisuan-hdmi-audio-bind.service`
  开机自绑，并立即生效。修复后 `aplay -l` 出现 `card 1: ... HDMI 0 [BenQ BL2780]`，
  PipeWire 自动多出 "HDA Intel 数字立体声 (HDMI)" 输出，`speaker-test -D hw:1,3`
  正常结束。
- 三层根因（2026-10-21 本机实测）：
  1. `05:00.1` 是**标准 HDA 控制器**（BAR0 16 KiB、`GCAP=0x4001`、
     `VMIN/VMAJ=0/1`，INTCTL/INTSTS/CORB/RIRB/流描述符全按规范排布），
     后挂标准 HDMI codec（`Vendor Id: 0x4c545020`，4 组 converter/pin），
     `snd_hda_codec_hdmi` 无需 quirk 即可驱动；
  2. `4c54` 不在 `snd_hda_intel` 的静态 PCI id 表，且本内核
     （`7.0.0-38-generic`）上写 `new_id` 返回 **EIO**——只能用
     `driver_override` 绕过 id 表；
  3. 该功能 **MSI 不可用**：MSI 绑定后 `/proc/interrupts` 计数恒 0，
     `INTSTS` 的 CIS/GIS 常置位，内核刷 `Spurious interrupt (vector 0xef)`，
     PCM 一开就卡住；`msi_bus=0` 改走 INTx（ACPI `_PRT`：INB → GSI 31）后
     中断计数正常增长、播放正常。
- 新增诊断工具 `audio/hda-probe.c`（mmap BAR0 读 GCAP/VMIN/VMAJ/STATESTS）
  与 `audio/hda-peek.c`（读写单个寄存器，用于复现 INTSTS 常置位）；
  文档 `audio/README.md` 记录完整证据链与遗留问题。
- `install.py` 新增菜单 [20]（安装/修复）与 [21]（说明文档），分组 `音频`；
  `collect_statuses` 按 "驱动是否绑定 + msi_bus=0 + 卡/ELD 是否就绪" 判状态；
  `packaging/build-deb.sh` 打包 `audio/` 目录。

### 实测更正（推翻 2026.08.23 的结论）

- 旧记录称 `4c54:5010` 是"`gf` 显示驱动内部的透明 codec，不创建 ALSA 卡，
  绑 `snd_hda_intel` 是死路"——**错误**。当时只试了 `new_id`（返回 EINVAL/EIO）
  就下了结论；实际上 `driver_override` 可以强制绑定，且 codec 被
  `snd_hda_codec_hdmi` 正常认出（ELD 里能读到显示器型号）。
- 真正的"死路"是 **MSI**：那条路会静默丢中断并刷 spurious 日志；
  INTx 才是这台机器的可用路径。
- 旧记录中"真正出声只有海光板载 HDA"同步更正为"板载 + 显卡 HDMI/DP 两路"；
  `README.md` §7.1 技术要点表、章节结构与修复清单已更新。

### 遗留（未在本次改动范围内）

- 厂商 `disp_codec.c` 的 `Reg_Hau_Reg105` 两个 stream-format 位仍用 `if/else`
  互斥（converter 3 的采样率/位深不更新）；仓库 `dkms/vendor-src/` 已修，
  已安装的 `/usr/src/lisuangpu-dkms-262.2.0.56/` 仍是旧代码。只影响
  PCM 8/9（HDMI 2/3）多路输出，当前单显示器走 PCM 3 不受影响。

---

## 2026.08.23 — HDMI 音频（HDAudio codec）修复

### 修复

- **7G100 在本系统（Ubuntu 24.04）HDMI 音频不正常**（converter 3 无声 / 音频流
  重配置后丢声）：根因在 `vendor-src/display/hdaudio/disp_codec.c` 的
  `disp_codec_irq_prepare`。`Reg_Hau_Reg105` 的 `set_cov2_strm_fmt` 与
  `set_cov3_strm_fmt` 是**两个相互独立**的 bit（可同时置位），原代码却用
  `if/else` 互斥处理——当两者同时置位（HDMI 音频同时重配置 converter 2、3
  是常见场景）时，converter[3] 的 `stream_format`（采样率/位深）永远不更新、
  停留在 0（无效），导致该 converter 输出的音频无声或异常。改为两个独立 `if`。
  顺带修正其 `disp_print` 把 `pconverter[3]` 错打印为 `pconverter[1]` 的复制笔误。
  - 载体：补丁 `patches/0002-audio-fix-hdmi-converter-23-stream-format.patch`
    （在转录源码上干净应用）。

- **声音设置只显示"虚拟输出"（无真实声卡）**：这是**用户态音频服务没枚举到
  任何 ALSA 声卡**，与上面的内核 codec 逻辑是两层问题。本机有两张音频 PCI
  功能：
  - `05:00.1` `4c54:5010`：7G100 GPU 伴生 HD-Audio，是 `gf` 显示驱动**内部
    的透明 codec**（`disp_codec.c` / `display_hda_handle` / ELD / infoframe
    全自管），**不创建 ALSA 声卡、也不是标准 Intel HD-A 控制器**，绑给
    `snd_hda_intel` 是死路——已排除。
  - `07:00.6` `1d94:14c9`：海光（Hygon）板载 **标准 HD-A 控制器**（源于 AMD
    Zen），才是本机真实可出声的声卡；但其 vendor id `1d94` 不在
    `snd_hda_intel` 内置 PCI id 表，内核不自动 probe → BAR 不启用 → 无
    `/dev/snd/card*`。
  - 修复：`hygon/fix-hygon-audio.sh`（幂等，需 `sudo`）。完整根因链：
    1. `1d94` 不在 `snd_hda_intel` 的 PCI id 表 → `new_id` 返回 `-EINVAL`、
       `bind` 因 id 不匹配被静默拒绝。用 `driver_override` 绕过 id 表强制匹配。
    2. `snd_intel_dspcfg` 对未知 vendor 默认选 SOF/SST 路径，本机无固件 →
       `azx_probe` 静默失败（dmesg 无日志）。须在加载 `snd_hda_intel` **之前**
       以 `dsp_driver=1` 加载 `snd-intel-dspcfg` 强制原生 HDA。
    3. 绑定成功后 codec (ALC222) 上电慢，`azx_get_response` 超时。加
       `probe_mask=0xff delay=2000` 探测全部 codec 地址并等待上电。
    最终 `aplay -l` 出现 `card 0: Intel [HDA Intel], device 0: ALC222 Analog`。
  - 持久化：`/etc/modprobe.d/hygon-audio.conf`（`install` 指令带
    `probe_mask=0xff delay=2000` + `options snd-intel-dspcfg dsp_driver=1`）
    与 `hygon-audio-bind.service` 开机自绑（driver_override + bind）；
    重启 PipeWire/WirePlumber 接管新声卡。`--status` 看报告、`--uninstall` 撤销。
  - 注：7G100 的 HDMI 边带音频（`4c54:5010`）是 `gf` 显示驱动内部透明 codec，
    不创建 ALSA 卡，需厂商驱动支持，超出本脚本范围；板载出声走海光 HDA。

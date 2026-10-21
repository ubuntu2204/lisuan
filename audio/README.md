# 砺算 7G100 显卡 HDMI/DP 音频修复

> **至砺算：** 官方只需做三件事，本目录的 workaround 即可退役：
> 1. 把伴生音频功能 `4c54:5010` 加进内核 `sound/pci/hda/hda_intel.c` 的
>    `azx_ids[]`（或在厂商 DKMS 里注册一个等价的 HDA 控制器驱动），让系统
>    开箱即绑；
> 2. 修 `05:00.1` 的 MSI 投递（现在一条 MSI 都进不来，改为 INTx 才可用）；
> 3. 修 `display/hdaudio/disp_codec.c` 里 `Reg_Hau_Reg105` 的
>    `set_cov2_strm_fmt` / `set_cov3_strm_fmt` 的 `if/else`（见文末"遗留问题"）。

## 症状

- 系统"设置 → 声音"里只有板载模拟输出，**看不到显卡的 HDMI/DP 输出**；
- `aplay -l` 只有 `card 0: ... ALC222 Analog`，没有 `HDMI 0`；
- `lspci -nnk -s 05:00.1` 显示：

  ```
  05:00.1 Audio device [0403]: Device [4c54:5010]
          Subsystem: Device [4c54:5000]
  ```

  注意后面**没有** `Kernel driver in use`——音频功能根本没被任何驱动接管。

## 根因（2026-10-21 本机实测）

### 1. `05:00.1` 是标准 HDA 控制器，不是"厂商内部透明 codec"

2026-08-23 的记录曾判定它"不创建 ALSA 卡、绑 `snd_hda_intel` 是死路"，
**该结论是错的**。用 `audio/hda-probe.c` 直接 mmap BAR0 读全局寄存器：

```
0000:05:00.1 HDA global registers:
  0x00: 0x01004001      # GCAP: 64BIT=1, NSDO=4, NSDI=0
  0x04: 0x001d003c      # OUTPAY=0x3c(60), INPAY=0x1d(29)
  0x08: 0x00000101      # GCTL: CRST=1, UNSOL=1
  0x0e: STATESTS=0x0101 # reset 后 codec 0 已应答
```

0x20 INTCTL、0x24 INTSTS、0x40 CORBLBASE、0x50 RIRBLBASE、0x80 起的流描述符
全部按 Intel HDA 规范排布；BAR0 恰好 16 KiB。绑定后 codec 被
`snd_hda_codec_hdmi` 直接认成标准 HDMI codec：

```
Codec: Generic 4c54 Generic HDMI
Vendor Id: 0x4c545020
Node 0x02 [Audio Output] ...  Node 0x03 [Pin Complex] ... Pincap: OUT Detect HDMI DP
（共 4 组 converter+pin，对应 PCM device 3/7/8/9）
```

### 2. 厂商号 `4c54` 不在 `snd_hda_intel` 的 PCI id 表里

内核不会自动 probe。本内核（`7.0.0-38-generic`）上往
`/sys/bus/pci/drivers/snd_hda_intel/new_id` 写 `4c54 5010` **直接返回 EIO**，
只能用 `driver_override` 绕过 id 表强制匹配：

```sh
echo snd_hda_intel > /sys/bus/pci/devices/0000:05:00.1/driver_override
echo 0000:05:00.1   > /sys/bus/pci/drivers/snd_hda_intel/bind
```

### 3. 该功能的 MSI 是坏的，必须用 INTx

直接用 MSI 绑定时：`/proc/interrupts` 里 `snd_hda_intel:card1` 的计数恒为 0，
`INTSTS`（0x24）的 CIS/GIS 一直置位，内核刷

```
Spurious interrupt (vector 0xef) on CPU#0. Acked
```

PCM 打开后能 set_params，但播放卡住、指针不走、没有声音。把
`/sys/bus/pci/devices/0000:05:00.1/msi_bus` 置 0（并重新 probe）后，
控制器改走 INTx（ACPI `_PRT` 把 INB 路由到 GSI 31）：

```
116: ... IO-APIC 31-fasteoi snd_hda_intel:card1   # 播放时计数正常增长
```

`speaker-test -D hw:1,3` 正常结束，无 spurious 日志。

## 修复

```sh
# 安装/修复（幂等；立即生效 + 开机自绑）
sudo ./audio/fix-lisuan-hdmi-audio.sh

# 查看状态（普通用户即可）
./audio/fix-lisuan-hdmi-audio.sh --status

# 撤销（解绑、msi_bus 还原为 1、删除服务；海光板载声卡不受影响）
sudo ./audio/fix-lisuan-hdmi-audio.sh --uninstall
```

脚本做三件事：

1. 安装 `/usr/local/sbin/lisuan-hdmi-audio-bind`（自绑逻辑，可手工执行、幂等）；
2. 安装 `lisuan-hdmi-audio-bind.service`（`WantedBy=multi-user.target`、
   `Before=graphical.target`、`After=hygon-audio-bind.service`——板载声卡的服务
   启动时会 `modprobe -r snd_hda_intel`，必须排在它之后）；
3. 立即执行一次并校验 `cardN` / `pcm3p` / ELD。

安装后正常应看到：

```
== 砺算 7G100 HDMI/DP 音频状态 ==
  PCI 功能   : 0000:05:00.1 [0x4c54:0x5010]
  驱动       : snd_hda_intel（已绑定）
  中断       : msi_bus=0 → INTx（该功能 MSI 不可用，必须走 INTx）
  ALSA 卡    : card1
  显示器 ELD : BenQ BL2780 / DisplayPort / 2 声道
  HDMI 0     : 就绪（hw:1,3）
  开机自绑   : 已启用（active）
```

然后在「设置 → 声音 → 输出」选择 **"HDA Intel 数字立体声 (HDMI)"**
（PipeWire 会自动发现新卡，无需重启会话）。

## 验证

```sh
# 1) 卡与 ELD（插着显示器时 eld_valid=1，monitor_name 是显示器型号）
cat /proc/asound/card1/eld#1.0

# 2) 直出测试音（<card> 换成 --status 里的卡号）
speaker-test -D hw:1,3 -c 2 -r 48000 -t sine -f 440 -l 1

# 3) 桌面栈：应出现 "HDA Intel 数字立体声 (HDMI)" 输出设备
wpctl status | sed -n '/Audio/,/Video/p'
```

只想验证"这个 PCI 功能到底是不是真 HDA 控制器"时，用诊断工具
（不依赖声卡栈，直接读 BAR0）：

```sh
gcc -O2 -o /tmp/hda-probe audio/hda-probe.c && sudo /tmp/hda-probe 0000:05:00.1
gcc -O2 -o /tmp/hda-peek  audio/hda-peek.c  && sudo /tmp/hda-peek  0000:05:00.1 0x24
```

## 遗留问题（与本次修复无关，但会影响多路输出）

- **converter 2/3 的 stream format 不会更新**：厂商源码
  `display/hdaudio/disp_codec.c` 的 `disp_codec_irq_prepare()` 里，
  `Reg_Hau_Reg105` 的 `set_cov2_strm_fmt` 与 `set_cov3_strm_fmt` 是两个独立
  bit，原代码却用 `if/else` 互斥处理；两者同时置位时 converter 3 的
  采样率/位深停留在 0。本仓库 `dkms/vendor-src/` 里的源码已修好（两个独立
  `if`，并修正 `pconverter[1]` 的打印笔误），但**已安装的厂商 DKMS 源码
  `/usr/src/lisuangpu-dkms-262.2.0.56/` 仍是旧代码**。
  只有同时驱动 PCM 8/9（HDMI 2/3）时才会踩到；当前单显示器走 PCM 3，不受影响。
  如需要，可按 `dkms/patches/` 的流程把这处改动做成补丁后重编 DKMS。
- **MSI 不可用**属硬件/固件问题，本修复用 INTx 绕过；若厂商后续修好 MSI，
  删掉 `msi_bus=0` 那一行即可回到 MSI（`--uninstall` 会还原）。
- 该系统上 `snd_hda_intel` 的 `delay` 模块参数已被内核删除，
  `/etc/modprobe.d/hygon-audio.conf` 里的 `delay=2000` 会被忽略（不影响出声），
  属于海光声卡修复的遗留噪音。

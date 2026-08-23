# 音频修复：让 Ubuntu 看到真实声卡（海光板载 HDA）

> 适用机型：**砺算 7G100 GPU + 海光（Hygon）整机**。
> 现象：声音设置里只有"虚拟输出 / Dummy Output"，没有任何真实声卡。
> 修复脚本：`lisuan-7g100-ubuntu-fix/scripts/fix-hygon-audio.sh`

---

## TL;DR

机器上有**两个**音频 PCI 功能，但只有一个能出声：

| PCI 功能 | 设备 | 说明 |
| --- | --- | --- |
| `05:00.1` `4c54:5010` | 砺算 7G100 GPU 伴生 HD-Audio | **透明 codec**，完全跑在厂商 `gf` 显示驱动内部（`disp_codec.c`），**不会创建 ALSA 卡**，不能作为普通声卡使用 —— 绑定它是死路。 |
| `07:00.6` `1d94:14c9` | 海光板载 HD-Audio 控制器 | **标准 HDA 控制器**（海光 Dhyana 源于 AMD Zen），这才是能出声的设备；只是它的 vendor id 不在 `snd_hda_intel` 默认 PCI id 表里，内核从不自动 probe，BAR 处于 disabled → 没有 `/dev/snd/card*` → 只剩虚拟输出。 |

**真正的修复**：用 `driver_override` 把这个海光 HDA 强制绑定到 `snd_hda_intel`，启用 BAR/IRQ 并注册 ALSA 卡。砺算 7G100 的 HDMI 音频当前在内核侧没有 ALSA 路径，不在本脚本范围内。

修复后 `aplay -l` 出现：

```text
card 0: Intel [HDA Intel], device 0: ALC222 Analog
```

---

## 根因（三层叠加）

1. **vendor id 不在 snd_hda_intel 的 PCI id 表**
   `1d94` 是海光私有 id，内核不会自动 probe → 设备一直 unbound，BAR `[disabled]`，无 IRQ。
2. **snd_intel_dspcfg 误选 SOF/SST 路径**
   对未知 vendor，`snd_intel_dspcfg` 默认可能走 SOF/SST，而本机没有对应固件 → `azx_probe` 静默失败（dmesg 无任何日志）。
3. **codec（ALC222）上电慢**
   `azx_get_response` 在默认超时内拿不到 codec 应答 → probe 失败。

---

## 修复步骤（脚本已自动化，此处为原理说明）

```bash
sudo ./scripts/fix-hygon-audio.sh            # 立即修复 + 开机自启
sudo ./scripts/fix-hygon-audio.sh --status   # 只看状态，不改任何东西
sudo ./scripts/fix-hygon-audio.sh --uninstall
```

脚本依次做（均幂等）：

1. 卸载已加载的 `snd_hda_intel`（模块已在内存时其 modprobe options 不会被重读，必须先卸载）。
2. **先** `modprobe snd-intel-dspcfg dsp_driver=1`（强制原生 HDA，避免走 SOF/DSP）。
3. `modprobe snd_hda_intel probe_mask=0xff delay=2000`（`probe_mask=0xff` 探测所有 codec 地址，`delay=2000` 给 ALC222 上电时间）。
4. 写入 `/sys/bus/pci/devices/0000:07:00.6/driver_override = snd_hda_intel`，再 `bind`（内核自动 `pci_enable_device`，启用 BAR 并分配 IRQ；海光 HDA 硬件与 `snd_hda_intel` 兼容）。若 `driver_override` 属性不存在，退化为 `new_id` + `bind`（`new_id` 对 `1d94` 会返回 `-EINVAL`，driver_override 才是正解）。
5. 落地持久化：
   - `/etc/modprobe.d/hygon-audio.conf`（`install snd_hda_intel ... probe_mask=0xff delay=2000`、`options snd-intel-dspcfg dsp_driver=1`、`options snd-hda-intel ...`）；
   - `/etc/systemd/system/hygon-audio-bind.service`（开机 oneshot，在 pipewire 之前强制绑定）。
6. 重启 PipeWire / WirePlumber，使其发现新 ALSA 卡。

> 注意：每次开机音频功能都是 unbound 状态，所以必须靠 `hygon-audio-bind.service` 开机自绑。

---

## 验证

```text
# lspci -nnk -s 07:00.6
07:00.6 Audio device [0403]: Hygon ... [1d94:14c9]
        Kernel driver in use: snd_hda_intel      # ← 已绑定

# aplay -l
card 0: Intel [HDA Intel], device 0: ALC222 Analog   # ← 真实声卡出现

# 声音设置里出现 "Hygon on-board HDMI/analog" 而非 "Dummy Output"
```

若卡出现了但没声音：检查 HDMI 线缆与 pavucontrol 里的输出 profile（HDMI 还是模拟）。

---

## 常见问题

**Q：为什么不让 7G100 的 GPU 音频（4c54:5010）出声？**
A：那是 `gf` 显示驱动内部的透明 codec（`disp_codec.c` / `display_hda_handle` / ELD / infoframe），不创建标准 ALSA 卡，也无法用 `snd_hda_intel` 绑定。它依赖厂商显示驱动栈，当前内核侧没有可用路径，不在本修复范围。

**Q：`new_id` 写入后没生效？**
A：对 `1d94` 这种不在 id 表里的 vendor，`new_id` 会被 `driver_probe_device` 因 id 不匹配静默拒绝（dmesg 无日志）。正确机制是 `driver_override`（脚本已优先使用）。

**Q：重启后还是虚拟输出？**
A：确认 `systemctl status hygon-audio-bind.service` 是否 enabled 且成功；若开机早于显示驱动绑定导致 `/sys` 路径不同，检查 service 的 `After=` 顺序。

#!/usr/bin/env python3
"""
patch_gf_map32.py — 给砺算 gf Vulkan 驱动的 vkMapMemory 打上 MAP_32BIT 补丁。

背景
  32 位游戏（如《随从大师》）经 Proton 的 WoW64 调用 64 位 gf 驱动时，
  gf 的 vkMapMemory 返回的 CPU 指针位于高地址（实测 0x7625ebf1a000）。
  wine 的 WoW64 thunk 无法把 >4GB 的指针交给 32 位代码，于是主动 unmap
  并合成 VK_ERROR_OUT_OF_HOST_MEMORY，导致 DXVK 分配失败、游戏崩溃：
      d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]

原理
  gf 内部函数 pil::device_icdlinux_c::map_buffer_cb() 里有 mmap64 调用：

      17a068: 31 ff              xor    %edi,%edi        ; addr = NULL
      17a06a: b9 01 00 00 00     mov    $0x1,%ecx        ; flags = MAP_SHARED
      17a06f: ba 03 00 00 00     mov    $0x3,%edx        ; prot  = RW
      17a07a: e8 61 73 f0 ff     call   mmap64@plt

  mmap64 的 flags 参数在 %ecx。当前是 0x01 (MAP_SHARED)。
  Linux x86_64 上 MAP_32BIT = 0x40，把它改成 0x41 (MAP_SHARED|MAP_32BIT)
  即可强制把显存映射进低 4GB 地址空间。

  只需修改【1 个字节】：文件偏移 0x17a06b 的 0x01 -> 0x41。
  （该库 PT_LOAD 段 vaddr 与 offset 相等，故文件偏移 == 虚拟地址）

安全性
  仅改动用户态 .so 的一个立即数，不写寄存器、不改内核、不碰固件。
  脚本会先备份原文件（.orig），随时可用 --restore 还原。

用法（写系统目录需 root）
  sudo python3 patch_gf_map32.py --check      # 查看当前状态
  sudo python3 patch_gf_map32.py --apply      # 打补丁（自动备份）
  sudo python3 patch_gf_map32.py --restore    # 还原

注意
  加 MAP_32BIT 后，该驱动的所有映射都会落在低 4GB。
  若同时运行多个重度 Vulkan 应用，4GB 空间可能吃紧。
"""
import os
import sys
import shutil

LIB = "/usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
BACKUP = LIB + ".orig"

# 需要修改的字节
OFFSET = 0x17A06B          # mov $0x1,%ecx 的立即数低位
OLD_BYTE = 0x01            # MAP_SHARED
NEW_BYTE = 0x41            # MAP_SHARED | MAP_32BIT

# 打补丁前校验的上下文（确保改对了地方）
CONTEXT_OFFSET = 0x17A068
CONTEXT_EXPECT = bytes([0x31, 0xFF, 0xB9, 0x01, 0x00, 0x00, 0x00,
                        0xBA, 0x03, 0x00, 0x00, 0x00])


def read_bytes(path, off, n):
    with open(path, "rb") as f:
        f.seek(off)
        return f.read(n)


def state():
    """返回: 'missing' / 'patched' / 'unpatched' / 'unknown'"""
    if not os.path.exists(LIB):
        return "missing"
    ctx = read_bytes(LIB, CONTEXT_OFFSET, len(CONTEXT_EXPECT))
    if ctx == CONTEXT_EXPECT:
        return "unpatched"
    b = read_bytes(LIB, OFFSET, 1)[0]
    if b == NEW_BYTE:
        # 上下文其余部分不变，仅立即数被改
        expect2 = bytearray(CONTEXT_EXPECT)
        expect2[3] = NEW_BYTE
        if ctx == bytes(expect2):
            return "patched"
    return "unknown"


def do_check():
    st = state()
    print("驱动文件: %s" % LIB)
    print("  存在: %s" % ("是" if os.path.exists(LIB) else "否"))
    if st == "missing":
        return
    b = read_bytes(LIB, OFFSET, 1)[0]
    print("  偏移 0x%X 当前字节: 0x%02x" % (OFFSET, b))
    print("  上下文 0x%X: %s" % (CONTEXT_OFFSET,
                                 " ".join("%02x" % x for x in
                                          read_bytes(LIB, CONTEXT_OFFSET,
                                                     len(CONTEXT_EXPECT)))))
    print("  状态: %s" % {"unpatched": "未打补丁 (flags=0x01 MAP_SHARED)",
                          "patched": "已打补丁 (flags=0x41 MAP_SHARED|MAP_32BIT)",
                          "unknown": "未知（驱动版本可能不同，偏移量需重新定位）"}[st])
    print("  备份: %s" % (BACKUP if os.path.exists(BACKUP) else "无"))


def do_apply():
    st = state()
    if st == "patched":
        print("已经是已打补丁状态，无需重复操作。")
        return 0
    if st == "unknown":
        print("!! 上下文校验失败：驱动版本可能不匹配，偏移量需重新定位。")
        print("   请先用 objdump 确认 mmap64 调用与 mov $0x1,%ecx 的位置。")
        return 1

    if not os.path.exists(BACKUP):
        shutil.copy2(LIB, BACKUP)
        print("已备份: %s" % BACKUP)

    with open(LIB, "r+b") as f:
        f.seek(OFFSET)
        f.write(bytes([NEW_BYTE]))
    print("已打补丁: 偏移 0x%X 的 0x%02x -> 0x%02x" % (OFFSET, OLD_BYTE, NEW_BYTE))
    print("  flags: MAP_SHARED(0x01) -> MAP_SHARED|MAP_32BIT(0x41)")
    print("还原命令: sudo python3 %s --restore" % os.path.abspath(__file__))
    return 0


def do_restore():
    if not os.path.exists(BACKUP):
        print("!! 未找到备份 %s，无法还原。" % BACKUP)
        return 1
    shutil.copy2(BACKUP, LIB)
    print("已从备份还原: %s -> %s" % (BACKUP, LIB))
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    cmd = sys.argv[1]
    if cmd == "--check":
        do_check()
        return 0
    if cmd == "--apply":
        return do_apply()
    if cmd == "--restore":
        return do_restore()
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())

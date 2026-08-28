#!/usr/bin/env python3
"""
patch_soname.py — 就地修改 ELF64 共享库的 DT_SONAME。

用途：为绕开 dlmopen 的静态 TLS 限制，我们需要在同一命名空间里加载
第二份 Vulkan loader。动态链接器是按 SONAME 判定"该库是否已加载"的，
因此只复制文件不够，必须改掉 SONAME，它才会被当成新库重新初始化
（从而拥有独立的 ICD 列表，不会把调用 dispatch 回我们的 shim）。

限制：新 SONAME 必须【不长于】原 SONAME（原地覆盖，不重建 .dynstr）。
      libvulkan.so.1 为 14 字符，故新名字取 ≤14，例如 libvk_gfs.so.1。

用法: python3 patch_soname.py <lib.so> <new_soname>
"""
import struct
import sys


def vaddr_to_off(loads, va):
    for vaddr, memsz, offset, filesz in loads:
        if vaddr <= va < vaddr + memsz:
            return offset + (va - vaddr)
    return None


def patch_soname(path, new_soname):
    with open(path, 'rb') as f:
        data = bytearray(f.read())

    if data[:4] != b'\x7fELF':
        raise SystemExit("不是 ELF 文件")
    if data[4] != 2:
        raise SystemExit("仅支持 ELF64（当前 EI_CLASS=%d）" % data[4])

    e_phoff = struct.unpack_from('<Q', data, 0x20)[0]
    e_phentsize = struct.unpack_from('<H', data, 0x36)[0]
    e_phnum = struct.unpack_from('<H', data, 0x38)[0]

    loads = []
    dyn_off = dyn_size = None
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type = struct.unpack_from('<I', data, off)[0]
        p_offset = struct.unpack_from('<Q', data, off + 0x08)[0]
        p_vaddr = struct.unpack_from('<Q', data, off + 0x10)[0]
        p_filesz = struct.unpack_from('<Q', data, off + 0x20)[0]
        p_memsz = struct.unpack_from('<Q', data, off + 0x28)[0]
        if p_type == 1:            # PT_LOAD
            loads.append((p_vaddr, p_memsz, p_offset, p_filesz))
        elif p_type == 2:          # PT_DYNAMIC
            dyn_off, dyn_size = p_offset, p_filesz

    if dyn_off is None:
        raise SystemExit("未找到 PT_DYNAMIC")

    strtab_va = soname_off_in_str = None
    pos = dyn_off
    while pos + 16 <= dyn_off + dyn_size:
        d_tag = struct.unpack_from('<q', data, pos)[0]
        d_val = struct.unpack_from('<Q', data, pos + 8)[0]
        if d_tag == 0:             # DT_NULL
            break
        if d_tag == 5:             # DT_STRTAB
            strtab_va = d_val
        elif d_tag == 14:          # DT_SONAME
            soname_off_in_str = d_val
        pos += 16

    if strtab_va is None:
        raise SystemExit("未找到 DT_STRTAB")
    if soname_off_in_str is None:
        raise SystemExit("未找到 DT_SONAME（该库可能没有设置 SONAME）")

    strtab_off = vaddr_to_off(loads, strtab_va)
    if strtab_off is None:
        raise SystemExit("无法把 DT_STRTAB 虚拟地址转换为文件偏移")

    target = strtab_off + soname_off_in_str
    end = data.index(b'\x00', target)
    old = data[target:end].decode('utf-8', errors='replace')

    if len(new_soname) > len(old):
        raise SystemExit("新 SONAME 过长: %d > %d（需 ≤ 原名长度）"
                         % (len(new_soname), len(old)))

    nb = new_soname.encode()
    data[target:target + len(nb)] = nb
    data[target + len(nb):end + 1] = b'\x00' * (end + 1 - target - len(nb))

    with open(path, 'wb') as f:
        f.write(bytes(data))

    print("SONAME: %r -> %r" % (old, new_soname))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(__doc__)
        raise SystemExit(1)
    patch_soname(sys.argv[1], sys.argv[2])

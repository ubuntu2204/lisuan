#!/usr/bin/env python3
"""从 Wine 内置 user32.dll 的 PE 导出表生成包装层 user32.def。

除 UpdateLayeredWindow / UpdateLayeredWindowIndirect（包装层自实现）外，全部
导出以 PE forwarder 转回 user32_orig.dll（内置 user32 的重命名副本）。纯标准
库解析 PE，不依赖 pefile。用法：
  gen-kk-user32-def.py <内置 user32.dll> <输出 user32.def>
License: Apache-2.0
"""
import struct
import sys


def main() -> None:
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, 'rb') as f:
        d = f.read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    if magic != 0x10b:
        raise SystemExit('错误：期望 PE32（i386）内置 user32.dll')
    ddir = opt + 96
    exp_rva, _ = struct.unpack_from('<II', d, ddir)
    secs = []
    so = opt + struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(nsec):
        o = so + i * 40
        vs, va, rs, pr = struct.unpack_from('<IIII', d, o + 8)
        secs.append((va, vs, rs, pr))

    def r2o(rva):
        for va, vs, rs, pr in secs:
            if va <= rva < va + max(vs, rs):
                return pr + (rva - va)
        raise SystemExit(f'错误：RVA 越界 {rva:#x}')

    e = r2o(exp_rva)
    base = struct.unpack_from('<I', d, e + 16)[0]
    nfunc, nnames = struct.unpack_from('<II', d, e + 20)
    addr_funcs, addr_names, addr_ords = struct.unpack_from('<III', d, e + 28)
    named = {}
    for i in range(nnames):
        nr = struct.unpack_from('<I', d, r2o(addr_names) + i * 4)[0]
        no = r2o(nr)
        nm = d[no:d.index(b'\0', no)].decode()
        ordv = struct.unpack_from('<H', d, r2o(addr_ords) + i * 2)[0]
        named[base + ordv] = nm  # 序号表存相对 base 的索引
    self_impl = {'UpdateLayeredWindow', 'UpdateLayeredWindowIndirect'}
    lines = ['LIBRARY user32', 'EXPORTS']
    skipped = 0
    for i in range(nfunc):
        ordv = base + i
        if ordv not in named:
            # dlltool 不支持纯序号 forwarder；KK/Qt/Wine 均按名字导入 user32，
            # 序号导出（legacy 内部 stub）无人使用，跳过。
            skipped += 1
            continue
        nm = named[ordv]
        if nm in self_impl:
            lines.append(f'    {nm}')
        else:
            lines.append(f'    {nm} = user32_orig.{nm}')
    with open(dst, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'已生成 {dst}：{nfunc - skipped} 个导出（自实现 {len(self_impl)} 个，'
          f'跳过纯序号 {skipped} 个）')


if __name__ == '__main__':
    main()
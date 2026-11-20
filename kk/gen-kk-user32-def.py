#!/usr/bin/env python3
"""从 Wine 内置 DLL 的 PE 导出表生成包装层 .def。

默认从 user32.dll 生成 user32.def：除 UpdateLayeredWindow /
UpdateLayeredWindowIndirect 外转发到 user32_orig.dll。可选参数支持 win32u：
  gen-kk-user32-def.py <内置 DLL> <输出 .def> [库名=Attname] [转发目标DLL] [自实现逗号列表]
例：
  gen-kk-user32-def.py win32u.dll kk-win32u.def win32u win32u_orig NtUserSetParent
纯标准库解析 PE，不依赖 pefile。License: Apache-2.0
"""
import struct
import sys


def main() -> None:
    src, dst = sys.argv[1], sys.argv[2]
    lib = sys.argv[3] if len(sys.argv) > 3 else "user32"
    orig = sys.argv[4] if len(sys.argv) > 4 else lib + "_orig"
    self_impl = set(sys.argv[5].split(",")) if len(sys.argv) > 5 else {
        "UpdateLayeredWindow", "UpdateLayeredWindowIndirect"}
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
    lines = [f'LIBRARY {lib}', 'EXPORTS']
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
            lines.append(f'    {nm} = {orig}.{nm}')
    with open(dst, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'已生成 {dst}：{nfunc - skipped} 个导出（自实现 {len(self_impl)} 个，'
          f'跳过纯序号 {skipped} 个）')


if __name__ == '__main__':
    main()
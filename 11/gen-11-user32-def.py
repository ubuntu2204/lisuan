#!/usr/bin/env python3
"""从 Wine 内置 user32.dll 生成 11 平台 GDI 包装层的 user32.def。

除 GetDC / GetDCEx / ReleaseDC（包装层自实现）外，全部导出以 PE forwarder
转回 user32_orig.dll。纯标准库解析 PE，不依赖 pefile。

用法：gen-11-user32-def.py <内置 user32.dll> <输出 user32.def>
License: Apache-2.0
"""
import struct
import sys


def main() -> None:
    src, dst = sys.argv[1], sys.argv[2]
    libname = sys.argv[3] if len(sys.argv) > 3 else 'user32'
    data = open(src, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    opt = pe + 24
    if struct.unpack_from('<H', data, opt)[0] != 0x10b:
        raise SystemExit('错误：期望 PE32（i386）内置 user32.dll')
    exp_rva, _ = struct.unpack_from('<II', data, opt + 96)
    secs = []
    so = opt + struct.unpack_from('<H', data, pe + 20)[0]
    for i in range(nsec):
        vs, va, rs, pr = struct.unpack_from('<IIII', data, so + i * 40 + 8)
        secs.append((va, vs, rs, pr))

    def r2o(rva):
        for va, vs, rs, pr in secs:
            if va <= rva < va + max(vs, rs):
                return pr + (rva - va)
        raise SystemExit(f'错误：RVA 越界 {rva:#x}')

    e = r2o(exp_rva)
    base = struct.unpack_from('<I', data, e + 16)[0]
    nfunc, nnames = struct.unpack_from('<II', data, e + 20)
    funcs, names, ords = struct.unpack_from('<III', data, e + 28)
    named = {}
    for i in range(nnames):
        nr = struct.unpack_from('<I', data, r2o(names) + i * 4)[0]
        no = r2o(nr)
        name = data[no:data.index(b'\0', no)].decode()
        ordv = struct.unpack_from('<H', data, r2o(ords) + i * 2)[0]
        named[base + ordv] = name

    self_impl = {'GetDC', 'GetDCEx', 'ReleaseDC'}
    lines = [f'LIBRARY {libname}', 'EXPORTS']
    for i in range(nfunc):
        ordv = base + i
        if ordv not in named:
            continue
        name = named[ordv]
        lines.append(f'    {name}' if name in self_impl
                     else f'    {name} = user32_orig.{name}')
    open(dst, 'w').write('\n'.join(lines) + '\n')
    print(f'已生成 {dst}：{nfunc} 个导出，自实现 {len(self_impl)} 个')


if __name__ == '__main__':
    main()

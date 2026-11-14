#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 Windows PE 的 RT_ICON 资源里提取应用图标（war3 副本）。

与 dreamina/extract-icon.py 同源（2026-11-14 复制），逻辑保持一致；
改动其中一个时请同步另一个。war3 用 --exe Platform.exe --name kkduizhan。

原版背景（2026-11-09）：dreamina-wine.desktop 原先的 Icon 指向豆包图标，而且
安装脚本从不把官方图标写入图标主题目录，于是桌面/应用列表显示不出即梦图标。
本工具在用户本机运行安装脚本时，从本地官方 exe 的资源段提取 16/24/32/48/
64/128/256 共 7 个 PNG 尺寸，写入 hicolor 图标主题；**不把厂商图标资源提交进
仓库**，也不依赖 icoutils / pefile / Pillow，只用 Python 标准库。

用法：
  python3 extract-icon.py --exe <安装器或主程序.exe> \
      [--icons-dir ~/.local/share/icons/hicolor] [--name dreamina] \
      [--largest /tmp/dreamina.png]
"""

from __future__ import annotations

import argparse
import mmap
import os
import struct
import sys
import zlib
from pathlib import Path

PNG_SIG = b"\x89PNG\r\n\x1a\n"
RT_ICON = 3
RT_GROUP_ICON = 14
MAX_ICON_BYTES = 8 * 1024 * 1024


class IconError(Exception):
    pass


def _dir_entries(data, off: int) -> list[tuple[int, int]]:
    """IMAGE_RESOURCE_DIRECTORY 的所有表项（含 named 与 ID 两类）。"""
    if off < 0 or off + 16 > len(data):
        return []
    named, by_id = struct.unpack_from("<HH", data, off + 12)
    count = named + by_id
    if count > 4096 or off + 16 + 8 * count > len(data):
        raise IconError("资源目录表项数量异常")
    return [struct.unpack_from("<II", data, off + 16 + 8 * i)
            for i in range(count)]


def _pe_layout(data):
    """返回 (资源段 RVA, 尺寸, 节表)，节表项 = (RVA, 跨度, 文件偏移)。"""
    if len(data) < 64 or data[:2] != b"MZ":
        raise IconError("不是 PE 文件（缺少 MZ 头）")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise IconError("不是 PE 文件（缺少 PE 签名）")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    if magic == 0x10B:      # PE32
        dd = pe + 24 + 96
    elif magic == 0x20B:    # PE32+
        dd = pe + 24 + 112
    else:
        raise IconError("未知可选头 magic：0x%x" % magic)
    if dd + 24 > len(data):
        raise IconError("可选头被截断")
    if struct.unpack_from("<I", data, dd - 4)[0] < 3:
        raise IconError("PE 没有资源数据目录")
    rsrc_rva, rsrc_size = struct.unpack_from("<II", data, dd + 16)
    sections = []
    for i in range(nsec):
        off = pe + 24 + optsz + i * 40
        if off + 40 > len(data):
            raise IconError("节表被截断")
        vsize, vaddr, raw_size, raw_off = struct.unpack_from(
            "<IIII", data, off + 8)
        sections.append((vaddr, max(vsize, raw_size), raw_off))
    return rsrc_rva, rsrc_size, sections


def _rva_to_offset(rva: int, sections) -> int | None:
    for vaddr, span, raw_off in sections:
        if vaddr <= rva < vaddr + span:
            return raw_off + (rva - vaddr)
    return None


def _png_size(blob: bytes) -> tuple[int, int] | None:
    if len(blob) < 33 or not blob.startswith(PNG_SIG) or blob[12:16] != b"IHDR":
        return None
    w, h = struct.unpack_from(">II", blob, 16)
    if not (0 < w <= 1024 and 0 < h <= 1024 and w == h):
        return None
    if blob[-8:-4] != b"IEND":
        return None
    return w, h


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def _make_png(w: int, h: int, rgba_rows: bytes) -> bytes:
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (PNG_SIG + _chunk(b"IHDR", ihdr) +
            _chunk(b"IDAT", zlib.compress(rgba_rows, 9)) +
            _chunk(b"IEND", b""))


def _dib_to_png(blob: bytes, want_w: int | None,
                want_h: int | None) -> tuple[int, int, bytes] | None:
    """把 ICO 里的 BITMAPINFOHEADER 图像转成 PNG（兼容以后版本改用 BMP 图标）。"""
    if len(blob) < 40:
        return None
    hdr_size = struct.unpack_from("<I", blob, 0)[0]
    if hdr_size not in (40, 108, 124):
        return None
    w, h2, planes, bpp, compression, _ = struct.unpack_from(
        "<iiHHII", blob, 4)
    if compression != 0 or bpp not in (1, 4, 8, 24, 32) or planes != 1:
        return None
    width = w or want_w
    height = (abs(h2) // 2) or want_h   # ICO 的 DIB 高度是 XOR+AND 两份
    if not width or not height or not (0 < width <= 1024 and 0 < height <= 1024):
        return None
    palette_count = 0 if bpp > 8 else (1 << bpp)
    palette_off = hdr_size
    xor_off = palette_off + 4 * palette_count
    stride = ((width * bpp + 31) // 32) * 4
    and_stride = ((width + 31) // 32) * 4
    and_off = xor_off + stride * height
    has_mask = and_off + and_stride * height <= len(blob)
    if xor_off + stride * height > len(blob):
        return None
    palette = []
    for i in range(palette_count):
        b, g, r, _ = struct.unpack_from("<BBBB", blob, palette_off + 4 * i)
        palette.append((r, g, b, 255))
    raw = bytearray()
    for y in range(height - 1, -1, -1):     # DIB 自下而上
        row = blob[xor_off + y * stride: xor_off + y * stride + stride]
        mask = (blob[and_off + y * and_stride:
                     and_off + y * and_stride + and_stride]
                if has_mask else None)
        raw.append(0)                        # PNG 每行 filter=0
        for x in range(width):
            if bpp == 32:
                b, g, r, a = row[x * 4:x * 4 + 4]
            elif bpp == 24:
                b, g, r = row[x * 3:x * 3 + 3]
                a = 255
            elif bpp == 8:
                r, g, b, a = palette[row[x]]
            elif bpp == 4:
                idx = (row[x // 2] >> (4 if x % 2 == 0 else 0)) & 0xF
                r, g, b, a = palette[idx]
            else:
                idx = (row[x // 8] >> (7 - x % 8)) & 1
                r, g, b, a = palette[idx]
            if mask is not None and (mask[x // 8] >> (7 - x % 8)) & 1:
                a = 0
            raw += bytes((r, g, b, a))
    return width, height, _make_png(width, height, bytes(raw))


def _to_png(blob: bytes, want_w: int | None,
            want_h: int | None) -> tuple[int, int, bytes] | None:
    if blob.startswith(PNG_SIG):
        size = _png_size(blob)
        if size:
            return size[0], size[1], blob
        return None
    return _dib_to_png(blob, want_w, want_h)


def _resource_images(data, rsrc_base: int,
                     sections) -> list[tuple[int, int, bytes]]:
    icons: dict[int, bytes] = {}
    groups: list[bytes] = []
    for rtype, off in _dir_entries(data, rsrc_base):
        if rtype & 0x80000000:
            continue
        kind = rtype & 0x7FFFFFFF
        if kind not in (RT_ICON, RT_GROUP_ICON):
            continue
        for name, off2 in _dir_entries(
                data, rsrc_base + (off & 0x7FFFFFFF)):
            if name & 0x80000000:
                continue
            rid = name & 0x7FFFFFFF
            for _lang, off3 in _dir_entries(
                    data, rsrc_base + (off2 & 0x7FFFFFFF)):
                if off3 & 0x80000000:
                    continue
                entry = rsrc_base + (off3 & 0x7FFFFFFF)
                if entry + 8 > len(data):
                    continue
                data_rva, size = struct.unpack_from("<II", data, entry)
                if not 0 < size <= MAX_ICON_BYTES:
                    continue
                file_off = _rva_to_offset(data_rva, sections)
                if file_off is None or file_off + size > len(data):
                    continue
                blob = bytes(data[file_off:file_off + size])
                if kind == RT_ICON:
                    icons.setdefault(rid, blob)
                else:
                    groups.append(blob)

    frames: list[tuple[int, int, int]] = []
    for blob in groups:
        if len(blob) < 6:
            continue
        _reserved, typ, count = struct.unpack_from("<HHH", blob, 0)
        if typ != 1 or count > 64 or 6 + 14 * count > len(blob):
            continue
        for i in range(count):
            w, h, _c, _r, _p, _bpp, _sz, rid = struct.unpack_from(
                "<BBBBHHIH", blob, 6 + 14 * i)
            frames.append((w or 256, h or 256, rid))

    found: dict[str, tuple[int, int, bytes]] = {}
    for w, h, rid in frames:
        blob = icons.get(rid)
        if not blob:
            continue
        conv = _to_png(blob, w, h)
        if conv:
            found["%dx%d" % (conv[0], conv[1])] = conv
    # 组图标目录缺项时，RT_ICON 自身仍可能是完整 PNG。
    for blob in icons.values():
        conv = _to_png(blob, None, None)
        if conv:
            found.setdefault("%dx%d" % (conv[0], conv[1]), conv)
    return list(found.values())


def _scan_pngs(data) -> list[tuple[int, int, bytes]]:
    """PE 资源解析失败时的兜底：全文件扫描 PNG 签名，只收正方形图标。"""
    found: dict[str, tuple[int, int, bytes]] = {}
    pos = 0
    while len(found) < 64:
        start = data.find(PNG_SIG, pos)
        if start < 0:
            break
        pos = start + 8
        iend = data.find(b"IEND", start + 8)
        if iend < 0:
            continue
        end = iend + 8
        if end - start > MAX_ICON_BYTES:
            continue
        blob = bytes(data[start:end])
        size = _png_size(blob)
        if size:
            found["%dx%d" % size] = (size[0], size[1], blob)
    return list(found.values())


def extract_images(path: Path) -> list[tuple[int, int, bytes]]:
    if not path.is_file():
        raise IconError("文件不存在：%s" % path)
    with path.open("rb") as fh, mmap.mmap(
            fh.fileno(), 0, access=mmap.ACCESS_READ) as data:
        images: list[tuple[int, int, bytes]] = []
        try:
            rsrc_rva, _size, sections = _pe_layout(data)
            rsrc_base = _rva_to_offset(rsrc_rva, sections)
            if rsrc_base is not None:
                images = _resource_images(data, rsrc_base, sections)
        except (IconError, struct.error, ValueError) as exc:
            print("警告：PE 资源解析失败（%s），改用原始 PNG 扫描" % exc,
                  file=sys.stderr)
        if not images:
            images = _scan_pngs(data)
        return images


def _atomic_write(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_bytes(payload)
    os.replace(tmp, path)


def write_images(images, hicolor: Path, name: str,
                 largest: str | None) -> None:
    if not images:
        raise IconError("在 exe 里没找到可用的正方形 PNG/DIB 图标")
    by_size: dict[str, bytes] = {}
    for w, h, png in images:
        if 8 <= w <= 1024 and w == h:
            key = "%dx%d" % (w, h)
            if key not in by_size or len(png) > len(by_size[key]):
                by_size[key] = png
    if not by_size:
        raise IconError("找到的图标帧都小于 8x8 或非正方形，不能作为桌面图标")
    biggest = max(by_size.items(), key=lambda kv: int(kv[0].split("x")[0]))
    for size, png in sorted(by_size.items(),
                            key=lambda kv: int(kv[0].split("x")[0])):
        target = hicolor / size / "apps" / ("%s.png" % name)
        _atomic_write(target, png)
        print("  %s  %d 字节" % (target, len(png)))
    # 无论官方包提供哪些尺寸，都保证主题名能解析到 256x256。
    target256 = hicolor / "256x256" / "apps" / ("%s.png" % name)
    _atomic_write(target256, biggest[1])
    print("  %s  最大尺寸 %s" % (target256, biggest[0]))
    if largest:
        _atomic_write(Path(largest), biggest[1])


def main() -> int:
    ap = argparse.ArgumentParser(
        description="从即梦 AI 官方 PE 中提取图标并写入 hicolor 主题")
    ap.add_argument("--exe", required=True, help="官方安装器或已安装主程序 .exe")
    ap.add_argument("--icons-dir", default=None,
                    help="hicolor 主题目录，默认 $XDG_DATA_HOME/icons/hicolor")
    ap.add_argument("--name", default="dreamina", help="图标主题名")
    ap.add_argument("--largest", default=None,
                    help="额外把最大尺寸 PNG 写到该路径（测试用）")
    args = ap.parse_args()

    xdg = os.environ.get("XDG_DATA_HOME")
    data_home = Path(xdg) if xdg and Path(xdg).is_absolute() \
        else Path.home() / ".local/share"
    hicolor = Path(args.icons_dir) if args.icons_dir \
        else data_home / "icons" / "hicolor"
    try:
        images = extract_images(Path(args.exe))
        write_images(images, hicolor, args.name, args.largest)
    except (IconError, OSError, ValueError, struct.error) as exc:
        print("错误：%s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

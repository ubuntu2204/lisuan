#!/usr/bin/env python3
"""
analyze_lowmem.py — 分析某个进程的 /proc/<pid>/maps，统计低 2GB / 低 4GB 占用。

用于判断：给 gf 打上 MAP_32BIT 补丁后 mmap 失败，是否因为进程【前 2GB】被占满。
MAP_32BIT 的语义是把映射放进进程地址空间的前 2GB（不是 4GB）。

用法:
  python3 analyze_lowmem.py <pid>
  python3 analyze_lowmem.py --watch <进程名关键字> [次数] [间隔秒]
"""
import os
import sys
import time
import subprocess


def analyze(pid):
    path = "/proc/%d/maps" % pid
    if not os.path.exists(path):
        return None
    low2 = 0        # < 2GB
    b24 = 0         # 2GB ~ 4GB
    total = 0
    vma = 0
    biggest = []    # 低 2GB 内最大的映射
    with open(path, "r", errors="ignore") as f:
        for line in f:
            parts = line.split()
            if "-" not in parts[0]:
                continue
            s_e = parts[0].split("-", 1)
            try:
                s = int(s_e[0], 16)
                e = int(s_e[1], 16)
            except ValueError:
                continue
            size = e - s
            total += size
            vma += 1
            if e <= 0x80000000:
                low2 += size
                biggest.append((size, s, e, parts[-1] if len(parts) > 5 else ""))
            elif e <= 0x100000000:
                b24 += size
    biggest.sort(reverse=True)
    return {
        "pid": pid, "vma": vma,
        "low2": low2, "b24": b24, "total": total,
        "biggest": biggest[:6],
    }


def show(r):
    if not r:
        print("  进程已退出或无 maps")
        return
    mb = 1048576.0
    print("  pid=%s  VMA=%d" % (r["pid"], r["vma"]))
    print("    低 2GB 已用 : %8.1f MB" % (r["low2"] / mb))
    print("    2~4GB 已用  : %8.1f MB" % (r["b24"] / mb))
    print("    总映射      : %8.1f MB" % (r["total"] / mb))
    print("    低2GB空闲约 : %8.1f MB  (MAP_32BIT 可用空间)" % ((0x80000000 - r["low2"]) / mb))
    if r["biggest"]:
        print("    --- 低 2GB 内最大映射 ---")
        for size, s, e, name in r["biggest"]:
            print("      %8.1f MB  0x%x-0x%x  %s" % (size / mb, s, e, name))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    if sys.argv[1] == "--watch":
        key = sys.argv[2] if len(sys.argv) > 2 else "MinionMasters.exe"
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 10
        t = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0
        for i in range(n):
            out = subprocess.run(["pgrep", "-f", key],
                                 capture_output=True, text=True).stdout.split()
            shown = False
            for p in out:
                try:
                    pid = int(p)
                except ValueError:
                    continue
                # 只看真正的游戏进程：排除命令行里恰好含关键字的 python/脚本自身
                try:
                    with open("/proc/%d/comm" % pid, "r") as cf:
                        comm = cf.read().strip()
                except OSError:
                    continue
                if "python" in comm.lower() or "sh" == comm.lower():
                    continue
                r = analyze(pid)
                if r:
                    print("===== [%d] comm=%s =====" % (i + 1, comm))
                    show(r)
                    shown = True
            if not shown:
                print("[%d] 未找到游戏进程(comm 匹配)" % (i + 1))
            time.sleep(t)
        return 0

    try:
        pid = int(sys.argv[1])
    except ValueError:
        print(__doc__)
        return 1
    show(analyze(pid))
    return 0


if __name__ == "__main__":
    sys.exit(main())

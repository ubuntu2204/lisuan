#!/usr/bin/env python3
"""诊断进程的图形环境、加载库、线程 CPU 与 llvmpipe 误用信号。"""

import argparse
import json
import os
import re
import time
from pathlib import Path


ENV_PREFIXES = (
    "VK_", "DXVK_", "PROTON_", "WINE", "WINEPREFIX", "LD_PRELOAD",
    "LD_LIBRARY_PATH", "__GLX", "LIBGL", "Steam", "STEAM_",
)
LIB_PATTERN = re.compile(
    r"vulkan_gf|vulkan_lvp|libLLVM|libSC|gf_shim|d3d11|dxgi|winex11|winewayland",
    re.IGNORECASE,
)


def read_text(path, default=""):
    try:
        return Path(path).read_text(errors="replace")
    except OSError:
        return default


def read_bytes(path, default=b""):
    try:
        return Path(path).read_bytes()
    except OSError:
        return default


def parse_stat(data):
    text = data.decode(errors="replace")
    left, _, right = text.rpartition(")")
    fields = right.split()
    if len(fields) < 13:
        raise ValueError("stat 字段不足")
    return {
        "state": fields[0],
        "ppid": int(fields[1]),
        "utime": int(fields[11]),
        "stime": int(fields[12]),
        "threads": int(fields[17]) if len(fields) > 17 else 0,
    }


def sample_process(pid):
    return parse_stat(read_bytes(f"/proc/{pid}/stat"))


def sample_threads(pid):
    result = {}
    task_dir = Path(f"/proc/{pid}/task")
    try:
        entries = list(task_dir.iterdir())
    except OSError:
        return result
    for entry in entries:
        try:
            stat = parse_stat(read_bytes(entry / "stat"))
        except (OSError, ValueError):
            continue
        result[entry.name] = {
            "name": read_text(entry / "comm").strip(),
            "ticks": stat["utime"] + stat["stime"],
            "state": stat["state"],
        }
    return result


def collect_environment(pid):
    values = {}
    for item in read_bytes(f"/proc/{pid}/environ").split(b"\0"):
        if b"=" not in item:
            continue
        key, value = item.split(b"=", 1)
        key = key.decode(errors="replace")
        if key.startswith(ENV_PREFIXES):
            values[key] = value.decode(errors="replace")
    return values


def collect_libraries(pid):
    libraries = set()
    for line in read_text(f"/proc/{pid}/maps").splitlines():
        if LIB_PATTERN.search(line):
            libraries.add(line.split(maxsplit=5)[-1])
    return sorted(libraries)


def collect_drm_fds(pid):
    result = []
    fd_dir = Path(f"/proc/{pid}/fd")
    try:
        entries = list(fd_dir.iterdir())
    except OSError:
        return result
    for entry in entries:
        try:
            target = os.readlink(entry)
        except OSError:
            continue
        if target.startswith("/dev/dri/"):
            result.append({"fd": entry.name, "target": target})
    return sorted(result, key=lambda item: int(item["fd"]))


def collect_status(pid):
    values = {}
    for line in read_text(f"/proc/{pid}/status").splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        if key in {"Name", "State", "VmPeak", "VmSize", "VmRSS", "RssAnon", "Threads"}:
            values[key] = value.strip()
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("--interval 必须大于 0")

    pid = args.pid
    if not Path(f"/proc/{pid}").exists():
        print(f"进程不存在: {pid}", file=os.sys.stderr)
        return 2

    before_process = sample_process(pid)
    before_threads = sample_threads(pid)
    started = time.monotonic()
    time.sleep(args.interval)
    elapsed = time.monotonic() - started
    after_process = sample_process(pid)
    after_threads = sample_threads(pid)
    clock_ticks = os.sysconf("SC_CLK_TCK")

    process_ticks = (
        after_process["utime"] + after_process["stime"]
        - before_process["utime"] - before_process["stime"]
    )
    thread_rows = []
    for tid, current in after_threads.items():
        previous = before_threads.get(tid)
        if not previous:
            continue
        delta = current["ticks"] - previous["ticks"]
        thread_rows.append({
            "tid": int(tid),
            "name": current["name"],
            "state": current["state"],
            "cpu_percent": round(delta / clock_ticks / elapsed * 100, 1),
        })
    thread_rows.sort(key=lambda row: row["cpu_percent"], reverse=True)

    environment = collect_environment(pid)
    libraries = collect_libraries(pid)
    llvmpipe_threads = [
        row for row in thread_rows
        if row["name"].lower().startswith("llvmpipe") and row["cpu_percent"] > 1
    ]
    has_lvp = any("vulkan_lvp" in item or "libLLVM" in item for item in libraries)
    has_gf = any("vulkan_gf" in item or "gf_shim" in item for item in libraries)
    warnings = []
    if llvmpipe_threads or has_lvp:
        warnings.append("检测到 llvmpipe/LLVM 信号，不能按硬件加速验收")
    if has_gf and (llvmpipe_threads or has_lvp):
        warnings.append("gf 与 lvp 同时存在，必须查 DXVK 实际选中的 adapter")
    if not has_gf and not has_lvp:
        warnings.append("未从 maps 确认 gf/lvp，需结合渲染日志判断")

    payload = {
        "pid": pid,
        "cmdline": read_bytes(f"/proc/{pid}/cmdline").decode(errors="replace").replace("\0", " ").strip(),
        "interval_seconds": round(elapsed, 3),
        "process_cpu_percent": round(process_ticks / clock_ticks / elapsed * 100, 1),
        "status": collect_status(pid),
        "environment": environment,
        "libraries": libraries,
        "drm_fds": collect_drm_fds(pid),
        "top_threads": thread_rows[:20],
        "llvmpipe_active_threads": len(llvmpipe_threads),
        "warnings": warnings,
    }

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(f"PID: {pid}")
        print(f"命令行: {payload['cmdline']}")
        print(f"进程 CPU: {payload['process_cpu_percent']}%")
        print(f"状态: {payload['status']}")
        print("环境变量:")
        for key, value in sorted(environment.items()):
            print(f"  {key}={value}")
        print("图形库:")
        for item in libraries:
            print(f"  {item}")
        print("DRM fd:")
        for item in payload["drm_fds"]:
            print(f"  {item['fd']} -> {item['target']}")
        print("Top threads:")
        for row in thread_rows[:15]:
            print(f"  {row['tid']:>7} {row['cpu_percent']:>6}% {row['name']} [{row['state']}]")
        print(f"活跃 llvmpipe 线程: {len(llvmpipe_threads)}")
        for warning in warnings:
            print(f"警告: {warning}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

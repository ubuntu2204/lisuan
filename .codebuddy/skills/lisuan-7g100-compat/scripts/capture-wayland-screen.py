#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在本机 GNOME Wayland 会话里非交互抓一帧【合成后】的真实画面。

背景（2026-11-14）：XWayland 下 xwd / ffmpeg x11grab / PIL ImageGrab 只能读
X11 backing store，实测连 1920x1080 root 全屏都是纯黑；GNOME Shell 的
org.gnome.Shell.Screenshot D-Bus 返回 "Screenshot is not allowed"；xdotool
模拟 Shift+Print（XTEST）到不了 Wayland 合成器，不会落盘。这些通道都不能用来
判断应用是否渲染、错位或黑屏。

唯一可用通道：org.gnome.Mutter.ScreenCast（D-Bus）+ PipeWire + GStreamer
pipewiresrc。**必须在同一条 D-Bus 连接内**完成 CreateSession → RecordMonitor →
订阅 PipeWireStreamAdded → Start → 抓帧 → Stop；用 gdbus 逐条调用会因为连接
断开被 Mutter 回收 session（对象路径随即 "不存在"）。

用法：
  capture-wayland-screen.py [-o /tmp/shot.png] [--connector DP-1]
  capture-wayland-screen.py --check      # 只读检查依赖与 D-Bus 接口
  capture-wayland-screen.py --help

依赖：python3-dbus、gstreamer1.0-tools、gstreamer1.0-plugins-good(pngenc)、
      gstreamer1.0-plugins-bad(pipewiresrc)、pipewire。
结束后一定会 Stop 会话，不留后台录屏；不做自动重试或守护。
License: Apache-2.0
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SV = "org.gnome.Mutter.ScreenCast"
DEFAULT_CONNECTOR = "DP-1"


def die(msg: str) -> None:
    print(f"错误：{msg}", file=sys.stderr)
    raise SystemExit(1)


def check_deps() -> None:
    missing = []
    try:
        import dbus  # noqa: F401
        import dbus.mainloop.glib  # noqa: F401
        from gi.repository import GLib  # noqa: F401
    except ImportError:
        missing.append("python3-dbus / python3-gi")
    if not shutil.which("gst-launch-1.0"):
        missing.append("gstreamer1.0-tools")
    else:
        for plugin in ("pipewiresrc", "pngenc", "videoconvert"):
            r = subprocess.run(["gst-inspect-1.0", plugin],
                               capture_output=True, text=True)
            if r.returncode != 0:
                missing.append(f"GStreamer 插件 {plugin}")
    if missing:
        die("缺少依赖：" + "、".join(missing))
    try:
        import dbus
        bus = dbus.SessionBus()
        if not bus.name_has_owner(SV):
            die(f"会话总线没有 {SV}（是否运行在 GNOME Wayland 会话？）")
    except SystemExit:
        raise
    except Exception as e:
        die(f"无法连接会话总线：{e}")


def current_connector() -> str:
    """优先从 Mutter DisplayConfig 读当前 connector，失败回退 DP-1。"""
    try:
        import dbus
        bus = dbus.SessionBus()
        disp = bus.get_object("org.gnome.Mutter.DisplayConfig",
                              "/org/gnome/Mutter/DisplayConfig")
        state = dbus.Interface(
            disp, "org.gnome.Mutter.DisplayConfig").GetCurrentState()
        name = str(state[1][0][0][0])
        if name:
            return name
    except Exception:
        pass
    return DEFAULT_CONNECTOR


def capture(out: Path, connector: str, timeout: int) -> None:
    import dbus
    from gi.repository import GLib

    bus = dbus.SessionBus()
    root = dbus.Interface(bus.get_object(SV, "/org/gnome/Mutter/ScreenCast"), SV)
    session_path = str(root.CreateSession({}))
    print(f"  session: {session_path}")
    sess_obj = bus.get_object(SV, session_path)
    sess = dbus.Interface(sess_obj, SV + ".Session")
    try:
        stream_path = str(sess.RecordMonitor(connector, {}))
    except dbus.exceptions.DBusException as e:
        raise SystemExit(f"RecordMonitor({connector}) 失败：{e}")
    print(f"  stream : {stream_path}")

    stream_obj = bus.get_object(SV, stream_path)
    node_ids: list[int] = []
    loop = GLib.MainLoop()

    def on_added(nid):
        node_ids.append(int(nid))
        loop.quit()
        return False

    stream_obj.connect_to_signal("PipeWireStreamAdded", on_added,
                                 dbus_interface=SV + ".Stream")
    GLib.timeout_add(8000, lambda: (loop.quit(), False)[1])
    try:
        sess.Start()
        loop.run()
        if not node_ids:
            die("8 秒内没收到 PipeWireStreamAdded（ScreenCast 未产出视频流）")
        node_id = node_ids[0]
        print(f"  pipewire node: {node_id}")
        out.parent.mkdir(parents=True, exist_ok=True)
        cmd = ["gst-launch-1.0", "-q", "pipewiresrc", f"path={node_id}",
               "num-buffers=1", "!", "videoconvert", "!", "pngenc", "!",
               "filesink", f"location={out}"]
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=timeout)
        if r.returncode != 0:
            die(f"GStreamer 抓帧失败（rc={r.returncode}）：{r.stderr[-500:]}")
        if not out.is_file() or out.stat().st_size < 4096:
            die(f"抓帧结果异常：{out}")
        print(f"  已保存：{out}（{out.stat().st_size} 字节）")
        print("  验收：用 PIL 看尺寸/颜色数，或直接用看图工具打开确认不是纯黑。")
    finally:
        try:
            sess.Stop()
            print("  session 已 Stop（未留下录屏会话）")
        except Exception as e:
            print(f"  警告：Stop 失败：{e}", file=sys.stderr)


def main() -> None:
    ap = argparse.ArgumentParser(
        description="GNOME Wayland 合成画面的非交互抓帧（Mutter ScreenCast + PipeWire）")
    ap.add_argument("-o", "--output", default="/tmp/wayland-screen.png",
                    help="输出 PNG 路径（默认 /tmp/wayland-screen.png）")
    ap.add_argument("--connector", default=None,
                    help="显示器 connector（默认从 Mutter DisplayConfig 自动读取）")
    ap.add_argument("--timeout", type=int, default=25,
                    help="GStreamer 抓帧超时秒数（默认 25）")
    ap.add_argument("--check", action="store_true",
                    help="只读检查依赖与 D-Bus 接口，不抓帧")
    args = ap.parse_args()
    # 必须在任何 SessionBus 创建之前挂好 GLib 主循环，否则订阅
    # PipeWireStreamAdded 会报 "connections must be attached to a main loop"。
    import dbus.mainloop.glib
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    check_deps()
    connector = args.connector or current_connector()
    print(f"  connector: {connector}")
    if args.check:
        print("  [OK] 依赖与 org.gnome.Mutter.ScreenCast 接口可用")
        return
    capture(Path(args.output).expanduser(), connector, args.timeout)


if __name__ == "__main__":
    main()

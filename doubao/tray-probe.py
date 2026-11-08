#!/usr/bin/env python3
"""豆包托盘菜单探针：合成一次真实右键，并判断菜单窗口是否被创建。

为什么需要这个工具
------------------
Ubuntu 24.04 / GNOME(Wayland) 的顶栏是 Wayland 原生 surface，``xdotool`` 的
XTEST 模拟点击到不了它；只有真实鼠标点击才会被 GNOME 转发。但为了能自动
复现/自检，本探针像 GNOME Shell 那样，用 XSendEvent 把按钮事件直接投递给
Wine systray 的托盘图标窗口（16x16，被 reparent 进 appindicator 宿主），
等价于 XEmbed 转发，Wine 侧会看到与真实点击完全一致的消息流：

    ButtonPress/ButtonRelease(detail=3)
      -> systray: tray_icon_wndproc msg 0x204 / 0x205
      -> notify_owner relaying 0x204
      -> send_inter_thread_message hwnd <Doubao_StatusTrayWindow> msg 8001 wp 8 lp 204

判定
----
豆包托盘菜单是 Chromium 自绘的独立顶层窗口，类名 ``Chrome_WidgetWin_1``，
尺寸固定 165x284（见 doubao/README.md 2.6.3）。本探针只做「看菜单在不在 +
不在则点一次 + 看有没有 165x284 窗口出现」，且只统计 IsViewable 的窗口，
**绝不主动关闭它** —— 关闭菜单正是让托盘菜单进入不可恢复坏态的触发动作
（关一次就再也不弹，重启客户端才恢复）。

若菜单已经在屏幕上，直接判 GOOD 且不再点击：右键菜单开着时再右键不会新建窗口
（窗口 id 不变），旧版只看「新建窗口集合差」会把这种正常状态误判成坏态，
让 tray-doctor.sh --fix 白白重启客户端（2026-11-08 续查发现并修正）。

用法:
    python3 tray-probe.py            # 右键一次并判定
    python3 tray-probe.py --list     # 只列出候选托盘图标窗口
    python3 tray-probe.py --timeout 8
"""
import argparse
import sys
import time

from Xlib import X, display
from Xlib.protocol import event as Xevent

MENU_W = 165
MENU_H = 284


def find_systray_icons(d, root):
    """找出 Wine systray 的 16x16 图标窗口（取层级最深的一个）。"""
    cands = []

    def walk(w, depth):
        try:
            geo = w.get_geometry()
        except Exception:
            return
        try:
            cls = w.get_wm_class()
        except Exception:
            cls = None
        clsname = cls[1] if cls and len(cls) > 1 else ""
        if geo.width == 16 and geo.height == 16 and clsname:
            cands.append((w, depth, clsname))
        try:
            for child in w.query_tree().children:
                walk(child, depth + 1)
        except Exception:
            pass

    walk(root, 0)
    cands.sort(key=lambda t: -t[1])
    return cands


def menu_windows(d, root):
    """当前【可见】的 165x284 托盘菜单窗口 id 集合。

    只看 IsViewable：客户端若把菜单窗口留成 UnMapped 空壳，不能算菜单正常。
    """
    found = set()

    def walk(w):
        try:
            geo = w.get_geometry()
        except Exception:
            return
        if geo.width == MENU_W and geo.height == MENU_H:
            try:
                if w.get_attributes().map_state == X.IsViewable:
                    found.add(w.id)
            except Exception:
                pass
        try:
            for child in w.query_tree().children:
                walk(child)
        except Exception:
            pass

    walk(root)
    return found


def send_button(d, win, button, press):
    """像 XEmbed 宿主那样把按钮事件 XSendEvent 给嵌入窗口。

    注意：Xevent.ButtonPress/Release 必须带 ``display=d.display``，
    否则 X server 收到的会是被篡改的坐标/类型（表现为只有 WM_MOUSEMOVE）。
    """
    root = d.screen().root
    geo = win.get_geometry()
    cx = geo.x + geo.width // 2
    cy = geo.y + geo.height // 2
    cls = Xevent.ButtonPress if press else Xevent.ButtonRelease
    ev = cls(
        display=d.display,
        type=X.ButtonPress if press else X.ButtonRelease,
        detail=button,
        time=X.CurrentTime,
        root=root,
        root_x=cx,
        root_y=cy,
        event_x=geo.width // 2,
        event_y=geo.height // 2,
        window=win,
        child=X.NONE,
        same_screen=True,
        state=0,
    )
    mask = X.ButtonPressMask | X.ButtonReleaseMask | X.ButtonMotionMask
    win.send_event(ev, event_mask=mask, propagate=False)
    d.sync()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="只列出候选图标窗口")
    ap.add_argument("--button", type=int, default=3, help="1=左 2=中 3=右")
    ap.add_argument("--timeout", type=float, default=6.0, help="等待菜单出现的秒数")
    args = ap.parse_args()

    try:
        d = display.Display()
    except Exception as e:
        print("无法打开 X display：%s" % e, file=sys.stderr)
        return 2
    root = d.screen().root

    cands = find_systray_icons(d, root)
    if args.list:
        for w, depth, cls in cands:
            print("0x%x depth=%d cls=%s" % (w.id, depth, cls))
        return 0

    if not cands:
        print("NOICON: 没找到 16x16 的 Wine systray 托盘图标窗口", file=sys.stderr)
        return 3

    win, depth, clsname = cands[0]
    before = menu_windows(d, root)
    if before:
        # 菜单已经显示在屏幕上时，再右键不会「新建」窗口（窗口 id 不变）。
        # 旧版探针只看 now-before，会把这种正常状态误判成坏态，进而让
        # tray-doctor.sh --fix 白白重启客户端。这里直接判定为可用。
        print("GOOD: 托盘菜单已在屏幕上 %s（未重复点击）"
              % sorted(hex(i) for i in before))
        return 0

    send_button(d, win, args.button, True)
    time.sleep(0.08)
    send_button(d, win, args.button, False)
    d.sync()

    deadline = time.time() + args.timeout
    while time.time() < deadline:
        now = menu_windows(d, root)
        if now:
            print("GOOD: 托盘菜单窗口已创建 %s" % sorted(hex(i) for i in now))
            return 0
        time.sleep(0.15)

    print("BAD: 右键已送达 Wine systray（图标窗口 0x%x cls=%s），"
          "但 %0.1f 秒内没有出现 %dx%d 菜单窗口 —— 托盘菜单处于不可恢复坏态"
          % (win.id, clsname, args.timeout, MENU_W, MENU_H))
    return 1


if __name__ == "__main__":
    sys.exit(main())

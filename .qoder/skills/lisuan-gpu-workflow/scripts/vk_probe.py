#!/usr/bin/env python3
"""通过 Vulkan loader 枚举物理设备，适合宿主与 Steam Runtime 对照诊断。"""

import argparse
import ctypes
import ctypes.util
import json
import struct
import sys


class VkApplicationInfo(ctypes.Structure):
    _fields_ = [
        ("sType", ctypes.c_int),
        ("pNext", ctypes.c_void_p),
        ("pApplicationName", ctypes.c_char_p),
        ("applicationVersion", ctypes.c_uint32),
        ("pEngineName", ctypes.c_char_p),
        ("engineVersion", ctypes.c_uint32),
        ("apiVersion", ctypes.c_uint32),
    ]


class VkInstanceCreateInfo(ctypes.Structure):
    _fields_ = [
        ("sType", ctypes.c_int),
        ("pNext", ctypes.c_void_p),
        ("flags", ctypes.c_uint32),
        ("pApplicationInfo", ctypes.POINTER(VkApplicationInfo)),
        ("enabledLayerCount", ctypes.c_uint32),
        ("ppEnabledLayerNames", ctypes.c_void_p),
        ("enabledExtensionCount", ctypes.c_uint32),
        ("ppEnabledExtensionNames", ctypes.c_void_p),
    ]


def parse_properties(buffer):
    raw = buffer.raw

    def text_at(offset):
        end = raw.find(b"\0", offset)
        return raw[offset:end if end >= 0 else len(raw)].decode(errors="replace")

    def uint_at(offset):
        return struct.unpack_from("<I", raw, offset)[0]

    spec_name = text_at(0)
    legacy_name = text_at(20)
    known = ("lisuan", "llvmpipe", "lavapipe", "nvidia", "geforce", "radeon",
             "intel", "arc", "amd", "mesa", "graphics")

    def looks_like_name(value):
        return len(value) >= 3 and all(32 <= ord(char) < 127 for char in value)

    if any(token in spec_name.lower() for token in known):
        return {
            "name": spec_name,
            "api_version": version_text(uint_at(256)),
            "driver_version": uint_at(260),
            "vendor_id": f"0x{uint_at(264):04x}",
            "device_id": f"0x{uint_at(268):04x}",
            "device_type": struct.unpack_from("<i", raw, 272)[0],
        }

    # 本机 gf/llvmpipe 实测为 api-first 布局：deviceName 从偏移 20 开始。
    return {
        "name": legacy_name if looks_like_name(legacy_name) else spec_name,
        "api_version": version_text(uint_at(0)),
        "driver_version": uint_at(4),
        "vendor_id": f"0x{uint_at(8):04x}",
        "device_id": f"0x{uint_at(12):04x}",
        "device_type": struct.unpack_from("<i", raw, 16)[0],
    }


def version_text(value):
    return f"{value >> 22}.{(value >> 12) & 0x3FF}.{value & 0xFFF}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="输出 JSON")
    parser.add_argument("loader", nargs="?", help="libvulkan 路径，默认使用系统 loader")
    args = parser.parse_args()

    loader_path = args.loader or ctypes.util.find_library("vulkan") or "libvulkan.so.1"
    try:
        vk = ctypes.CDLL(loader_path)
    except OSError as exc:
        print(f"无法加载 Vulkan loader {loader_path}: {exc}", file=sys.stderr)
        return 2

    vk.vkGetInstanceProcAddr.restype = ctypes.c_void_p
    vk.vkGetInstanceProcAddr.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

    def instance_function(name, restype, *argtypes):
        address = vk.vkGetInstanceProcAddr(None, name.encode())
        if not address:
            raise RuntimeError(f"loader 未提供 {name}")
        return ctypes.CFUNCTYPE(restype, *argtypes)(address)

    try:
        create_instance = instance_function(
            "vkCreateInstance", ctypes.c_int,
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
        )
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    app = VkApplicationInfo(
        1, None, b"lisuan-gpu-skill", 1, b"lisuan-gpu-skill", 1,
        (1 << 22) | (3 << 12),
    )
    extensions = (ctypes.c_char_p * 1)(b"VK_KHR_surface")
    create_info = VkInstanceCreateInfo(
        1, None, 0, ctypes.pointer(app), 0, None, 1,
        ctypes.cast(extensions, ctypes.c_void_p),
    )
    instance = ctypes.c_void_p()
    result = create_instance(ctypes.byref(create_info), None, ctypes.byref(instance))
    if result != 0:
        payload = {"ok": False, "stage": "vkCreateInstance", "result": result}
        print(json.dumps(payload, ensure_ascii=False) if args.json else
              f"vkCreateInstance(+VK_KHR_surface) -> {result}")
        return 1

    enumerate_devices = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p,
    )(vk.vkGetInstanceProcAddr(instance, b"vkEnumeratePhysicalDevices"))
    get_properties = ctypes.CFUNCTYPE(
        None, ctypes.c_void_p, ctypes.c_void_p,
    )(vk.vkGetInstanceProcAddr(instance, b"vkGetPhysicalDeviceProperties"))

    count = ctypes.c_uint32(0)
    result = enumerate_devices(instance, ctypes.byref(count), None)
    payload = {
        "ok": result == 0 and count.value > 0,
        "loader": loader_path,
        "stage": "vkEnumeratePhysicalDevices",
        "result": result,
        "count": count.value,
        "devices": [],
    }
    if result == 0 and count.value:
        devices = (ctypes.c_void_p * count.value)()
        result = enumerate_devices(instance, ctypes.byref(count), devices)
        payload["result"] = result
        for device in devices:
            buffer = ctypes.create_string_buffer(824)
            get_properties(device, buffer)
            payload["devices"].append(parse_properties(buffer))

    destroy = vk.vkGetInstanceProcAddr(instance, b"vkDestroyInstance")
    if destroy:
        ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)(destroy)(instance, None)

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(f"loader: {loader_path}")
        print("vkCreateInstance(+VK_KHR_surface) -> 0")
        print(f"vkEnumeratePhysicalDevices -> {payload['result']} count={payload['count']}")
        for device in payload["devices"]:
            print(
                f"设备: {device['name']} "
                f"vendor={device['vendor_id']} device={device['device_id']} "
                f"api={device['api_version']} type={device['device_type']}"
            )
    return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

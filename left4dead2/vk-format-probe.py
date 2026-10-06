#!/usr/bin/env python3
"""查询原生 64 位 gf ICD 对各深度格式的能力，定位 D16 失败发生在驱动还是 Venus 层。

用法：python3 vk-format-probe.py
"""
import ctypes as C

VK_FORMAT_D16_UNORM = 126
VK_FORMAT_D16_SFLOAT = 127
VK_FORMAT_X8_D24_UNORM_PACK32 = 129
VK_FORMAT_D24_UNORM_S8_UINT = 130
VK_FORMAT_D32_SFLOAT = 126 - 2  # 126? 实际 D32_SFLOAT=126，D16_UNORM=124
FORMATS = {
    "D16_UNORM": 124,
    "D16_SFLOAT": 125,
    "D32_SFLOAT": 126,
    "X8_D24_UNORM_PACK32": 129,
    "D24_UNORM_S8_UINT": 130,
    "D32_SFLOAT_S8_UINT": 129 + 6,
}

F_SAMPLED_IMAGE = 0x1
F_STORAGE_IMAGE = 0x2
F_STORAGE_IMAGE_ATOMIC = 0x4
F_UNIFORM_TEXEL_BUFFER = 0x8
F_STORAGE_TEXEL_BUFFER = 0x10
F_STORAGE_TEXEL_BUFFER_ATOMIC = 0x20
F_COLOR_ATTACHMENT = 0x40
F_COLOR_ATTACHMENT_BLEND = 0x80
F_DEPTH_STENCIL_ATTACHMENT = 0x100
F_BLIT_SRC = 0x200
F_BLIT_DST = 0x400
F_SAMPLED_IMAGE_FILTER_LINEAR = 0x800


class VkFormatProperties(C.Structure):
    _fields_ = [("linearTilingFeatures", C.c_uint32),
                ("optimalTilingFeatures", C.c_uint32),
                ("bufferFeatures", C.c_uint32)]


class VkApplicationInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p),
                ("pApplicationName", C.c_char_p), ("applicationVersion", C.c_uint32),
                ("pEngineName", C.c_char_p), ("engineVersion", C.c_uint32),
                ("apiVersion", C.c_uint32)]


class VkInstanceCreateInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p), ("flags", C.c_uint32),
                ("pApplicationInfo", C.POINTER(VkApplicationInfo)),
                ("enabledLayerCount", C.c_uint32), ("ppEnabledLayerNames", C.c_void_p),
                ("enabledExtensionCount", C.c_uint32), ("ppEnabledExtensionNames", C.c_void_p)]


def decode(bits):
    names = {
        F_SAMPLED_IMAGE: "SAMPLED_IMAGE",
        F_STORAGE_IMAGE: "STORAGE_IMAGE",
        F_COLOR_ATTACHMENT: "COLOR_ATTACHMENT",
        F_COLOR_ATTACHMENT_BLEND: "COLOR_ATTACHMENT_BLEND",
        F_DEPTH_STENCIL_ATTACHMENT: "DEPTH_STENCIL_ATTACHMENT",
        F_BLIT_SRC: "BLIT_SRC", F_BLIT_DST: "BLIT_DST",
        F_SAMPLED_IMAGE_FILTER_LINEAR: "FILTER_LINEAR",
    }
    return "|".join(n for b, n in names.items() if bits & b) or "(无)"


def main():
    vk = C.CDLL("libvulkan.so.1")

    ai = VkApplicationInfo(sType=0, pNext=None,
                           pApplicationName=b"probe", applicationVersion=0,
                           pEngineName=b"probe", engineVersion=0,
                           apiVersion=(1 << 22) | (3 << 12))
    ci = VkInstanceCreateInfo(sType=1, pNext=None, flags=0,
                              pApplicationInfo=C.pointer(ai),
                              enabledLayerCount=0, ppEnabledLayerNames=None,
                              enabledExtensionCount=0, ppEnabledExtensionNames=None)
    inst = C.c_void_p()
    r = vk.vkCreateInstance(C.byref(ci), None, C.byref(inst))
    if r != 0:
        raise SystemExit(f"vkCreateInstance 失败 rc={r}")
    print("instance ok")

    n = C.c_uint32(0)
    vk.vkEnumeratePhysicalDevices(inst, C.byref(n), None)
    if n.value == 0:
        raise SystemExit("无物理设备（ICD 未枚举到 GPU）")
    devs = (C.c_void_p * n.value)()
    vk.vkEnumeratePhysicalDevices(inst, C.byref(n), devs)
    print(f"物理设备数: {n.value}")

    # 取设备名
    class VkPhysicalDeviceProperties(C.Structure):
        _fields_ = [("apiVersion", C.c_uint32), ("driverVersion", C.c_uint32),
                    ("vendorID", C.c_uint32), ("deviceID", C.c_uint32),
                    ("deviceType", C.c_uint32), ("deviceName", C.c_char * 256),
                    ("pipelineCacheUUID", C.c_uint8 * 16),
                    ("limits", C.c_uint8 * 1024), ("sparseProperties", C.c_uint8 * 32)]

    for d in devs:
        p = VkPhysicalDeviceProperties()
        vk.vkGetPhysicalDeviceProperties(d, C.byref(p))
        print(f"设备: {p.deviceName.decode()} (vendor=0x{p.vendorID:04x} device=0x{p.deviceID:04x})")
        for name, fmt in FORMATS.items():
            fp = VkFormatProperties()
            vk.vkGetPhysicalDeviceFormatProperties(d, C.c_uint(fmt), C.byref(fp))
            print(f"  {name:24s} optimal={decode(fp.optimalTilingFeatures)}")
            print(f"  {'':24s} linear ={decode(fp.linearTilingFeatures)}")


if __name__ == "__main__":
    main()

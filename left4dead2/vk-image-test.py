#!/usr/bin/env python3
"""直接测试原生 64 位 gf ICD 能否创建深度附件，区分 D16 失败的责任方。

- 若 D16/D24S8/D32 全部创建失败 -> 驱动缺 DEPTH_STENCIL_ATTACHMENT 能力，Venus 无辜
- 若原生能建而游戏经 Venus 失败  -> Venus 层丢了能力，可改 virglrenderer

用法：VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json python3 vk-image-test.py
"""
import ctypes as C

F_DEPTH_STENCIL_ATTACHMENT = 0x100
F_COLOR_ATTACHMENT = 0x40

# 格式名 -> (VkFormat, 是否深度类)
FORMATS = [
    ("D16_UNORM", 124, True),
    ("D24_UNORM_S8_UINT", 130, True),
    ("D32_SFLOAT", 126, True),
    ("R8G8B8A8_UNORM", 37, False),
]

# usage 名 -> (usage, 需要深度能力)
USAGES = [
    ("DEPTH_STENCIL_ATTACHMENT", 0x8, True),
    ("DEPTH_STENCIL_ATTACHMENT|SAMPLED_IMAGE", 0x8 | 0x1, True),
    ("COLOR_ATTACHMENT", 0x10, False),
    ("TRANSFER_DST", 0x1000, False),
]


class VkFormatProperties(C.Structure):
    _fields_ = [("linear", C.c_uint32), ("optimal", C.c_uint32), ("buffer", C.c_uint32)]


class VkExtent3D(C.Structure):
    _fields_ = [("width", C.c_uint32), ("height", C.c_uint32), ("depth", C.c_uint32)]


class VkImageCreateInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p), ("flags", C.c_uint32),
                ("imageType", C.c_int), ("format", C.c_int),
                ("extent", VkExtent3D), ("tiling", C.c_int), ("usage", C.c_uint32),
                ("sharingMode", C.c_int), ("queueFamilyIndexCount", C.c_uint32),
                ("pQueueFamilyIndices", C.POINTER(C.c_uint32)),
                ("initialLayout", C.c_int)]


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


class VkMemoryType(C.Structure):
    _fields_ = [("propertyFlags", C.c_uint32), ("heapIndex", C.c_uint32)]


class VkMemoryProperties(C.Structure):
    _fields_ = [("memoryTypeCount", C.c_uint32), ("memoryTypes", VkMemoryType * 32),
                ("memoryHeapCount", C.c_uint32), ("memoryHeaps", C.c_uint8 * (16 * 16))]


RC = {0: "VK_SUCCESS", -1: "VK_ERROR_OUT_OF_HOST_MEMORY", -2: "VK_ERROR_OUT_OF_DEVICE_MEMORY",
      -9: "VK_ERROR_INCOMPATIBLE_DRIVER", -11: "VK_ERROR_FORMAT_NOT_SUPPORTED",
      -14: "VK_ERROR_FEATURE_NOT_PRESENT", -1002: "VK_ERROR_EXTENSION_NOT_PRESENT",
      -1000000004: "VK_ERROR_UNKNOWN"}


def main():
    vk = C.CDLL("libvulkan.so.1")
    ai = VkApplicationInfo(0, None, b"t", 0, b"t", 0, (1 << 22) | (3 << 12))
    ci = VkInstanceCreateInfo(1, None, 0, C.pointer(ai), 0, None, 0, None)
    inst = C.c_void_p()
    r = vk.vkCreateInstance(C.byref(ci), None, C.byref(inst))
    if r != 0:
        raise SystemExit(f"vkCreateInstance rc={r}")

    n = C.c_uint32(0)
    vk.vkEnumeratePhysicalDevices(inst, C.byref(n), None)
    if n.value == 0:
        raise SystemExit("无物理设备")
    devs = (C.c_void_p * n.value)()
    vk.vkEnumeratePhysicalDevices(inst, C.byref(n), devs)
    dev = C.c_void_p(devs[0])
    print(f"物理设备数 {n.value}")

    mp = VkMemoryProperties()
    vk.vkGetPhysicalDeviceMemoryProperties(dev, C.byref(mp))
    mtype = 0
    for i in range(mp.memoryTypeCount):
        if (mp.memoryTypes[i].propertyFlags & 0x1) and (mp.memoryTypes[i].heapIndex == 0):
            mtype = i
            break

    for fname, fmt, is_depth in FORMATS:
        fp = VkFormatProperties()
        vk.vkGetPhysicalDeviceFormatProperties(dev, C.c_int(fmt), C.byref(fp))
        dsa = bool(fp.optimal & F_DEPTH_STENCIL_ATTACHMENT)
        ca = bool(fp.optimal & F_COLOR_ATTACHMENT)
        print(f"\n== {fname} (VkFormat={fmt}) ==")
        print(f"   formatProperties: DEPTH_STENCIL_ATTACHMENT={dsa} COLOR_ATTACHMENT={ca}")
        for uname, usage, need_depth in USAGES:
            # 不信 formatProperties（gf 对 R8G8B8A8 都报无 COLOR_ATTACHMENT，显然不准），
            # 一律实测 vkCreateImage 的真实返回码
            ici = VkImageCreateInfo(9, None, 0, 2, fmt,
                                    VkExtent3D(400, 400, 1), 1, usage, 0, 0, None, 0)
            img = C.c_void_p()
            rc = vk.vkCreateImage(dev, C.byref(ici), None, C.byref(img))
            tag = "成功" if rc == 0 else "失败 " + RC.get(rc, str(rc))
            print(f"   createImage 400x400 usage={uname:38s} -> {tag}")
            if rc == 0:
                vk.vkDestroyImage(dev, img, None)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""直接测试原生 64 位 gf ICD 能否创建深度附件，区分 D16 失败的责任方。

- 若 D16/D24S8/D32 全部创建失败 -> 驱动缺 DEPTH_STENCIL_ATTACHMENT 能力，Venus 无辜
- 若原生能建而游戏经 venus 失败  -> Venus 层丢了能力，可改 virglrenderer

用法：VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json python3 vk-image-test.py

注：2026-10-07 修复——旧版没调用 vkCreateDevice 就用 physical device 调 vkCreateImage，
必然 VK_ERROR_INVALID_DEVICE 随后 core dump；并补上 vkAllocateMemory/
vkBindImageMemory 实测（有些驱动 image 能建但显存分配失败）。
"""
import ctypes as C

F_DEPTH_STENCIL_ATTACHMENT = 0x100
F_COLOR_ATTACHMENT = 0x40

MEM_PROPERTY_DEVICE_LOCAL = 0x1

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


class VkQueueFamilyProperties(C.Structure):
    _fields_ = [("queueFlags", C.c_uint32), ("queueCount", C.c_uint32),
                ("timestampValidBits", C.c_uint32),
                ("minImageTransferGranularity", VkExtent3D)]


class VkDeviceQueueCreateInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p), ("flags", C.c_uint32),
                ("queueFamilyIndex", C.c_uint32), ("queueCount", C.c_uint32),
                ("pQueuePriorities", C.POINTER(C.c_float))]


class VkDeviceCreateInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p), ("flags", C.c_uint32),
                ("queueCreateInfoCount", C.c_uint32),
                ("pQueueCreateInfos", C.POINTER(VkDeviceQueueCreateInfo)),
                ("enabledLayerCount", C.c_uint32), ("ppEnabledLayerNames", C.c_void_p),
                ("enabledExtensionCount", C.c_uint32), ("ppEnabledExtensionNames", C.c_void_p),
                ("pEnabledFeatures", C.c_void_p)]


class VkMemoryRequirements(C.Structure):
    _fields_ = [("size", C.c_uint64), ("alignment", C.c_uint64),
                ("memoryTypeBits", C.c_uint32)]


class VkMemoryAllocateInfo(C.Structure):
    _fields_ = [("sType", C.c_int), ("pNext", C.c_void_p),
                ("allocationSize", C.c_uint64), ("memoryTypeIndex", C.c_uint32)]


class VkMemoryType(C.Structure):
    _fields_ = [("propertyFlags", C.c_uint32), ("heapIndex", C.c_uint32)]


class VkMemoryProperties(C.Structure):
    _fields_ = [("memoryTypeCount", C.c_uint32), ("memoryTypes", VkMemoryType * 32),
                ("memoryHeapCount", C.c_uint32), ("memoryHeaps", C.c_uint8 * (16 * 16))]


RC = {0: "VK_SUCCESS", -1: "VK_ERROR_OUT_OF_HOST_MEMORY", -2: "VK_ERROR_OUT_OF_DEVICE_MEMORY",
      -4: "VK_ERROR_INITIALIZATION_FAILED", -9: "VK_ERROR_INCOMPATIBLE_DRIVER",
      -11: "VK_ERROR_FORMAT_NOT_SUPPORTED", -14: "VK_ERROR_FEATURE_NOT_PRESENT",
      -1002: "VK_ERROR_EXTENSION_NOT_PRESENT", -2: "VK_ERROR_OUT_OF_DEVICE_MEMORY",
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

    # --- 关键：必须先建 logical device 才能创建 image/分配内存 ---
    nq = C.c_uint32(0)
    vk.vkGetPhysicalDeviceQueueFamilyProperties(dev, C.byref(nq), None)
    qf = (VkQueueFamilyProperties * max(nq.value, 1))()
    vk.vkGetPhysicalDeviceQueueFamilyProperties(dev, C.byref(nq), qf)
    qidx = None
    for i in range(nq.value):
        if qf[i].queueFlags & 0x1:  # VK_QUEUE_GRAPHICS_BIT
            qidx = i
            break
    if qidx is None:
        raise SystemExit("无 graphics 队列族")
    prio = (C.c_float * 1)(1.0)
    qci = VkDeviceQueueCreateInfo(2, None, 0, qidx, 1, prio)
    dci = VkDeviceCreateInfo(3, None, 0, 1, C.pointer(qci), 0, None, 0, None, None)
    devl = C.c_void_p()
    r = vk.vkCreateDevice(dev, C.byref(dci), None, C.byref(devl))
    if r != 0:
        raise SystemExit(f"vkCreateDevice rc={r} —— 没有 logical device 无法测")
    print(f"logical device 已建（graphics 队列族 #{qidx}）")

    mp = VkMemoryProperties()
    vk.vkGetPhysicalDeviceMemoryProperties(dev, C.byref(mp))

    def pick_mtype(bits):
        for i in range(mp.memoryTypeCount):
            if (bits & (1 << i)) and (mp.memoryTypes[i].propertyFlags & MEM_PROPERTY_DEVICE_LOCAL):
                return i
        for i in range(mp.memoryTypeCount):
            if bits & (1 << i):
                return i
        return None

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
            rc = vk.vkCreateImage(devl, C.byref(ici), None, C.byref(img))
            if rc != 0:
                print(f"   createImage 400x400 usage={uname:38s} -> 失败 {RC.get(rc, rc)}")
                continue
            req = VkMemoryRequirements()
            vk.vkGetImageMemoryRequirements(devl, img, C.byref(req))
            mi = pick_mtype(req.memoryTypeBits)
            note = ""
            if mi is None:
                note = " / 无匹配 memoryType"
                print(f"   createImage 400x400 usage={uname:38s} -> 成功{note}")
            else:
                mai = VkMemoryAllocateInfo(5, None, req.size, mi)
                mem = C.c_void_p()
                rc2 = vk.vkAllocateMemory(devl, C.byref(mai), None, C.byref(mem))
                if rc2 != 0:
                    note = f" / allocateMemory 失败 {RC.get(rc2, rc2)}"
                    print(f"   createImage 400x400 usage={uname:38s} -> 成功{note}")
                else:
                    rc3 = vk.vkBindImageMemory(devl, img, mem, 0)
                    ok = "完整可用" if rc3 == 0 else "bind 失败 " + str(RC.get(rc3, rc3))
                    print(f"   createImage+alloc+bind {uname:24s} -> {ok} "
                          f"(size={req.size} mtype={mi})")
                    vk.vkFreeMemory(devl, mem, None)
            vk.vkDestroyImage(devl, img, None)

    vk.vkDestroyDevice(devl, None)
    vk.vkDestroyInstance(inst, None)


if __name__ == "__main__":
    main()

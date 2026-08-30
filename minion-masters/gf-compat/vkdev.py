import ctypes, sys, struct

path = sys.argv[1] if len(sys.argv) > 1 else "/usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
vk = ctypes.CDLL(path)

gpa = vk.vk_icdGetInstanceProcAddr
gpa.restype = ctypes.c_void_p
gpa.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

def fn(name):
    p = gpa(None, name.encode())
    return ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(p)

def fni(inst, name, restype_int=True):
    p = gpa(inst, name.encode())
    return p

class VkApplicationInfo(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p),
                ("pApplicationName", ctypes.c_char_p), ("applicationVersion", ctypes.c_uint32),
                ("pEngineName", ctypes.c_char_p), ("engineVersion", ctypes.c_uint32),
                ("apiVersion", ctypes.c_uint32)]

class VkInstanceCreateInfo(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("flags", ctypes.c_uint32),
                ("pApplicationInfo", ctypes.POINTER(VkApplicationInfo)),
                ("enabledLayerCount", ctypes.c_uint32), ("ppEnabledLayerNames", ctypes.c_void_p),
                ("enabledExtensionCount", ctypes.c_uint32), ("ppEnabledExtensionNames", ctypes.c_void_p)]

CreateInstance = fn("vkCreateInstance")
app = VkApplicationInfo(1, None, b"probe", 1, b"probe", 1, (1 << 22) | (3 << 12))
ci = VkInstanceCreateInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)
inst = ctypes.c_void_p()
r = CreateInstance(ctypes.byref(ci), None, ctypes.byref(inst))
print("CreateInstance ->", r)
assert r == 0

EnumDevs_p = gpa(inst, b"vkEnumeratePhysicalDevices")
EnumDevs = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(EnumDevs_p)
count = ctypes.c_uint32(0)
EnumDevs(inst, ctypes.byref(count), None)
print("phys count =", count.value)
assert count.value > 0
devs = (ctypes.c_void_p * count.value)()
EnumDevs(inst, ctypes.byref(count), devs)

# queue family count
GetQF_p = gpa(inst, b"vkGetPhysicalDeviceQueueFamilyProperties")
GetQF = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(GetQF_p)
qc = ctypes.c_uint32(0)
GetQF(devs[0], ctypes.byref(qc), None)
print("queue families =", qc.value)

# create device: 1 queue, no features, no extensions
class DQCI(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("flags", ctypes.c_uint32),
                ("queueFamilyIndex", ctypes.c_uint32), ("queueCount", ctypes.c_uint32),
                ("pQueuePriorities", ctypes.POINTER(ctypes.c_float))]

class DCI(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("flags", ctypes.c_uint32),
                ("queueCreateInfoCount", ctypes.c_uint32), ("pQueueCreateInfos", ctypes.POINTER(DQCI)),
                ("enabledLayerCount", ctypes.c_uint32), ("ppEnabledLayerNames", ctypes.c_void_p),
                ("enabledExtensionCount", ctypes.c_uint32), ("ppEnabledExtensionNames", ctypes.c_void_p),
                ("pEnabledFeatures", ctypes.c_void_p)]

prio = ctypes.c_float(1.0)
import os
if os.environ.get("MULTIQ"):
    prios = (ctypes.c_float * 2)(1.0, 1.0)
    dqs = (DQCI * 2)(DQCI(3, None, 0, 0, 1, ctypes.pointer(prio)),
                     DQCI(3, None, 0, 1, 1, ctypes.pointer(ctypes.byref(prios, 8) and ctypes.cast(ctypes.addressof(prios)+8, ctypes.POINTER(ctypes.c_float)).contents)))
    dq_arr = dqs
else:
    dq_arr = (DQCI * 1)(DQCI(3, None, 0, 0, 1, ctypes.pointer(prio)))

exts = [b"VK_EXT_attachment_feedback_loop_layout", b"VK_EXT_conservative_rasterization",
        b"VK_EXT_custom_border_color", b"VK_EXT_depth_clip_enable", b"VK_EXT_memory_priority",
        b"VK_EXT_non_seamless_cube_map", b"VK_EXT_robustness2", b"VK_EXT_transform_feedback",
        b"VK_EXT_vertex_attribute_divisor", b"VK_KHR_external_semaphore",
        b"VK_KHR_pipeline_library", b"VK_KHR_swapchain", b"VK_KHR_timeline_semaphore",
        b"VK_KHR_external_semaphore_fd"]
import os
if os.environ.get("FEWER_EXT"):
    exts = exts[:int(os.environ["FEWER_EXT"])]
arr = (ctypes.c_char_p * len(exts))(*exts)
dci = DCI(2, None, 0, len(dq_arr), dq_arr, 0, None, len(exts),
          ctypes.cast(arr, ctypes.c_void_p), None)

CreateDevice_p = gpa(inst, b"vkCreateDevice")
CreateDevice = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(CreateDevice_p)
dev = ctypes.c_void_p()
r = CreateDevice(devs[0], ctypes.byref(dci), None, ctypes.byref(dev))
print("CreateDevice ->", r, "dev=", dev)

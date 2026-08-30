import ctypes, sys, os

path = sys.argv[1] if len(sys.argv) > 1 else "/usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
vk = ctypes.CDLL(path)
gpa = vk.vk_icdGetInstanceProcAddr
gpa.restype = ctypes.c_void_p
gpa.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

class AppInfo(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p),
                ("pApplicationName", ctypes.c_char_p), ("applicationVersion", ctypes.c_uint32),
                ("pEngineName", ctypes.c_char_p), ("engineVersion", ctypes.c_uint32),
                ("apiVersion", ctypes.c_uint32)]

class InstCI(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("flags", ctypes.c_uint32),
                ("pApplicationInfo", ctypes.POINTER(AppInfo)),
                ("enabledLayerCount", ctypes.c_uint32), ("ppEnabledLayerNames", ctypes.c_void_p),
                ("enabledExtensionCount", ctypes.c_uint32), ("ppEnabledExtensionNames", ctypes.c_void_p)]

CreateInstance = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(
    gpa(None, b"vkCreateInstance"))
app = AppInfo(1, None, b"probe", 1, b"probe", 1, (1 << 22) | (3 << 12))
ci = InstCI(1, None, 0, ctypes.pointer(app), 0, None, 0, None)
inst = ctypes.c_void_p()
assert CreateInstance(ctypes.byref(ci), None, ctypes.byref(inst)) == 0

EnumDevs = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(
    gpa(inst, b"vkEnumeratePhysicalDevices"))
count = ctypes.c_uint32(0)
EnumDevs(inst, ctypes.byref(count), None)
assert count.value > 0
devs = (ctypes.c_void_p * count.value)()
EnumDevs(inst, ctypes.byref(count), devs)

B = ctypes.c_uint32
class Feat2(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("features", B * 55)]
class Feat11(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("f", B * 16)]
class Feat12(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("f", B * 47)]
class Feat13(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("f", B * 15)]

mode = os.environ.get("FMODE", "all1")
val = 1 if mode == "all1" else 0

f13 = Feat13(53, None, (B * 15)(*([val] * 15)))
f12 = Feat12(51, ctypes.cast(ctypes.byref(f13), ctypes.c_void_p), (B * 47)(*([val] * 47)))
f11 = Feat11(49, ctypes.cast(ctypes.byref(f12), ctypes.c_void_p), (B * 16)(*([val] * 16)))
f2 = Feat2(1000059000, ctypes.cast(ctypes.byref(f11), ctypes.c_void_p), (B * 55)(*([val] * 55)))

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
dq = (DQCI * 1)(DQCI(3, None, 0, 0, 1, ctypes.pointer(prio)))
exts = [b"VK_KHR_swapchain"]
arr = (ctypes.c_char_p * len(exts))(*exts)
dci = DCI(2, None, 0, 1, dq, 0, None, len(exts), ctypes.cast(arr, ctypes.c_void_p),
          ctypes.cast(ctypes.byref(f2), ctypes.c_void_p))

CreateDevice = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(
    gpa(inst, b"vkCreateDevice"))
dev = ctypes.c_void_p()
r = CreateDevice(devs[0], ctypes.byref(dci), None, ctypes.byref(dev))
print("FMODE=%s CreateDevice ->" % mode, r)

# --- vkCreateBuffer test ---
class BCI(ctypes.Structure):
    _fields_ = [("sType", ctypes.c_int), ("pNext", ctypes.c_void_p), ("flags", ctypes.c_uint32),
                ("size", ctypes.c_uint64), ("usage", ctypes.c_uint32), ("sharingMode", ctypes.c_int),
                ("queueFamilyIndexCount", ctypes.c_uint32), ("pQueueFamilyIndices", ctypes.c_void_p)]

if r == 0:
    CreateBuffer_p = gpa(inst, b"vkCreateBuffer")
    CreateBuffer = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(CreateBuffer_p)
    bci = BCI(12, None, 0, 20, 0x00000001, 0, 0, None)  # 20 bytes, USAGE_VERTEX_BUFFER
    buf = ctypes.c_void_p()
    rb = CreateBuffer(dev, ctypes.byref(bci), None, ctypes.byref(buf))
    print("vkCreateBuffer(20B VERTEX) ->", rb, "buf=", buf)

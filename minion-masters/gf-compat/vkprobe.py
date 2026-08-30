import ctypes, sys

path = sys.argv[1] if len(sys.argv) > 1 else "/usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
vk = ctypes.CDLL(path)

gpa = vk.vk_icdGetInstanceProcAddr
gpa.restype = ctypes.c_void_p
gpa.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

def getfn(inst, name):
    p = gpa(inst, name.encode())
    if not p:
        raise RuntimeError("no fn " + name)
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

CreateInstance = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(
    getfn(None, "vkCreateInstance"))

app = VkApplicationInfo(1, None, b"probe", 1, b"probe", 1, (1 << 22) | (3 << 12))
ci = VkInstanceCreateInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)

import sys as _s
print("===VKCALL START===", file=_s.stderr)
inst = ctypes.c_void_p()
r = CreateInstance(ctypes.byref(ci), None, ctypes.byref(inst))
print("vkCreateInstance ->", r, "inst=", inst)
if r != 0:
    sys.exit(1)

EnumDevs = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(
    getfn(inst, "vkEnumeratePhysicalDevices"))
count = ctypes.c_uint32(0)
r = EnumDevs(inst, ctypes.byref(count), None)
print("vkEnumeratePhysicalDevices ->", r, "count=", count.value)

if count.value:
    devs = (ctypes.c_void_p * count.value)()
    EnumDevs(inst, ctypes.byref(count), devs)
    GetProps = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)(
        getfn(inst, "vkGetPhysicalDeviceProperties"))
    buf = ctypes.create_string_buffer(1024)
    for d in devs:
        GetProps(d, buf)
        import struct
        api, drv, vid, did, dtype = struct.unpack_from("IIIII", buf, 0)
        name = buf[8:8+256].split(b"\0")[0]
        print("  dev:", name, "vendor=0x%x" % vid, "type=%d" % dtype)

Destroy = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)(
    getfn(inst, "vkDestroyInstance"))
Destroy(inst, None)

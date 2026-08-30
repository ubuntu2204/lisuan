import ctypes, sys, os
vk = ctypes.CDLL("libvulkan.so.1")
ST_APP, ST_INST, ST_DEVQ, ST_DEV, ST_MEMALLOC = 0, 1, 2, 3, 5
class A(ctypes.Structure):
    _fields_=[("sType",ctypes.c_uint32),("pNext",ctypes.c_void_p),("pApplicationName",ctypes.c_char_p),
              ("applicationVersion",ctypes.c_uint32),("pEngineName",ctypes.c_char_p),
              ("engineVersion",ctypes.c_uint32),("apiVersion",ctypes.c_uint32)]
class I(ctypes.Structure):
    _fields_=[("sType",ctypes.c_uint32),("pNext",ctypes.c_void_p),("flags",ctypes.c_uint32),
              ("pApplicationInfo",ctypes.POINTER(A)),("enabledLayerCount",ctypes.c_uint32),
              ("ppEnabledLayerNames",ctypes.c_void_p),("enabledExtensionCount",ctypes.c_uint32),
              ("ppEnabledExtensionNames",ctypes.c_void_p)]
class Q(ctypes.Structure):
    _fields_=[("sType",ctypes.c_uint32),("pNext",ctypes.c_void_p),("flags",ctypes.c_uint32),
              ("queueFamilyIndex",ctypes.c_uint32),("queueCount",ctypes.c_uint32),
              ("pQueuePriorities",ctypes.POINTER(ctypes.c_float))]
class D(ctypes.Structure):
    _fields_=[("sType",ctypes.c_uint32),("pNext",ctypes.c_void_p),("flags",ctypes.c_uint32),
              ("queueCreateInfoCount",ctypes.c_uint32),("pQueueCreateInfos",ctypes.POINTER(Q)),
              ("enabledLayerCount",ctypes.c_uint32),("ppEnabledLayerNames",ctypes.c_void_p),
              ("enabledExtensionCount",ctypes.c_uint32),("ppEnabledExtensionNames",ctypes.c_void_p),
              ("pEnabledFeatures",ctypes.c_void_p)]
class MA(ctypes.Structure):
    _fields_=[("sType",ctypes.c_uint32),("pNext",ctypes.c_void_p),
              ("allocationSize",ctypes.c_uint64),("memoryTypeIndex",ctypes.c_uint32)]
PD=IN=DE=ME=ctypes.c_void_p
vk.vkCreateInstance.restype=ctypes.c_int
vk.vkCreateInstance.argtypes=[ctypes.POINTER(I),ctypes.c_void_p,ctypes.POINTER(IN)]
vk.vkEnumeratePhysicalDevices.restype=ctypes.c_int
vk.vkEnumeratePhysicalDevices.argtypes=[IN,ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(PD)]
vk.vkGetPhysicalDeviceProperties.restype=None
vk.vkGetPhysicalDeviceProperties.argtypes=[PD,ctypes.c_void_p]
vk.vkCreateDevice.restype=ctypes.c_int
vk.vkCreateDevice.argtypes=[PD,ctypes.POINTER(D),ctypes.c_void_p,ctypes.POINTER(DE)]
vk.vkGetDeviceProcAddr.restype=ctypes.c_void_p
vk.vkGetDeviceProcAddr.argtypes=[DE,ctypes.c_char_p]
a=A(); a.sType=ST_APP; a.apiVersion=0x400000
i=I(); i.sType=ST_INST; i.pApplicationInfo=ctypes.cast(ctypes.byref(a),ctypes.POINTER(A))
ins=IN()
assert vk.vkCreateInstance(ctypes.byref(i),None,ctypes.byref(ins))==0
n=ctypes.c_uint32(0); vk.vkEnumeratePhysicalDevices(ins,ctypes.byref(n),None)
ds=(PD*n.value)(); vk.vkEnumeratePhysicalDevices(ins,ctypes.byref(n),ds)
buf=ctypes.create_string_buffer(1024); vk.vkGetPhysicalDeviceProperties(ds[0],buf)
print("设备:", ctypes.string_at(ctypes.byref(buf,20),256).split(b'\0')[0].decode(errors="replace"))
pr=ctypes.c_float(1.0); q=Q(); q.sType=ST_DEVQ; q.queueFamilyIndex=0; q.queueCount=1
q.pQueuePriorities=ctypes.cast(ctypes.byref(pr),ctypes.POINTER(ctypes.c_float))
d=D(); d.sType=ST_DEV; d.queueCreateInfoCount=1
d.pQueueCreateInfos=ctypes.cast(ctypes.byref(q),ctypes.POINTER(Q))
dev=DE()
r=vk.vkCreateDevice(ds[0],ctypes.byref(d),None,ctypes.byref(dev))
if r!=0:
    print("vkCreateDevice 失败 r=%d"%r); sys.exit(1)
AM=ctypes.CFUNCTYPE(ctypes.c_int,DE,ctypes.POINTER(MA),ctypes.c_void_p,ctypes.POINTER(ME))
MM=ctypes.CFUNCTYPE(ctypes.c_int,DE,ME,ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint32,ctypes.POINTER(ctypes.c_void_p))
FM=ctypes.CFUNCTYPE(None,DE,ME,ctypes.c_void_p)
alloc=AM(vk.vkGetDeviceProcAddr(dev,b"vkAllocateMemory"))
mmap_=MM(vk.vkGetDeviceProcAddr(dev,b"vkMapMemory"))
free=FM(vk.vkGetDeviceProcAddr(dev,b"vkFreeMemory"))
print("\n== 不同大小映射测试 (memoryTypeIndex=0) ==")
for MB in [4, 16, 64, 128, 256]:
    sz = MB*1024*1024
    ma=MA(); ma.sType=ST_MEMALLOC; ma.allocationSize=sz; ma.memoryTypeIndex=0
    m=ME()
    r=alloc(dev,ctypes.byref(ma),None,ctypes.byref(m))
    if r!=0: print("  %4dMB: 分配失败 r=%d"%(MB,r)); continue
    p=ctypes.c_void_p()
    r=mmap_(dev,m,0,sz,0,ctypes.byref(p))
    if r!=0: print("  %4dMB: 映射失败 r=%d"%(MB,r))
    else:
        ad=p.value or 0
        print("  %4dMB: 0x%x  %s"%(MB,ad,"✅ <4GB" if ad<0x100000000 else "❌ >=4GB"))
    free(dev,m,None)

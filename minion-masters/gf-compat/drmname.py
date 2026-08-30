import os, fcntl, struct, ctypes

DRM_IOCTL_VERSION = 0xC0406400

for node in ("/dev/dri/card1", "/dev/dri/renderD128"):
    try:
        fd = os.open(node, os.O_RDWR)
    except OSError as e:
        print(node, "open failed:", e)
        continue
    name = ctypes.create_string_buffer(64)
    date = ctypes.create_string_buffer(64)
    desc = ctypes.create_string_buffer(128)
    # struct drm_version: 3 ints, then 3 (len, ptr) pairs
    buf = bytearray(struct.pack("iii4x", 0, 0, 0) + struct.pack("QQ", 64, ctypes.addressof(name)) +
          struct.pack("QQ", 64, ctypes.addressof(date)) + struct.pack("QQ", 128, ctypes.addressof(desc)))
    ret = fcntl.ioctl(fd, DRM_IOCTL_VERSION, buf, True)
    major, minor, pl = struct.unpack_from("iii", buf, 0)
    print(node, "ret=", ret, "ver=%d.%d.%d" % (major, minor, pl),
          "name=", name.value, "date=", date.value, "desc=", desc.value[:60])
    os.close(fd)

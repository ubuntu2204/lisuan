import sys
p = sys.argv[1] if len(sys.argv) > 1 else "/run/host/usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
try:
    f = open(p, "rb"); f.seek(0x17A068); d = f.read(12)
except OSError as e:
    print("无法读取 %s: %s" % (p, e)); sys.exit(1)
print("路径: %s" % p)
print("上下文: %s" % " ".join("%02x" % b for b in d))
print("flags 字节: 0x%02x -> %s" % (d[3],
      "已打补丁 (MAP_SHARED|MAP_32BIT) ✓" if d[3] == 0x41 else
      "未打补丁 (MAP_SHARED) ✗" if d[3] == 0x01 else "未知 ✗"))

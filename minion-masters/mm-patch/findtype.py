import struct
# .NET assembly type def names via #Strings/#US heap is complex; simpler: grep UTF-8 strings in the interop dll for VivoxChatServiceAdapter
import glob, os
interop="/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters/BepInEx/interop"
# search Assembly-CSharp.dll (main game assembly) for the type name string
targets=[]
for f in glob.glob(interop+"/*.dll"):
    d=open(f,'rb').read()
    idx=d.find(b'VivoxChatServiceAdapter')
    if idx>=0:
        # extract surrounding string
        s=d[idx-200:idx+60]
        # find namespace-like preceding
        # get the type name with namespace: search back for a readable run
        st=d.find(b'Assets.Rework')
        if st>=0 and st<idx+500:
            seg=d[st:idx+len(b'VivoxChatServiceAdapter')]
            txt=seg.split(b'\x00')[0].decode('utf-8','replace')
            targets.append((os.path.basename(f), txt))
        else:
            seg=d[max(0,idx-100):idx+30]
            txt=seg.decode('utf-8','replace')
            targets.append((os.path.basename(f), txt))
for t in targets[:10]:
    print(t)

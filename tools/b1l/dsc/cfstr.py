# cfstr.py MACHO addr... : text of the CFString constants at these addresses (arm64e chained fixups: low 32 bits = target offset)
import struct, sys, subprocess
f=sys.argv[1]; d=open(f,'rb').read()
segs=[]
ncmds=struct.unpack_from('<I',d,16)[0]; o=32
for _ in range(ncmds):
    cmd,sz=struct.unpack_from('<II',d,o)
    if cmd==0x19:
        va,vs,fo,fs=struct.unpack_from('<QQQQ',d,o+24); segs.append((va,vs,fo,fs))
    o+=sz
def off(va):
    for a,vs,fo,fs in segs:
        if a<=va<a+fs: return fo+va-a
def rd(va,n): return d[off(va):off(va)+n]
for a in sys.argv[2:]:
    a=int(a,16); isa,flags,p,ln=struct.unpack_from('<QQQQ',rd(a,32))
    t=p&0xffffffff; t+=0x100000000 if t<0x100000000 else 0
    print(hex(a), repr(rd(t,ln).decode('utf-8','replace')))

# dscfind.py DIR substring... : local symbols whose name contains a substring
import mmap, os, struct, sys
d=sys.argv[1]; pats=[p.encode() for p in sys.argv[2:]]
f=open(os.path.join(d,'dyld_shared_cache_arm64e.symbols'),'rb'); mm=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
lso,lss=struct.unpack_from('<QQ',mm,0x48); nlo,nlc,sto,sts,eno,enc=struct.unpack_from('<IIIIII',mm,lso)
base=lso+nlo; strs=lso+sto
for i in range(nlc):
    strx,ntype,nsect,ndesc,nval=struct.unpack_from('<IBBHQ',mm,base+16*i)
    if ntype&0xe0: continue
    s=strs+strx; e=mm.find(b'\0',s); n=mm[s:e]
    if any(p in n for p in pats): print(hex(nval), n.decode(errors='replace'))

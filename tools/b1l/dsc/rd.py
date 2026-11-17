import glob,struct,mmap,sys
M=[]
for f in sorted(glob.glob('dyld_shared_cache_arm64e*')):
    if f.endswith('.symbols'): continue
    d=open(f,'rb'); mm=mmap.mmap(d.fileno(),0,access=mmap.ACCESS_READ)
    if mm[:7]!=b'dyld_v1': continue
    off,cnt=struct.unpack_from('<II',mm,0x10)
    for i in range(cnt):
        a,sz,fo,mp,ip=struct.unpack_from('<QQQII',mm,off+32*i); M.append((a,sz,fo,mm))
def rd(va,n):
    for a,sz,fo,mm in M:
        if a<=va<a+sz: return mm[fo+va-a:fo+va-a+n]

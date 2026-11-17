import struct, sys, uuid, plistlib
d=open(sys.argv[1]+'/timesync/0000000000000002.timesync','rb').read(); o=0; recs=[]; boot=None
while o+8<=len(d):
    sig=struct.unpack_from('<H',d,o)[0]
    if sig==0xbbb0:
        hs=struct.unpack_from('<H',d,o+2)[0]; boot=str(uuid.UUID(bytes=d[o+8:o+24])).upper(); wall=struct.unpack_from('<q',d,o+32)[0]
        recs.append((boot,0,wall)); o+=hs
    elif struct.unpack_from('<I',d,o)[0]==0x207354:
        ct,wall=struct.unpack_from('<Qq',d,o+8); recs.append((boot,ct,wall)); o+=32
    else: o+=8
for r in recs[-6:]: print(r)
ref=lambda r:{'ContinuousTime':r[1],'UUID':r[0],'WallTime':r[2]}
old={'OldestTimeRef':ref(recs[0])}
end=(recs[-1][0],recs[-1][1]+24000000*3600,recs[-1][2]+3600*10**9)  # timesync stops early; log show cuts at EndTimeRef
p={'OSArchiveVersion':4,'ArchiveIdentifier':str(uuid.uuid4()).upper(),'EndTimeRef':ref(end),
 'HighVolumeMetadata':old,'LiveMetadata':old,'PersistMetadata':old,'SignpostMetadata':old,'SpecialMetadata':old}
plistlib.dump(p,open(sys.argv[1]+'/Info.plist','wb'))

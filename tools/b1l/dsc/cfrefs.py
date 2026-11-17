# cfrefs.py MACHO DISASM from to : resolve each 'cfstring ref' add in a line range from its adrp
import re, sys, subprocess
lines=open(sys.argv[2]).read().split('\n')[int(sys.argv[3])-1:int(sys.argv[4])]
page={}; out=[]
for l in lines:
    m=re.match(r'([0-9a-f]+)\s+adrp\s+(x\d+), \S+ ; 0x([0-9a-f]+)',l)
    if m: page[m.group(2)]=int(m.group(3),16); continue
    m=re.match(r'([0-9a-f]+)\s+add\s+(x\d+), (x\d+), #0x([0-9a-f]+) ; Objc cfstring',l)
    if m and m.group(3) in page: out.append((m.group(1), hex(page[m.group(3)]+int(m.group(4),16))))
r=subprocess.run(['python3','-I',sys.argv[0].replace('cfrefs','cfstr'),sys.argv[1]]+[a for _,a in out],capture_output=True,text=True).stdout.split('\n')
for (pc,_),t in zip(out,r): print(pc,t)

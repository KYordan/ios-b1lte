# Symbolication tools for the iOS 26 shared cache (23G90)

All of them work on the original cache `~/Developer/s10-port/static-work/orig` (run from this folder).

- `dscsym.py DIR pc...`: finds the slide of the cache from a `B2L BT` stack (the return addresses must follow a BL/BLR), then gives the image and the offset of each address.
- `dscname.py DIR unslid_va...`: nearest local symbol (from `dyld_shared_cache_arm64e.symbols`).
- `rd.py`: reads an unslid address in the cache (`rd(va, n)`), used by the others.
- `iomfbexp.py DIR`: exports of IOMobileFramebuffer and the selector passed to IOConnectCall (heuristic `mov w1, #N`).

Disassembler: `~/Developer/tools/mamba/envs/s10/bin/llvm-mc --disassemble -triple=arm64e-apple-ios` (bytes in hex `0x..` on standard input).
Example: `cd ~/Developer/s10-port/static-work/orig && python3 ~/Developer/s10-port/b1l-tools/dsc/dscsym.py . 0x19f75b420 0x1a361b044 ...`

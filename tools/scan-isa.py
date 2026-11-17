#!/usr/bin/env python3
"""Count post-v8.2 AArch64 instructions in the real CODE sections of a dyld shared cache.

Usage: python3 -I scan-isa.py [--self-test] [--seed N] [cache_main_file]

Walks the image list of the main cache header, maps each image's Mach-O header to
(subcache file, offset), parses LC_SEGMENT_64 and scans only sections flagged
S_ATTR_PURE_INSTRUCTIONS / S_ATTR_SOME_INSTRUCTIONS (__text, __stubs, __auth_stubs ...),
deduplicated. Per family it reports the count, the number of distinct dylibs, the top
dylibs, and up to 8 random hits disassembled with llvm-mc (family flagged UNRELIABLE
if a sample does not decode as the expected mnemonic).
"""
import collections, glob, os, random, re, struct, subprocess, sys, time
from array import array
from itertools import compress

DEFAULT = os.path.join(os.environ.get("S10_PORT", os.path.expanduser("~/Developer/s10-port")), "static-work/orig/dyld_shared_cache_arm64e")
LLVM_MC = os.path.join(os.environ.get("PY_ENV", os.path.expanduser("~/Developer/tools/mamba/envs/s10")), "bin/llvm-mc")
MATTR = "+v8.6a,+complxnum,+rcpc-immo,+jsconv,+flagm,+sb,+dit,+ssbs,+pauth,+fp16"

# name -> list of (mask, value)
CLASSES = {
    "FJCVTZS (v8.3 JSCVT)":        [(0xfffffc00, 0x1e7e0000)],
    # FCMLA/FCADD vector: size 00 is reserved (see PRED)
    "FCMLA vector (v8.3 FCMA)":    [(0xbf20e400, 0x2e00c400)],
    "FCMLA by-element (v8.3 FCMA)": [(0xbf009400, 0x2f001000)],
    "FCADD vector (v8.3 FCMA)":    [(0xbf20ec00, 0x2e00e400)],
    "LDAPR (v8.3 LRCPC) [emulated]": [(0x3ffffc00, 0x38bfc000)],
    # size:011001:opc:0:imm9:00:Rn:Rt (GPR; SIMD LRCPC3 forms have bit26=1 and are not counted)
    "LDAPUR/STLUR (v8.4 LRCPC2)":  [(0x3f200c00, 0x19000000)],
    "CFINV (v8.4 FlagM)":          [(0xffffffff, 0xd500401f)],
    "RMIF (v8.4 FlagM)":           [(0xffe07c10, 0xba000400)],
    "SETF8/SETF16 (v8.4 FlagM)":   [(0xfffffc1f, 0x3a00080d), (0xfffffc1f, 0x3a00480d)],
    "AXFLAG (v8.5 FlagM2)":        [(0xffffffff, 0xd500405f)],
    "XAFLAG (v8.5 FlagM2)":        [(0xffffffff, 0xd500403f)],
    # scalar: bit15 = Z/X, bit16 = 32/64, bit22 = type s/d (old mask 0xffbcfc00 missed the X forms)
    "FRINT32/64 Z/X scalar (v8.5)": [(0xffbe7c00, 0x1e284000)],
    # vector: Q(30), U(29)=Z/X, sz(22), bit12 = 32/64
    "FRINT32/64 Z/X vector (v8.5)": [(0x9fbfec00, 0x0e21e800)],
    "SB (v8.5)":                   [(0xffffffff, 0xd50330ff)],
    "MSR DIT, #imm (v8.4)":        [(0xfffff0ff, 0xd503405f)],
    "MSR SSBS, #imm (v8.5)":       [(0xfffff0ff, 0xd503403f)],
    "PACGA (v8.3) [ref]":          [(0xffe0fc00, 0x9ac03000)],
}
NAMES = list(CLASSES)
FLAT = [(n, m, v) for n in NAMES for (m, v) in CLASSES[n]]

# expected llvm-mc mnemonic per family
EXPECT = {
    "FJCVTZS (v8.3 JSCVT)": r"fjcvtzs",
    "FCMLA vector (v8.3 FCMA)": r"fcmla",
    "FCMLA by-element (v8.3 FCMA)": r"fcmla",
    "FCADD vector (v8.3 FCMA)": r"fcadd",
    "LDAPR (v8.3 LRCPC) [emulated]": r"ldapr[bh]?",
    "LDAPUR/STLUR (v8.4 LRCPC2)": r"(ldapur|stlur)(s?[bhw])?",
    "CFINV (v8.4 FlagM)": r"cfinv",
    "RMIF (v8.4 FlagM)": r"rmif",
    "SETF8/SETF16 (v8.4 FlagM)": r"setf(8|16)",
    "AXFLAG (v8.5 FlagM2)": r"axflag",
    "XAFLAG (v8.5 FlagM2)": r"xaflag",
    "FRINT32/64 Z/X scalar (v8.5)": r"frint(32|64)[zx]",
    "FRINT32/64 Z/X vector (v8.5)": r"frint(32|64)[zx]",
    "SB (v8.5)": r"sb",
    "MSR DIT, #imm (v8.4)": r"msr",
    "MSR SSBS, #imm (v8.5)": r"msr",
    "PACGA (v8.3) [ref]": r"pacga",
}

# (size, opc) -> LDAPUR/STLUR opcode name; unlisted combos are unallocated
LDAPUR_OPS = {
    (0, 0): "STLURB", (0, 1): "LDAPURB", (0, 2): "LDAPURSB (64)", (0, 3): "LDAPURSB (32)",
    (1, 0): "STLURH", (1, 1): "LDAPURH", (1, 2): "LDAPURSH (64)", (1, 3): "LDAPURSH (32)",
    (2, 0): "STLUR W", (2, 1): "LDAPUR W", (2, 2): "LDAPURSW",
    (3, 0): "STLUR X", (3, 1): "LDAPUR X",
}
LDAPUR = "LDAPUR/STLUR (v8.4 LRCPC2)"


def _ldapur_ok(w):
    return ((w >> 30) & 3, (w >> 22) & 3) in LDAPUR_OPS


# extra predicates beyond mask/value
PRED = {
    "FCMLA vector (v8.3 FCMA)": lambda w: (w >> 22) & 3 != 0,
    "FCADD vector (v8.3 FCMA)": lambda w: (w >> 22) & 3 != 0,
    "FCMLA by-element (v8.3 FCMA)": lambda w: (w >> 22) & 3 in (1, 2),
    LDAPUR: _ldapur_ok,
}


def classify(w):
    return [n for n, m, v in FLAT if (w & m) == v and PRED.get(n, lambda _: True)(w)]


# 65536-entry prefilter on the high halfword (all masks cover >=16 high bits)
TAB = bytes(
    1 if any(((h & (m >> 16)) == (v >> 16)) for _, m, v in FLAT) else 0
    for h in range(65536)
)


def self_test():
    pos = {
        0x1e7e0020: "FJCVTZS (v8.3 JSCVT)",
        0x6e82c420: "FCMLA vector (v8.3 FCMA)",
        0x6e82cc20: "FCMLA vector (v8.3 FCMA)",
        0x6f821020: "FCMLA by-element (v8.3 FCMA)",
        0x6f827820: "FCMLA by-element (v8.3 FCMA)",
        0x6e82e420: "FCADD vector (v8.3 FCMA)",
        0x38bfc020: "LDAPR (v8.3 LRCPC) [emulated]",
        0x99400020: LDAPUR,
        0x19000020: LDAPUR,  # STLURB
        0xd9400020: LDAPUR,  # LDAPUR X
        0x19c00020: LDAPUR,  # LDAPURSB W
        0xd500401f: "CFINV (v8.4 FlagM)",
        0xba000400: "RMIF (v8.4 FlagM)",
        0x3a00080d: "SETF8/SETF16 (v8.4 FlagM)",
        0x3a00480d: "SETF8/SETF16 (v8.4 FlagM)",
        0xd500405f: "AXFLAG (v8.5 FlagM2)",
        0xd500403f: "XAFLAG (v8.5 FlagM2)",
        0x1e284020: "FRINT32/64 Z/X scalar (v8.5)",  # 32Z S
        0x1e28c020: "FRINT32/64 Z/X scalar (v8.5)",  # 32X S
        0x1e294020: "FRINT32/64 Z/X scalar (v8.5)",  # 64Z S
        0x1e69c020: "FRINT32/64 Z/X scalar (v8.5)",  # 64X D
        0x4e21e820: "FRINT32/64 Z/X vector (v8.5)",  # 32Z 4s
        0x4e61e820: "FRINT32/64 Z/X vector (v8.5)",  # 32Z 2d
        0x2e21e820: "FRINT32/64 Z/X vector (v8.5)",  # 32X 2s
        0x4e21f820: "FRINT32/64 Z/X vector (v8.5)",  # 64Z 4s
        0x6e61f820: "FRINT32/64 Z/X vector (v8.5)",  # 64X 2d
        0xd50330ff: "SB (v8.5)",
        0xd503415f: "MSR DIT, #imm (v8.4)",
        0xd503413f: "MSR SSBS, #imm (v8.5)",
        0x9ac23020: "PACGA (v8.3) [ref]",
    }
    for w, n in pos.items():
        assert n in classify(w), (hex(w), n, classify(w))
        assert TAB[w >> 16], hex(w)
    for w in (0xd503201f, 0xd65f03c0, 0x910003fd, 0x1e202800, 0x9ac20800, 0xd503233f,
              0x1e2a4020,   # opcode 0101xx, not FRINT32/64
              0x4e21d820,   # frintn-ish vector, not FRINT32/64
              0x99c00020, 0xd9800020):  # unallocated LDAPUR opc/size combos
        assert classify(w) == [], (hex(w), classify(w))
    for w in (0x6e02c420, 0x6f021020, 0x2f1e7000):  # size 00 / 11 reserved
        assert classify(w) == [], (hex(w), classify(w))
    assert LDAPUR_OPS[((0x99400020 >> 30) & 3, (0x99400020 >> 22) & 3)] == "LDAPUR W"
    print("self-test OK")


# ---------------------------------------------------------------- cache parsing
def read_maps(path):
    """Return [(addr, size, fileoff)] or None if not a dyld cache file."""
    with open(path, "rb") as f:
        hdr = f.read(0x18)
        if hdr[:7] != b"dyld_v1":
            return None
        off, cnt = struct.unpack_from("<II", hdr, 0x10)
        f.seek(off)
        return [struct.unpack("<QQQ", f.read(32)[:24]) for _ in range(cnt)]


def code_sections(files, maps):
    """Yield (image_path, file_idx, file_off, size) for every code section of every image."""
    with open(files[0], "rb") as f:
        h = f.read(0x1d0)
        mapoff = struct.unpack_from("<I", h, 0x10)[0]
        old_off, old_cnt = struct.unpack_from("<II", h, 0x18)  # legacy imagesOffset/Count
        new_off, new_cnt = struct.unpack_from("<II", h, 0x1c0)
        # validate: image table must sit past the mapping table
        ioff, icnt = (new_off, new_cnt) if new_cnt else (old_off, old_cnt)
        assert icnt and ioff >= mapoff, "bad image table"
        f.seek(ioff)
        imgs = [struct.unpack("<QQQII", f.read(32)) for _ in range(icnt)]

        def path_of(po):
            f.seek(po)
            return f.read(512).split(b"\0")[0].decode()

        paths = [path_of(i[3]) for i in imgs]

    def locate(addr):
        for fi, ms in enumerate(maps):
            for a, s, o in ms:
                if a <= addr < a + s:
                    return fi, o + addr - a
        return None

    fds = [open(p, "rb") for p in files]
    try:
        for (addr, *_), path in zip(imgs, paths):
            loc = locate(addr)
            assert loc, "image header not mapped: " + path
            fi, off = loc
            fd = fds[fi]
            fd.seek(off)
            mh = fd.read(32)
            magic, _, _, _, ncmds, sizeofcmds = struct.unpack_from("<6I", mh)
            assert magic == 0xfeedfacf, (path, hex(magic))
            cmds = fd.read(sizeofcmds)
            p = 0
            for _ in range(ncmds):
                cmd, csz = struct.unpack_from("<II", cmds, p)
                if cmd == 0x19:  # LC_SEGMENT_64
                    nsects = struct.unpack_from("<I", cmds, p + 64)[0]
                    for k in range(nsects):
                        s = p + 72 + 80 * k
                        saddr, ssize = struct.unpack_from("<QQ", cmds, s + 32)
                        flags = struct.unpack_from("<I", cmds, s + 64)[0]
                        if flags & (0x80000000 | 0x400) and ssize:
                            l = locate(saddr)
                            assert l, "section not mapped: " + path
                            yield path, l[0], l[1], ssize
                p += csz
    finally:
        for fd in fds:
            fd.close()


# ---------------------------------------------------------------- scanning
def disasm(words):
    """llvm-mc decode of 32-bit words; returns list of text lines (one per word)."""
    inp = "\n".join("0x%02x 0x%02x 0x%02x 0x%02x" % tuple(w.to_bytes(4, "little")) for w in words)
    r = subprocess.run([LLVM_MC, "--disassemble", "-triple=arm64", "-mattr=" + MATTR],
                       input=inp, capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        line = line.split("//")[0].strip()
        if line and not line.startswith("."):
            out.append(line)
    # undecodable words produce a warning and no line; align by marking
    return out if len(out) == len(words) else None


def main():
    args = sys.argv[1:]
    seed = 1
    if "--seed" in args:
        i = args.index("--seed")
        seed = int(args[i + 1])
        del args[i:i + 2]
    self_test()
    if "--self-test" in args:
        return
    random.seed(seed)
    base = next((a for a in args if not a.startswith("-")), DEFAULT)
    cand = [base] + sorted(p for p in glob.glob(base + ".*") if p.rsplit(".", 1)[1].isdigit())
    files, maps = [], []
    for p in cand:
        m = read_maps(p)
        if m is not None:
            files.append(p)
            maps.append(m)
    t0 = time.time()

    # collect and deduplicate code ranges (first image wins on overlap)
    secs = sorted(code_sections(files, maps), key=lambda x: (x[1], x[2]))
    ranges, ends = [], {}
    for path, fi, off, size in secs:
        start, end = (off + 3) & ~3, off + size
        start = max(start, ends.get(fi, 0))
        if end > start:
            ranges.append((path, fi, start, end - start))
            ends[fi] = end
    total_bytes = sum(r[3] for r in ranges)
    print("images with code sections: %d, ranges: %d (raw sections %d), %.1f MB of code"
          % (len({r[0] for r in ranges}), len(ranges), len(secs), total_bytes / 1e6), file=sys.stderr, flush=True)

    count = dict.fromkeys(NAMES, 0)
    per_dylib = {n: collections.Counter() for n in NAMES}
    samples = {n: [] for n in NAMES}  # reservoir of (path, fi, off, word)
    seen = dict.fromkeys(NAMES, 0)
    ld_split = collections.Counter()  # (op name, imm9==0)
    CH = 32 << 20
    fds = [open(p, "rb", buffering=0) for p in files]
    done_b = 0
    for path, fi, off, size in ranges:
        fd = fds[fi]
        pos = 0
        while pos < size:
            n = min(CH, size - pos) & ~3
            buf = bytearray(n)
            fd.seek(off + pos)
            if fd.readinto(buf) != n:
                raise IOError("short read " + files[fi])
            a = array("I")
            a.frombytes(buf)
            hi = array("H")
            hi.frombytes(buf)
            hi = hi[1::2]
            for i in compress(range(len(a)), map(TAB.__getitem__, hi)):
                w = a[i]
                for nm in classify(w):
                    count[nm] += 1
                    per_dylib[nm][path] += 1
                    seen[nm] += 1
                    s = samples[nm]
                    if len(s) < 8:
                        s.append((path, fi, off + pos + i * 4, w))
                    else:
                        j = random.randrange(seen[nm])
                        if j < 8:
                            s[j] = (path, fi, off + pos + i * 4, w)
                    if nm == LDAPUR:
                        ld_split[(LDAPUR_OPS[((w >> 30) & 3, (w >> 22) & 3)], ((w >> 12) & 0x1ff) == 0)] += 1
            pos += n
        done_b += size
        if len(ranges) > 1 and done_b // (200 << 20) != (done_b - size) // (200 << 20):
            print("  %6.0f MB / %.0f MB  %5.0fs" % (done_b / 1e6, total_bytes / 1e6, time.time() - t0),
                  file=sys.stderr, flush=True)
    for fd in fds:
        fd.close()

    print("Scanned only S_ATTR_PURE_INSTRUCTIONS/SOME_INSTRUCTIONS sections of %d images (%d subcache files)."
          % (len({r[0] for r in ranges}), len(files)))
    print("Code scanned: %d words (%.1f MB)\n" % (total_bytes // 4, total_bytes / 1e6))
    print("%-34s %9s %7s  top 3 dylibs" % ("family", "count", "#dylibs"))
    for n in NAMES:
        top = ", ".join("%s(%d)" % (os.path.basename(p), c) for p, c in per_dylib[n].most_common(3))
        print("%-34s %9d %7d  %s" % (n, count[n], len(per_dylib[n]), top))

    print("\nLDAPUR/STLUR split by opcode and imm9 (imm9==0 / imm9!=0):")
    tz = tn = 0
    for op in LDAPUR_OPS.values():
        z, nz = ld_split[(op, True)], ld_split[(op, False)]
        tz += z
        tn += nz
        print("  %-16s %9d %9d" % (op, z, nz))
    print("  %-16s %9d %9d  (total %d)" % ("ALL", tz, tn, tz + tn))

    print("\nTop 10 dylibs per family:")
    for n in NAMES:
        print(" %s (%d total, %d dylibs)" % (n, count[n], len(per_dylib[n])))
        for p, c in per_dylib[n].most_common(10):
            print("    %8d  %s" % (c, p))

    print("\nSample hits (up to 8 random per family), decoded with llvm-mc:")
    unreliable = []
    for n in NAMES:
        s = samples[n]
        print(" %s" % n)
        if not s:
            print("    (no hits)")
            continue
        txt = disasm([x[3] for x in s])
        if txt is None:  # some word failed to decode: do them one by one
            txt = [(disasm([x[3]]) or ["<<UNDECODABLE>>"])[0] for x in s]
        bad = 0
        for (p, fi, o, w), t in zip(s, txt):
            ok = re.match(r"(?:%s)\b" % EXPECT[n], t) is not None
            if n.startswith("MSR"):
                ok = ok and ("dit" if "DIT" in n else "ssbs") in t.lower()
            bad += not ok
            print("    %s+0x%08x  0x%08x  %-44s %s  [%s]" % (
                os.path.basename(files[fi])[-6:], o, w, t, "" if ok else "<-- MISMATCH", os.path.basename(p)))
        if bad:
            unreliable.append(n)
    print()
    for n in unreliable:
        print("UNRELIABLE: %s (sample did not decode as expected)" % n)
    if not unreliable:
        print("All families: samples decode as expected.")
    print("\nelapsed %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()

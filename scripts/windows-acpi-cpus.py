#!/usr/bin/env python3
"""Enable the S10e's secondary CPUs in the S5E9820 MADT and DSDT for Windows (dual boot, B2L_WINDOWS_ACPI=1).

    windows-acpi-cpus.py APIC.dsl DSDT_minimal.dsl [--cpus N]

The pinned beyond2lte-uefi overlay disabled CPU1..CPU7 while its authors hunted a reset on the S10+ (MADT GICC
UID 1..7 "Processor Enabled" 0, DSDT CPU1..CPU7 _STA Zero), so Windows on the S10e ran on CPU0 alone: one
Cortex-A55 at the ~1.05 GHz S-Boot leaves it. Their Linux ACPI test booted all eight CPUs through PSCI over SMC
(FACP ARM flags 0001, kept) with this table's MPIDRs (0-3 A55, 4-5 A75, 0x100-0x101 M4) and efficiency classes.

--cpus N (default 8, env B2L_WINDOWS_CPUS in build-boot-firmware.sh) enables CPU0..CPU(N-1) in both tables and
disables the rest, so the two tables always agree. The BCD "numproc" still limits how many Windows starts.
XNU ignores ACPI, so iOS is unaffected. Idempotent.
"""
import argparse
import re
from pathlib import Path

GICC_FLAGS = re.compile(
    r"(Processor UID : )([0-9A-Fa-f]{8})(\n\[[^\]]+\]\s+Flags \(decoded below\) : )([0-9A-Fa-f]{8})"
    r"(\n\s+Processor Enabled : )([01])")
CPU_STA = re.compile(
    r"(Device \(CPU(\d)\)\s*\{\s*Name \(_HID, \"ACPI0007\"[^\n]*\n\s*Name \(_UID, [^)]*\)[^\n]*\n"
    r"\s*Method \(_STA, 0, NotSerialized\)[^\n]*\n\s*\{\s*Return \()(Zero|0x0F)(\))")


def madt(text, cpus):
    seen = []

    def fix(m):
        uid = int(m.group(2), 16)
        on = uid < cpus
        flags = (int(m.group(4), 16) & ~1) | int(on)
        seen.append(uid)
        return f"{m.group(1)}{m.group(2)}{m.group(3)}{flags:08X}{m.group(5)}{int(on)}"

    out = GICC_FLAGS.sub(fix, text)
    if sorted(seen) != list(range(8)):
        raise SystemExit(f"APIC: expected GICC UIDs 0..7, found {sorted(seen)}")
    return out


def dsdt(text, cpus):
    seen = []

    def fix(m):
        n = int(m.group(2))
        seen.append(n)
        return m.group(1) + ("0x0F" if n < cpus else "Zero") + m.group(4)

    out = CPU_STA.sub(fix, text)
    if sorted(seen) != list(range(8)):
        raise SystemExit(f"DSDT: expected Device (CPU0)..(CPU7) with _STA, found {sorted(seen)}")
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("apic", type=Path)
    p.add_argument("dsdt", type=Path)
    p.add_argument("--cpus", type=int, default=8)
    a = p.parse_args()
    if not 1 <= a.cpus <= 8:
        p.error("--cpus must be 1..8")
    a.apic.write_text(madt(a.apic.read_text(), a.cpus))
    a.dsdt.write_text(dsdt(a.dsdt.read_text(), a.cpus))
    print(f"{a.apic.name}/{a.dsdt.name}: CPU0..CPU{a.cpus - 1} enabled, {8 - a.cpus} disabled")


if __name__ == "__main__":
    main()

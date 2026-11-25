#!/usr/bin/env python3
"""scripts/windows-acpi-cpus.py: MADT GICC flags and DSDT CPU _STA always agree."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/windows-acpi-cpus.py"
MPIDR = [0, 1, 2, 3, 4, 5, 0x100, 0x101]


def apic(enabled):
    out = ['[000h 0000 004h]                   Signature : "APIC"',
           "[028h 0040 004h]       Flags (decoded below) : 00000000",
           "                         PC-AT Compatibility : 0", ""]
    for uid in range(8):
        on = int(uid in enabled)
        out += [f"[034h 0052 004h]               Processor UID : {uid:08X}",
                f"[038h 0056 004h]       Flags (decoded below) : {on:08X}",
                f"                           Processor Enabled : {on}",
                "          Performance Interrupt Trigger Mode : 0",
                f"[070h 0112 008h]                   ARM MPIDR : {MPIDR[uid]:016X}", ""]
    return "\n".join(out)


def dsdt(enabled):
    devs = "".join(f"""
        Device (CPU{n})
        {{
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, {n})  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {{
                Return ({"0x0F" if n in enabled else "Zero"})
            }}
        }}
""" for n in range(8))
    return 'DefinitionBlock ("", "DSDT", 2, "SAMSUN", "S5E9820 ", 5)\n{\n    Scope (_SB)\n    {' + devs + "    }\n}\n"


class WindowsAcpiCpus(unittest.TestCase):
    def run_script(self, a, d, *args):
        with tempfile.TemporaryDirectory() as tmp:
            pa, pd = Path(tmp, "APIC.dsl"), Path(tmp, "DSDT_minimal.dsl")
            pa.write_text(a)
            pd.write_text(d)
            r = subprocess.run([sys.executable, str(SCRIPT), str(pa), str(pd), *args], capture_output=True, text=True)
            return r, pa.read_text(), pd.read_text()

    def test_overlay_single_cpu_becomes_eight(self):
        r, a, d = self.run_script(apic({0}), dsdt({0}))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(a.count("Processor Enabled : 1"), 8)
        self.assertEqual(a.count("Flags (decoded below) : 00000001"), 8)
        self.assertIn("Flags (decoded below) : 00000000\n                         PC-AT", a)  # MADT flags untouched
        self.assertEqual(d.count("Return (0x0F)"), 8)
        self.assertEqual(self.run_script(a, d)[1:], (a, d))  # idempotent

    def test_cpus_limit_disables_the_rest_in_both_tables(self):
        r, a, d = self.run_script(apic(set(range(8))), dsdt(set(range(8))), "--cpus", "4")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(a.count("Processor Enabled : 1"), 4)
        self.assertEqual(d.count("Return (0x0F)"), 4)
        self.assertNotIn("Processor Enabled : 1", a.split("Processor UID : 00000004")[1])

    def test_refuses_unexpected_tables(self):
        r, _, _ = self.run_script(apic({0}).replace("Processor UID : 00000007", "Processor UID : 00000009"), dsdt({0}))
        self.assertNotEqual(r.returncode, 0)
        r, _, _ = self.run_script(apic({0}), dsdt({0}).replace("Device (CPU7)", "Device (CPUX)"))
        self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()

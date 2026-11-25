#!/usr/bin/env python3
"""Add the Galaxy S10e touchscreen to the S5E9820 DSDT for Windows (dual boot, B2L_WINDOWS_ACPI=1).

    windows-acpi-devices.py DSDT_minimal.dsl

Appends two devices to Scope (_SB), the S9+ (exynos9810-woa) layout with the S10e's values:
  I2C0  _HID SAMS9820: HSI2C @0x104B0000 (stock DT hsi2c@104B0000, interrupts <0 0x184 0> -> SPI 388, GSIV 420).
        Only its own window: the firmware's B2LTouchDxe has set clocks, USI mode and pins, and the exynos9810-woa
        HSI2C driver leaves them alone when the node carries the HSI2C window alone.
  TSP0  _HID SCSY0761: sec_ts @0x48 on I2C0 (touchscreen@48), its interrupt gpa1-0 = wakeup EINT SPI 13 (GSIV 45),
        level low (sec,irq_type 0x2008), and the ALIVE pinctrl page 0x15850000 the touch driver acknowledges the
        EINT pending latch in. XNU ignores ACPI, so iOS is unaffected.
Idempotent: a DSDT that already has the devices is left as is.
"""
import sys
from pathlib import Path

MARK = "// S10e touchscreen for Windows"
DEVICES = r"""
        // S10e touchscreen for Windows (scripts/windows-acpi-devices.py; drivers: exynos9810-woa s10e)
        Device (I2C0)
        {
            Name (_HID, "SAMS9820")
            Name (_UID, 10)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x104B0000, 0x00001000)
                Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive)
                {
                    420
                }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (TSP0)
        {
            Name (_HID, "SCSY0761")
            Name (_UID, 0)
            Name (_DEP, Package (1)
            {
                \_SB.I2C0
            })
            Name (_CRS, ResourceTemplate ()
            {
                I2cSerialBusV2 (
                    0x0048,
                    ControllerInitiated,
                    400000,
                    AddressingMode7Bit,
                    "\\_SB.I2C0",
                    0x00,
                    ResourceConsumer,
                    ,
                    Exclusive
                )
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive)
                {
                    45
                }
                Memory32Fixed (ReadWrite, 0x15850000, 0x00001000)
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
"""


def main():
    path = Path(sys.argv[1])
    text = path.read_text()
    if MARK in text:
        print(f"{path.name}: touch devices already present")
        return
    end = text.rstrip()
    tail = "\n    }\n}"
    if not end.endswith(tail):
        raise SystemExit(f"{path}: unexpected end of DSDT (Scope (_SB) closing brace not found)")
    path.write_text(end[: -len(tail)] + "\n" + DEVICES + tail + "\n")
    print(f"{path.name}: I2C0 (SAMS9820) and TSP0 (SCSY0761) added")


if __name__ == "__main__":
    main()

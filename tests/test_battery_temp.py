#!/usr/bin/env python3
"""S10e build 30: B2LBatteryTemp.h (thermistor tables of the S10e DTB, charge permission), ASan/UBSan."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "B2LBatteryTemp.h"
static const uint16_t *tabs[3] = { b2l_temp_bat_adc, b2l_temp_chg_adc, b2l_temp_usb_adc };
int main() {
  for (const uint16_t *t : tabs) {
    for (int i = 2; i < B2L_TEMP_STEPS; i++) assert(t[i] > t[i - 1]);              // usable part rises
    for (int i = 1; i < B2L_TEMP_STEPS; i++) assert(b2l_temp_from_adc(t[i], t) == 900 - 50 * i);   // exact entries
    int last = 10000;
    for (int raw = t[1]; raw <= t[B2L_TEMP_STEPS - 1]; raw++) {                    // cooler as the ADC rises
      int c = b2l_temp_from_adc(raw, t);
      assert(c != B2L_TEMP_UNKNOWN && c <= last && c >= -200 && c <= 850);
      last = c;
    }
    assert(b2l_temp_from_adc(t[1] - 1, t) == B2L_TEMP_UNKNOWN);                  // 85 C or hotter
    assert(b2l_temp_from_adc(t[B2L_TEMP_STEPS - 1] + 1, t) == B2L_TEMP_UNKNOWN); // colder than -20 C / open
    assert(b2l_temp_from_adc(-1, t) == B2L_TEMP_UNKNOWN && b2l_temp_from_adc(0x1000, t) == B2L_TEMP_UNKNOWN);
    assert(b2l_temp_from_adc(0, t) == B2L_TEMP_UNKNOWN);                         // shorted
  }
  // battery table: 0x7cf is its 25 C entry, halfway to 0x8af (20 C) is 22.5 C
  assert(b2l_temp_from_adc(0x7cf, b2l_temp_bat_adc) == 250);
  assert(b2l_temp_from_adc((0x7cf + 0x8af) / 2, b2l_temp_bat_adc) == 225);
  struct b2l_charge_gate g = { 0, "", 0 };
  assert(b2l_charge_allowed(&g, 300, 300, 300) == 1);
  assert(b2l_charge_allowed(&g, 449, 300, 300) == 1);
  assert(b2l_charge_allowed(&g, 450, 300, 300) == 0 && !strcmp(g.why, "battery at 45 C"));
  assert(b2l_charge_allowed(&g, 430, 300, 300) == 0);                               // hysteresis
  assert(b2l_charge_allowed(&g, 420, 300, 300) == 1);
  assert(b2l_charge_allowed(&g, 50, 300, 300) == 0 && b2l_charge_allowed(&g, 69, 300, 300) == 0);
  assert(b2l_charge_allowed(&g, 70, 300, 300) == 1);
  // build 33: the charger is hot from 46 C (input lowered), stops only at 55 C
  assert(b2l_charge_allowed(&g, 300, 468, 300) == 1 && g.chg_hot == 1);              // build 32's 46.8 C
  assert(b2l_charge_allowed(&g, 300, 450, 300) == 1 && g.chg_hot == 1);              // hysteresis
  assert(b2l_charge_allowed(&g, 300, 440, 300) == 1 && g.chg_hot == 0);
  assert(b2l_charge_allowed(&g, 300, 550, 300) == 0 && g.chg_hot == 1);
  assert(b2l_charge_allowed(&g, 300, 501, 300) == 0);
  assert(b2l_charge_allowed(&g, 300, 500, 300) == 1);
  assert(b2l_charge_allowed(&g, 300, 440, 300) == 1 && g.chg_hot == 0);
  assert(b2l_charge_allowed(&g, 300, 300, 500) == 0 && b2l_charge_allowed(&g, 300, 300, 451) == 0);
  assert(b2l_charge_allowed(&g, 300, 300, 450) == 1);
  assert(b2l_charge_allowed(&g, B2L_TEMP_UNKNOWN, 300, 300) == 0);
  assert(b2l_charge_allowed(&g, 300, B2L_TEMP_UNKNOWN, 300) == 0);
  assert(b2l_charge_allowed(&g, 300, 300, B2L_TEMP_UNKNOWN) == 0);
  assert(b2l_charge_allowed(&g, 300, 300, 300) == 1);
  // build 32: currents by cable type and temperature; encodings as Samsung's driver
  unsigned in, cc;
  b2l_charge_limits(0x83, 300, 1, 0, &in, &cc); assert(in == 1500 && cc == 1000);   // wall charger
  b2l_charge_limits(0x83, 300, 1, 1, &in, &cc); assert(in == 1000 && cc == 1000);   // charger hot
  b2l_charge_limits(0x82, 300, 1, 0, &in, &cc); assert(in == 1500 && cc == 1000);   // charging host
  b2l_charge_limits(0x81, 300, 1, 0, &in, &cc); assert(in == 0 && cc == 0);         // computer port: S-Boot's
  b2l_charge_limits(0x80, 300, 1, 0, &in, &cc); assert(in == 0 && cc == 0);         // unknown type
  b2l_charge_limits(0x03, 300, 1, 0, &in, &cc); assert(in == 0 && cc == 0);         // no VBUS
  b2l_charge_limits(0x83, 300, 0, 0, &in, &cc); assert(in == 0 && cc == 0);         // not allowed
  b2l_charge_limits(0x83, 149, 1, 0, &in, &cc); assert(in == 0 && cc == 0);         // cool
  b2l_charge_limits(0x83, 411, 1, 0, &in, &cc); assert(in == 0 && cc == 0);         // warm
  b2l_charge_limits(0x83, B2L_TEMP_UNKNOWN, 1, 0, &in, &cc); assert(in == 0);
  assert(b2l_chg_cnfg09(0x0f, 400) == 0x0f);                                     // S-Boot's 400 mA
  assert(b2l_chg_cnfg09(0x0f, 1500) == 0x3b && b2l_chg_cnfg09(0x8f, 1500) == 0xbb);
  assert(b2l_chg_cnfg02(0x49, 450) == 0x49 && b2l_chg_cnfg02(0x49, 1000) == 0x54);
  puts("battery temperature: S10e DTB tables, unknown ends, charge limits with hysteresis, cable currents passed");
}
'''
with tempfile.TemporaryDirectory() as td:
    c = Path(td) / 't.cpp'; c.write_text(code); exe = Path(td) / 't'
    subprocess.run(['clang++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root / 'xnu-overlay/iokit/Kernel/arm'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

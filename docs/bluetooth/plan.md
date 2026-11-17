# S10 Bluetooth: review of the existing work and plan (2026-11-13)

Goal: the Bluetooth toggle in Settings turns on, search finds devices, pairing a headset survives a reboot, and audio switches to A2DP.

## 1. Review of the existing work

### Hardware (measured in the live DTB `device-s10/dump/live.dtb` and the Samsung 4.14 kernel `ref-kernel/k9820`)

| Item | Value | Source |
| --- | --- | --- |
| Chip | BCM4375, Bluetooth over UART HCI (H4), Wi-Fi over PCIe | prompt, `bcm43xx.c` |
| UART | `uart@10840000` (alias `uart1`), USI v2, `samsung,rts-gpio-control`, 256 B FIFO, IRQ 0x198 | live DTB |
| UART pins | gpq0-0..3, function 2, pull-up (pinctrl@15850000 +0xA0) | live DTB, `pinctrl-exynos-arm64.c` |
| BT_EN | gpm8-0, output (pinctrl@15C30000 +0x100) | DTB `/bluetooth gpios[0]` |
| BT_WAKE | gpg2-0, output (pinctrl@10430000 +0xC0) | `gpios[1]` |
| HOST_WAKE | gpa2-3, input, pull-down, rising edge (pinctrl@15850000 +0x40) | `gpios[2]` |
| Clock | CMU_PERIC1 0x10800000: `UART_BT_USER` mux +0x1c0 (bit 4: 0 = OSCCLK 26 MHz), divider +0x1814, gates +0x2020/+0x2098/+0x209c, QCH +0x3038 | `cmucal-sfr.c` |
| Power-up | BT_WAKE=1, BT_EN=1, 100 ms; power-down: BT_EN=0 | `bcm43xx_bt_rfkill_set_power` |
| Firmware | `bcm4375B0_semco.hcd`, `B0_murata`, `B1_semco`, `B1_semco_sem`, `B1_murata` already copied (outside git) into `device-s10/wifi-bt-fw/vendor-firmware/` | Wi-Fi extraction |

The choice of `.hcd` depends on the chip revision (B0/B1) and the module (Semco/Murata). The Wi-Fi session read
the revision (`B2L_WIFI_EP_ID`, OTP/CIS); to be confirmed before step 3.

### What the port already has
- `B2LPlatformExpert.cpp` publishes `mac-address-bluetooth0` (synthetic).
- `B2LWifi.cpp`: the model to follow (IOKit service on IOResources, kernel thread, `b2l_diag_record` log,
  firmware read from SYSTEM by `b2l_userspace_seed.h`). Nothing for Bluetooth, no access to CMU_PERIC1 or the UART.
- Nobody else touches gpq0, gpm8, gpg2 or gpa2 (B2LInput and B2LWifi use other banks of the same block).
- Warning: `B2L BT` in the log = kernel call stack. Bluetooth logs as `B2L BLUETOOTH`.

### References
- **ios-pipa** (QCA6390, iOS 26), summary "BLUETOOTH WORKS IN iOS": kernel = power + firmware + H4 point
  (`kernel control` + registry node `bluetooth` with `local-mac-address`, `transport-speed` as **4-byte OSData**);
  BlueTool replaced by an XPC responder (`RunBuiltinScript boot/init/deepsleep` → 0); hole at +0x60 in the H4 table
  of bluetoothd; disk sync every 10 s to keep pairings; UART at 921600 for A2DP.
  Their answers to the Broadcom vendor commands (0xfcxx) and 0x204a come from their chip being Qualcomm:
  **ours is a Broadcom, like Apple's**; bluetoothd may talk to it without these fakes. To be measured.
- **HakiOS** `drivers/bluetooth/hakios.bt.c`: only a read of the pins; useful for the method
  (one step per boot-arg level, never touch a block whose clock is off).

## 2. Plan

| Step | Content | Sign of success |
| --- | --- | --- |
| 1 (B1L-0210) | `B2LBluetooth.cpp`: read DT/pins/CMU; if the UART_BT clock is on OSCCLK and open: UART 115200 8N1 RTS/CTS, BT_EN, `HCI_Reset`, `Read_Local_Version`, then BT_EN=0 | `THE CHIP ANSWERS`, `manufacturer=0xf` |
| 2 | `.hcd` copied into SYSTEM (like `b2l-wifi/`) and read by the seed; `HCI_Download_Minidriver` (0xfc2e), `Write_RAM` records (0xfc4c), `Launch_RAM` (0xfc4e), 250 ms, `HCI_Reset`; version read back | `lmp_subver` changes after loading |
| 3 | Baud rate: `Update_UART_Baud_Rate` (0xfc18) + UART at 921600 then 3 Mbit/s (mux on CLKCMU_PERIC1_IP, to be read first) | `Read_Local_Version` answers at the new rate |
| 4 | H4 point for bluetoothd (kernel control + `bluetooth` node, `transport-speed` OSData), BlueTool replaced, receive thread on IRQ 0x198 or polling at 1 ms | `H4: connected`, Settings toggle |
| 5 | Search, pairing, A2DP, kept pairings (disk sync, to be checked with the storage session) | headset still paired after reboot |

Rules kept: one step per build, no writes to storage, no CMU register written at step 1,
every register write is logged with its previous value, `b2l_bt=0` turns everything off, `b2l_bt=1` = read-only.

## 3. Step 1: what goes into build 0210

File created: `xnu-overlay/iokit/Kernel/arm/B2LBluetooth.cpp` (checked: compiles without warnings with the options
of `B2LWifi.cpp`, `-Wall -Wextra`). To be added on the `xnu/` side by the session that builds (same form as B2LWifi):
- `xnu/iokit/conf/files`: `iokit/Kernel/arm/B2LBluetooth.cpp optional iokitcpp b2l`
- `xnu/iokit/KernelConfigTables.cpp`, after the `B2LWifi` line:
  `"   {'IOClass'=B2LBluetooth;'IOProviderClass'=IOResources;'IOMatchCategory'=B2LBluetooth;},"`

Sequence: when the service starts, a thread reads `/bluetooth`, the four pin banks and the CMU registers of UART_BT.
If a clock gate is off, or if the mux is not on OSCCLK, it stops there (no write). Otherwise it waits
for the Wi-Fi seed to be ready, then 30 s (so as not to raise BT_REG_ON during Wi-Fi startup), configures the UART,
powers the chip, sends `HCI_Reset` and `Read_Local_Version`, and powers it down again.

Hardware writes (all logged before/after): gpq0 CON/PUD (pins 0..3 only), gpm8-0 and gpg2-0
(DAT then CON), UART_BT registers (USI_CON, USI_OPTION, UINTM, UINTP, ULCON, UBRDIV, UFRACVAL, UCON, UFCON, UMCON).
No writes to storage, nor to CMU, PMU, or pins of another component.

Lines to read in the log:
- `B2L BLUETOOTH: /bluetooth present, gpios <…>` and `B2L BLUETOOTH PINS[boot]`;
- `B2L BLUETOOTH CMU: mux=… div=… gate … qch=…` then `UART_BT clock on, 26000000 Hz` (or the reason for stopping);
- `B2L BLUETOOTH UART[before]: …`;
- **`HCI_Reset -> 7 byte(s): 04 0e 04 01 03 0c 00`** and **`THE CHIP ANSWERS`**, then
  `hci=… rev=… lmp=… manufacturer=0xf lmp_subver=…` (0xf = Broadcom);
- on failure: `not sent (CTS …)` (the chip does not raise CTS) or `no HCI answer, uerstat=…`;
- `PINS[off]` with gpm8 dat bit 0 at 0, then `step 1 done`.

What must stay intact: Wi-Fi (join, IP), screen, GPU, touch, UFS. If any regression appears, `b2l_bt=0`.

### Result observed in build 0210 (log `out/logs/0210-boot.txt`)
Wi-Fi intact (join + IP). The step stopped before any write: the chip was not powered on.
- `/bluetooth missing`: the iOS DT is generated by `uefi/XnuLoader/DeviceTree.c`, which adds `/wlan` by hand but
  not `/bluetooth`. No effect: the addresses come from the live Android DTB.
- Starting values (to keep for any write): gpm8 con=0 dat=0 pud=0x1; gpg2 con=0 dat=0xc pud=0x1110111;
  gpa2 con=0 dat=0x14 pud=0x11101111 (hostwake 0); gpq0 con=0x2220000 dat=0x70 pud=0x3331111.
- CMU_PERIC1 UART_BT: **mux=0x10** (on CLKCMU_PERIC1_IP, baud rate unknown), mux2=0, div=0, rst/ipclk/pclk gates=0x200000
  (manual 0, value 1: open), qch=0x3f0002 (software request, clock forced).

## 4. Step 1b (next build)
Same file, two changes:
- `/bluetooth` is no longer required, it is only reported in the log.
- Before touching the UART: `UART_BT_USER` mux (0x108001c0) 0x10 → 0x00 (OSCCLK 26 MHz), wait for the BUSY bit, and
  set back to 0x10 at the end. This mux only feeds UART_BT. 26 MHz is enough up to 921600 bauds (0.75 % error),
  the rate ios-pipa chose for A2DP.
Extra expected lines: `UART_BT gates open, 26000000 Hz once on OSCCLK (mux now CLKCMU_PERIC1_IP)`,
`write cmu uart_bt mux +0x1c0 0x10 -> 0 (read back 0)` then, at the end, `… 0 -> 0x10 (read back 0x10)`.

### Result observed in build 0211: THE CHIP ANSWERS
Wi-Fi stayed connected. `HCI_Reset -> 04 0e 04 01 03 0c 00`, then ROM: **hci=9 rev=0 lmp=9 manufacturer=0xf (Broadcom)
lmp_subver=0x1111**. CTS raised by the chip (`umstat=0x11`). The mux went back to 0x10.
Harmless: the DAT writes read back unchanged while the pin is an input (the read gives the pin level; the lock applies when switching to output, `PINS[on]` does show dat=1). At the end, gpm8-0 and gpg2-0 stay
as outputs at 0: this is the state that the Linux `bcm43xx` driver sets at startup (`gpio_direction_output(…, 0)`).

## 5. Step 2 (next build): loading the firmware
- File: `bcm4375B1_semco.hcd` (sha256 8c8fb286…), copied to `device-s10/wifi-bt-fw/system/bt.hcd` (outside git);
  `system-add-fast.sh` already copies this whole folder into `SYSTEM:/b2l-wifi/`. Choice: Wi-Fi runs with
  `nvram.txt_CS01_semco_b1`, so chip **B1**, module **Semco**. Two candidates remain: `bcm4375B1_semco.hcd`
  ("Beyond E43 Ant0") and `_semco_sem.hcd` ("Beyond SEMCO BT EPA E43 Ant0", external amplifier). I took the
  first one, the base configuration; to be confirmed later by reading the selection rule in the `libbt-vendor` of the
  Android vendor partition (read-only).
- Kernel: `b2l_bt_fetch()` (in `B2LBluetooth.cpp`) reads `b2l-wifi/bt.hcd` at seed time. Two lines
  to add in `bsd/kern/b2l_userspace_seed.h`: the declaration and the call right after `b2l_wifi_fetch();`.
- Sequence, after the ROM version: `Read_Local_Name`, `Download_Minidriver` (0xfc2e), 50 ms, the 1261 records
  (1260 × `Write_RAM` 0xfc4c, then `Launch_RAM` 0xfc4e), each waiting for its Command Complete, 250 ms, `HCI_Reset`,
  version and name read back, then power-down as before. About 30 s at 115200 bauds.
- Cut-offs: `b2l_bt=2` = step 1 only (without firmware), `b2l_bt=1` read-only, `b2l_bt=0` nothing.
- Success: `THE FIRMWARE RUNS (lmp_subver 0x1111 -> …)` and `Read_Local_Name` = "BCM4375B1 Beyond E43 Ant0 …".

### Result observed in build 0212: THE FIRMWARE RUNS
1261 records sent in 17 s. After `Launch_RAM`: HCI revision **0 → 0xad**, name
"BCM4375B1 Beyond E43 Ant0 [Baseline: 0173]". `lmp_subver` stays 0x1111 on this chip, so the test wrongly displayed "not confirmed". Wi-Fi intact.

## 6. Step 3 (next build): the chip stays on and goes to bluetoothd
> **Paused (2026-11-14, owner's request)**: never flashed, parked in
> `~/Developer/s10-port/bluetooth-step3-parked/`. State and resumption: `docs/bluetooth/status.md`.
Reading of bluetoothd 23G90 (disassembly, read-only):
- `getenv("BT_HCI_TRANSPORT")` (0x10029798c), format `transport:driver:path` separated by `:`. Known transports:
  USB, H4DS, H4BC, **H4**, H5, APPLEBT, VIRT, PCIE, PCIESW, PCIESUN, APPLEPCIE; drivers: USB, **POSIX**, APPLEBT,
  VIRT, PCIE… Without the variable, bluetoothd reads a plist ("BT_HCI_TRANSPORT not set").
- The POSIX driver reads `name,speed` (0x100192f90); default name **`com.apple.uart.sk.bluetooth`**, opened by
  `socket(PF_SYSTEM)` + `ioctl(CTLIOCGINFO)`: it is a **kernel control**, not an IOKit service nor AppleConvergedIPC.
- bluetoothd asks BlueTool (XPC `com.apple.BlueTool`) for `RunBuiltinScript` boot/init/deepsleep and reads the
  `result` reply (0 = good), `firmware`, `manufacturer`.

What changes:
- **Corrected criterion**: the compared value is `lmp_subver<<16 | HCI revision`; success = it changed.
- **Kernel (`B2LBluetooth.cpp`)**: default level 4 = chip left powered on. After the firmware:
  `Write_BD_ADDR` (0xfc01) with the synthetic address from B2LPlatformExpert (locally administered, not the phone's),
  read back by `Read_BD_ADDR` (only the status is logged). Then kernel control
  `com.apple.uart.sk.bluetooth` (stream, unprivileged) and registry entry `bluetooth` (local-mac-address 6 bytes,
  transport-speed OSData 4 bytes = 115200, compatible `brcm,bcm4375`, `B2LBluetoothReady`). A thread relays by
  1 ms polling: UART RX FIFO → client, 16 KiB TX ring → UART.
- **Execution hook (`b2l_exec_bluetooth.h`)**: bluetoothd receives
  `BT_HCI_TRANSPORT=H4:POSIX:com.apple.uart.sk.bluetooth,115200` and `BT_BAUDRATE=115200`; BlueTool receives
  `DYLD_INSERT_LIBRARIES=/usr/local/lib/b2l-bluetool-v1.dylib` if the file exists, and its pid is added to the
  dyld policy of AMFI.
- **BlueTool replacement (`userspace/b2l-bluetool.c`, built by `scripts/build-bluetool-dylib.py`)**: interposes `getopt`; when BlueTool calls `getopt("c:hf:F:R")` at the top of main, the dylib listens on
  `com.apple.BlueTool` and never returns control. boot/init: waits (up to 60 s) for the kernel control to exist, then
  replies 0 (19 if the chip never came up); everything else: 0. Its log lines go through
  `setsockopt(0xb2)` on the kernel control.
- **Seed (`b2l_bt_seed.h`)**: `b2l_bt_fetch()` then publication of the dylib into `/usr/local/lib` (SYSTEM).
  **Nothing on USERDATA.**
- Cut-offs: `b2l_bt_ios=0` (neither dylib nor variables, the chip still stays on), `b2l_bt=3` (firmware
  then power-down, like 0212), `b2l_bt=2`, `b2l_bt=1`, `b2l_bt=0` as before.
- Known risks: BlueTool's sandbox (socket or XPC refused), bluetoothd crash at power-down (+0x60 hole
  in the H4 table seen by ios-pipa), 115200 bauds too slow for A2DP (921600 later), pairings to be synced to disk.

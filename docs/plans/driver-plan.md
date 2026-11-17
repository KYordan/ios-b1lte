# Galaxy S10 driver integration plan (beyond1lte, Exynos 9820)

**Goal:** integrate or adapt, from https://github.com/oosexgang/hakios-xnu-kernel, the drivers and kernel support for the Galaxy S10 (SM-G973F, beyond1lte, Exynos 9820): Wi-Fi, Bluetooth, audio (speaker, microphone, jack/USB-C), vibration/haptics, and possibly other missing peripherals so that iOS 26 can use the phone's hardware.

**Protocol:** speak French only. One change per build; a diagnostic may accompany that change. Document each build in `docs/journal/build-b1l-NNNN.md` and update the build tracking (`progress/`). Reason from evidence (log, photos, code read), prefer a decisive diagnostic over a guessed fix. Do not break what works (GPU Mali-G76 / HakiosMali, IOSurface / B2LSurface, DECON/IDMA display, HID touch, on-screen keyboard, UFS storage).

---

## 1. Current state (initial diagnosis)

### What already works (do not break)
- GPU Mali-G76 (HakiosMali, `B2LMali*` in `xnu-overlay/iokit/Kernel/arm/`)
- IOSurface (B2LSurface)
- DECON/IDMA framebuffer, display
- Touch (HID, `B2LInput.cpp`, `B2LHID.cpp`)
- On-screen keyboard
- UFS storage (`b2l_ufs_*.c` in `bsd/dev/`)
- `B2LWifi` exists (stage 1: pinctrl and `WL_EN`); no audio, Bluetooth or haptic driver exists yet in `xnu-overlay/` or in `xnu/`

### What the beyond1lte device tree already declares
Reference file: `ref-dts/exynos9820-beyond1lte_eur_open_26.dts` (compatible `samsung,BEYOND1LTE EUR OPEN 26`, `samsung,EXYNOS9820`). Nodes declared with `status = "okay"`:

| Node | Compatible | Role |
|------|-----------|------|
| `wlan` | `samsung,brcm-wlan` | Broadcom Wi-Fi |
| `bluetooth` | `samsung,bcm43xx` | Broadcom BCM43xx Bluetooth |
| `haptic` | (motor `LINEAR_1040`, regulator `VDD_MOTOR_3P3`, `pwm_id = 1`) | Vibration/haptics |
| `vib-pwm` | pins `gpp3-1` | Motor PWM |
| `motor-boost-en` | pins `gpg1-7` | Motor boost |
| `sound` | `samsung,beyond-madera` | Cirrus Logic Madera audio codec |
| `max77705` | `maxim,max77705` (`max77705,support-audio`) | Audio PMIC |
| `muic` | — | USB-C / jack detection |
| `BCM4773` | `ssp,BCM4773` | SSP sensor hub (SPI) |
| `qbt2000` | `qcom,qbt2000` | — |

⚠️ To verify at the first build: that the flashed DTB is indeed **beyond1lte** (and not beyond2lte) and that these nodes are present and enabled (`status = "okay"`).

### Proprietary firmware (to extract from the S10's Android system, absent from the repository)
- Wi-Fi: Broadcom firmware (`/vendor/firmware`)
- Bluetooth: BCM43xx firmware/HCD
- Audio: Madera codec + max77705 firmware/codec
- Haptics: linear motor waveforms

To be declared "missing" if the user cannot extract them.

---

## 2. Inventory of hakios-xnu-kernel (cloned repository)

All the drivers exist but **target the POCO X5 Pro (Qualcomm SM7325/SC7280)**, not the Exynos 9820. None of them can be grafted as is onto the S10.

| Subsystem | Hakios driver | Targeted hardware | S10 incompatibility |
|--------------|--------------|---------------|---------------------|
| `wifi` | `hakios.wifi.c`, `.wlfw`, `.lx`, `.sta`, `.apple80211`, `hakios.wlan.cpp` | WCN6750 (Qualcomm) + ath11k, SMEM/SMP2P/IPCC/RPMh | S10 = Broadcom BCM43xx |
| `bluetooth` | `hakios.bt.c` | WCN6750 BT half (UART/H4, read-only) | S10 = BCM43xx |
| `haptic` | `hakios.haptic.c` | Awinic AW86927 + LRA (I2C POCO X5 Pro) | S10 = `LINEAR_1040` motor |
| `audio` | `hakios.audio/.sound/.speaker/.mic/.adsp/.guard` | Qualcomm ADSP + AW88261 amplifiers on TDM | S10 = Cirrus Madera codec + max77705 |
| `gpu` | `hakios.adreno.c` | Adreno 642L (SC7280) | S10 = Mali-G76 (already done, HakiosMali) |
| `power` | `hakios.pmic/.battery/.cpufreq/.thermal` | SPMI arbiter, RPMh, SMEM (Qualcomm) | S10 = max77705 + Exynos power domains |
| `modem` | `hakios.mpss/.cell/.qmi/.nas/.ipa` | Qualcomm modem | S10 = Exynos modem, research only |
| `input` | `hakios.input/.goodix/.buttons` | Goodix GT9916 / FocalTech FT3680 (POCO X5 Pro) | S10 = touch already done (HID) |
| `core` | `hakios.platform/.socinfo/.version/.stubs` | Qualcomm SoC identity (SMEM item 137) | To adapt for Exynos 9820 |

**Planned use of hakios:** structure reference only — `bsd/dev` driver + IOKit object, progressive bring-up through a boot argument (`N=1..5`), firmware loading from `/fw`, clean error handling, read-only at the low levels. Each S10 driver will have to be written/adapted for the phone's real hardware.

---

## 3. Integration plan per build

State as of 10/11/2026: builds **0138** (diagnostic), **0139**, **140** (Wi-Fi stage 1) and **141** (synthetic DT) are done and tested. Numbering resumes at **142**. Deliverables use the format `docs/journal/build-b1l-NNNN.md`.

### Build 140 — Wi-Fi, stage 1: pinctrl and WL_EN (done)
- `B2LWifi` driver: check of `/wlan`, read of `gpg0`/`gpa0`, assertion of `cfg_wlanen` (`gpg0-5`).
- Result: iOS boot without regression, `WL_EN` asserted (`con=0x100000`, `dat=0x21`), `/wlan` absent from the synthetic DT (`dt=0`). The offsets `gpg0 = 0x080` (PERIC0) and `gpa0 = 0x000` (ALIVE) are confirmed by `pinctrl-exynos-arm64.c`.
- Limit: no PCIe/CMU/PMU/PHY driver and no Broadcom firmware; functional Wi-Fi remains blocked at this stage.
- Deliverable: `docs/journal/build-b1l-140.md`.

### Build 141 — Synthetic device tree: `/wlan` and pinctrl (done)
- Add `/wlan` (`samsung,brcm-wlan`, `gpios`, `pinctrl-0`) and the nodes `pinctrl@10430000` (`gpg0`, `cfg_wlanen`) and `pinctrl@15850000` (`gpa0`, `wlan_host_wake`) in `uefi/XnuLoader/DeviceTree.c`.
- No hardware access: the change makes the DT consistent with the hardware and provides the base that the following drivers will need.
- Result: `dt=1`, `gpios = <0x8e 5 0x1 0x134 7 0x1>`, iOS boot without regression.
- Deliverable: `docs/journal/build-b1l-141.md`.

### Build 142 — Wi-Fi, read-only audit of CMU/PMU/pinctrl (FSYS1)
- `B2LWifi` driver extended: read-only audit of the FSYS1 CMU (`0x13c00000`) — APB/DBI/MSTR/PCS/PHY QCH gates and PCIe clock-gating registers — plus the pinctrl registers already read.
- No access to the PCIe controller (ELBI/DBI/PHY) until the FSYS1 clocks and power domains are under control.
- Nothing is written: read only, to decide whether build 143 can start the PCIe clocks.
- Deliverable: `docs/journal/build-b1l-142.md`.

### Build 143 — Wi-Fi, enabling the PCIe GEN2 QCH gates (CMU FSYS1)
- Write ENABLE (bit 0 = 1) on the 5 PCIe GEN2 QCH gates of the FSYS1 CMU (APB `0x3034`, DBI `0x3038`, MSTR `0x303c`, PCS `0x3040`, PHY `0x3044`).
- Read-after-write to confirm that bit 0 = 1 on each register.
- No access to the PCIe controller (ELBI/DBI/PHY), nothing written elsewhere.
- Deliverable: `docs/journal/build-b1l-143.md`.

### Build 144 — Speaker audio (postponed)
- Cirrus Logic CS47L93 codec (`samsung,beyond-madera`, SPI2 at `0x10470000`, reset `gpg1-0`) and CS35L40 amplifiers (HSI2C38 at `0x109A0000`, addresses `0x40`/`0x41`), on the ABOX channels (UAIF/SIFS/RDMA).
- First step: availability diagnostic (ABOX clocks, SPI2/HSI2C38 pinmux, identity read), before any write.
- Required firmware: `cs47l93-dsp1.wmfw` and CS35L40 firmware — to extract, or declare missing.
- **Expected proof:** audible sound on the speaker (log + listening test).
- Deliverable: `docs/journal/build-b1l-144.md`.

### Build 145 — Microphone audio + jack / USB-C (postponed)
- Microphone via the Madera codec; jack/USB-C detection by the `muic` node and max77705 (`max77705,support-audio`).
- **Expected proof:** microphone detection, jack/USB-C switching in the logs.
- Deliverable: `docs/journal/build-b1l-145.md`.

### Build 146 — Vibration / haptics (postponed)
- `LINEAR_1040` motor: PWM `vib-pwm` (gpp3-1), boost `motor-boost-en` (gpg1-7), regulator `VDD_MOTOR_3P3`.
- **Expected proof:** kernel log (motor detection) + physical vibration felt.
- Deliverable: `docs/journal/build-b1l-146.md`.

### Build 147 — Bluetooth (postponed)
- Broadcom BCM43xx controller (`samsung,bcm43xx` node), UART/H4 + firmware.
- Depends on Wi-Fi (radio module and shared power lines).
- **Expected proof:** BT controller detection, pairing in the kernel log.
- Required firmware: BCM43xx firmware/HCD — to extract, or declare missing.
- Deliverable: `docs/journal/build-b1l-147.md`.

### Build 148+ — Sensors (research, outside the immediate scope)
- `BCM4773` hub (SSP, SPI) — to be evaluated.
- Modem/RIL: marked "research" in hakios (`drivers/modem/`, QMI, IPA) — Qualcomm, not applicable as is to the Exynos 9820.

---

## 4. Rules to follow for each build (protocol)

1. **A single change per build**; a diagnostic may accompany it.
2. **French only** (reports, messages, buttons).
3. **Document**: `docs/journal/build-NNNN.md` with the sections *Diagnostic*, *Changement*, *Installé avec hash BOOT*, *Résultat*; and update the build tracking (`progress/`).
4. **Proof before fix:** reason from the log, the code read, or a physical test; prefer a decisive diagnostic over a guessed fix.
5. **Do not break what works:** GPU Mali, IOSurface, display, touch must remain functional.
6. **Flash and test:** kernel recompiled (`build-kernel.sh` with `KDKROOT` and `B2L_DEPSROOT`), packaged into BOOT, flashed (`flash-boot.sh`), the phone must boot up to iOS.
7. **After the last flash** (if no other build is planned): reboot to iOS with `adb -s <SERIAL> reboot`.
8. The owner tests, puts the phone back into TWRP, and says so; I then read the log.

---

## 5. Expected deliverables

- A working branch or copy in `~/Developer/s10-port/beyond2lte-ios/xnu-overlay/` (and `xnu/`) integrating the chosen drivers.
- A recompiled, packaged and flashed kernel (BOOT) that boots and reaches iOS.
- If possible, proof that the driver works (kernel log, device detection, sound, vibration, or Wi-Fi).
- Documentation: `docs/journal/build-b1l-NNNN.md` (Diagnostic, Changement, Installé avec hash BOOT, Résultat) + update of the build tracking (`progress/`).

---
*Plan established on 09/11/2026, revised on 10/11/2026 after build 140 (Wi-Fi stage 1).*

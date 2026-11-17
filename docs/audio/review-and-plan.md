# Audio: review of the existing code and plan (2026-11-13)

Mission: iOS 26 sound through the S10 loudspeakers, then through the headphone jack (with headset detection).
Prompt: `docs/prompt-session-audio.md`. Audio session, first change shipped in build **0210** of the Wi-Fi session.

## 1. What exists in the port

- **No audio driver.** The only trace: `b2l_exec_graphics_diag.h` (B1L-0052) blocks four voice synthesizers
  (KonaSynthesizer, MauiAUSP, MacinTalkAUSP, SiriAUSP) that kept crashing in a loop.
- Reusable building blocks:
  - `B2LInput.cpp`: HSI2C transfers (same Samsung IP as the amplifiers), but on a controller that UEFI has already
    configured (`r(0x60) && r(0x64)`);
  - `B2LWifi.cpp`: the method "read-only audit of the CMU / QCH gates, then write", and reading of the GPIO banks;
  - `B2LCPU.cpp`: probes at startup (`b2l_gpu_probe`), ACPM mailbox (`b2l_mfd`) that will give access to the PMIC
    (codec power supplies);
  - no `conf/files` entry or `KernelConfigTables.cpp` entry is needed as long as the code fits in a header included by
    a file that is already built (like `B2LThermal.h`, `B2LACPM.h`).

## 2. S10 hardware (live DTB `device-s10/dump/live.dtb`, verified)

| Element | Where | Details |
| --- | --- | --- |
| ABOX (audio DSP) | `abox@18c50000` | sfr 0x18C50000, sysreg 0x18C10000, sram 0x18D00000 (0x59000), timer 0x18CE0000; 2 CA32 cores (PMU 0x2D80 / 0x2E00); firmware `calliope_sram.bin`, `calliope_dram.bin`, `calliope_sram_2.bin`…; RDMA 0–11 0x18C51000+n·0x100, WDMA 0–7 0x18C53000+n·0x100, UAIF0–3 0x18C50500+n·0x10; IOMMU; clocks PLL_AUD0/1, AUDIF, DMIC, BUS, CNT |
| Power domain | `pd-aud@15861900` (PMU) | `need_smc = 0x18c62000`: Linux calls the secure world at power-on |
| Audio CMU | CMU_AUD 0x18C00000 | inside pd-aud: do not read while pd-aud is off |
| UAIF pins | `pinctrl@18C60000` (gpb0–2) | inside pd-aud |
| **Loudspeaker** (bottom) | CS35L40 `0x40` on `hsi2c@109A0000` | `cs35l40-bot`, right channel, reset **gph1-1**, IRQ gpm24 |
| **Earpiece** (top) | CS35L40 `0x41`, same bus | `cs35l40-rcv`, reset **gph1-2** |
| Amplifier bus | `hsi2c@109A0000` = **USI17_I2C** (CMU_PERIC1 0x10800000) | 400 kHz, pins gph0-6 (SCL) / gph0-7 (SDA), pinctrl PERIC1 0x10830000 (gph0 0xE0, gph1 0x100) |
| Audio link to the amplifiers | **UAIF1**, format `dsp_a`, inverted clock | TDM, like the lesson from ios-pipa |
| Codec (jack, mics) | CS47L93 on `spi@10470000` (cs 0) | reset **gpg1-0** (PERIC0 0x10430000 + 0xA0), IRQ on bank gpa2 (ALIVE), supplies phandles 0x32c / 0x32d (PMIC, to be identified); UAIF0 = AIF1 (i2s), UAIF2 = AIF3 |
| Jack | `cirrus,accdet` of the CS47L93 | `jd-use-jd2`, `hpd-pins <2 5 0 0>`, `micd-ranges`, output HPOUT2L/R |

Samsung kernel sources (LineageOS, partial clone `ref-samsung/k9820`, read with `git show`):
`sound/soc/codecs/cs35l41.c` (also handles `cirrus,cs35l40`, DEVID 0x00000000 = **0x35A40**, REVID 0x4),
`sound/soc/codecs/{madera,cs47l92}.c`, `drivers/mfd/madera-core.c`, `drivers/extcon/extcon-madera.c`,
`sound/soc/samsung/abox/*` (including `abox_rdma.c`, `abox_soc.c`, `abox_core.c`), `sound/soc/samsung/beyond_madera.c`.
CMU registers: `ref-kernel/k9820/drivers/soc/samsung/cal-if/exynos9820/cmucal-sfr.c`.

## 3. References read

- **HakiOS** `drivers/audio/` + `docs/driver-review/sound.md`: `/dev/hakios-audio` fed by CoreAudio, speaker guard
  (`hakios.guard.c`: 200 Hz high-pass, power limits 0.2 / 3 / 60 s, peak limiter), probes by boot-arg
  (`speaker_probe=1` reads only). Review: 16 defects, including S-01 (unbounded volume → amplifiers at maximum, blind
  guard) and S-04/S-05 (amplifiers left on after an I2C failure): **to be fixed before reusing this code.**
- **ios-pipa** (`NOTES.md`, audio steps A–E, patches 0138/0139): on the iOS side, an `AudioServerPlugIn`
  (`HakiosAudio.driver` in `/System/Library/Audio/Plug-Ins/HAL`, "Speaker" output → `/dev/hakios-audio`),
  `VirtualAudio.plugin` put back in place, nothing else to patch in mediaserverd. Their CS35L41 (cousins of ours)
  are driven over **bit-banged I2C on GPIO**, amplifiers switched on 42–46 ms after the first sound, switched off after 3 s of
  silence. Their DSP (Qualcomm) is of no use to us.

## 4. Plan

One change per build up to 0212, then grouped batches (owner's agreement, 2026-11-13); each stage conditional on the previous one; no register of an unknown component is written
before its starting value has been re-read and noted.

| Step | Build | Content | Success if |
| --- | --- | --- | --- |
| **A0** | **0210** | **Read-only** probe (`B2LAudio.h`): PMU pd-aud and ABOX cores, CMU_CMU gates (AUD, PERIC1), PERIC1 CMU gates/QCH (GPIO, USI17_I2C), gph0/gph1/gpg1/gpa2 banks, HSI2C controller registers if its clock is open | line `B2L AUDIO: verdict` in the log, no blocking |
| A1 | 0211 | Read the identity of both CS35L40 (DEVID/REVID, I2C read only) over simulated I2C by GPIO on gph0-6/7 (ios-pipa method), reset gph1-1/2 released for the duration of the read, then pins put back as they were | `DEVID=0x35a40` for 0x40 and 0x41 |
| A2 | 0212 | pd-aud is already on (0210/0211): read CMU_AUD read-only (PLL_AUD0, ABOX/UAIF1/GPIO_AUD clocks), then the ABOX (version, routing, SPUS, UAIF0–3, RDMA0) and the AUD pins gpb0–2 if their clocks are open | line `abox-verdict` |
| A3 | 0212 (grouped batch 1) | Set up both amplifiers over I2C (rev. A0 errata, OTP, ASP dsp_a slave 4×32 bits, 48 kHz, PLL on SCLK 6.144 MHz, boost 4100 mA), **output stage and boost off, volume muted**; each write: before / written / read back, logged | `amp-prep … \| ready` for bot@40 and rcv@41 |
| A4 | 0212 (same batch) | UAIF1 clock 6.144 MHz output from the ABOX (CMU divider, mux, gpb1 fn2 pins, CTRL0/CTRL1, QCH), then the amplifiers' GLOBAL_EN: PUP_DONE proves they see the clock; then everything is undone and the amplifiers put back in reset | `uaif1-verdict amps-clocked-by-uaif1 \| abox-cmu-pins-undone=1` |
| A5 | batch 2 | First beep: RDMA0 → SPUS → SIFS0 → UAIF1 programmed by the AP (without Calliope), 1 kHz −10 dB 0.7 s on bot@40 only; A3 also copies the amplifiers' factory OTP. See §10 | beep audible at the bottom loudspeaker, `beep played=1` |
| B1 | | `/dev/b2l-audio` (modeled on `hakios.audio.c`, defaults S-01/S-02/S-04/S-05 fixed) + speaker guard | PCM write → sound |
| B2 | (USERDATA, ask) | `AudioServerPlugIn` modeled on HakiosAudio.driver, VirtualAudio put back in place | keyboard click, ringtone, music |
| C1 | | Jack: JD2 detection by the CS47L93 (SPI), output switching UAIF0/HPOUT2 | plug insertion seen, sound on headphones |
| C2 | | Volume with the buttons, without crackling | |

Open questions (to be settled by the logs, not by guessing):
1. Can the ABOX output an RDMA → UAIF stream without its Calliope firmware? To be read in `abox_rdma.c` / `abox_cmpnt_v20.c`
   before A3. Otherwise, load `calliope_*.bin` from the `vendor` partition (read-only from TWRP).
2. Is the protection firmware of the CS35L40 (`cs35l40-dsp1-spk-prot.wmfw` in `vendor`) needed? Without it: bypass mode
   + software guard at a low level.
3. Supplies of the CS47L93 (phandles 0x32c / 0x32d): which LDO of the PMIC, and are they switched on by S-Boot?

## 5. Step A0 (build 0210)

Files: `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` (new) and `xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp`
(two lines: `#include "B2LAudio.h"` after `B2LThermal.h`, `b2l_audio_probe();` after `b2l_gpu_probe();`).

- **No write** of any register, storage or GPIO. Kill switch: boot-arg `b2l_audio=0`.
- Read order, so that no block without a clock is ever touched: PMU and CMU_CMU (always on); CMU_PERIC1
  only if the `GATE_CLKCMU_PERIC1_BUS` gate is open; PERIC1 pins only if the gate and the QCH of
  `GPIO_PERIC1` are open; HSI2C controller only if its IPCLK/PCLK gates and its QCH are open. Nothing
  in pd-aud is read.
- Tested on the Mac with simulated memory (gates open, PERIC1 bus closed, I2C QCH closed, kill switch): temporary test outside the repository (the code is included as is, `ml_io_map` and the log simulated), 4 cases OK, `clang++ -Wall -Wextra` without warnings. Not compiled into the kernel by this
  session (it is the Wi-Fi session that builds 0210).

Lines to look for (prefix `B2L AUDIO:`):
```
B2L AUDIO: pd-aud conf=… status=… (on|off) core0=…/… core1=…/…
B2L AUDIO: cmu_top aud_cpu=… peric1_bus=… peric1_ip=…
B2L AUDIO: gpg1(codec reset=0) con=… dat=…
B2L AUDIO: gpa2(codec irq) con=… dat=…
B2L AUDIO: cmu_peric1 …            (if the PERIC1 bus is open)
B2L AUDIO: gph0(amp scl=6 sda=7) … / gph1(amp reset bot=1 rcv=2) …   (if GPIO_PERIC1 is open)
B2L AUDIO: hsi2c@109a0000 …         (if USI17_I2C is open)
B2L AUDIO: verdict pd-aud=… amp-bus=… scl_fn=… sda_fn=… amp_reset bot=fnX/Y rcv=fnX/Y codec_reset=fnX/Y i2c_regs=…
```
Reading the verdict: `amp-bus` = `peric1-bus-gated` | `peric1-gpio-gated` | `i2c-gated` | `i2c-untimed` (controller
not set up by S-Boot) | `i2c-timed` (set up). `fnN/D` = function of the pin (0 input, 1 output, 2 I2C function) / level.
Failure: no line at all after `B2L AUDIO: cmu_top` (read blocked) → reflash 0209 and pass `b2l_audio=0`.

## 6. Results

### Build 0210 (log `beyond2lte-ios/out/logs/0210-boot.txt`)

The probe ran through to the verdict, without blocking. Three lines are unreadable: `b2l_early_trace` writes character by
character and `B2L THERMAL` was writing at the same time on another CPU (fixed in A1: `b2l_diag_record`, one line
reserved per block). What is readable:

| Measurement | Value | Reading |
| --- | --- | --- |
| pd-aud | conf=00000001, `on` | the audio domain is **on** (set by S-Boot) |
| CMU_PERIC1 GPIO | pclk=00200000, qch=003f0002 | gate open, clock forced |
| hsi2c@109a0000 | ctl=00000008 conf=980110ff fs1=0180ff00 fs2=030003f8 **usi_con=00000001** | controller clocked but **USI in reset** |
| gph0-6/7 (SCL/SDA) | function 0 (input) | pins **not** in I2C function (3) |
| gph1-1/2 (amplifier resets) | function 0, level 0 | amplifiers **in reset** (line pulled low) |
| gpg1-0 (codec reset) | function 1, level 0 | S-Boot holds the CS47L93 in reset |

Conclusion: S-Boot did not touch the amplifiers. Starting values noted for A1: gph0 and gph1 as inputs on the
four pins. The full CON/DAT registers of gph0/gph1 were not readable (mixed output): A1 re-reads them and logs them
before writing.

## 7. Step A1 (build 0211)

File: `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` only (B2LCPU.cpp unchanged since 0210).

- Boot-arg `b2l_audio`: 0 off, 1 = A0 only (read), **2 = default** = A0 + A1.
- A1 only starts if the four pins are still in the state measured in 0210 (inputs), otherwise
  `amp-id skipped`.
- Writes, all in the PERIC1 GPIO bank (known component), only on the bits of these four pins:
  DAT locks of gph0-6/7 set to 0 (a single write); if both lines are high once released (board pull-ups), reset of
  gph1-1/2 as output low for 1.1 ms then high for 2.1 ms (sequence from `cs35l41-core.c`), then simulated I2C: a
  line is either an input (released) or an output at 0; **never driven high**. Reads: DEVID (0x0)
  and REVID (0x4) of 0x40 then 0x41. **No I2C write to the amplifiers**, which are therefore never activated.
- End: gph1 then gph0 take back their starting CON/DAT bits (the amplifiers go back into reset), control read back
  (`pins-restored=1`).
- If the lines are not high at rest: nothing is driven (`bus-not-idle`).
- Tested on the Mac with a simulated I2C bus and two simulated CS35L40 (which only answer when reset is released): amplifiers
  present, bus held low, amplifiers without power, read-only mode, pins out of their starting state. The test found
  a bug fixed before sending: DAT reads back the level of the pins when they are inputs, so two successive read-modify-write
  operations would have put a 1 into the SCL lock.

Line to look for:
```
B2L AUDIO: amp-id idle scl=1 sda=1 | bot@40 err=0 id=00035a40 rev=… err=0 | rcv@41 err=0 id=00035a40 rev=… err=0 | both-cs35l40 | pins-restored=1
```
`err`: 0 success, 1 address not acknowledged (amplifier silent, without power or still in reset), 2–5 register byte not acknowledged, 6 read refused, 7 SCL stuck low, 9 not attempted. Verdicts: `both-cs35l40`, `no-answer`,
`bus-not-idle`, `scl-stuck`.

### Build 0211 (log `beyond2lte-ios/out/logs/0211-boot.txt`)

Complete lines (no more mixing). **Both amplifiers answer**: `bot@40` and `rcv@41` DEVID `0x35a40`, REVID
`0xa0` (CS35L40 revision A0), pins put back to their starting state (`pins-restored=1`). The Wi-Fi stayed connected.

Complete starting values (before any write):

| Register | Value | Reading |
| --- | --- | --- |
| pd-aud | conf=1, status=1 | on |
| CMU_CMU aud_cpu / peric1_bus / peric1_ip | 00200000 ×3 | gates open |
| gph0 (amplifier bus) | con=00000000 dat=c0 pud=11111111 drv=22222222 | inputs, internal pull-down (1) but lines high: board pull-ups are stronger |
| gph1 (amplifier resets) | con=00000000 dat=00 pud=00011111 | inputs pulled low: amplifiers in reset |
| gpg1 (codec reset) | con=10000001 dat=d6 pud=01000001 | gpg1-0 output at 0: CS47L93 in reset |
| gpa2 (codec IRQ) | con=00000000 dat=14 pud=11101111 | |
| USI17_I2C | ipclk/pclk 00200000, qch 003f0002, mux 00000010 | clock present; controller `usi_con=1` (in reset) |

For later (real I2C bus): the DTB wants gph0-6/7 in function 3 with no pull (`pud 0`); today the internal pull-down
is active.

## 8. Step A2 (build 0212)

File: `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` only (B2LCPU.cpp unchanged).

- Boot-arg `b2l_audio`: 0 off, 1 A0, 2 A0+A1, **3 (default) A0+A1+A2**. A1 keeps running at each boot
  (amplifier identity, pins put back).
- **No write.** CMU_AUD (0x18C00000, inside pd-aud) is only read if pd-aud is on and the AUD_CPU gate of
  CMU_CMU is open; the ABOX registers (0x18C50000) only if the ABOX ACLK gate and ACLK QCH are open;
  the AUD pins (0x18C60000) only if the gate and the QCH of GPIO_AUD are open.
- ABOX registers read (`abox_soc.h`, v2): IP_INDEX, VERSION, SYSPOWER_CTRL/STATUS, SYSTEM_CONFIG0, DYN_CLOCK_OFF(1),
  QCHANNEL_DISABLE, ROUTE_CTRL0–2, SPUS_CTRL0/1, UAIF0–3 CTRL0/CTRL1/STATUS, RDMA0 CTRL0/CTRL1/STATUS (0x30). No
  FIFO and no register with destructive reads.
- Tested on the Mac (simulated registers): full path, ABOX clock off (ABOX not read), pd-aud off (nothing from the AUD domain
  read), level 2 (no A2), plus the earlier A0/A1 cases.

Lines to look for:
```
B2L AUDIO: cmu_aud pll_aud0=… mux_cpu_user=… div_bus=… div_uaif1=… aclk=… qch_aclk=… bclk_uaif1=… qch_bclk1=… gpio_pclk=… qch_gpio=… sysreg_pclk=…
B2L AUDIO: abox ip=… version=… syspower=…/… config0=… dyn_clk_off=…/… qch_disable=… route=…/…/… spus=…/…
B2L AUDIO: abox uaif0=…/…/… uaif1=…/…/… uaif2=… uaif3=… rdma0=…/…/…
B2L AUDIO: gpb0(mclk=0) … / gpb1(uaif1 0..3) … / gpb2 …
B2L AUDIO: abox-verdict abox_regs=0|1 aud_pins=0|1 uaif1_pins_fn=NNNN bclk_uaif1=open|gated
```
Failure: no line at all after `cmu_aud` (read blocked) → `b2l_audio=2`.

What A2 decides for A3 (first beep): if the ABOX answers (version readable) and PLL_AUD0 runs, A3 programs
RDMA → UAIF1 directly; otherwise the clock sequence from cmucal (PLL_AUD0, UAIF1 dividers) is needed first.

### Fix to A2 (in batch 0213)

A2 read the ABOX registers as soon as the ACLK gate and its QCH were open. However, the ABOX bus clock
(DIV_CLK_AUD_BUS) divides PLL_AUD0: if this PLL was off, the read could block the bus. From 0213, A2 only
reads the ABOX if PLL_AUD0 has ENABLE (bit 31) **and** STABLE (bit 29). The `cmu_aud` line also gives the computed
frequency of the PLL (0831x: 26 MHz × (M + K/65536) / (P << S)), DIV_AUDIF and MUX_UAIF1. A2 now runs **before**
A1 (still read-only) so that A4 knows the state of the ABOX.

If 0212 blocked after the `cmu_aud` line: this was the case, fixed by 0213.

## 9. Grouped batch A3 + A4 (build 0213)

Owner's agreement: group the audio steps, each conditional on the previous one. File:
`xnu-overlay/iokit/Kernel/arm/B2LAudio.h` only (B2LCPU.cpp unchanged).

Boot-arg `b2l_audio`: 0 off, 1 A0, 2 +A1, 3 +A2, 4 +A3, **5 (default) +A4**.

**Order and conditions**

1. A0 (read), then A2 (read, PLL guard above).
2. A1: amplifier resets released, DEVID/REVID. A3 only if both answer 0x35a40.
3. A3, for each amplifier (code from `cs35l41.c` for this board):
   - rev. A0 errata between the 0x40 keys (0x5555/0xAAAA … 0xCCCC/0x3333): 0x3854, OTP_TRIM_30 0x7418, 0x3014,
     BSTCVRT_DCM_CTRL 0x381C, 0x54, IRQ1_DB3, IRQ2_DB3;
   - wait for OTP_BOOT_DONE (IRQ1_STATUS4 bit 1, ≤ 100 ms);
   - BSTCVRT_PEAK_CUR = 0x42 (4100 mA, DTB), SP_HIZ_CTRL = 3, SP_FORMAT dsp_a slave, SCLK inverted, width 32/32,
     FRAME_RX_SLOT (bot = right channel → RX1 slot 1; rcv → RX1 slot 0), GLOBAL_FS 48 kHz, SP_RATE_CTRL 0x28,
     **PWR_CTRL2: AMP_EN = 0 and BST_EN = 0**, PCM volume = mute (0x4CF), SP_ENABLES ASPRX1;
   - PLL_CLK_CTRL in the order of `set_sysclk` (open loop, REFCLK 0x28, stop, source SCLK, closed loop,
     start);
   - each register: read before, write, read back; logged `reg:before>after`.
   - `otp_unpack` (copying the factory settings from OTP) is **not** ported: useless while the output stage stays
     off; to be done before A5.
4. A4 only if A3 is `ready` on both amplifiers and A2 found ABOX + AUD pins + locked PLL, and if
   DIV_AUDIF gives a frequency that is a multiple of 6.144 MHz to within 0.1 % (otherwise nothing is written):
   - PMU 0x1920 bit 11 (ABOX_PAD_NORMAL, `abox.c` for 9820) set to 1 if it is not already — the only setting left in place,
     as `abox.c` does;
   - DIV_UAIF1 = AUDIF / 6.144 MHz − 1, MUX_UAIF1 = divider, BCLK_UAIF1 gate opened if it was forced closed;
   - gpb1-0..3 in function 2;
   - UAIF1 CTRL1 = 0x1B4DF7DF (dsp_a, IB_NF, 4 × 32 bits, `abox_if.c`), CTRL0 MODE master, QCHANNEL_DISABLE bit 3
     (clock always active, `set_tristate(0)`), then SPK_ENABLE (no source: zeros);
   - for each amplifier: power-up patch then GLOBAL_EN; wait for PUP_DONE (IRQ1_STATUS1 bit 24); IRQ1 state
     1–4 before/after; then GLOBAL_EN = 0, shutdown patch, wait for PDN_DONE (bit 23);
   - everything A4 wrote to the ABOX, the CMU and the pins goes back to its starting value (verified).
5. End: reset and bus pins put back as they were, the amplifiers go back into reset.

**What this batch does not do**: no sound. The output stage (AMP_EN) and the boost stay off from start to finish.
It proves that the amplifiers are set up and that they see the UAIF1 clock of the ABOX.

Caveat: PUP_DONE might not come if the amplifier expects AMP_EN = 1 (Samsung sets AMP_EN before GLOBAL_EN).
In that case, `amps-not-clocked` would be a false negative: the raw IRQ1_STATUS1–4 "after" values decide
(clock absent → errors or nothing; PLL locked → PLL status bits in STATUS3).

Tested on the Mac (simulated I2C bus and amplifiers, `t/test2.cpp`): full path; clock not reaching the amplifiers;
PLL_AUD0 off (ABOX neither read nor written); DIV_AUDIF incompatible (nothing written); levels 4, 3, 2, 1; other amplifier
revision; bus held low; amplifiers without power; pins not in their starting state; ABOX clock off; pd-aud
off. Checked every time: no I2C line driven high, pins and AUD registers back at their starting value,
AMP_EN never 1, volume always muted.

Lines to look for (`grep -a "B2L AUDIO"`):
```
B2L AUDIO: abox-verdict pll_aud0=locked abox_regs=1 aud_pins=1 …            (A2)
B2L AUDIO: amp-id … | both-cs35l40                                          (A1)
B2L AUDIO: amp-errata bot@40 … readback-same i2c=ok                         (A3)
B2L AUDIO: amp-prep bot@40 otp_boot=1 reg:before>after … | ready             (A3, same for rcv@41)
B2L AUDIO: uaif1-on pad=…>… div_uaif1=…>… … bclk=6144…                      (A4)
B2L AUDIO: amp-power bot@40 io=0 pup_done=1(Nms) pdn_done=1(Nms) irq1 before=… after=… err=00000000
B2L AUDIO: uaif1-verdict amps-clocked-by-uaif1 | abox-cmu-pins-undone=1      (A4)
B2L AUDIO: amps-off pins-restored=1 …                                       (end)
```
Expected failures: `| i2c-error` or `| readback-mismatch` (A3, A4 not run), `uaif1 skipped (…)` (condition not
met, nothing written), `amps-not-clocked` (see the caveat). Blocking → `b2l_audio=4` (without A4) or `=3` (read only).

**For A5 (first sound)**: the data source is missing. The ABOX RDMA (buffer address, format,
trigger) is programmed by the Calliope firmware via IPC, not by the AP. Two routes:
(a) start Calliope (`calliope_*.bin` from the vendor partition) and speak its IPC (`abox_ipc.h`);
(b) program RDMA0 directly (BUF_STR/END/OFFSET known from `abox_soc.h`, ABOX SysMMU bypassed) and
the chain SPUS → UAIF1 (ROUTE_CTRL0, SPUS_CTRL0/1), starting from the values read by A2.
Needed to decide: `calliope*.bin`, `cs35l40*.wmfw/.bin` and `mixer_paths*.xml` copied read-only from vendor.

## 10. Batch 2: first beep (A5)

File: `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` (only). Default `b2l_audio=6`; `=5` = batch 1 without beep.

**Chosen route: (b), the AP programs the RDMA itself.** The Calliope firmware accessors
(`vendor-audio/calliope_dram.bin`, disassembled) give the fields that the firmware writes to start an RDMA:
VOL_FACTOR, BUF_STR, BUF_END = start + size, STR_POINT, BUF_OFFSET, BURST 8, FORMAT, bit 5 = 1, then ENABLE. The SBANKs
are in bytes (STR 13:4, SIZE 29:20 in units of 16 bytes); the default mixer of the firmware is at 0x3000, 512 bytes.
SYSPOWER_CTRL bit 0 is the "DRAM on" request (wait for STATUS bit 0).

Additions:
- **A3**: after OTP_BOOT_DONE, copy of the factory OTP (`cs35l41_otp_unpack` from Samsung): test key, OTPID, 32 words
  of OTP_MEM, header check, 99 fields written by read-modify-write. Values never logged (0x17040/44
  = chip lot and wafer). Line `amp-otp … result=ok writes=N`. Header or OTPID failure → no beep, the rest
  continues; I2C error → amplifier not ready.
- **A5**, within A4 once UAIF1 is running:
  1. RDMA0 must be stopped at the start (otherwise `beep skipped (rdma0 busy …)`, nothing written); buffer of 3840 bytes contiguous
     below 4 GB (240 frames × 4 channels × 32 bits, 1 kHz at −20 dBFS in channel 1 = right = bot), cache cleaned;
  2. 13 ABOX registers read then written (SYSPOWER, SBANK RDMA0/mixer, SPUS_CTRL0/1, SIFS0_CNT, ROUTE_CTRL0, RDMA0); the
     `beep-path` line gives each `before>after`; the RDMA read pointer must move (`rdma-moving`);
  3. bot@40 only, before GLOBAL_EN, in DAPM order: PCM source = ASPRX1, CLASS H, output not cut, AMP_EN; **boost
     off** (VBST = battery through the inductor, as in the earpiece path); rcv@41 stays AMP_EN = 0;
  4. after PUP_DONE without error bit: volume −10 dB with a ramp, 700 ms, IRQ1 error bits read every 100 ms
     (error → immediate mute), mute, 60 ms, GLOBAL_EN off, AMP_EN = 0;
  5. RDMA stopped (UAIF1 still running), wait for PROGRESS = 0, the 13 registers put back in reverse (SYSPOWER last),
     buffer freed only if the RDMA is stopped; then A4 undone as before, amplifiers put back in reset.

Lines to look for in addition to those of §9:
```
B2L AUDIO: amp-otp bot@40 otpid=… result=ok writes=N                        (A3, same for rcv@41)
B2L AUDIO: beep-path ring_pa=… dram=ok 0010:…>… … 1000:…>… status=…>… | rdma-moving
B2L AUDIO: beep-amp bot@40 04c00:…>00000008 0201c:…>…10 02024:…>… 02018:…>…01 | ready
B2L AUDIO: beep played=1 ms=700 vol_up=1 vol_down=0 err=00000000 … | rdma-idle-ring-freed=1 abox-path-restored=1
```
By ear: a 0.7 s beep at 1 kHz, faint, at the bottom loudspeaker, during boot.

Expected failures:
- `dram=TIMEOUT` or `rdma-NOT-moving`: the ABOX does not read the buffer (SysMMU active, clock, unknown field); nothing is
  sent to the amplifiers, everything is put back;
- `played=0 err=…`: amplifier error during the beep, immediate mute;
- `played=1` without sound: the RDMA may run only once (compare `rdma_status_end` with `status`), or the data
  does not reach channel 1;
- `rdma-idle-ring-freed=0`: the RDMA does not stop; buffer kept (4 KB lost), registers put back anyway;
- blocking → `b2l_audio=5` (batch 1 only), then `=4`.

Known risks: bit 4 of RDMA CTRL0 of unknown meaning (left at its value); ABOX SysMMU assumed bypassed
(never enabled since reset), otherwise a DMA page fault that XNU does not handle; amplifier without protection firmware (DSP),
hence the low volume, the short duration and the boost off.

Tested on the Mac (`t/test3.cpp`, amplifiers, ABOX, RDMA and DMA simulated), 11 cases: full beep; OTPID 1 and 2 compared with
the Samsung algorithm copied as is; level 5 without beep; RDMA motionless; DRAM refused; amplifier error during the beep;
wrong OTP header; unknown OTPID; RDMA already busy; RDMA that does not stop; no contiguous memory; amplifiers without
clock. Checked every time: boost never switched on, AMP_EN and volume −10 dB only on bot during the beep, ABOX/CMU/pin
registers back at their starting value, amplifiers put back in reset, lines < 640 bytes.

## 11. Batches 3 and 4: iOS plays the sound (prepared, build after B1L-0270)

**Files**
- `xnu-overlay/iokit/Kernel/arm/B2LAudio.h`: batch 3 (A6).
- `xnu-overlay/bsd/kern/b2l_audio_seed.h` (new): batch 4, kernel side.
- `b2l_userspace_seed.h`: 2 lines (`#include` + `b2l_audio_seed(ctx)` after `b2l_bt_seed`).
- HakiOS plug-in (MIT): `audio/plugin/` → `build-audio/b2l-audio-v1/` → SYSTEM:/b2l-audio-v1/.

**Boot-arg `b2l_audio`**: … 5 = A4 without path · 6 = audible beep from 0213 · **7 (default) = A6, silent**.
- Other switches: `b2l_audio_max` (ceiling in 1/8 dB below 0 dB, default 48 = −6 dB, minimum 24) and `b2l_audio_ios=0` (no /dev, no plug-in).

**A6 (batch 3)**
- Same check as in 0213 (ABOX path, amplifier ready, RDMA moving), but the ring stays at zero: no beep.
- If everything is good, the path stays open: RDMA0 → SIFS0 → UAIF1 runs in silence. Both amplifiers stay powered, bot on standby (GLOBAL_EN = 0), rcv never switched on.
- Ring: 65,280 bytes = 4080 frames × 4 channels × 32 bits (85 ms), period 4080 bytes (less than 4 KB, like abox_rdma_pointer). Mono (L+R)/2 in channel 1 (bot).
- `b2l_audio_sink()` writes 1024 frames (21 ms) ahead of the RDMA, never waiting; whatever does not fit is lost.
- Resynchronization in three cases: first call; gap too small or too large; RDMA that has overtaken the writer during a silence. This last case is detected by the thread, which follows the writer's lead every 10 ms.
- Kernel thread every 10 ms:
  - zeroes what has been played;
  - switches on bot when a non-zero sound arrives (Samsung DAPM order, PUP patch, GLOBAL_EN, PUP_DONE, then volume with a ramp);
  - switches it off after 3 s of silence;
  - applies the iOS volume under the ceiling;
  - reads the errors every 100 ms.
- Amplifier error: cut off for 30 s. After 3 errors, the loudspeaker stays off until reboot. Boost still off.
- Log: `ios-ready`, `amps-kept`, `ios-play`, `ios-stop`, `ios-volume`, `ios-fault`.

**Batch 4**
- `/dev/hakios-audio` (0666): write = s16 stereo 48 kHz → `b2l_audio_sink`; ioctl 'r' 1 = volume, 'r' 2 = plug-in log line (`B2L AUDIO HAL:`, 400 lines at most).
- The HakiOS plug-in, unchanged, is published once by the kernel in `/System/Library/Audio/Plug-Ins/HAL/HakiosAudio.driver/`. Nothing on USERDATA.

**Tested on the host** (`audio/tests/test4.cpp`, `run.sh`, ASan + UBSan):
- level 7: guarded path without any sound, service started;
- write into channel 1, RDMA advance, oversized writes;
- zeros behind the RDMA, volume and mute, stop after 3 s;
- gap then resynchronization, 3 errors giving a permanent stop, ceiling;
- RDMA motionless and amplifiers without clock: everything undone;
- level 6: beep from 0213 then everything undone; levels 5, 3, 0; bus blocked; amplifiers without power.

**Risks**
- VirtualAudio of iOS 26 may not route to a device named "Speaker" from a third-party plug-in.
- The audiomxd sandbox may refuse /dev/hakios-audio (b2l_sandbox is permissive, to be confirmed).
- The path stays on permanently: a little power consumption.

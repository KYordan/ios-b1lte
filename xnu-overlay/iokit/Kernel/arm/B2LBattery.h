/* SPDX-License-Identifier: BSD-2-Clause
 * Battery for iOS: an IOPMPowerSource (what powerd, SpringBoard and wifid look for; without one iOS showed 0 %)
 * fed by the S10e/S10 MAX77705 fuel gauge and charger. Included by B2LPlatformExpert.cpp, which starts it.
 *
 * Bus: hsi2c@15D60000 (DT /hsi2c@15D60000/max77705@66), the same HSI2C transfer sequence as B2LInput.cpp; the bus
 * is set up here when the bootloader left it unconfigured (setupBus). The one PMIC write is the charger watchdog kick
 * (build 30, below, within the thermistor limits); everything else is read only. Addresses and registers after
 * Samsung's max77705 drivers: fuel gauge 0x36 (RepCap 0x05, RepSOC 0x06, VCell 0x09, Current 0x0A, FullCapRep 0x10),
 * charger 0x69 (CHG_INT_OK 0xB2: CHGIN_OK 0x40, WCIN_OK 0x20; CHG_DETAILS_00 0xB3 bits 6-5 CHGIN_DTLS, 3 = VBUS
 * valid; CHG_DETAILS_01 0xB4 low nibble: 0-2 charging, 3-4 full, 5-7 not charging, 8/A/B discharging).
 * Units: RepSOC 1/256 %, VCell 78.125 uV, Current 1.5625 uV / Rsense with DT fuelgauge,fg_resistor 2 (5 mOhm), i.e.
 * 0.3125 mA, capacities 5 uVh / 5 mOhm = 1 mAh. DT fuelgauge,capacity_max 1000 / capacity_min 0: the UI value is RepSOC.
 */
#ifndef B2L_BATTERY_H
#define B2L_BATTERY_H
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSSymbol.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define B2L_BAT_I2C_PA     0x15D60000ULL
/* The bootloader leaves this bus unconfigured (B1L S10e boot 11: FS timings 0). As Samsung's exynos-usi-v2 and
 * i2c-exynos5 drivers: USI_CMGP03 mode = I2C in SYSREG_CMGP (DT /usi@15C22030 usi_v2_mode "i2c"), USI_CON reset
 * released, USI_OPTION clock-stop on, then fast-mode timings from the DT ipclk (default-clk 200 MHz) for 100 kHz. */
#define B2L_BAT_SYSREG_PA  0x15C22000ULL
#define B2L_BAT_USI_MODE   0x30u      /* /usi@15C22030 */
#define B2L_USI_V2_I2C     0x4u
/* Pins (build 12: the bus took the USI mode and timings, but a written byte never left the FIFO). DT
 * /pinctrl@15C30000/hsi2c45-bus: gpm12-0 (SCL), gpm13-0 (SDA), function 2, no pull. Samsung's exynos9820 CMGP bank
 * table: gpm12 at 0x180, gpm13 at 0x1A0, alive bank type (CON 4 bits at +0, PUD 4 bits at +8). */
#define B2L_BAT_PINCTRL_PA 0x15C30000ULL
#define B2L_BAT_PIN_SCL    0x180u
#define B2L_BAT_PIN_SDA    0x1A0u
#define B2L_BAT_PIN_FUNC   0x2u
#define B2L_BAT_IPCLK_HZ   200000000u
#define B2L_BAT_BUS_HZ     100000u
#define B2L_BAT_FG         0x36
#define B2L_BAT_CHG        0x69
#define B2L_BAT_DESIGN_MAH 3100        /* S10e (EB-BG970ABU) */
#define B2L_BAT_PERIOD_MS  10000
/* S10e build 30: thermistors and the charger's watchdog.
 * - ADC: DT /adc@15C40000 "samsung,exynos-adc-v3" (no CHUB arbitration sysreg in the S10e DT: the AP alone uses it), its
 *   clock gate_adcif = CMU_CMGP 0x15C00000 + 0x2004 (CLK_CON_GAT_GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0: bit 20
 *   MANUAL, bit 21 CG_VAL, Samsung cal-if exynos9820 cmucal-sfr.c), checked before the ADC is touched. Samsung's
 *   exynos_adc.c v3: CON1 0x00 (SOFT_RESET 4, NON_RESET 2, START 1), CON2 0x04 (C_TIME(6) << 4 | channel), DAT 0x08
 *   (bit 31 done, 12 bits), INT_EN 0x10; polled here, as Jesvi-iOS does on the S20's same block.
 * - Watchdog: CHG_CNFG_00 0xB7 = 0x15 from S-Boot (WDTEN on): unkicked, CHG_DTLS reads 0xB about a minute after boot
 *   and the phone does not charge (build 27). Samsung's max77705_chg_set_wdtmr_kick: CHG_CNFG_06 0xBD WDTCLR (bits 1:0)
 *   = 01, read-modify-write; the only PMIC write in this file. Done at every poll while a cable is in and
 *   B2LBatteryTemp.h allows charging; otherwise the watchdog runs out and the charger stops by itself. Currents and
 *   voltages stay as S-Boot set them (CNFG_02 0x49: 450 mA charge, CNFG_09 0x0f: ~500 mA input).
 *   Boot-arg b2l_chg_wdt=0: never kick (reads only, as before). */
#define B2L_BAT_CMGP_PA    0x15C00000ULL
#define B2L_BAT_ADC_GATE   0x2004u
#define B2L_BAT_ADC_QCH    0x3008u
#define B2L_BAT_ADC_PA     0x15C40000ULL
#define B2L_BAT_CNFG_06    0xBD
#include "B2LBatteryTemp.h"

class B2LBattery : public IOPMPowerSource {
	OSDeclareDefaultStructors(B2LBattery);

	IOMemoryDescriptor *fDesc;
	IOMemoryMap        *fMap;
	volatile uint8_t   *fI2c;
	IOWorkLoop         *fLoop;
	IOTimerEventSource *fTimer;
	unsigned            fFailures, fLogs, fLastPercent, fUpdates;
	uint32_t            fLastChg;
	bool                fPublished, fLastExternal;
	IOMemoryMap        *fAdcMap;
	volatile uint32_t  *fAdc;
	int                 fTemp[3];      /* 0.1 C: battery, charger, USB port */
	int                 fRaw[3];
	struct b2l_charge_gate fGate;
	bool                fGateLogged, fGateWas, fKickOn;
	unsigned            fKicks, fKickFails, fExpiredSeen;

	/* Exynos ADC v3, one polled conversion; -1 when it does not finish in 2 ms. */
	int adcRead(unsigned ch)
	{
		if (!fAdc) return -1;
		fAdc[1] = 0x60u | (ch & 0xfu);   /* CON2: C_TIME(6), channel */
		__asm__ volatile ("dsb sy" ::: "memory");
		fAdc[0] = 0x3u;                  /* CON1: non-reset, start */
		__asm__ volatile ("dsb sy" ::: "memory");
		for (int i = 0; i < 200; i++) {
			uint32_t d = fAdc[2];
			if ((d & 0x80000000u) && i > 0) return (int)(d & 0xfffu);
			IODelay(10);
		}
		return -1;
	}

	/* The ADC's clock first (gated: opened, as clk_prepare_enable would), then Samsung's v3 init. */
	bool adcSetup()
	{
		IOMemoryDescriptor *cd = IOMemoryDescriptor::withPhysicalAddress(B2L_BAT_CMGP_PA, 0x4000, kIODirectionInOut);
		IOMemoryMap *cm = cd ? cd->map(kIOMapInhibitCache) : NULL;
		uint32_t gate = 0xffffffffu, gate2 = 0, qch = 0;
		if (cm) {
			volatile uint32_t *g = (volatile uint32_t *)(cm->getVirtualAddress() + B2L_BAT_ADC_GATE);
			gate = *g;
			qch = *(volatile uint32_t *)(cm->getVirtualAddress() + B2L_BAT_ADC_QCH);
			if ((gate & (1u << 20)) && !(gate & (1u << 21))) {   /* manual and off */
				*g = gate | (1u << 21);
				__asm__ volatile ("dsb sy" ::: "memory");
				IODelay(5);
			}
			gate2 = *g;
		}
		OSSafeReleaseNULL(cm);
		OSSafeReleaseNULL(cd);
		bool clockOn = !(gate2 & (1u << 20)) || (gate2 & (1u << 21));   /* automatic gating, or manual and on */
		IOLog("B2L BATTERY: ADC clock gate 0x%x -> 0x%x (%s), qch 0x%x\n", gate, gate2,
		    gate == 0xffffffffu ? "CMU_CMGP not mapped" : clockOn ? "open" : "STILL CLOSED", qch);
		if (gate == 0xffffffffu || !clockOn) return false;
		IOMemoryDescriptor *ad = IOMemoryDescriptor::withPhysicalAddress(B2L_BAT_ADC_PA, 0x1000, kIODirectionInOut);
		fAdcMap = ad ? ad->map(kIOMapInhibitCache) : NULL;
		OSSafeReleaseNULL(ad);
		fAdc = fAdcMap ? (volatile uint32_t *)fAdcMap->getVirtualAddress() : NULL;
		if (!fAdc) return false;
		fAdc[0] = 0x4u;                  /* CON1: soft reset */
		__asm__ volatile ("dsb sy" ::: "memory");
		fAdc[0] = 0x2u;                  /* out of it */
		fAdc[1] = 0x60u;                 /* CON2: C_TIME(6) */
		fAdc[4] = 0;                     /* INT_EN: polled */
		__asm__ volatile ("dsb sy" ::: "memory");
		return true;
	}

	void readTemps()
	{
		static const unsigned ch[3] = { 4, 3, 1 };
		static const uint16_t *const tab[3] = { b2l_temp_bat_adc, b2l_temp_chg_adc, b2l_temp_usb_adc };
		for (unsigned i = 0; i < 3; i++) {
			fRaw[i] = adcRead(ch[i]);
			fTemp[i] = b2l_temp_from_adc(fRaw[i], tab[i]);
		}
	}

	bool writeReg(uint8_t addr, uint8_t reg, uint8_t value)
	{
		uint8_t b[2] = { reg, value };
		resetController();
		return message(addr, b, 2, false);
	}

	/* Samsung's max77705_chg_set_wdtmr_kick. */
	void kickWatchdog(unsigned state)
	{
		uint8_t c6 = 0, after = 0;
		if (state == 0xB && fExpiredSeen++ < 3) IOLog("B2L BATTERY: charger watchdog had run out (CHG_DTLS 0xB)\n");
		if (!readRegs(B2L_BAT_CHG, B2L_BAT_CNFG_06, &c6, 1)) { fKickFails++; return; }
		uint8_t want = (uint8_t)((c6 & ~0x03u) | 0x01u);
		if (!writeReg(B2L_BAT_CHG, B2L_BAT_CNFG_06, want)) { fKickFails++; return; }
		if (fKicks++ == 0) {
			readRegs(B2L_BAT_CHG, B2L_BAT_CNFG_06, &after, 1);
			IOLog("B2L BATTERY: charger watchdog kicked: CNFG_06 0x%x -> wrote 0x%x, reads 0x%x\n", c6, want, after);
		}
	}

	static uint64_t uptimeUS() { uint64_t ns = 0; absolutetime_to_nanoseconds(mach_absolute_time(), &ns); return ns / 1000; }
	uint32_t r(unsigned off) { uint32_t v = *(volatile uint32_t *)(fI2c + off); __asm__ volatile ("dmb oshld" ::: "memory"); return v; }
	void w(unsigned off, uint32_t v) { *(volatile uint32_t *)(fI2c + off) = v; __asm__ volatile ("dsb oshst" ::: "memory"); }
	void quiesce() { w(0x20, 0); w(0x24, r(0x24)); w(0x44, r(0x44) & ~(1u << 31)); w(0, r(0) & ~0xc0u); }
	void resetController() { w(0, r(0) | (1u << 31)); w(0, r(0) & ~(1u << 31)); w(0, 8); w(8, 0x00ffffff); w(0x40, r(0x40) | (1u << 31)); }

	/* B2LInput::message() with the slave address as a parameter and a 20 ms budget. */
	bool message(uint8_t addr, uint8_t *buf, unsigned n, bool read)
	{
		const uint64_t begin = uptimeUS(), budgetUS = 20000;
		w(0x48, r(0x48) & ~(1u << 31));
		unsigned level = n < 8 ? n : 8;
		w(4, 3u | (level << 4) | (level << 16));
		w(0, (r(0) & ~0xc0u) | 8u | (read ? 0x40u : 0x80u));
		w(0x40, r(0x40) | (1u << 31));
		w(0x70, (r(0x70) & ~0xff0fffffu) | ((uint32_t)addr << 10));
		w(0x24, r(0x24)); w(0x20, 0x80);
		w(0x44, (r(0x44) & ~0x8003ffffu) | 0x80020000u | (read ? 0x10000u : 0) | n);
		unsigned at = 0;
		while (uptimeUS() - begin < budgetUS) {
			unsigned before = at;
			while (at < n && uptimeUS() - begin < budgetUS) {
				uint32_t fifo = r(0x30);
				if (read) {
					if (fifo & (1u << 24)) break;
					buf[at++] = (uint8_t)r(0x38);
				} else {
					if ((fifo & 0x7f) >= 16) break;
					w(0x34, buf[at++]);
				}
			}
			if (at == n && (r(0x24) & 0x80) && (read || (r(0x30) & 0x100))) { quiesce(); return true; }
			if (at == before || at == n) IODelay(1);
		}
		if (at == n && (r(0x24) & 0x80) && (read || (r(0x30) & 0x100))) { quiesce(); return true; }
		if (fLogs < 6) {
			fLogs++;
			IOLog("B2L BATTERY: i2c addr=0x%x read=%d %u/%u bytes ctrl=0x%x int=0x%x err=0x%x trans=0x%x fifo=0x%x\n",
			    addr, read, at, n, r(0), r(0x24), r(0x2c), r(0x50), r(0x30));
		}
		resetController(); quiesce();
		return false;
	}

	bool readRegs(uint8_t addr, uint8_t reg, uint8_t *out, unsigned n)
	{
		resetController();
		return message(addr, &reg, 1, false) && message(addr, out, n, true);
	}
	bool read16(uint8_t reg, uint16_t *v)
	{
		uint8_t b[2];
		if (!readRegs(B2L_BAT_FG, reg, b, 2)) return false;
		*v = (uint16_t)(b[0] | (b[1] << 8));   /* MAX77705 FG: low byte first */
		return true;
	}

	void setNumber(const char *key, long long value)
	{
		const OSSymbol *k = OSSymbol::withCString(key);
		OSNumber *n = OSNumber::withNumber((unsigned long long)value, 64);
		if (k && n) setPSProperty(k, n);
		OSSafeReleaseNULL(n);
		if (k) k->release();
	}
	void setFlag(const char *key, bool value)
	{
		const OSSymbol *k = OSSymbol::withCString(key);
		if (k) { setPSProperty(k, value ? kOSBooleanTrue : kOSBooleanFalse); k->release(); }
	}

	void update()
	{
		uint16_t soc = 0, vcell = 0, current = 0, repcap = 0, fullcap = 0;
		if (!read16(0x06, &soc) || !read16(0x09, &vcell)) {
			if (++fFailures <= 3) IOLog("B2L BATTERY: fuel gauge not answering (%u)\n", fFailures);
			return;
		}
		bool haveCurrent = read16(0x0A, &current);
		bool haveCap = read16(0x05, &repcap) && read16(0x10, &fullcap);
		/* As Samsung's max77705_charger.c: ONLINE = CHG_INT_OK WCIN_OK (0x20) or CHGIN_OK (0x40); DETAILS_00 bits 6-5
		 * CHGIN_DTLS = 3 is a valid VBUS; DETAILS_01 low nibble CHG_DTLS: 0-2 charging, 3-4 full, 5-7 not charging,
		 * 8/A/B discharging. */
		uint8_t intok = 0, dtls0 = 0, dtls = 0;
		bool haveChg = readRegs(B2L_BAT_CHG, 0xB2, &intok, 1) && readRegs(B2L_BAT_CHG, 0xB3, &dtls0, 1) &&
		    readRegs(B2L_BAT_CHG, 0xB4, &dtls, 1);

		unsigned percent = (soc + 128u) >> 8;
		if (percent > 100) percent = 100;
		unsigned mV = (unsigned)vcell * 625u / 8000u;
		int mA = haveCurrent ? (int)(int16_t)current * 5 / 16 : 0;
		bool external = haveChg ? ((intok & 0x60) != 0 || ((dtls0 >> 5) & 3) == 3) : mA > 20;
		unsigned state = dtls & 0x0f;

		/* S10e build 30: temperatures, then the charge permission and the watchdog kick. */
		readTemps();
		bool allowed = b2l_charge_allowed(&fGate, fTemp[0], fTemp[1], fTemp[2]);
		if (!fGateLogged || allowed != fGateWas) {
			fGateLogged = true;
			fGateWas = allowed;
			IOLog("B2L BATTERY: charging %s (battery %d, charger %d, USB %d in 0.1 C; adc %d %d %d)%s%s\n",
			    allowed ? "allowed" : "NOT allowed", fTemp[0], fTemp[1], fTemp[2], fRaw[0], fRaw[1], fRaw[2],
			    allowed ? "" : ": ", allowed ? "" : fGate.why);
		}
		if (fKickOn && haveChg && external && allowed) kickWatchdog(state);
		bool charging = external && (haveChg ? state <= 2 : mA > 0);
		bool full = external && ((haveChg && (state == 3 || state == 4)) || percent >= 100);

		setBatteryInstalled(true);
		setExternalConnected(external);
		setExternalChargeCapable(external);
		setIsCharging(charging);
		setFlag(kIOPMFullyChargedKey, full);
		setMaxCapacity(100);
		setCurrentCapacity(percent);
		setAtWarnLevel(percent <= 10);
		setAtCriticalLevel(percent <= 5);
		setVoltage(mV);
		setAmperage(mA);
		setTimeRemaining(0);
		setNumber(kIOPMPSDesignCapacityKey, B2L_BAT_DESIGN_MAH);
		if (fTemp[0] != B2L_TEMP_UNKNOWN) setNumber("Temperature", (long long)fTemp[0] * 10);   /* 0.01 C, as AppleSmartBattery */
		setNumber("AppleRawBatteryVoltage", mV);
		setNumber("InstantAmperage", mA);
		if (haveCap) {
			setNumber("AppleRawCurrentCapacity", repcap);
			setNumber("AppleRawMaxCapacity", fullcap);
			setNumber("NominalChargeCapacity", fullcap);
		}
		updateStatus();

		if (!fPublished) {
			fPublished = true;
			registerService();
		}
		/* S10e build 18, read only: the charger's settings as the bootloader left them (CHG_DETAILS_02 0xB5 to
		 * CHG_CNFG_12 0xC3 in Samsung's max77705-private.h; CNFG_02 holds the charge current, CNFG_09 the CHGIN input
		 * current limit), at the first poll and whenever a charger comes or goes. Charging was ~4 W on USB (build 16):
		 * these values say whether a limit or the source is the cause, before anything is written. */
		if (haveChg && (fUpdates == 0 || external != fLastExternal)) {
			uint8_t cfg[15];
			bool all = true;
			for (unsigned i = 0; i < sizeof(cfg) && all; i++) all = readRegs(B2L_BAT_CHG, (uint8_t)(0xB5 + i), &cfg[i], 1);
			if (all)
				IOLog("B2L BATTERY: charger regs (external=%d) dtls02=0x%x dtls03=0x%x cnfg00..12 = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				    external, cfg[0], cfg[1], cfg[2], cfg[3], cfg[4], cfg[5], cfg[6], cfg[7], cfg[8], cfg[9], cfg[10], cfg[11],
				    cfg[12], cfg[13], cfg[14]);
			fLastExternal = external;
		}
		/* One line when the level or the charger state changes (the first two always), not every poll. */
		uint32_t chgState = (uint32_t)intok << 16 | (uint32_t)dtls0 << 8 | dtls;
		if (percent != fLastPercent || chgState != fLastChg || fUpdates < 2 || fUpdates % 30 == 0) {
			fLastPercent = percent;
			fLastChg = chgState;
			IOLog("B2L BATTERY: %u %% %u mV %d mA cap %u/%u mAh external=%d charging=%d full=%d (int_ok=0x%x dtls00=0x%x dtls01=0x%x chg=%d) "
			    "temp %d/%d/%d dC kicks %u failed %u\n",
			    percent, mV, mA, repcap, fullcap, external, charging, full, intok, dtls0, dtls, haveChg, fTemp[0], fTemp[1],
			    fTemp[2], fKicks, fKickFails);
		}
		fUpdates++;
	}

	/* exynos-usi-v2 + i2c-exynos5 exynos5_i2c_set_timing() (fast mode). Returns false when the block does not take
	 * the values (clock gated). */
	bool setupBus()
	{
		IOMemoryDescriptor *sd = IOMemoryDescriptor::withPhysicalAddress(B2L_BAT_SYSREG_PA, 4096, kIODirectionInOut);
		IOMemoryMap *sm = sd ? sd->map(kIOMapInhibitCache) : NULL;
		uint32_t mode = 0xffffffffu;
		if (sm) {
			volatile uint32_t *m = (volatile uint32_t *)(sm->getVirtualAddress() + B2L_BAT_USI_MODE);
			mode = *m;
			if ((mode & 7u) != B2L_USI_V2_I2C) {
				*m = (mode & ~7u) | B2L_USI_V2_I2C;
				__asm__ volatile ("dsb sy" ::: "memory");
			}
		}
		OSSafeReleaseNULL(sm);
		OSSafeReleaseNULL(sd);
		/* SCL and SDA to the I2C function, pulls off, as pinctrl "hsi2c45-bus" does. Only pin 0 of each bank. */
		uint32_t pins[4] = { 0, 0, 0, 0 };
		IOMemoryDescriptor *pd = IOMemoryDescriptor::withPhysicalAddress(B2L_BAT_PINCTRL_PA, 4096, kIODirectionInOut);
		IOMemoryMap *pm = pd ? pd->map(kIOMapInhibitCache) : NULL;
		if (pm) {
			const uint32_t banks[2] = { B2L_BAT_PIN_SCL, B2L_BAT_PIN_SDA };
			for (unsigned i = 0; i < 2; i++) {
				volatile uint32_t *con = (volatile uint32_t *)(pm->getVirtualAddress() + banks[i]);
				volatile uint32_t *pud = (volatile uint32_t *)(pm->getVirtualAddress() + banks[i] + 0x8);
				pins[2 * i] = *con;
				pins[2 * i + 1] = *pud;
				*con = (*con & ~0xfu) | B2L_BAT_PIN_FUNC;
				*pud = *pud & ~0xfu;
			}
			__asm__ volatile ("dsb sy" ::: "memory");
			IOLog("B2L BATTERY: pins scl con 0x%x pud 0x%x, sda con 0x%x pud 0x%x -> function %u, no pull\n",
			    pins[0], pins[1], pins[2], pins[3], B2L_BAT_PIN_FUNC);
		}
		OSSafeReleaseNULL(pm);
		OSSafeReleaseNULL(pd);
		uint32_t con = r(0xC4), opt = r(0xC8);
		w(0xC4, con & ~1u);                     /* USI_RESET is active high */
		IODelay(1);
		w(0xC8, (opt & ~(1u << 1)) | (1u << 2)); /* CLKREQ_ON off, CLKSTOP_ON on */
		uint32_t ftl = (r(0x40) >> 16) & 7u;
		uint32_t temp = B2L_BAT_IPCLK_HZ / B2L_BAT_BUS_HZ - 8u - 2u * ftl;
		uint32_t div = temp / 512u, cycle = temp / (div + 1u) - 2u, l = cycle / 2u, h = cycle / 2u;
		if (!r(0x60) || !r(0x64)) {
			w(0x60, l << 24 | l << 16 | l << 8);              /* FS1: start su/hd, stop su */
			w(0x64, (l / 2u) << 24 | l << 8 | h);             /* FS2: data su, scl low/high */
			w(0x68, div << 16 | cycle);                       /* FS3: divider, sr release */
			w(0x6C, l / 2u);                                  /* SLA: data hold */
		}
		IOLog("B2L BATTERY: bus setup usi_mode 0x%x->0x%x usi_con 0x%x->0x%x opt 0x%x->0x%x fs 0x%x 0x%x 0x%x sla 0x%x\n",
		    mode, (mode & ~7u) | B2L_USI_V2_I2C, con, r(0xC4), opt, r(0xC8), r(0x60), r(0x64), r(0x68), r(0x6C));
		return r(0x60) && r(0x64);
	}

	static void tick(OSObject *owner, IOTimerEventSource *sender)
	{
		B2LBattery *self = (B2LBattery *)owner;
		self->update();
		sender->setTimeoutMS(B2L_BAT_PERIOD_MS);
	}

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE
	{
		if (!IOPMPowerSource::start(provider)) return false;
		fDesc = IOMemoryDescriptor::withPhysicalAddress(B2L_BAT_I2C_PA, 4096, kIODirectionInOut);
		fMap = fDesc ? fDesc->map(kIOMapInhibitCache) : NULL;
		fI2c = fMap ? (volatile uint8_t *)fMap->getVirtualAddress() : NULL;
		/* Bus timings: the bootloader's if it left any, else our own (setupBus). Clocks are never touched. */
		if (!fI2c || !setupBus()) {
			IOLog("B2L BATTERY: hsi2c@15d60000 %s, no battery\n", fI2c ? "does not take timings (clock off?)" : "not mapped");
			return false;
		}
		fLoop = IOWorkLoop::workLoop();
		fTimer = fLoop ? IOTimerEventSource::timerEventSource(this, &B2LBattery::tick) : NULL;
		if (!fTimer || fLoop->addEventSource(fTimer) != kIOReturnSuccess) {
			IOLog("B2L BATTERY: no timer\n");
			return false;
		}
		setProperty("built-in", kOSBooleanTrue);
		fTemp[0] = fTemp[1] = fTemp[2] = B2L_TEMP_UNKNOWN;
		fRaw[0] = fRaw[1] = fRaw[2] = -1;
		fGate.allowed = 0;
		fGate.why = "";
		uint32_t kick = 1;
		PE_parse_boot_argn("b2l_chg_wdt", &kick, sizeof(kick));
		bool adc = adcSetup();
		fKickOn = kick && adc;   /* no thermistors: no kick, the charger stays stopped as before */
		IOLog("B2L BATTERY: thermistors %s, charger watchdog kick %s\n", adc ? "on the ADC" : "UNAVAILABLE",
		    fKickOn ? "on (within the temperature limits)" : kick ? "off (no thermistors)" : "off (b2l_chg_wdt=0)");
		fTimer->setTimeoutMS(3000);
		IOLog("B2L BATTERY: MAX77705 fuel gauge on hsi2c@15d60000, polled every %u s\n", B2L_BAT_PERIOD_MS / 1000);
		return true;
	}

	void free() APPLE_KEXT_OVERRIDE
	{
		if (fTimer) {
			fTimer->cancelTimeout();
			if (fLoop) fLoop->removeEventSource(fTimer);
		}
		OSSafeReleaseNULL(fTimer);
		OSSafeReleaseNULL(fLoop);
		OSSafeReleaseNULL(fMap);
		OSSafeReleaseNULL(fDesc);
		OSSafeReleaseNULL(fAdcMap);
		IOPMPowerSource::free();
	}
};
OSDefineMetaClassAndStructors(B2LBattery, IOPMPowerSource);

#endif

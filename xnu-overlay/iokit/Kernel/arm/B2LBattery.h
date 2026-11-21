/* SPDX-License-Identifier: BSD-2-Clause
 * Battery for iOS: an IOPMPowerSource (what powerd, SpringBoard and wifid look for; without one iOS showed 0 %)
 * fed by the S10e/S10 MAX77705 fuel gauge and charger. Included by B2LPlatformExpert.cpp, which starts it.
 *
 * Bus: hsi2c@15D60000 (DT /hsi2c@15D60000/max77705@66), the same HSI2C transfer sequence as B2LInput.cpp; the bus
 * is set up here when the bootloader left it unconfigured (setupBus). Nothing is written to the PMIC: reads only. Addresses and registers after
 * Samsung's max77705 drivers: fuel gauge 0x36 (RepCap 0x05, RepSOC 0x06, VCell 0x09, Current 0x0A, FullCapRep 0x10),
 * charger 0x69 (CHG_INT_OK 0xB2: CHGIN_OK 0x40, WCIN_OK 0x20; CHG_DETAILS_01 0xB4 low nibble: 0-3 charging, 4 done).
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
#define B2L_BAT_PERIOD_MS  20000

class B2LBattery : public IOPMPowerSource {
	OSDeclareDefaultStructors(B2LBattery);

	IOMemoryDescriptor *fDesc;
	IOMemoryMap        *fMap;
	volatile uint8_t   *fI2c;
	IOWorkLoop         *fLoop;
	IOTimerEventSource *fTimer;
	unsigned            fFailures, fLogs, fLastPercent;
	bool                fPublished;

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
		uint8_t intok = 0, dtls = 0;
		bool haveChg = readRegs(B2L_BAT_CHG, 0xB2, &intok, 1) && readRegs(B2L_BAT_CHG, 0xB4, &dtls, 1);

		unsigned percent = (soc + 128u) >> 8;
		if (percent > 100) percent = 100;
		unsigned mV = (unsigned)vcell * 625u / 8000u;
		int mA = haveCurrent ? (int)(int16_t)current * 5 / 16 : 0;
		bool external = haveChg ? (intok & 0x60) != 0 : mA > 20;
		unsigned state = dtls & 0x0f;
		bool charging = external && (haveChg ? state <= 3 : mA > 0);
		bool full = external && ((haveChg && state == 4) || percent >= 100);

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
		if (percent != fLastPercent || fLogs < 2) {
			fLastPercent = percent;
			IOLog("B2L BATTERY: %u %% %u mV %d mA cap %u/%u mAh external=%d charging=%d full=%d (int_ok=0x%x dtls=0x%x chg=%d)\n",
			    percent, mV, mA, repcap, fullcap, external, charging, full, intok, dtls, haveChg);
		}
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
		IOPMPowerSource::free();
	}
};
OSDefineMetaClassAndStructors(B2LBattery, IOPMPowerSource);

#endif

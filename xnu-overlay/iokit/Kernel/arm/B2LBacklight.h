/* SPDX-License-Identifier: BSD-2-Clause
 * S10e build 25: display brightness for iOS.
 *
 * iOS side (23G90 shared cache strings of CoreBrightness, backboardd, PowerLog): the internal display's backlight is
 * an IOService of class AppleARMBacklight with "backlight-control" = true (IOPropertyMatch). CoreBrightness reads
 * IODisplayParameters/brightness {min, max, value} and BrightnessMilliNits, and writes through
 * IORegistryEntrySetCFProperty (__SetBLDriverProperty, "brightness-nits" among others). Which key and which unit
 * 23G90 really sends is not known yet, so every setProperties is logged ("B2L BL: set ...") and read as:
 *   brightness-nits             nits (<= 2000), 16.16 nits (low 16 bits 0 or > 1 000 000) or millinits
 *   IODisplayParameters/brightness, brightness   value / max of the published range (0..65536)
 *   BrightnessMilliNits         millinits
 *
 * Panel side: the S10e's S6E3FA7 (DT panel-name s6e3fa7_beyond0_a3_s0) behind DSIM0 at 0x19080000, command mode.
 * Samsung's driver (LineageOS android_kernel_samsung_exynos9820: panel/s6e3fa7/s6e3fa7_beyond0_a3_s0_panel.h,
 * dpu20/dsim_drv.c, dpu20/cal_9820/regs-dsim.h) sets a brightness level with gamma (computed from the panel's own
 * calibration), AOR (B1), ELVSS, ACL and IRC, latched by F7 03, between the level-2 key F0 5A 5A / F0 A5 A5. Only
 * the AOR changes here: the share of each frame (vtotal 2304 lines) the pixels do not emit, 0x0014 (Samsung's
 * 420-nit entry) to 0x08D4 (its darkest), at whatever gamma the bootloader left. The nits are therefore approximate,
 * the dimming real (about 8 to 420 nits). Packets as dsim_write_data(): payload words, then PKTHDR (1 byte: DCS
 * short write 0x05, longer: DCS long write 0x39, as Samsung's mipi_write), then wait for EMPTY_PH_SFR.
 * Everything written is volatile panel state: a reboot restores it.
 *
 * Boot-args: b2l_backlight=0 (off: no service, nothing sent), b2l_bl_test=0 (no visible dim-and-back at start).
 * Only on the S10e panel (boot framebuffer 1080x2280); the S10's s6e3ha9 has other tables.
 */
#ifndef B2L_BACKLIGHT_H
#define B2L_BACKLIGHT_H
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSSymbol.h>
#include <kern/clock.h>
#include <kern/thread_call.h>
#include <sys/proc.h>
#include <pexpert/pexpert.h>

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

#define B2L_DSIM0_PA                 0x19080000ULL
#define B2L_DSIM_MAP_SIZE            0x1000
#define B2L_DSIM_VERSION             0x00
#define B2L_DSIM_LINK_STATUS0        0x08
#define B2L_DSIM_LINK_STATUS1        0x0c
#define B2L_DSIM_ESCMODE             0x2c
#define B2L_DSIM_PKTHDR              0x58
#define B2L_DSIM_PKTHDR_DATA         0x1ffffffu
#define B2L_DSIM_PAYLOAD             0x5c
#define B2L_DSIM_FIFOCTRL            0x68
#define B2L_DSIM_FIFO_FULL_PH        (1u << 11)
#define B2L_DSIM_FIFO_EMPTY_PH       (1u << 10)
#define B2L_DSIM_FIFO_FULL_PL        (1u << 9)
#define B2L_DSIM_CMD_CONFIG          0x80
#define B2L_DSIM_CMD_PKT_GO_RDY      (1u << 17)
#define B2L_DSIM_CMD_PKT_GO_EN       (1u << 16)
#define B2L_MIPI_DCS_SHORT_WRITE     0x05
#define B2L_MIPI_DCS_LONG_WRITE      0x39

#include "B2LBacklightMath.h"

class AppleARMBacklight : public IOService {
	OSDeclareDefaultStructors(AppleARMBacklight);

	volatile uint8_t *fDsim;
	IOLock           *fLock;
	thread_call_t     fTest;
	uint32_t          fNits;
	uint32_t          fSets;
	uint32_t          fSent, fFailed;

	uint32_t rd(uint32_t off) { return *(volatile uint32_t *)(fDsim + off); }
	void wr(uint32_t off, uint32_t v) { *(volatile uint32_t *)(fDsim + off) = v; __asm__ volatile ("dsb sy" ::: "memory"); }
	bool waitClear(uint32_t bit, uint32_t us);
	bool waitSet(uint32_t bit, uint32_t us);
	bool dcs(const uint8_t *cmd, uint32_t len);
	static void testCall(thread_call_param_t p0, thread_call_param_t p1);
	void publish(void);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
	bool setNits(uint32_t nits, const char *why);
};

OSDefineMetaClassAndStructors(AppleARMBacklight, IOService);

bool
AppleARMBacklight::waitClear(uint32_t bit, uint32_t us)
{
	for (uint32_t t = 0; t < us; t += 10) {
		if ((rd(B2L_DSIM_FIFOCTRL) & bit) == 0) {
			return true;
		}
		IODelay(10);
	}
	return false;
}

bool
AppleARMBacklight::waitSet(uint32_t bit, uint32_t us)
{
	for (uint32_t t = 0; t < us; t += 10) {
		if (rd(B2L_DSIM_FIFOCTRL) & bit) {
			return true;
		}
		IODelay(10);
	}
	return false;
}

/* One DCS write, as dsim_write_data(): payload words first, then the header. Caller holds fLock. */
bool
AppleARMBacklight::dcs(const uint8_t *cmd, uint32_t len)
{
	if (fDsim == NULL || len == 0 || len > 64 || !waitClear(B2L_DSIM_FIFO_FULL_PH, 20000)) {
		fFailed++;
		return false;
	}
	uint32_t hdr;
	if (len == 1) {
		hdr = B2L_MIPI_DCS_SHORT_WRITE | ((uint32_t)cmd[0] << 8);
	} else {
		for (uint32_t i = 0; i < len; i += 4) {
			uint32_t word = 0;
			for (uint32_t b = 0; b < 4 && i + b < len; b++) {
				word |= (uint32_t)cmd[i + b] << (8 * b);
			}
			if (!waitClear(B2L_DSIM_FIFO_FULL_PL, 20000)) {
				fFailed++;
				return false;
			}
			wr(B2L_DSIM_PAYLOAD, word);
		}
		hdr = B2L_MIPI_DCS_LONG_WRITE | ((len & 0xff) << 8) | (((len >> 8) & 0xff) << 16);
	}
	wr(B2L_DSIM_PKTHDR, (rd(B2L_DSIM_PKTHDR) & ~B2L_DSIM_PKTHDR_DATA) | hdr);
	uint32_t cfg = rd(B2L_DSIM_CMD_CONFIG);
	if (cfg & B2L_DSIM_CMD_PKT_GO_EN) {
		wr(B2L_DSIM_CMD_CONFIG, cfg | B2L_DSIM_CMD_PKT_GO_RDY);
	}
	if (!waitSet(B2L_DSIM_FIFO_EMPTY_PH, 40000)) {   /* Samsung's MIPI_WR_TIMEOUT is longer; a frame is ~17 ms */
		fFailed++;
		return false;
	}
	fSent++;
	return true;
}

bool
AppleARMBacklight::setNits(uint32_t nits, const char *why)
{
	if (nits > B2L_BL_MAX_NITS) {
		nits = B2L_BL_MAX_NITS;
	}
	uint32_t aor = b2l_bl_aor_for_nits(nits);
	static const uint8_t key_on[] = { 0xF0, 0x5A, 0x5A }, key_off[] = { 0xF0, 0xA5, 0xA5 }, latch[] = { 0xF7, 0x03 };
	const uint8_t aor_cmd[] = { 0xB1, (uint8_t)(aor >> 8), (uint8_t)aor };
	IOLockLock(fLock);
	bool ok = dcs(key_on, sizeof(key_on)) && dcs(aor_cmd, sizeof(aor_cmd)) && dcs(latch, sizeof(latch));
	bool off = dcs(key_off, sizeof(key_off));   /* always tried: the key must not stay open */
	if (ok) {
		fNits = nits;
	}
	uint32_t fifo = rd(B2L_DSIM_FIFOCTRL), sent = fSent, failed = fFailed;
	IOLockUnlock(fLock);
	static unsigned said;
	if (said++ < 48 || !ok || !off) {
		IOLog("B2L BL: %u nits (%s) -> AOR %#06x: %s, key closed %s; packets %u sent %u failed, fifo %08x\n", nits, why,
		    aor, ok ? "ok" : "FAILED", off ? "ok" : "FAILED", sent, failed, fifo);
	}
	if (ok) {
		publish();
	}
	return ok;
}

void
AppleARMBacklight::publish(void)
{
	uint32_t nits = fNits;
	OSDictionary *params = OSDictionary::withCapacity(1), *brt = OSDictionary::withCapacity(3), *mn = OSDictionary::withCapacity(3);
	OSNumber *lo = OSNumber::withNumber(0ULL, 32), *hi = OSNumber::withNumber((unsigned long long)B2L_BL_RANGE, 32);
	OSNumber *val = OSNumber::withNumber((unsigned long long)nits * B2L_BL_RANGE / B2L_BL_MAX_NITS, 32);
	OSNumber *mmin = OSNumber::withNumber(8000ULL, 32), *mmax = OSNumber::withNumber((unsigned long long)B2L_BL_MAX_NITS * 1000, 32);
	OSNumber *mval = OSNumber::withNumber((unsigned long long)nits * 1000, 32);
	OSNumber *n16 = OSNumber::withNumber((unsigned long long)nits << 16, 32);
	if (params && brt && mn && lo && hi && val && mmin && mmax && mval && n16) {
		brt->setObject("min", lo);
		brt->setObject("max", hi);
		brt->setObject("value", val);
		params->setObject("brightness", brt);
		mn->setObject("min", mmin);
		mn->setObject("max", mmax);
		mn->setObject("value", mval);
		setProperty("IODisplayParameters", params);
		setProperty("BrightnessMilliNits", mn);
		setProperty("brightness-nits", n16);
	}
	OSSafeReleaseNULL(params); OSSafeReleaseNULL(brt); OSSafeReleaseNULL(mn);
	OSSafeReleaseNULL(lo); OSSafeReleaseNULL(hi); OSSafeReleaseNULL(val);
	OSSafeReleaseNULL(mmin); OSSafeReleaseNULL(mmax); OSSafeReleaseNULL(mval); OSSafeReleaseNULL(n16);
}

IOReturn
AppleARMBacklight::setProperties(OSObject *properties)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
	if (dict == NULL) {
		return kIOReturnBadArgument;
	}
	char line[320];
	int at = 0;
	line[0] = '\0';
	int64_t nits = -1;
	OSCollectionIterator *it = OSCollectionIterator::withCollection(dict);
	OSSymbol *key;
	while (it != NULL && (key = OSDynamicCast(OSSymbol, it->getNextObject())) != NULL) {
		OSObject *o = dict->getObject(key);
		OSNumber *n = OSDynamicCast(OSNumber, o);
		OSDictionary *d = OSDynamicCast(OSDictionary, o);
		const char *k = key->getCStringNoCopy();
		if (at < (int)sizeof(line) - 64) {
			if (n != NULL) {
				at += snprintf(line + at, sizeof(line) - at, " %s=%llu", k, n->unsigned64BitValue());
			} else if (OSDynamicCast(OSBoolean, o) != NULL) {
				at += snprintf(line + at, sizeof(line) - at, " %s=%s", k, o == kOSBooleanTrue ? "true" : "false");
			} else {
				at += snprintf(line + at, sizeof(line) - at, " %s=<%s>", k, o != NULL ? o->getMetaClass()->getClassName() : "null");
			}
		}
		if (n != NULL && strcmp(k, "brightness-nits") == 0) {
			nits = b2l_bl_nits_from_raw(n->unsigned64BitValue());
		} else if (n != NULL && strcmp(k, "BrightnessMilliNits") == 0) {
			nits = (int64_t)(n->unsigned64BitValue() / 1000);
		} else if (n != NULL && strcmp(k, "brightness") == 0) {
			nits = (int64_t)(n->unsigned64BitValue() * B2L_BL_MAX_NITS / B2L_BL_RANGE);
		} else if (d != NULL && strcmp(k, "IODisplayParameters") == 0) {
			OSObject *b = d->getObject("brightness");
			OSNumber *bn = OSDynamicCast(OSNumber, b);
			OSDictionary *bd = OSDynamicCast(OSDictionary, b);
			if (bn == NULL && bd != NULL) {
				bn = OSDynamicCast(OSNumber, bd->getObject("value"));
			}
			if (bn != NULL) {
				nits = (int64_t)(bn->unsigned64BitValue() * B2L_BL_MAX_NITS / B2L_BL_RANGE);
			}
		}
	}
	OSSafeReleaseNULL(it);
	unsigned count = __atomic_fetch_add(&fSets, 1, __ATOMIC_RELAXED);
	if (count < 64 || (count & 63) == 0) {
		char who[32];
		proc_selfname(who, sizeof(who));
		IOLog("B2L BL: set %u from %s:%s -> %lld nits\n", count, who, line, nits);
	}
	if (nits < 0) {
		return kIOReturnSuccess;   /* (unknown keys: accepted, logged, nothing sent) */
	}
	return setNits((uint32_t)nits, "iOS") ? kIOReturnSuccess : kIOReturnTimeout;
}

/* A visible check of the panel path alone, a few seconds into the boot: dim, then back to 300 nits. */
void
AppleARMBacklight::testCall(thread_call_param_t p0, thread_call_param_t)
{
	AppleARMBacklight *self = (AppleARMBacklight *)p0;
	self->setNits(40, "start test: dim");
	IOSleep(1500);
	self->setNits(300, "start test: back");
}

bool
AppleARMBacklight::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	fLock = IOLockAlloc();
	fDsim = (volatile uint8_t *)ml_io_map(B2L_DSIM0_PA, B2L_DSIM_MAP_SIZE);
	if (fLock == NULL || fDsim == NULL) {
		IOLog("B2L BL: DSIM0 not mapped: no backlight\n");
		return false;
	}
	fNits = 300;
	IOLog("B2L BL: DSIM0 version %08x link %08x %08x escmode %08x pkthdr %08x fifo %08x cmd_config %08x\n",
	    rd(B2L_DSIM_VERSION), rd(B2L_DSIM_LINK_STATUS0), rd(B2L_DSIM_LINK_STATUS1), rd(B2L_DSIM_ESCMODE),
	    rd(B2L_DSIM_PKTHDR), rd(B2L_DSIM_FIFOCTRL), rd(B2L_DSIM_CMD_CONFIG));
	setProperty("backlight-control", kOSBooleanTrue);
	setProperty("built-in", kOSBooleanTrue);
	setProperty("B2L Panel", "s6e3fa7_beyond0 (AOR only)");
	publish();
	registerService();
	uint32_t test = 1;
	PE_parse_boot_argn("b2l_bl_test", &test, sizeof(test));
	if (test) {
		fTest = thread_call_allocate(&AppleARMBacklight::testCall, this);
		uint64_t deadline;
		clock_interval_to_deadline(6, NSEC_PER_SEC, &deadline);
		if (fTest != NULL) {
			thread_call_enter_delayed(fTest, deadline);
		}
	}
	IOLog("B2L BL: AppleARMBacklight published (backlight-control), start test %s\n", test ? "in 6 s" : "off");
	return true;
}

/* Called by B2LPlatformExpert::start. */
static void
b2l_backlight_start(IOService *platform)
{
	uint32_t enable = 1;
	PE_parse_boot_argn("b2l_backlight", &enable, sizeof(enable));
	PE_Video video;
	PE_current_console(&video);
	if (!enable || video.v_width != 1080 || video.v_height != 2280) {
		IOLog("B2L BL: no backlight service (%s, panel %lux%lu)\n", enable ? "not the S10e panel" : "b2l_backlight=0",
		    (unsigned long)video.v_width, (unsigned long)video.v_height);
		return;
	}
	AppleARMBacklight *bl = OSTypeAlloc(AppleARMBacklight);
	if (bl != NULL && bl->init() && bl->attach(platform)) {
		if (!bl->start(platform)) {
			bl->detach(platform);
		}
	}
	OSSafeReleaseNULL(bl);
}
#endif

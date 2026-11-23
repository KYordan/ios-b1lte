/* SPDX-License-Identifier: BSD-2-Clause
 * B2L WiFi bring-up, step 2: device-tree check, pinctrl state, WL_EN and
 * a read-only audit of the FSYS1 clock unit.
 *
 * beyond1lte drives the Broadcom WiFi chip from PCIe0 (0x13ED0000).  The
 * Exynos PCIe host controller, its clocks, PHY and power domains are not
 * initialized yet, so this stage never touches PCIe registers.  It
 * validates /wlan, reads the PERI0 gpg0 bank (cfg_wlanen, gpg0-5) and the
 * ALIVE gpa0 bank (wlan_host_wake, gpa0-7), then asserts WL_EN by putting
 * gpg0-5 in output mode with a high data latch.
 *
 * Build 144 restores the QCH boot state (build 143 had handed the PCIe
 * clocks to the hardware Q-channel) and samples gpf2/PMU/sysreg.
 *
 * Step 2 adds the CMU_FSYS1 audit: QCH gate state for the GEN2 PCIe
 * controller (APB/DBI/MSTR/PCS/PHY) and the PCIe clock gate control
 * registers.  Everything is read-only; no clock, PHY or PCIe register is
 * written.  The values decide whether build 143 may start the clocks.
 *
 * The Exynos9820 pinctrl layout is the one already used by B2LInput:
 * one 0x20-byte block per bank, CON at 0x00, DAT at 0x04, PUD at 0x08,
 * DRV at 0x0c, CON_PDN at 0x10, PUD_PDN at 0x14; four bits per pin in
 * CON (function 1 = output), PUD and DRV (exynos9810_bank_type_off,
 * field widths {4,1,4,4,2,4}).
 *
 * Kill switches: b2l_wifi=0 (read only), b2l_wifi_nowrite=1 (same).
 */
#include <pexpert/arm64/board_config.h>
#if defined(B2L)
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include <arm64/b2l_diag.h>
#include <kern/thread.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLocks.h>
extern "C" {
#include <sys/errno.h>
#include "../../../bsd/sys/hakios.sta.h"   /* not an exported header */
#include "../../../bsd/sys/hakios.net.h"
#include "../../../bsd/sys/b2l_wpa.h"      /* build 163: WPA2-PSK on the host (ios-pipa lx_wpa, MIT) */
}

#define B2L_WIFI_PERI0_PHYS   0x10430000ULL
#define B2L_WIFI_ALIVE_PHYS   0x15850000ULL
#define B2L_WIFI_GPG0_OFF     0x080u   /* gpp0..3, gpg0, gpg1, gpg2, gpg4 */
#define B2L_WIFI_GPA0_OFF     0x000u   /* gpa0..4, gpq0 */
#define B2L_WIFI_EN_PIN       5u       /* cfg_wlanen: gpg0-5 */
#define B2L_WIFI_WAKE_PIN     7u       /* wlan_host_wake: gpa0-7 */
#define B2L_WIFI_REG_CON      0x00u
#define B2L_WIFI_REG_DAT      0x04u
#define B2L_WIFI_REG_PUD      0x08u
#define B2L_WIFI_REG_DRV      0x0cu
#define B2L_WIFI_REG_CON_PDN  0x10u
#define B2L_WIFI_REG_PUD_PDN  0x14u

/* CMU_FSYS1 lives at 0x13c00000 (cmucal-sfr.c).  The QCH and clock gate
 * registers are inside the first 16 KiB.  QCH: bit 0 = ENABLE (1: the
 * hardware Q-channel decides, 0: software decides with bit 1), bit 1 =
 * CLOCK_REQ (software request), bits 16..25 = expire value (ra.c).  The
 * boot value 0x3f0002 keeps the clock forced on.  Gates: bit 20 = manual,
 * bit 21 = clock gating value, bit 28 = automatic gating. */
#define B2L_WIFI_CMU_FSYS1_PHYS   0x13c00000ULL
#define B2L_WIFI_CMU_MAP_SIZE     0x4000u
#define B2L_WIFI_QCH_APB          0x3034u
#define B2L_WIFI_QCH_DBI          0x3038u
#define B2L_WIFI_QCH_MSTR         0x303cu
#define B2L_WIFI_QCH_PCS          0x3040u
#define B2L_WIFI_QCH_PHY          0x3044u
#define B2L_WIFI_CLK_PHY_REFCLK   0x2000u
#define B2L_WIFI_CLK_DBI          0x2030u
#define B2L_WIFI_CLK_PHY_APB      0x2034u
#define B2L_WIFI_CLK_MSTR         0x2038u
#define B2L_WIFI_CLK_SUB_APB      0x203cu
#define B2L_WIFI_CLK_PIPE2        0x2040u
#define B2L_WIFI_CLK_SLV          0x2044u

/* Build 144 read-only audit (DTB pcie0 node + pci-exynos9820_cal.c):
 * FSYS1 pinctrl gpf2 (pcie0_clkreq = gpf2-0, pcie0_perst = gpf2-2),
 * PMU PCIE_PHY_CONTROL (0x71c + ch * 4, bit 0 = PHY isolation released),
 * FSYS1 sysreg PCIe PHY words (DT "sysreg" 0x13c11044, +0xc) and the
 * sharability control (0x700). */
#define B2L_WIFI_FSYS1_PIN_PHYS   0x13c40000ULL
#define B2L_WIFI_GPF2_OFF         0x020u
#define B2L_WIFI_PMU_PHYS         0x15860000ULL
#define B2L_WIFI_PMU_PCIE_PHY     0x71cu
#define B2L_WIFI_SYSREG_PHYS      0x13c10000ULL
#define B2L_WIFI_SYSREG_SIZE      0x2000u
#define B2L_WIFI_SYSREG_PCIE      0x1044u
#define B2L_WIFI_SYSREG_SHARE     0x700u

/* pcie0 ELBI (DT reg "elbi"), registers from pcie-exynos-host-v0.h. */
#define B2L_WIFI_ELBI_PHYS        0x13ed0000ULL
#define B2L_WIFI_ELBI_LTSSM_EN    0x02cu
#define B2L_WIFI_ELBI_LINKUP      0x074u
#define B2L_WIFI_ELBI_PM_DSTATE   0x088u
#define B2L_WIFI_ELBI_EXIT_L1     0x0f4u
#define B2L_WIFI_ELBI_SOFT_RESET  0x1d0u
#define B2L_WIFI_ELBI_QCH_SEL     0x2c8u
#define B2L_WIFI_ELBI_LINKDOWN_SEL 0x1b8u
#define B2L_WIFI_ELBI_PCS_G_RST   0x288u
#define B2L_WIFI_ELBI_PHY_CMN_RST 0x28cu
#define B2L_WIFI_ELBI_MAC_RST     0x290u
#define B2L_WIFI_ELBI_L12ERR_CTRL 0x2f0u

/* Build 147: the rest of the pcie0 register map (DT reg-names). */
#define B2L_WIFI_PHY_PHYS         0x13ef0000ULL
#define B2L_WIFI_PCS_PHYS         0x13ee0000ULL
#define B2L_WIFI_DBI_PHYS         0x13f00000ULL
#define B2L_WIFI_SYSREG_PAGE      0x13c11000ULL   /* DT "sysreg" 0x13c11044 */
#define B2L_WIFI_SYSREG_PCIE_OFF  0x44u
#define B2L_WIFI_LINK_POLLS       2000u           /* MAX_TIMEOUT x 10 us */
#define B2L_WIFI_CFG0_PHYS        0x14ffe000ULL   /* DT reg "config", first half = cfg0 */
#define B2L_WIFI_CFG0_SIZE        0x1000u
#define B2L_WIFI_ATU_BUS1         (1u << 24)      /* PCIE_ATU_BUS(1), dev 0, fn 0 */
#define B2L_WIFI_MEM_BASE         0x14000000u     /* DT ranges: 32-bit mem window */
#define B2L_WIFI_MEM_LIMIT        0x14feffffu
#define B2L_WIFI_EP_ID            0x447514e4u     /* BCM4375_D11AX_ID, VENDOR_BROADCOM */
#define B2L_WIFI_BAR0_WIN         0x80u           /* PCI_BAR0_WIN (pcicfg.h) */
#define B2L_WIFI_BP_CHIPC         0x18000000u     /* chipcommon on the backplane */
/* read_otp_from_bp (dhd_custom_cis.c, BCM4375_CHIP) */
#define B2L_WIFI_OTP_CTRL1        0x18010324u
#define B2L_WIFI_PMU_MINRES       0x18012618u
#define B2L_WIFI_CIS_START        0x18011120u
#define B2L_WIFI_CIS_WORDS        22u             /* 0x18011120..0x18011177 */

/* Build 149: firmware download (dhdpcie_bus_download_state, CR4 path).
 * Core map from the build 148 EROM dump: ARM CR4 registers 0x18002000,
 * its only (master) wrapper 0x18102000, TCM at 0x170000 (dongle_ram_base,
 * CR4_4369_RAM_BASE used for 4375), window 0x170000..0x300000. */
#define B2L_WIFI_BAR1_WIN         0x84u           /* PCI_BAR1_WIN: BAR2 -> backplane 0 */
#define B2L_WIFI_CR4_REGS         0x18002000u
#define B2L_WIFI_CR4_WRAP         0x18102000u
#define B2L_WIFI_RAM_BASE         0x170000u
#define B2L_WIFI_RAM_WINDOW       0x190000u
#define B2L_WIFI_AI_IOCTRL        0x408u
#define B2L_WIFI_AI_RESETCTRL     0x800u
#define B2L_WIFI_AI_RESETSTATUS   0x804u
#define B2L_WIFI_SICF_CPUHALT     0x20u
#define B2L_WIFI_SICF_FGC         0x2u
#define B2L_WIFI_SICF_CLOCK_EN    0x1u
#define B2L_WIFI_RNG_SIGNATURE    0xfeedc0deu     /* BCM_NVRAM_RNG_SIGNATURE */
#define B2L_WIFI_RNG_BYTES        128u            /* BCM_ENTROPY_HOST_NBYTES */
#define B2L_WIFI_NVRAM_CAP_K      (64u << 10)
#define BCMPCIE_RING_CTRL_SUB     0u
#define BCMPCIE_RING_RXPOST       1u
#define BCMPCIE_RING_CTRL_CPL     2u
#define BCMPCIE_RING_TX_CPL       3u
#define BCMPCIE_RING_RX_CPL       4u
#define BCMPCIE_RING_FLOW         5u              /* build 163: our one TX flowring (flow_ring_id 2) */

extern "C" {
extern void *b2l_wifi_fw, *b2l_wifi_nvram, *b2l_wifi_clm, *b2l_wifi_conf;
extern size_t b2l_wifi_fw_len, b2l_wifi_nvram_len, b2l_wifi_clm_len, b2l_wifi_conf_len;
void FlushPoC_DcacheRegion(vm_offset_t va, size_t length);
extern volatile int b2l_wifi_fw_state;
void b2l_wifi_fw_release(void);
void read_random(void *buffer, u_int numBytes);
}

/* cmn_config_val, pci-exynos9820_cal.c (Ch0 Gen2, 26 MHz) */
static const uint8_t b2l_wifi_cmn_config_val[48] = {
	0x01, 0xE1, 0x05, 0x00, 0x88, 0x88, 0x88, 0x0C,
	0x61, 0x45, 0x65, 0x24, 0x33, 0x18, 0xE3, 0xFC,
	0xD8, 0x05, 0xE6, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x60, 0x11, 0x00, 0xA0, 0x05, 0x04, 0x18, 0x88,
	0xC0, 0xFF, 0x9B, 0x52, 0x22, 0x30, 0x4F, 0xDC,
	0x40, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x80,
};
/* trsv_config_val, pci-exynos9820_cal.c (Ch0 Gen2, 26 MHz) */
static const uint8_t b2l_wifi_trsv_config_val[80] = {
	0x31, 0x40, 0x37, 0x99, 0x85, 0x00, 0xC0, 0xFF,
	0xFF, 0x3F, 0x8C, 0xC8, 0x02, 0x01, 0x88, 0x80,
	0x06, 0x90, 0x6C, 0x66, 0x09, 0x61, 0x42, 0x44,
	0xC6, 0x50, 0x0A, 0x33, 0x58, 0xE7, 0x20, 0x22,
	0x80, 0x38, 0x05, 0x85, 0x00, 0x00, 0x00, 0x7E,
	0x00, 0x00, 0x55, 0x15, 0xAC, 0xAA, 0x3E, 0x00,
	0x00, 0x00, 0x20, 0x3F, 0x00, 0x03, 0x01, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
	0x05, 0x85, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

class B2LWifi : public IOService {
	OSDeclareDefaultStructors(B2LWifi);
	IOMemoryDescriptor *periDesc, *aliveDesc, *cmuDesc, *fsysPinDesc;
	IOMemoryMap *periMap, *aliveMap, *cmuMap, *fsysPinMap;
	volatile uint8_t *peri, *alive, *cmu, *fsysPin;
	bool havePeri, haveAlive, haveCmu, haveFsysPin;
	bool chipOk;
	uint64_t bar0Phys, bar2Phys;
	uint32_t bar2Size;

	static uint32_t rd(volatile uint8_t *base, unsigned off) {
		uint32_t v = *(volatile uint32_t *)(base + off);
		__asm__ volatile("dmb oshld" ::: "memory");
		return v;
	}
	static void wr(volatile uint8_t *base, unsigned off, uint32_t v) {
		*(volatile uint32_t *)(base + off) = v;
		__asm__ volatile("dsb oshst" ::: "memory");
	}
	void auditCmu(const char *when);
	void restoreCmuQch(void);
	void auditPcieSide(void);
	void configurePciePins(void);
	void releasePhyIsolation(void);
	void auditElbi(void);
	void bringUpLink(void);
	void probeEndpoint(volatile uint8_t *dbi);
	void probeChip(volatile uint8_t *cfg, uint64_t bar0, uint32_t bar0Size);
	void startFirmware(void);
	static void firmwareThread(void *arg, wait_result_t wr);
	void downloadFirmware(void);
	bool runDongle(volatile uint8_t *cfg, volatile uint8_t *bar0, volatile uint8_t *tcm,
	    uint32_t ramtop, uint32_t sh);
public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free() APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(B2LWifi, IOService);

bool
B2LWifi::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	peri = nullptr;
	alive = nullptr;
	cmu = nullptr;
	periMap = nullptr;
	aliveMap = nullptr;
	cmuMap = nullptr;
	periDesc = nullptr;
	aliveDesc = nullptr;
	cmuDesc = nullptr;
	fsysPin = nullptr;
	fsysPinMap = nullptr;
	fsysPinDesc = nullptr;
	havePeri = haveAlive = haveCmu = haveFsysPin = false;
	chipOk = false;
	bar0Phys = bar2Phys = 0;
	bar2Size = 0;

	int enabled = 1, nowrite = 0;
	PE_parse_boot_argn("b2l_wifi", &enabled, sizeof(enabled));
	PE_parse_boot_argn("b2l_wifi_nowrite", &nowrite, sizeof(nowrite));

	/* Step 1: confirm the device tree still declares the chip. */
	DTEntry wlan;
	bool haveDT = (SecureDTLookupEntry(nullptr, "/wlan", &wlan) == kSuccess);
	uint32_t gpios[6] = { 0 };
	const void *prop = nullptr;
	unsigned propSize = 0;
	if (haveDT && SecureDTGetProperty(wlan, "gpios", &prop, &propSize) == kSuccess &&
	    propSize == sizeof(gpios)) {
		for (unsigned i = 0; i < 6; i++) {
			gpios[i] = ((const uint32_t *)prop)[i];
		}
	}
	IOLog("B2L WIFI: /wlan %s, gpios <%#x %u %#x %#x %u %#x>\n",
	    haveDT ? "present" : "missing",
	    gpios[0], gpios[1], gpios[2], gpios[3], gpios[4], gpios[5]);

	/* Step 2: map the two pin controllers.  Both are always-on blocks;
	 * only the two banks we need are read or written. */
	periDesc = IOMemoryDescriptor::withPhysicalAddress(B2L_WIFI_PERI0_PHYS, 4096,
	    kIODirectionInOut);
	if (periDesc) {
		periMap = periDesc->map(kIOMapInhibitCache);
	}
	if (periMap) {
		peri = (volatile uint8_t *)periMap->getVirtualAddress();
		havePeri = peri != nullptr;
	}
	aliveDesc = IOMemoryDescriptor::withPhysicalAddress(B2L_WIFI_ALIVE_PHYS, 4096,
	    kIODirectionInOut);
	if (aliveDesc) {
		aliveMap = aliveDesc->map(kIOMapInhibitCache);
	}
	if (aliveMap) {
		alive = (volatile uint8_t *)aliveMap->getVirtualAddress();
		haveAlive = alive != nullptr;
	}
	if (!havePeri || !haveAlive) {
		IOLog("B2L WIFI: pinctrl map failed (peri=%d alive=%d)\n",
		    havePeri, haveAlive);
		b2l_diag_record("B2L WIFI: map failed peri=%d alive=%d\n",
		    havePeri, haveAlive);
		return true;   /* keep booting: this driver is not on the boot path */
	}

	/* Step 2: map CMU_FSYS1 read-only.  This block is always powered and
	 * was readable from Linux throughout the port; we only sample it. */
	cmuDesc = IOMemoryDescriptor::withPhysicalAddress(B2L_WIFI_CMU_FSYS1_PHYS,
	    B2L_WIFI_CMU_MAP_SIZE, kIODirectionInOut);
	if (cmuDesc) {
		cmuMap = cmuDesc->map(kIOMapInhibitCache);
	}
	if (cmuMap) {
		cmu = (volatile uint8_t *)cmuMap->getVirtualAddress();
		haveCmu = cmu != nullptr;
	}
	if (!haveCmu) {
		IOLog("B2L WIFI: CMU_FSYS1 map failed\n");
		b2l_diag_record("B2L WIFI: CMU_FSYS1 map failed\n");
	}

	const unsigned reg = B2L_WIFI_GPG0_OFF;
	const unsigned wakeReg = B2L_WIFI_GPA0_OFF;
	uint32_t con = rd(peri, reg + B2L_WIFI_REG_CON);
	uint32_t dat = rd(peri, reg + B2L_WIFI_REG_DAT);
	uint32_t pud = rd(peri, reg + B2L_WIFI_REG_PUD);
	uint32_t drv = rd(peri, reg + B2L_WIFI_REG_DRV);
	uint32_t conPdn = rd(peri, reg + B2L_WIFI_REG_CON_PDN);
	uint32_t pudPdn = rd(peri, reg + B2L_WIFI_REG_PUD_PDN);
	uint32_t wakeCon = rd(alive, wakeReg + B2L_WIFI_REG_CON);
	uint32_t wakeDat = rd(alive, wakeReg + B2L_WIFI_REG_DAT);
	uint32_t wakePud = rd(alive, wakeReg + B2L_WIFI_REG_PUD);

	unsigned enFunc = (con >> (B2L_WIFI_EN_PIN * 4)) & 0xfu;
	unsigned enData = (dat >> B2L_WIFI_EN_PIN) & 1u;
	unsigned enPud = (pud >> (B2L_WIFI_EN_PIN * 4)) & 0xfu;
	unsigned wakeFunc = (wakeCon >> (B2L_WIFI_WAKE_PIN * 4)) & 0xfu;
	unsigned wakeData = (wakeDat >> B2L_WIFI_WAKE_PIN) & 1u;

	IOLog("B2L WIFI: gpg0 con=%#x dat=%#x pud=%#x drv=%#x con_pdn=%#x pud_pdn=%#x\n",
	    con, dat, pud, drv, conPdn, pudPdn);
	IOLog("B2L WIFI: cfg_wlanen(gpg0-5) function=%u data=%u pull=%u; "
	    "wlan_host_wake(gpa0-7) function=%u data=%u pull=%u\n",
	    enFunc, enData, enPud, wakeFunc, wakeData,
	    (wakePud >> (B2L_WIFI_WAKE_PIN * 4)) & 0xfu);
	b2l_diag_record("B2L WIFI: dt=%d gpio=<%#x %u %#x %#x %u %#x> "
	    "gpg0[con=%#x dat=%#x pud=%#x drv=%#x] "
	    "wl_en=%u/%u/%u wake=%u/%u/%u\n",
	    haveDT, gpios[0], gpios[1], gpios[2], gpios[3], gpios[4], gpios[5],
	    con, dat, pud, drv, enFunc, enData, enPud,
	    wakeFunc, wakeData, (wakePud >> (B2L_WIFI_WAKE_PIN * 4)) & 0xfu);

	auditCmu("start");

	/* Build 144: undo build 143.  Put ENABLE (bit 0) of the five PCIe GEN2
	 * QCH back to 0 so the clocks stay forced on by CLOCK_REQ, as the boot
	 * loader left them.  Done before any early return below. */
	restoreCmuQch();
	auditPcieSide();

	/* Build 145: PCIe pins as the DT pcie0 default state, with PERST held
	 * low (asserted) before WL_EN comes up, as pcie-exynos-host-v0.c
	 * requests the PERST gpio. */
	if (enabled && !nowrite) {
		configurePciePins();
		releasePhyIsolation();
		auditElbi();
	}

	/* Step 3: assert WL_EN when requested.  Function 1 is GPIO output on
	 * this pin controller; the data latch is set before the pin is
	 * switched to output so it never glitches low. */
	if (!enabled || nowrite) {
		IOLog("B2L WIFI: WL_EN write skipped (b2l_wifi=%d b2l_wifi_nowrite=%d)\n",
		    enabled, nowrite);
		b2l_diag_record("B2L WIFI: WL_EN write skipped enabled=%d nowrite=%d\n",
		    enabled, nowrite);
		registerService();
		return true;
	}
	if (enFunc == 1 && enData == 1 && enPud == 0) {
		IOLog("B2L WIFI: WL_EN already asserted\n");
		b2l_diag_record("B2L WIFI: WL_EN already asserted\n");
		bringUpLink();
		startFirmware();
		registerService();
		return true;
	}

	/* DT cfg_wlanen: pull 0, drive 3 (4-bit fields). */
	pud &= ~(0xfu << (B2L_WIFI_EN_PIN * 4));
	wr(peri, reg + B2L_WIFI_REG_PUD, pud);
	drv = (drv & ~(0xfu << (B2L_WIFI_EN_PIN * 4))) | (3u << (B2L_WIFI_EN_PIN * 4));
	wr(peri, reg + B2L_WIFI_REG_DRV, drv);
	dat |= (1u << B2L_WIFI_EN_PIN);
	wr(peri, reg + B2L_WIFI_REG_DAT, dat);
	con = (con & ~(0xfu << (B2L_WIFI_EN_PIN * 4))) | (1u << (B2L_WIFI_EN_PIN * 4));
	wr(peri, reg + B2L_WIFI_REG_CON, con);
	IODelay(1000);

	uint32_t conAfter = rd(peri, reg + B2L_WIFI_REG_CON);
	uint32_t datAfter = rd(peri, reg + B2L_WIFI_REG_DAT);
	uint32_t pudAfter = rd(peri, reg + B2L_WIFI_REG_PUD);
	uint32_t drvAfter = rd(peri, reg + B2L_WIFI_REG_DRV);
	uint32_t wakeAfter = rd(alive, wakeReg + B2L_WIFI_REG_DAT);
	IOLog("B2L WIFI: WL_EN asserted, con=%#x dat=%#x pud=%#x drv=%#x host_wake=%u\n",
	    conAfter, datAfter, pudAfter, drvAfter, (wakeAfter >> B2L_WIFI_WAKE_PIN) & 1u);
	b2l_diag_record("B2L WIFI: WL_EN asserted con=%#x dat=%#x pud=%#x drv=%#x host_wake=%u\n",
	    conAfter, datAfter, pudAfter, drvAfter, (wakeAfter >> B2L_WIFI_WAKE_PIN) & 1u);

	auditCmu("wl_en");
	bringUpLink();
	startFirmware();

	registerService();
	return true;
}

/* Read-only snapshot of the FSYS1 PCIe clock gates.  Bit 1 of a QCH
 * register tells whether the clock is actually supplied; bit 0 is the
 * software request.  On the clock gate registers, bit 21 is the gating
 * value and bit 20 selects manual mode.  Nothing is written here. */
void
B2LWifi::auditCmu(const char *when)
{
	if (!haveCmu) {
		return;
	}
	uint32_t qchApb = rd(cmu, B2L_WIFI_QCH_APB);
	uint32_t qchDbi = rd(cmu, B2L_WIFI_QCH_DBI);
	uint32_t qchMstr = rd(cmu, B2L_WIFI_QCH_MSTR);
	uint32_t qchPcs = rd(cmu, B2L_WIFI_QCH_PCS);
	uint32_t qchPhy = rd(cmu, B2L_WIFI_QCH_PHY);
	uint32_t clkRef = rd(cmu, B2L_WIFI_CLK_PHY_REFCLK);
	uint32_t clkDbi = rd(cmu, B2L_WIFI_CLK_DBI);
	uint32_t clkPhyApb = rd(cmu, B2L_WIFI_CLK_PHY_APB);
	uint32_t clkMstr = rd(cmu, B2L_WIFI_CLK_MSTR);
	uint32_t clkSubApb = rd(cmu, B2L_WIFI_CLK_SUB_APB);
	uint32_t clkPipe2 = rd(cmu, B2L_WIFI_CLK_PIPE2);
	uint32_t clkSlv = rd(cmu, B2L_WIFI_CLK_SLV);

	IOLog("B2L WIFI CMU[%s]: qch apb=%#x dbi=%#x mstr=%#x pcs=%#x phy=%#x "
	    "clk ref=%#x dbi=%#x phyapb=%#x mstr=%#x subapb=%#x pipe2=%#x slv=%#x\n",
	    when, qchApb, qchDbi, qchMstr, qchPcs, qchPhy,
	    clkRef, clkDbi, clkPhyApb, clkMstr, clkSubApb, clkPipe2, clkSlv);
	b2l_diag_record("B2L WIFI CMU[%s]: qch=<%#x %#x %#x %#x %#x> "
	    "req=%u/%u/%u/%u/%u clk=<%#x %#x %#x %#x %#x %#x %#x>\n",
	    when, qchApb, qchDbi, qchMstr, qchPcs, qchPhy,
	    (qchApb >> 1) & 1u, (qchDbi >> 1) & 1u, (qchMstr >> 1) & 1u,
	    (qchPcs >> 1) & 1u, (qchPhy >> 1) & 1u,
	    clkRef, clkDbi, clkPhyApb, clkMstr, clkSubApb, clkPipe2, clkSlv);
}

/* Build 144: clear ENABLE (bit 0) on the five PCIe GEN2 QCH registers,
 * back to the boot value 0x3f0002.  CLOCK_REQ and expire are preserved.
 * Each register is re-read after the write.  No ELBI, DBI or PHY access. */
void
B2LWifi::restoreCmuQch(void)
{
	if (!haveCmu) {
		IOLog("B2L WIFI CMU[restore]: CMU not mapped\n");
		b2l_diag_record("B2L WIFI CMU[restore]: CMU not mapped\n");
		return;
	}
	const unsigned qchOff[5] = {
		B2L_WIFI_QCH_APB, B2L_WIFI_QCH_DBI, B2L_WIFI_QCH_MSTR,
		B2L_WIFI_QCH_PCS, B2L_WIFI_QCH_PHY,
	};
	uint32_t before[5], after[5];

	for (unsigned i = 0; i < 5; i++) {
		before[i] = rd(cmu, qchOff[i]);
		if (before[i] & 1u) {
			wr(cmu, qchOff[i], before[i] & ~1u);   /* ENABLE = 0 */
			IODelay(1000);
		}
		after[i] = rd(cmu, qchOff[i]);
	}

	IOLog("B2L WIFI CMU[restore]: before=<%#x %#x %#x %#x %#x> "
	    "after=<%#x %#x %#x %#x %#x> enable=%u/%u/%u/%u/%u\n",
	    before[0], before[1], before[2], before[3], before[4],
	    after[0], after[1], after[2], after[3], after[4],
	    (after[0] & 1u), (after[1] & 1u), (after[2] & 1u),
	    (after[3] & 1u), (after[4] & 1u));
	b2l_diag_record("B2L WIFI CMU[restore]: before=<%#x %#x %#x %#x %#x> "
	    "after=<%#x %#x %#x %#x %#x> enable=%u/%u/%u/%u/%u\n",
	    before[0], before[1], before[2], before[3], before[4],
	    after[0], after[1], after[2], after[3], after[4],
	    (after[0] & 1u), (after[1] & 1u), (after[2] & 1u),
	    (after[3] & 1u), (after[4] & 1u));
}

/* Build 144: read-only snapshot of what the PCIe link sequence will touch
 * next.  Each block is mapped, read and unmapped here; nothing is written. */
static bool
b2l_wifi_peek(uint64_t phys, unsigned size, const unsigned *off, uint32_t *val,
    unsigned n)
{
	IOMemoryDescriptor *d = IOMemoryDescriptor::withPhysicalAddress(phys, size,
	    kIODirectionIn);
	IOMemoryMap *m = d ? d->map(kIOMapInhibitCache) : nullptr;
	volatile uint8_t *b = m ? (volatile uint8_t *)m->getVirtualAddress() : nullptr;
	for (unsigned i = 0; i < n; i++) {
		val[i] = b ? *(volatile uint32_t *)(b + off[i]) : 0xdeadbeefu;
	}
	__asm__ volatile("dmb oshld" ::: "memory");
	if (m) {
		m->release();
	}
	if (d) {
		d->release();
	}
	return b != nullptr;
}

void
B2LWifi::auditPcieSide(void)
{
	const unsigned pinOff[4] = {
		B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_CON, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DAT,
		B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_PUD, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DRV,
	};
	const unsigned pmuOff[2] = { B2L_WIFI_PMU_PCIE_PHY, B2L_WIFI_PMU_PCIE_PHY + 4 };
	const unsigned sysOff[3] = {
		B2L_WIFI_SYSREG_PCIE, B2L_WIFI_SYSREG_PCIE + 0xc, B2L_WIFI_SYSREG_SHARE,
	};
	uint32_t pin[4], pmu[2], sys[3];
	bool okPin = b2l_wifi_peek(B2L_WIFI_FSYS1_PIN_PHYS, 4096, pinOff, pin, 4);
	bool okPmu = b2l_wifi_peek(B2L_WIFI_PMU_PHYS, 4096, pmuOff, pmu, 2);
	bool okSys = b2l_wifi_peek(B2L_WIFI_SYSREG_PHYS, B2L_WIFI_SYSREG_SIZE, sysOff, sys, 3);

	IOLog("B2L WIFI PCIE[audit]: gpf2 con=%#x dat=%#x pud=%#x drv=%#x "
	    "clkreq=f%u perst=f%u/%u pmu phy0=%#x phy1=%#x "
	    "sysreg pcie=%#x phyctl=%#x share=%#x map=%d/%d/%d\n",
	    pin[0], pin[1], pin[2], pin[3], pin[0] & 0xfu, (pin[0] >> 8) & 0xfu,
	    (pin[1] >> 2) & 1u, pmu[0], pmu[1], sys[0], sys[1], sys[2],
	    okPin, okPmu, okSys);
	b2l_diag_record("B2L WIFI PCIE[audit]: gpf2=<%#x %#x %#x %#x> "
	    "clkreq=f%u perst=f%u/%u pmu=<%#x %#x> sysreg=<%#x %#x %#x> "
	    "map=%d/%d/%d\n",
	    pin[0], pin[1], pin[2], pin[3], pin[0] & 0xfu, (pin[0] >> 8) & 0xfu,
	    (pin[1] >> 2) & 1u, pmu[0], pmu[1], sys[0], sys[1], sys[2],
	    okPin, okPmu, okSys);
}

/* Build 145: pcie0 pins from the DT (pinctrl@13C40000):
 *   pcie0_clkreq gpf2-0: function 2, pull 3, drive 3
 *   pcie0_perst  gpf2-2: function 1 (output), drive 3, driven low
 * The data latch is cleared before the pin turns into an output so PERST
 * never glitches high.  The map is kept for the link sequence. */
void
B2LWifi::configurePciePins(void)
{
	fsysPinDesc = IOMemoryDescriptor::withPhysicalAddress(B2L_WIFI_FSYS1_PIN_PHYS,
	    4096, kIODirectionInOut);
	if (fsysPinDesc) {
		fsysPinMap = fsysPinDesc->map(kIOMapInhibitCache);
	}
	if (fsysPinMap) {
		fsysPin = (volatile uint8_t *)fsysPinMap->getVirtualAddress();
		haveFsysPin = fsysPin != nullptr;
	}
	if (!haveFsysPin) {
		b2l_diag_record("B2L WIFI PCIE[pins]: map failed\n");
		return;
	}
	const unsigned b = B2L_WIFI_GPF2_OFF;
	uint32_t con = rd(fsysPin, b + B2L_WIFI_REG_CON);
	uint32_t dat = rd(fsysPin, b + B2L_WIFI_REG_DAT);
	uint32_t pud = rd(fsysPin, b + B2L_WIFI_REG_PUD);
	uint32_t drv = rd(fsysPin, b + B2L_WIFI_REG_DRV);

	wr(fsysPin, b + B2L_WIFI_REG_DAT, dat & ~(1u << 2));               /* PERST low */
	wr(fsysPin, b + B2L_WIFI_REG_PUD, (pud & ~0xfu) | 3u);             /* clkreq pull-up */
	wr(fsysPin, b + B2L_WIFI_REG_DRV, (drv & ~0xf0fu) | 0x303u);       /* drive 3 on 0 and 2 */
	wr(fsysPin, b + B2L_WIFI_REG_CON, (con & ~0xf0fu) | 0x102u);       /* perst out, clkreq f2 */
	IODelay(100);

	uint32_t conA = rd(fsysPin, b + B2L_WIFI_REG_CON);
	uint32_t datA = rd(fsysPin, b + B2L_WIFI_REG_DAT);
	uint32_t pudA = rd(fsysPin, b + B2L_WIFI_REG_PUD);
	uint32_t drvA = rd(fsysPin, b + B2L_WIFI_REG_DRV);
	IOLog("B2L WIFI PCIE[pins]: before=<%#x %#x %#x %#x> after=<%#x %#x %#x %#x> "
	    "clkreq=f%u perst=f%u/%u\n", con, dat, pud, drv, conA, datA, pudA, drvA,
	    conA & 0xfu, (conA >> 8) & 0xfu, (datA >> 2) & 1u);
	b2l_diag_record("B2L WIFI PCIE[pins]: before=<%#x %#x %#x %#x> after=<%#x %#x %#x %#x> "
	    "clkreq=f%u perst=f%u/%u\n", con, dat, pud, drv, conA, datA, pudA, drvA,
	    conA & 0xfu, (conA >> 8) & 0xfu, (datA >> 2) & 1u);
}

/* Build 146: release the PCIe PHY isolation, as exynos_pcie_poweron does
 * with regmap_update_bits(pmureg, PCIE_PHY_CONTROL + ch * 4, 1, 1).
 * Only bit 0 of the ch0 word is changed; the word is re-read. */
void
B2LWifi::releasePhyIsolation(void)
{
	IOMemoryDescriptor *d = IOMemoryDescriptor::withPhysicalAddress(B2L_WIFI_PMU_PHYS,
	    4096, kIODirectionInOut);
	IOMemoryMap *m = d ? d->map(kIOMapInhibitCache) : nullptr;
	volatile uint8_t *pmu = m ? (volatile uint8_t *)m->getVirtualAddress() : nullptr;
	if (pmu) {
		uint32_t before = rd(pmu, B2L_WIFI_PMU_PCIE_PHY);
		if (!(before & 1u)) {
			wr(pmu, B2L_WIFI_PMU_PCIE_PHY, before | 1u);
			IODelay(100);
		}
		uint32_t after = rd(pmu, B2L_WIFI_PMU_PCIE_PHY);
		IOLog("B2L WIFI PCIE[pmu]: phy_control before=%#x after=%#x\n", before, after);
		b2l_diag_record("B2L WIFI PCIE[pmu]: phy_control before=%#x after=%#x\n",
		    before, after);
	} else {
		b2l_diag_record("B2L WIFI PCIE[pmu]: map failed\n");
	}
	if (m) {
		m->release();
	}
	if (d) {
		d->release();
	}
}

/* Build 146 read-only diagnostic: first access to the pcie0 controller.
 * A trace is recorded before the read so a bus stall is attributable. */
void
B2LWifi::auditElbi(void)
{
	const unsigned off[6] = {
		B2L_WIFI_ELBI_LTSSM_EN, B2L_WIFI_ELBI_LINKUP, B2L_WIFI_ELBI_PM_DSTATE,
		B2L_WIFI_ELBI_EXIT_L1, B2L_WIFI_ELBI_SOFT_RESET, B2L_WIFI_ELBI_QCH_SEL,
	};
	uint32_t v[6];
	b2l_diag_record("B2L WIFI ELBI: reading 0x13ed0000\n");
	bool ok = b2l_wifi_peek(B2L_WIFI_ELBI_PHYS, 4096, off, v, 6);
	IOLog("B2L WIFI ELBI: ltssm_en=%#x linkup=%#x dstate=%#x exit_l1=%#x "
	    "soft_reset=%#x qch_sel=%#x map=%d\n", v[0], v[1], v[2], v[3], v[4], v[5], ok);
	b2l_diag_record("B2L WIFI ELBI: ltssm_en=%#x linkup=%#x dstate=%#x exit_l1=%#x "
	    "soft_reset=%#x qch_sel=%#x map=%d\n", v[0], v[1], v[2], v[3], v[4], v[5], ok);
}

/* Build 147: PCIe link bring-up, in the order of pcie-exynos-host-v0.c
 * (exynos_pcie_poweron -> exynos_pcie_establish_link) and
 * pci-exynos9820_cal.c (phy_all_pwrdn_clear, phy_config) for channel 0.
 * Left out: OTP tuning, the I/A CDR helper (L1.2 only), the history
 * buffer, MSI and L1SS.  Every DBI access runs with IGNORE_ELECIDLE set
 * and L12ERR_CTRL cleared, as exynos_pcie_{rd,wr}_own_conf do. */
static volatile uint8_t *
b2l_wifi_map(uint64_t phys, unsigned size, IOMemoryDescriptor **d, IOMemoryMap **m)
{
	*d = IOMemoryDescriptor::withPhysicalAddress(phys, size, kIODirectionInOut);
	*m = *d ? (*d)->map(kIOMapInhibitCache) : nullptr;
	return *m ? (volatile uint8_t *)(*m)->getVirtualAddress() : nullptr;
}

void
B2LWifi::bringUpLink(void)
{
	int enabled = 1;
	PE_parse_boot_argn("b2l_wifi", &enabled, sizeof(enabled));
	if (!enabled || !haveFsysPin) {
		b2l_diag_record("B2L WIFI LINK: skipped enabled=%d pins=%d\n", enabled, haveFsysPin);
		return;
	}
	IOMemoryDescriptor *dE, *dP, *dC, *dD, *dS;
	IOMemoryMap *mE, *mP, *mC, *mD, *mS;
	volatile uint8_t *elbi = b2l_wifi_map(B2L_WIFI_ELBI_PHYS, 4096, &dE, &mE);
	volatile uint8_t *phy = b2l_wifi_map(B2L_WIFI_PHY_PHYS, 4096, &dP, &mP);
	volatile uint8_t *pcs = b2l_wifi_map(B2L_WIFI_PCS_PHYS, 4096, &dC, &mC);
	volatile uint8_t *dbi = b2l_wifi_map(B2L_WIFI_DBI_PHYS, 4096, &dD, &mD);
	volatile uint8_t *sys = b2l_wifi_map(B2L_WIFI_SYSREG_PAGE, 4096, &dS, &mS);
	if (!elbi || !phy || !pcs || !dbi || !sys) {
		b2l_diag_record("B2L WIFI LINK: map failed %d%d%d%d%d\n",
		    elbi != nullptr, phy != nullptr, pcs != nullptr, dbi != nullptr, sys != nullptr);
		goto out;
	}
	{
	const unsigned sb = B2L_WIFI_SYSREG_PCIE_OFF;
	uint32_t v;

	/* WLAN_REG_ON must be high for WIFI_TURNON_DELAY before the link. */
	IOSleep(200);
	b2l_diag_record("B2L WIFI LINK: start ltssm=%#x\n", rd(elbi, B2L_WIFI_ELBI_LINKUP));

	/* exynos_phy_all_pwrdn_clear (ch0) */
	wr(pcs, 0x100, rd(pcs, 0x100) & ~(1u << 6));
	wr(pcs, 0x104, rd(pcs, 0x104) & ~(1u << 7));
	wr(phy, 0x20 * 4, 0xc0);
	wr(phy, 0x57 * 4, 0x7e);

	/* own_conf guards for DBI before the link: NCLK on, ignore elecidle */
	wr(elbi, B2L_WIFI_ELBI_L12ERR_CTRL, 0);
	wr(pcs, 0xec, rd(pcs, 0xec) | (1u << 3));

	/* establish_link: core soft reset, then PERST high and 18-20 ms */
	wr(elbi, B2L_WIFI_ELBI_SOFT_RESET, 0);
	IODelay(20);
	wr(elbi, B2L_WIFI_ELBI_SOFT_RESET, 1);
	wr(fsysPin, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DAT,
	    rd(fsysPin, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DAT) | (1u << 2));
	IOSleep(20);

	wr(elbi, B2L_WIFI_ELBI_EXIT_L1, rd(elbi, B2L_WIFI_ELBI_EXIT_L1) | 0x1u | (1u << 4));
	wr(elbi, B2L_WIFI_ELBI_LINKDOWN_SEL, 1u << 1);
	wr(elbi, B2L_WIFI_ELBI_QCH_SEL, rd(elbi, B2L_WIFI_ELBI_QCH_SEL) & ~0xfffu);

	/* exynos_pcie_phy_config (ch0): PCS high speed + lane 0 enable */
	wr(sys, sb, rd(sys, sb) | (1u << 1));
	v = rd(sys, sb + 0xc);
	v = (((v & ~(0xfu << 4)) & ~(0xfu << 2)) | (0x3u << 2)) & ~(1u << 1);
	wr(sys, sb + 0xc, v);
	wr(sys, sb + 0xc, rd(sys, sb + 0xc) | (1u << 12));
	/* pcs_g_rst */
	wr(elbi, B2L_WIFI_ELBI_PCS_G_RST, 1); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_PCS_G_RST, 0); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_PCS_G_RST, 1); IODelay(10);
	for (unsigned i = 0; i < sizeof(b2l_wifi_cmn_config_val); i++) {
		wr(phy, i * 4, b2l_wifi_cmn_config_val[i]);
	}
	for (unsigned i = 0; i < sizeof(b2l_wifi_trsv_config_val); i++) {
		wr(phy, (0x30 + i) * 4, b2l_wifi_trsv_config_val[i]);
	}
	wr(pcs, 0xf8, 0x70);     /* tx latency */
	wr(pcs, 0x100, 0x87);    /* pcs refclk out control */
	wr(pcs, 0x104, 0x50);
	wr(pcs, 0xc, (rd(pcs, 0xc) & ~(1u << 1)) | (1u << 4));   /* PRGM_TIMEOUT_L1SS */
	/* PCIE_MAC RST, then PCS & PMA(CMN) RST */
	wr(elbi, B2L_WIFI_ELBI_MAC_RST, 1); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_MAC_RST, 0); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_MAC_RST, 1); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_PHY_CMN_RST, 1); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_PHY_CMN_RST, 0); IODelay(10);
	wr(elbi, B2L_WIFI_ELBI_PHY_CMN_RST, 1); IODelay(100);
	/* CDR reset */
	wr(pcs, 0xd0, rd(pcs, 0xd0) | (0x3u << 6));
	IODelay(20);
	v = (rd(pcs, 0xd0) & ~(0x3u << 6)) | (0x2u << 6);
	wr(pcs, 0xd0, v);
	wr(pcs, 0xd0, v & ~(0x3u << 6));
	b2l_diag_record("B2L WIFI LINK: phy configured sysreg=<%#x %#x> pcs_ec=%#x\n",
	    rd(sys, sb), rd(sys, sb + 0xc), rd(pcs, 0xec));

	/* dw_pcie_setup (num-lanes 1) and dw_pcie_setup_rc; no ATU here
	 * because exynos provides rd_other_conf. */
	wr(dbi, 0x710, (rd(dbi, 0x710) & ~(0x3fu << 16)) | (1u << 16));
	wr(dbi, 0x80c, (rd(dbi, 0x80c) & ~(0x1fu << 8)) | (1u << 8));
	wr(dbi, 0x10, 0x4);                                  /* BAR0 */
	wr(dbi, 0x14, 0x0);                                  /* BAR1 */
	wr(dbi, 0x8bc, rd(dbi, 0x8bc) | 1u);                 /* DBI RO write on */
	wr(dbi, 0x3c, (rd(dbi, 0x3c) & 0xffff00ffu) | 0x100u);   /* INTA */
	wr(dbi, 0x8bc, rd(dbi, 0x8bc) & ~1u);
	wr(dbi, 0x18, (rd(dbi, 0x18) & 0xff000000u) | 0x00ff0100u);   /* bus 0/1/ff */
	wr(dbi, 0x04, (rd(dbi, 0x04) & 0xffff0000u) | 0x107u);   /* IO MEM MASTER SERR */
	wr(dbi, 0x10, 0);
	wr(dbi, 0x8bc, rd(dbi, 0x8bc) | 1u);
	wr(dbi, 0x08, (rd(dbi, 0x08) & 0x0000ffffu) | (0x0604u << 16));   /* class: bridge */
	wr(dbi, 0x8bc, rd(dbi, 0x8bc) & ~1u);
	wr(dbi, 0x80c, rd(dbi, 0x80c) | (1u << 17));         /* SPEED_CHANGE */

	/* exynos_pcie_setup_rc (vendor/device ID rewrite left out) */
	wr(dbi, 0x8bc, 1u);                                  /* DBI_RO_WR_EN */
	v = rd(dbi, 0x70 + 0x0c);                            /* LNKCAP */
	v &= ~((0x7u << 15) | (0x3fu << 4) | 0xfu);
	v |= (0x6u << 15) | (0x1u << 4) | 0x2u;              /* L1 64us, x1, 5 GT/s */
	wr(dbi, 0x70 + 0x0c, v);
	wr(dbi, 0xb40, 0x1a);                                /* aux clock 26 MHz */
	wr(dbi, 0xb44, 0xea);                                /* L1.2 durations */
	wr(dbi, 0x40 + 0x04, 0);                             /* PM_CTRL */
	wr(dbi, 0x70 + 0x10, rd(dbi, 0x70 + 0x10) | (1u << 5));   /* retrain */
	wr(dbi, 0x70 + 0x30, (rd(dbi, 0x70 + 0x30) & ~0xfu) | 1u);   /* target: DT max-link-speed 1 */

	wr(pcs, 0xec, rd(pcs, 0xec) & ~(1u << 3));           /* ENABLE_ELECIDLE */

	/* LTSSM enable, then wait for L0 (0x11) or later: 0x0d..0x14 */
	wr(elbi, B2L_WIFI_ELBI_LTSSM_EN, 1);
	unsigned n = 0;
	for (; n < B2L_WIFI_LINK_POLLS; n++) {
		v = rd(elbi, B2L_WIFI_ELBI_LINKUP) & 0x1fu;
		if (v >= 0x0d && v <= 0x14) {
			break;
		}
		IODelay(10);
	}
	uint32_t linkup = rd(elbi, B2L_WIFI_ELBI_LINKUP);
	uint32_t dbgH = rd(elbi, 0x070);
	if (n >= B2L_WIFI_LINK_POLLS) {
		/* Samsung on failure: PERST low, LTSSM off. */
		wr(fsysPin, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DAT,
		    rd(fsysPin, B2L_WIFI_GPF2_OFF + B2L_WIFI_REG_DAT) & ~(1u << 2));
		wr(elbi, B2L_WIFI_ELBI_LTSSM_EN, 0);
		IOLog("B2L WIFI LINK: down linkup=%#x debug_h=%#x\n", linkup, dbgH);
		b2l_diag_record("B2L WIFI LINK: down linkup=%#x debug_h=%#x host_wake=%u\n",
		    linkup, dbgH, (rd(alive, B2L_WIFI_GPA0_OFF + B2L_WIFI_REG_DAT) >> B2L_WIFI_WAKE_PIN) & 1u);
		goto out;
	}
	wr(elbi, B2L_WIFI_ELBI_L12ERR_CTRL, 0);
	uint32_t lnk = rd(dbi, 0x70 + 0x10);                 /* LNKCTL | LNKSTA << 16 */
	IOLog("B2L WIFI LINK: up after %u polls linkup=%#x lnksta=%#x\n", n, linkup, lnk >> 16);
	b2l_diag_record("B2L WIFI LINK: up after %u polls linkup=%#x lnksta=%#x speed=%u width=%u host_wake=%u\n",
	    n, linkup, lnk >> 16, (lnk >> 16) & 0xfu, (lnk >> 20) & 0x3fu,
	    (rd(alive, B2L_WIFI_GPA0_OFF + B2L_WIFI_REG_DAT) >> B2L_WIFI_WAKE_PIN) & 1u);
	probeEndpoint(dbi);
	}
out:
	IOMemoryMap *maps[5] = { mE, mP, mC, mD, mS };
	IOMemoryDescriptor *descs[5] = { dE, dP, dC, dD, dS };
	for (unsigned i = 0; i < 5; i++) {
		if (maps[i]) {
			maps[i]->release();
		}
		if (descs[i]) {
			descs[i]->release();
		}
	}
}

/* Build 148: first config read of the endpoint on bus 1.  Outbound ATU
 * viewport 1 as CFG0 (exynos_pcie_prog_viewport_cfg0), then vendor/device,
 * class, header and BAR sizing (write ~0, read, restore: standard PCI). */
void
B2LWifi::probeEndpoint(volatile uint8_t *dbi)
{
	wr(dbi, 0x900, 0x1);                                  /* VIEWPORT: outbound, index 1 */
	wr(dbi, 0x90c, (uint32_t)B2L_WIFI_CFG0_PHYS);         /* LOWER_BASE */
	wr(dbi, 0x910, 0);                                    /* UPPER_BASE */
	wr(dbi, 0x914, (uint32_t)B2L_WIFI_CFG0_PHYS + B2L_WIFI_CFG0_SIZE - 1);
	wr(dbi, 0x918, B2L_WIFI_ATU_BUS1);                    /* LOWER_TARGET */
	wr(dbi, 0x91c, 0);                                    /* UPPER_TARGET */
	wr(dbi, 0x904, 0x4);                                  /* CR1: CFG0 */
	wr(dbi, 0x908, 1u << 31);                             /* CR2: enable */
	IODelay(10);

	IOMemoryDescriptor *d;
	IOMemoryMap *m;
	volatile uint8_t *cfg = b2l_wifi_map(B2L_WIFI_CFG0_PHYS, B2L_WIFI_CFG0_SIZE, &d, &m);
	if (cfg) {
		b2l_diag_record("B2L WIFI EP: reading cfg0\n");
		uint32_t id = rd(cfg, 0x00), cmd = rd(cfg, 0x04), cls = rd(cfg, 0x08);
		uint32_t hdr = rd(cfg, 0x0c), sub = rd(cfg, 0x2c);
		uint32_t bar[6], size[6];
		for (unsigned i = 0; i < 6; i++) {
			unsigned o = 0x10 + i * 4;
			bar[i] = rd(cfg, o);
			wr(cfg, o, 0xffffffffu);
			size[i] = rd(cfg, o);
			wr(cfg, o, bar[i]);
		}
		IOLog("B2L WIFI EP: id=%#x cmd=%#x class=%#x hdr=%#x sub=%#x\n", id, cmd, cls, hdr, sub);
		b2l_diag_record("B2L WIFI EP: id=%#x cmd=%#x class=%#x hdr=%#x sub=%#x\n",
		    id, cmd, cls, hdr, sub);
		b2l_diag_record("B2L WIFI EP: bar=<%#x %#x %#x %#x %#x %#x> "
		    "size=<%#x %#x %#x %#x %#x %#x>\n",
		    bar[0], bar[1], bar[2], bar[3], bar[4], bar[5],
		    size[0], size[1], size[2], size[3], size[4], size[5]);

		/* Build 148 stage 2: place the memory BARs in the DT window,
		 * open the RC bridge window and the outbound MEM ATU (viewport
		 * 0, exynos_pcie_prog_viewport_mem_outbound), enable the EP. */
		if (id != B2L_WIFI_EP_ID) {
			b2l_diag_record("B2L WIFI EP: unexpected id, stop\n");
			goto done;
		}
		b2l_diag_record("B2L WIFI BAR: assigning\n");
		uint64_t next = B2L_WIFI_MEM_BASE, base[6] = { 0 };
		uint32_t sz[6] = { 0 };
		for (unsigned i = 0; i < 6; i++) {
			if (!size[i] || (size[i] & 1u)) {               /* unused or I/O */
				continue;
			}
			bool is64 = ((size[i] >> 1) & 3u) == 2u;
			sz[i] = ~(size[i] & ~0xfu) + 1;
			next = (next + sz[i] - 1) & ~(uint64_t)(sz[i] - 1);
			if (next + sz[i] - 1 > B2L_WIFI_MEM_LIMIT) {
				b2l_diag_record("B2L WIFI BAR: bar%u size %#x does not fit\n", i, sz[i]);
				goto done;
			}
			base[i] = next;
			wr(cfg, 0x10 + i * 4, (uint32_t)next);
			next += sz[i];
			if (is64) {
				wr(cfg, 0x10 + (i + 1) * 4, 0);
				i++;
			}
		}
		/* RC type-1 header: non-prefetchable window, prefetchable off */
		wr(dbi, 0x20, (B2L_WIFI_MEM_LIMIT & 0xfff00000u) | (B2L_WIFI_MEM_BASE >> 16));
		wr(dbi, 0x24, 0x0000fff0u);
		wr(dbi, 0x28, 0);
		wr(dbi, 0x2c, 0);
		wr(dbi, 0x900, 0x0);                              /* VIEWPORT: outbound, index 0 */
		wr(dbi, 0x904, 0x0);                              /* CR1: MEM */
		wr(dbi, 0x90c, B2L_WIFI_MEM_BASE);
		wr(dbi, 0x910, 0);
		wr(dbi, 0x914, B2L_WIFI_MEM_LIMIT);
		wr(dbi, 0x918, B2L_WIFI_MEM_BASE);
		wr(dbi, 0x91c, 0);
		wr(dbi, 0x908, 1u << 31);
		wr(cfg, 0x04, (cmd & 0xffff0000u) | 0x0006u);     /* MEM + MASTER */
		IODelay(10);
		b2l_diag_record("B2L WIFI BAR: bar0=%#llx/%#x bar2=%#llx/%#x cmd=%#x bridge=%#x\n",
		    base[0], sz[0], base[2], sz[2], rd(cfg, 0x04), rd(dbi, 0x20));
		bar0Phys = base[0];
		bar2Phys = base[2];
		bar2Size = sz[2];
		if (base[0] && sz[0] >= 0x1000) {
			probeChip(cfg, base[0], sz[0]);
		}
	} else {
		b2l_diag_record("B2L WIFI EP: cfg0 map failed\n");
	}
done:
	if (m) {
		m->release();
	}
	if (d) {
		d->release();
	}
}

/* Build 148 stages 3-4: chip identity through BAR0 window 0
 * (PCI_BAR0_WIN, config 0x80), a raw dump of the enumeration ROM for the
 * core map, and the OTP CIS tuples that name the module (read_otp_from_bp
 * in dhd_custom_cis.c: OTP control 1 and PMU min resources are saved,
 * set, the 22 tuple words read, then both registers restored). */
static uint32_t
b2l_wifi_bp_read(volatile uint8_t *cfg, volatile uint8_t *bar0, uint32_t addr)
{
	*(volatile uint32_t *)(cfg + B2L_WIFI_BAR0_WIN) = addr & ~0xfffu;
	__asm__ volatile("dsb sy" ::: "memory");
	uint32_t v = *(volatile uint32_t *)(bar0 + (addr & 0xfffu));
	__asm__ volatile("dmb oshld" ::: "memory");
	return v;
}

static void
b2l_wifi_bp_write(volatile uint8_t *cfg, volatile uint8_t *bar0, uint32_t addr, uint32_t v)
{
	*(volatile uint32_t *)(cfg + B2L_WIFI_BAR0_WIN) = addr & ~0xfffu;
	__asm__ volatile("dsb sy" ::: "memory");
	*(volatile uint32_t *)(bar0 + (addr & 0xfffu)) = v;
	__asm__ volatile("dsb sy" ::: "memory");
}

void
B2LWifi::probeChip(volatile uint8_t *cfg, uint64_t bar0Phys, uint32_t bar0Size)
{
	IOMemoryDescriptor *d;
	IOMemoryMap *m;
	volatile uint8_t *bar0 = b2l_wifi_map(bar0Phys, 4096, &d, &m);
	if (!bar0) {
		b2l_diag_record("B2L WIFI CHIP: bar0 map failed\n");
		goto out;
	}
	{
	b2l_diag_record("B2L WIFI CHIP: reading chipcommon\n");
	uint32_t chipid = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_BP_CHIPC + 0x00);
	uint32_t caps = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_BP_CHIPC + 0x04);
	uint32_t erom = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_BP_CHIPC + 0xfc);
	IOLog("B2L WIFI CHIP: chipid=%#x chip=%#x rev=%u caps=%#x erom=%#x\n",
	    chipid, chipid & 0xffffu, (chipid >> 16) & 0xfu, caps, erom);
	b2l_diag_record("B2L WIFI CHIP: chipid=%#x chip=%#x rev=%u pkg=%u socitype=%u "
	    "caps=%#x erom=%#x\n", chipid, chipid & 0xffffu, (chipid >> 16) & 0xfu,
	    (chipid >> 20) & 0xfu, (chipid >> 28) & 0xfu, caps, erom);
	if ((chipid & 0xffffu) != 0x4375u) {
		b2l_diag_record("B2L WIFI CHIP: unexpected chip, stop\n");
		goto out;
	}

	/* Raw EROM, 8 words per line, until the END marker (0xf) */
	uint32_t w[8];
	for (unsigned off = 0, line = 0; off < 0x1000 && line < 64; line++) {
		bool end = false;
		for (unsigned k = 0; k < 8; k++, off += 4) {
			w[k] = end ? 0 : b2l_wifi_bp_read(cfg, bar0, erom + off);
			if (w[k] == 0xfu) {
				end = true;
			}
		}
		b2l_diag_record("B2L WIFI EROM %03x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
		    off - 32, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
		if (end) {
			break;
		}
	}

	b2l_diag_record("B2L WIFI OTP: reading CIS\n");
	uint32_t saveCtl = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_OTP_CTRL1);
	uint32_t saveRes = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_PMU_MINRES);
	b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_OTP_CTRL1, 0x00fa0000u);
	b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_PMU_MINRES, 0x0e4fffffu);
	uint32_t cis[B2L_WIFI_CIS_WORDS];
	for (unsigned i = 0; i < B2L_WIFI_CIS_WORDS; i++) {
		cis[i] = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CIS_START + i * 4);
	}
	b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_OTP_CTRL1, saveCtl);
	b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_PMU_MINRES, saveRes);
	b2l_diag_record("B2L WIFI OTP: ctl=%#x minres=%#x restored=%#x/%#x\n", saveCtl, saveRes,
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_OTP_CTRL1),
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_PMU_MINRES));
	for (unsigned i = 0; i < B2L_WIFI_CIS_WORDS; i += 11) {
		b2l_diag_record("B2L WIFI CIS %02u: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
		    i, cis[i], cis[i + 1], cis[i + 2], cis[i + 3], cis[i + 4], cis[i + 5],
		    cis[i + 6], cis[i + 7], cis[i + 8], cis[i + 9], cis[i + 10]);
	}
	(void)bar0Size;
	chipOk = true;
	}
out:
	if (m) {
		m->release();
	}
	if (d) {
		d->release();
	}
}

/* Build 149: firmware and NVRAM come from SYSTEM:/b2l-wifi/ through the
 * seed (b2l_userspace_seed.h), later than start(); a thread waits for them. */
void
B2LWifi::startFirmware(void)
{
	if (!chipOk || !bar0Phys || !bar2Phys || bar2Size < B2L_WIFI_RAM_BASE + B2L_WIFI_RAM_WINDOW) {
		b2l_diag_record("B2L WIFI FW: not started chip=%d bar2=%#llx/%#x\n", chipOk, bar2Phys, bar2Size);
		return;
	}
	thread_t th;
	retain();
	if (kernel_thread_start(&B2LWifi::firmwareThread, this, &th) == KERN_SUCCESS) {
		thread_deallocate(th);
	} else {
		release();
		b2l_diag_record("B2L WIFI FW: thread start failed\n");
	}
}

void
B2LWifi::firmwareThread(void *arg, wait_result_t)
{
	B2LWifi *self = (B2LWifi *)arg;
	unsigned waited = 0;
	/* The seed runs well after start() (build 149: later than 120 s). */
	while (b2l_wifi_fw_state == 0 && waited < 600000) {
		IOSleep(250);
		waited += 250;
	}
	__asm__ volatile("dmb ish" ::: "memory");
	b2l_diag_record("B2L WIFI FW: files state=%d fw=%lu nvram=%lu after %u ms\n",
	    b2l_wifi_fw_state, (unsigned long)b2l_wifi_fw_len, (unsigned long)b2l_wifi_nvram_len, waited);
	if (b2l_wifi_fw_state == 1) {
		self->downloadFirmware();
		b2l_wifi_fw_release();
	}
	self->release();
	thread_terminate(current_thread());
}

/* _ai_core_reset (aiutils.c) on one wrapper, through the BAR0 window. */
static void
b2l_wifi_spin_idle(volatile uint8_t *cfg, volatile uint8_t *bar0, uint32_t wrap)
{
	for (unsigned i = 0; i < 300 && b2l_wifi_bp_read(cfg, bar0, wrap + B2L_WIFI_AI_RESETSTATUS); i++) {
		IODelay(1);
	}
}

static void
b2l_wifi_core_reset(volatile uint8_t *cfg, volatile uint8_t *bar0, uint32_t wrap, uint32_t bits, uint32_t resetbits)
{
	b2l_wifi_spin_idle(cfg, bar0, wrap);
	b2l_wifi_bp_write(cfg, bar0, wrap + B2L_WIFI_AI_RESETCTRL, 1);
	IODelay(10);
	b2l_wifi_spin_idle(cfg, bar0, wrap);
	b2l_wifi_bp_write(cfg, bar0, wrap + B2L_WIFI_AI_IOCTRL,
	    bits | resetbits | B2L_WIFI_SICF_FGC | B2L_WIFI_SICF_CLOCK_EN);
	(void)b2l_wifi_bp_read(cfg, bar0, wrap + B2L_WIFI_AI_IOCTRL);
	b2l_wifi_spin_idle(cfg, bar0, wrap);
	for (unsigned n = 10; b2l_wifi_bp_read(cfg, bar0, wrap + B2L_WIFI_AI_RESETCTRL) != 0 && n; n--) {
		b2l_wifi_spin_idle(cfg, bar0, wrap);
		b2l_wifi_bp_write(cfg, bar0, wrap + B2L_WIFI_AI_RESETCTRL, 0);
		b2l_wifi_spin_idle(cfg, bar0, wrap);
	}
	b2l_wifi_bp_write(cfg, bar0, wrap + B2L_WIFI_AI_IOCTRL, bits | B2L_WIFI_SICF_CLOCK_EN);
	(void)b2l_wifi_bp_read(cfg, bar0, wrap + B2L_WIFI_AI_IOCTRL);
	IODelay(1);
}

/* process_nvram_vars (bcmutils.c): drop '\r' and '#' comments, one NUL per
 * non-empty line, then NUL padding to a word; returns the padded length. */
static size_t
b2l_wifi_nvram_vars(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
	size_t o = 0;
	bool skip = false;
	unsigned column = 0;
	for (size_t n = 0; n < len && o + 2 < cap; n++) {
		uint8_t c = in[n];
		if (c == '\r' || (skip && c != '\n')) {
			continue;
		}
		skip = false;
		if (c == '#') {
			skip = true;
			continue;
		}
		if (c == '\n') {
			if (column) {
				out[o++] = 0;
				column = 0;
			}
			continue;
		}
		out[o++] = c;
		column++;
	}
	if (column) {
		out[o++] = 0;
	}
	out[o++] = 0;                     /* terminating double NUL */
	while (o % 4 && o < cap) {
		out[o++] = 0;
	}
	return o;
}

void
B2LWifi::downloadFirmware(void)
{
	IOMemoryDescriptor *dC, *d0, *d2;
	IOMemoryMap *mC, *m0, *m2;
	volatile uint8_t *cfg = b2l_wifi_map(B2L_WIFI_CFG0_PHYS, B2L_WIFI_CFG0_SIZE, &dC, &mC);
	volatile uint8_t *bar0 = b2l_wifi_map(bar0Phys, 4096, &d0, &m0);
	volatile uint8_t *tcm = b2l_wifi_map(bar2Phys, bar2Size, &d2, &m2);
	uint8_t *vars = (uint8_t *)IOMallocData(B2L_WIFI_NVRAM_CAP_K);
	if (!cfg || !bar0 || !tcm || !vars) {
		b2l_diag_record("B2L WIFI FW: map failed %d%d%d%d\n", cfg != nullptr, bar0 != nullptr,
		    tcm != nullptr, vars != nullptr);
		goto out;
	}
	{
	const uint32_t *fw = (const uint32_t *)b2l_wifi_fw;
	const size_t fwWords = (b2l_wifi_fw_len + 3) / 4;

	/* 1. halt the ARM CR4 (si_core_reset(CPUHALT, CPUHALT)), then read the
	 *    TCM size from its bank registers (si_tcm_size). */
	b2l_diag_record("B2L WIFI FW: halting CR4 ioctrl=%#x resetctrl=%#x\n",
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_WRAP + B2L_WIFI_AI_IOCTRL),
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_WRAP + B2L_WIFI_AI_RESETCTRL));
	b2l_wifi_core_reset(cfg, bar0, B2L_WIFI_CR4_WRAP, B2L_WIFI_SICF_CPUHALT, B2L_WIFI_SICF_CPUHALT);
	uint32_t cap = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_REGS + 0x04);
	unsigned banks = (cap & 0xfu) + ((cap >> 4) & 0xfu);
	uint32_t ramsize = 0;
	for (unsigned i = 0; i < banks && i < 32; i++) {
		b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_CR4_REGS + 0x40, i);
		uint32_t info = b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_REGS + 0x44);
		ramsize += ((info & 0x7fu) + 1) * ((info & 0x200u) ? 1024u : 8192u);
	}
	b2l_diag_record("B2L WIFI FW: halted ioctrl=%#x resetctrl=%#x cap=%#x banks=%u ramsize=%#x\n",
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_WRAP + B2L_WIFI_AI_IOCTRL),
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_WRAP + B2L_WIFI_AI_RESETCTRL), cap, banks, ramsize);
	if (!ramsize || ramsize > B2L_WIFI_RAM_WINDOW || b2l_wifi_fw_len + 0x10000 > ramsize) {
		b2l_diag_record("B2L WIFI FW: ram size outside the EROM window, stop\n");
		goto out;
	}
	const uint32_t ramtop = B2L_WIFI_RAM_BASE + ramsize;
	#define TCM32(a) (*(volatile uint32_t *)(tcm + (a)))

	/* 2. BAR2 -> backplane 0, clear the shared-address word, copy the image */
	*(volatile uint32_t *)(cfg + B2L_WIFI_BAR1_WIN) = 0;
	__asm__ volatile("dsb sy" ::: "memory");
	TCM32(ramtop - 4) = 0;
	for (size_t i = 0; i < fwWords; i++) {
		TCM32(B2L_WIFI_RAM_BASE + i * 4) = fw[i];
	}
	__asm__ volatile("dsb sy" ::: "memory");
	unsigned bad = 0;
	for (size_t i = 0; i < fwWords; i++) {
		if (TCM32(B2L_WIFI_RAM_BASE + i * 4) != fw[i]) {
			bad++;
		}
	}
	b2l_diag_record("B2L WIFI FW: image %lu bytes at %#x, readback mismatches %u\n",
	    (unsigned long)b2l_wifi_fw_len, B2L_WIFI_RAM_BASE, bad);
	if (bad) {
		goto out;
	}

	/* 3. NVRAM vars below the last word, then the length token
	 *    (dhdpcie_bus_write_vars) and the random seed (dhdpcie_wrt_rnd). */
	size_t varsz = b2l_wifi_nvram_vars((const uint8_t *)b2l_wifi_nvram, b2l_wifi_nvram_len,
	    vars, B2L_WIFI_NVRAM_CAP_K);
	uint32_t varaddr = ramtop - 4 - (uint32_t)varsz;
	for (size_t i = 0; i < varsz; i += 4) {
		TCM32(varaddr + i) = *(const uint32_t *)(vars + i);
	}
	uint32_t words = (uint32_t)varsz / 4;
	uint32_t token = (~words << 16) | (words & 0xffffu);
	TCM32(ramtop - 4) = token;
	uint32_t rndAddr = ramtop - 4 - (words * 4 + 8);
	TCM32(rndAddr) = B2L_WIFI_RNG_SIGNATURE;
	TCM32(rndAddr + 4) = B2L_WIFI_RNG_BYTES;
	uint32_t rnd[B2L_WIFI_RNG_BYTES / 4];
	read_random(rnd, sizeof(rnd));
	for (unsigned i = 0; i < B2L_WIFI_RNG_BYTES / 4; i++) {
		TCM32(rndAddr - B2L_WIFI_RNG_BYTES + i * 4) = rnd[i];
	}
	b2l_diag_record("B2L WIFI FW: nvram %lu -> %lu bytes at %#x token=%#x rnd=%#x\n",
	    (unsigned long)b2l_wifi_nvram_len, (unsigned long)varsz, varaddr, token, rndAddr);

	/* 4. reset vector at address 0, verified, then run the CR4 */
	uint32_t vec0 = TCM32(0);
	if (vec0 == 0xffffffffu) {
		b2l_diag_record("B2L WIFI FW: address 0 reads ~0 (secured), stop\n");
		goto out;
	}
	TCM32(0) = fw[0];
	__asm__ volatile("dsb sy" ::: "memory");
	b2l_diag_record("B2L WIFI FW: reset vector %#x (was %#x) readback %#x, releasing CR4\n",
	    fw[0], vec0, TCM32(0));
	if (TCM32(0) != fw[0]) {
		goto out;
	}
	b2l_wifi_core_reset(cfg, bar0, B2L_WIFI_CR4_WRAP, 0, 0);

	/* 5. wait up to 5 s for pciedev_shared (dhdpcie_readshared) */
	uint32_t sh = 0;
	unsigned ms = 0;
	for (; ms < 5000; ms += 10) {
		sh = TCM32(ramtop - 4);
		if (sh && sh != token) {
			break;
		}
		IOSleep(10);
	}
	uint32_t wake = (rd(alive, B2L_WIFI_GPA0_OFF + B2L_WIFI_REG_DAT) >> B2L_WIFI_WAKE_PIN) & 1u;
	b2l_diag_record("B2L WIFI FW: shared=%#x after %u ms host_wake=%u ioctrl=%#x\n", sh, ms, wake,
	    b2l_wifi_bp_read(cfg, bar0, B2L_WIFI_CR4_WRAP + B2L_WIFI_AI_IOCTRL));
	if (!sh || sh == token || sh == 0xffffffffu || sh < B2L_WIFI_RAM_BASE || sh > ramtop - 4) {
		b2l_diag_record("B2L WIFI FW: firmware did not publish pciedev_shared\n");
		goto out;
	}
	uint32_t w[16];
	for (unsigned i = 0; i < 16; i++) {
		w[i] = TCM32(sh + i * 4);
	}
	b2l_diag_record("B2L WIFI SHARED: flags=%#x ver=%u trap=%#x assert=%#x/%#x line=%u cons=%#x "
	    "fwid=%#x rxoff=%#x h2dmb=%#x d2hmb=%#x rings=%#x\n",
	    w[0], w[0] & 0xffu, w[1], w[2], w[3], w[4], w[5], w[7], w[9], w[10], w[11], w[12]);

	/* 6. firmware console (hnd_cons_t: log.buf +8, buf_size +12, idx +16) */
	IOSleep(1000);
	uint32_t cons = w[5];
	if (cons >= B2L_WIFI_RAM_BASE && cons < ramtop) {
		uint32_t buf = TCM32(cons + 8), size = TCM32(cons + 12), idx = TCM32(cons + 16);
		b2l_diag_record("B2L WIFI CONS: buf=%#x size=%u idx=%u\n", buf, size, idx);
		if (buf >= B2L_WIFI_RAM_BASE && size && size <= 0x10000 && buf + size <= ramtop && idx < size) {
			char line[160];
			unsigned l = 0, start = idx > 3000 ? idx - 3000 : 0;
			for (unsigned i = start; i < idx; i++) {
				uint32_t word = TCM32(buf + (i & ~3u));
				char c = (char)(word >> ((i & 3u) * 8));
				if (c == '\n' || l == sizeof(line) - 1) {
					line[l] = 0;
					if (l) {
						b2l_diag_record("B2L WIFI CONS| %s\n", line);
					}
					l = 0;
					continue;
				}
				if (c >= 0x20 && c < 0x7f) {
					line[l++] = c;
				}
			}
			if (l) {
				line[l] = 0;
				b2l_diag_record("B2L WIFI CONS| %s\n", line);
			}
		}
	}
	if (runDongle(cfg, bar0, tcm, ramtop, sh)) {
		/* not reached: the dongle's poll loop keeps the maps (build 152) */
	}
	#undef TCM32
	}
out:
	if (vars) {
		IOFreeData(vars, B2L_WIFI_NVRAM_CAP_K);
	}
	IOMemoryMap *maps[3] = { mC, m0, m2 };
	IOMemoryDescriptor *descs[3] = { dC, d0, d2 };
	for (unsigned i = 0; i < 3; i++) {
		if (maps[i]) {
			maps[i]->release();
		}
		if (descs[i]) {
			descs[i]->release();
		}
	}
}

/* ---------------------------------------------------------------------
 * Build 151: msgbuf host side (bcmdhd_101_16 dhd_msgbuf.c / dhd_pcie.c),
 * polled, for shared version 7 with indices in TCM (no DMA_INDEX, 4-byte
 * slots) and D2H XOR-checksum sync (flags 0x30040007 at build 150).
 * Rings live in one 64 KiB physically contiguous DMA block below 4 GiB,
 * used only through an uncached mapping.  Then ioctls: version, CLM,
 * MAC, country, up, and WPA2-PSK through the in-dongle supplicant.
 * --------------------------------------------------------------------- */
#define B2L_WIFI_PCIE2_REGS       0x18003000u
#define B2L_WIFI_PCIE2_DB0        0x140u          /* PCIH2D_MailBox */
#define B2L_WIFI_PCIE2_DB1        0x144u          /* PCIH2D_DB1 (host ready) */
#define B2L_WIFI_DB_VALUE         0x12345678u
#define B2L_WIFI_DMA_SIZE         0x20000u      /* build 172: + RXPOST 512x32 at 0x10000, RXCPL 512x40 at 0x14000 */
#define B2L_WIFI_EPOCH_MOD        253u
#define B2L_WIFI_EPOCH_INIT       254u
#define B2L_WIFI_NEVT             8u
#define B2L_WIFI_EVT_LEN          4096u           /* escan results carry a whole bss_info */
#define B2L_WIFI_IOC_LEN          8192u
#define B2L_WIFI_IOC_PKTID        0xfffeu

struct b2l_wifi_ring {
	volatile uint8_t *va;
	uint64_t pa;
	uint16_t max, len;
	uint32_t widx, ridx;     /* TCM addresses of the index slots */
	uint16_t wr, rd;
	uint32_t seq;
};

struct b2l_wifi_dongle {
	volatile uint8_t *cfg, *bar0, *tcm;
	uint32_t ramtop, cons, consIdx;
	b2l_wifi_ring ring[6];
	volatile uint8_t *ioc, *resp, *evt;
	/* build 163: data path. TX buffers busy until their TX_STATUS; RX buffers posted. */
	volatile uint8_t *data;
	uint64_t dataPa;
	uint32_t h2dw, h2dr, rxOffset, rxLen, rxCount;
	bool flowDone, flowReady, txBusy[32];
	int16_t flowStatus;
	unsigned txNext;
	/* EAPOL frames for the handshake (one waiting at a time), and the network joined */
	bool wantEapol, eapolHave, joining, joinedUp, lost;
	uint16_t eapolLen;
	uint8_t eapol[640];          /* MESSAGE_MOST in b2l_wpa.c */
	struct hakios_sta_bss joined;
	struct lx_wpa *wpa;
	uint64_t iocPa, respPa, evtPa;
	uint16_t transId;
	uint32_t respId;
	bool ioctlDone;
	int16_t ioctlStatus;
	uint16_t ioctlLen;
	uint32_t events, linkUp, lastEvent[4];
	bool up, scanning, scanDone;
	int scanStatus;
	uint16_t syncId;
	uint8_t mac[6];
	struct hakios_sta_bss *heard;
	unsigned heardCount;
};
#define B2L_WIFI_HEARD_MOST       64u

#define D_TCM32(d, a) (*(volatile uint32_t *)((d)->tcm + (a)))

/* build 179: what the RX path has done (the heartbeat after the keys) */
static unsigned g_rxn, g_rxbad, g_repost;


static void
b2l_wifi_cons_dump(b2l_wifi_dongle *d, const char *tag)
{
	if (d->cons < B2L_WIFI_RAM_BASE || d->cons >= d->ramtop) {
		return;
	}
	uint32_t buf = D_TCM32(d, d->cons + 8), size = D_TCM32(d, d->cons + 12), idx = D_TCM32(d, d->cons + 16);
	if (buf < B2L_WIFI_RAM_BASE || !size || size > 0x10000 || buf + size > d->ramtop || idx >= size) {
		return;
	}
	unsigned from = d->consIdx, n = (idx + size - from) % size;
	if (n > 3000) {
		from = (idx + size - 3000) % size;
		n = 3000;
	}
	char line[160];
	unsigned l = 0;
	for (unsigned k = 0; k < n; k++) {
		unsigned i = (from + k) % size;
		char c = (char)(D_TCM32(d, buf + (i & ~3u)) >> ((i & 3u) * 8));
		if (c == '\n' || l == sizeof(line) - 1) {
			line[l] = 0;
			if (l) {
				b2l_diag_record("B2L WIFI CONS[%s]| %s\n", tag, line);
			}
			l = 0;
			continue;
		}
		if (c >= 0x20 && c < 0x7f) {
			line[l++] = c;
		}
	}
	if (l) {
		line[l] = 0;
		b2l_diag_record("B2L WIFI CONS[%s]| %s\n", tag, line);
	}
	d->consIdx = idx;
}

static void
b2l_wifi_ring_submit(b2l_wifi_dongle *d, unsigned r, const uint32_t *item, bool bell = true)
{
	b2l_wifi_ring *g = &d->ring[r];
	uint32_t w0 = item[0];
	w0 = (w0 & 0x00ffffffu) | ((g->seq % B2L_WIFI_EPOCH_MOD) << 24);   /* cmn_hdr.epoch */
	g->seq++;
	volatile uint32_t *slot = (volatile uint32_t *)(g->va + (uint32_t)g->wr * g->len);
	slot[0] = w0;
	for (unsigned i = 1; i < g->len / 4u; i++) {
		slot[i] = item[i];
	}
	/* build 152: cacheable block; clean to the point of coherency for the device */
	FlushPoC_DcacheRegion((vm_offset_t)slot, g->len);
	g->wr = (uint16_t)((g->wr + 1) % g->max);
	__asm__ volatile("dsb sy" ::: "memory");
	D_TCM32(d, g->widx) = g->wr;
	__asm__ volatile("dsb sy" ::: "memory");
	if (bell) {
		b2l_wifi_bp_write(d->cfg, d->bar0, B2L_WIFI_PCIE2_REGS + B2L_WIFI_PCIE2_DB0, B2L_WIFI_DB_VALUE);
	}
}

static void
b2l_wifi_post_buf(b2l_wifi_dongle *d, uint8_t type, uint32_t id, uint64_t pa, uint16_t len)
{
	uint32_t it[10] = { 0 };
	it[0] = type;                     /* msg_type, if_id 0, flags 0 */
	it[1] = id;
	it[2] = len;
	it[4] = (uint32_t)pa;
	it[5] = (uint32_t)(pa >> 32);
	b2l_wifi_ring_submit(d, BCMPCIE_RING_CTRL_SUB, it);
}

static inline uint32_t
b2l_be32(volatile const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* One bss_info of an escan result (wl_bss_info_v109_t offsets) into the
 * list the station side reads; a BSSID heard twice keeps its best RSSI. */
static void
b2l_wifi_heard_add(b2l_wifi_dongle *d, volatile const uint8_t *b, uint32_t avail)
{
	uint32_t blen = *(volatile const uint32_t *)(b + 4);
	if (!d->heard || blen < 128 || blen > avail) {
		return;
	}
	struct hakios_sta_bss one;
	bzero(&one, sizeof(one));
	for (unsigned i = 0; i < 6; i++) one.bssid[i] = b[8 + i];
	one.beacon_interval = *(volatile const uint16_t *)(b + 14);
	one.capability = *(volatile const uint16_t *)(b + 16);
	one.ssid_length = b[18] > 32 ? 32 : b[18];
	for (unsigned i = 0; i < one.ssid_length; i++) one.ssid[i] = b[19 + i];
	uint16_t chanspec = *(volatile const uint16_t *)(b + 72);
	unsigned ch = b[88] ? b[88] : (chanspec & 0xffu);
	bool g2 = (chanspec & 0xc000u) == 0;
	one.frequency = (uint16_t)(g2 ? (ch == 14 ? 2484 : 2407 + 5 * ch) : 5000 + 5 * ch);
	one.rssi = *(volatile const int16_t *)(b + 78);
	uint16_t ieOff = *(volatile const uint16_t *)(b + 116);
	uint32_t ieLen = *(volatile const uint32_t *)(b + 120);
	if (ieOff >= 128 && ieOff + ieLen <= blen) {
		one.elements_length = (uint16_t)(ieLen > HAKIOS_STA_ELEMENTS_MOST ? HAKIOS_STA_ELEMENTS_MOST : ieLen);
		for (unsigned i = 0; i < one.elements_length; i++) one.elements[i] = b[ieOff + i];
	}
	for (unsigned i = 0; i < d->heardCount; i++) {
		if (!memcmp(d->heard[i].bssid, one.bssid, 6)) {
			if (one.rssi > d->heard[i].rssi) {
				d->heard[i] = one;
			}
			return;
		}
	}
	if (d->heardCount < B2L_WIFI_HEARD_MOST) {
		d->heard[d->heardCount++] = one;
	}
}

/* A WL event buffer: ether header (14) + bcmeth (10) + wl_event_msg_t (48, big endian) + data */
static void
b2l_wifi_event(b2l_wifi_dongle *d, volatile const uint8_t *e, uint32_t len)
{
	uint32_t et = b2l_be32(e + 28), st = b2l_be32(e + 32), rs = b2l_be32(e + 36), dl = b2l_be32(e + 44);
	uint16_t fl = (uint16_t)((e[26] << 8) | e[27]);
	d->events++;
	d->lastEvent[0] = et;
	d->lastEvent[1] = st;
	d->lastEvent[2] = rs;
	if (et == 69) {                   /* WLC_E_ESCAN_RESULT */
		if (st == 8 && dl >= 12 && 72 + dl <= len) {   /* WLC_E_STATUS_PARTIAL: wl_escan_result_t */
			volatile const uint8_t *r = e + 72;
			uint16_t count = *(volatile const uint16_t *)(r + 10);
			uint32_t at = 12;
			for (unsigned i = 0; i < count && at + 128 <= dl; i++) {
				b2l_wifi_heard_add(d, r + at, dl - at);
				at += *(volatile const uint32_t *)(r + at + 4);
			}
		} else if (st != 8) {
			d->scanDone = true;
			d->scanStatus = (int)st;
			b2l_diag_record("B2L WIFI SCAN: done status=%u heard=%u\n", st, d->heardCount);
		}
		return;
	}
	if (et == 16) {                   /* WLC_E_LINK: flags bit 0 = link up */
		d->linkUp = fl & 1u;
		if (!d->linkUp && d->joinedUp) {
			d->lost = true;           /* build 163: told to Darwin by the poll thread, outside the lock */
		}
	}
	b2l_diag_record("B2L WIFI EVENT: type=%u status=%u reason=%u flags=%#x len=%u\n", et, st, rs, fl, len);
}

/* A D2H item is complete when its epoch is the ring's and its words XOR to 0 (D2H sync, shared flags) */
static bool
b2l_wifi_d2h_ok(b2l_wifi_ring *g, volatile uint32_t *it)
{
	uint8_t want = (uint8_t)(g->seq % B2L_WIFI_EPOCH_MOD);
	bool ok = false;
	for (unsigned t = 0; t < 2000 && !ok; t++) {
		FlushPoC_DcacheRegion((vm_offset_t)it, g->len);
		uint32_t x = 0;
		for (unsigned i = 0; i < g->len / 4u; i++) {
			x ^= it[i];
		}
		ok = ((it[0] >> 24) == want) && x == 0;
		if (!ok) {
			IODelay(10);
		}
	}
	g->seq++;
	return ok;
}

/* Drain the control completion ring: ioctl acks/completions, WL events. */
static void
b2l_wifi_poll(b2l_wifi_dongle *d)
{
	b2l_wifi_ring *g = &d->ring[BCMPCIE_RING_CTRL_CPL];
	uint16_t w = (uint16_t)D_TCM32(d, g->widx);
	if (w >= g->max) {
		return;
	}
	while (g->rd != w) {
		volatile uint32_t *it = (volatile uint32_t *)(g->va + (uint32_t)g->rd * g->len);
		uint8_t want = (uint8_t)(g->seq % B2L_WIFI_EPOCH_MOD);
		bool ok = b2l_wifi_d2h_ok(g, it);
		uint8_t type = (uint8_t)it[0];
		if (!ok) {
			b2l_diag_record("B2L WIFI MSG: bad sync rd=%u type=%#x epoch=%u want=%u\n",
			    g->rd, type, it[0] >> 24, want);
		} else if (type == 0x0c) {        /* MSG_TYPE_IOCTL_CMPLT */
			d->ioctlStatus = (int16_t)(it[2] & 0xffffu);
			d->ioctlLen = (uint16_t)(it[3] & 0xffffu);
			if ((uint16_t)(it[3] >> 16) == d->transId) {
				d->ioctlDone = true;
			}
		} else if (type == 0x0e) {        /* MSG_TYPE_WL_EVENT */
			uint32_t id = it[1], len = it[3] & 0xffffu;
			if (id >= 0x2000 && id < 0x2000 + B2L_WIFI_NEVT) {
				volatile uint8_t *e = d->evt + (id - 0x2000) * B2L_WIFI_EVT_LEN;
				FlushPoC_DcacheRegion((vm_offset_t)e, B2L_WIFI_EVT_LEN);
				if (len >= 72) {
					b2l_wifi_event(d, e, len);
				}
				b2l_wifi_post_buf(d, 0x0d, id, d->evtPa + (id - 0x2000) * B2L_WIFI_EVT_LEN, B2L_WIFI_EVT_LEN);
			}
		} else if (type == 0x04 || type == 0x06) {     /* FLOW_RING_CREATE_CMPLT / _DELETE_CMPLT */
			d->flowStatus = (int16_t)(it[2] & 0xffffu);
			d->flowDone = true;
			b2l_diag_record("B2L WIFI FLOW: %s ring %u status=%d\n", type == 0x04 ? "created" : "deleted",
			    it[2] >> 16, d->flowStatus);
		} else if (type != 0x0a) {        /* not IOCTLPTR_REQ_ACK */
			b2l_diag_record("B2L WIFI MSG: type=%#x w1=%#x w2=%#x\n", type, it[1], it[2]);
		}
		g->rd = (uint16_t)((g->rd + 1) % g->max);
	}
	D_TCM32(d, g->ridx) = g->rd;
}

/* One ioctl through the control ring; returns the firmware status, or
 * -999 on timeout.  out/outLen receive the response bytes. */
static int
b2l_wifi_ioctl(b2l_wifi_dongle *d, uint32_t cmd, const void *in, uint32_t inLen,
    void *out, uint32_t outMax, uint32_t *outLen)
{
	uint32_t len = inLen > outMax ? inLen : outMax;
	if (len > B2L_WIFI_IOC_LEN - 64) {
		return -998;
	}
	for (uint32_t i = 0; i < B2L_WIFI_IOC_LEN / 4; i++) {
		((volatile uint32_t *)d->ioc)[i] = 0;
	}
	for (uint32_t i = 0; i < inLen; i++) {
		d->ioc[i] = ((const uint8_t *)in)[i];
	}
	FlushPoC_DcacheRegion((vm_offset_t)d->ioc, B2L_WIFI_IOC_LEN);
	FlushPoC_DcacheRegion((vm_offset_t)d->resp, B2L_WIFI_IOC_LEN);
	d->respId++;
	b2l_wifi_post_buf(d, 0x0b, d->respId, d->respPa, (uint16_t)B2L_WIFI_IOC_LEN);
	d->transId++;
	d->ioctlDone = false;
	uint32_t it[10] = { 0 };
	it[0] = 0x09;                     /* MSG_TYPE_IOCTLPTR_REQ */
	it[1] = B2L_WIFI_IOC_PKTID;
	it[2] = cmd;
	it[3] = d->transId | (len << 16); /* trans_id, input_buf_len */
	it[4] = len;                      /* output_buf_len */
	it[6] = (uint32_t)d->iocPa;
	it[7] = (uint32_t)(d->iocPa >> 32);
	__asm__ volatile("dsb sy" ::: "memory");
	b2l_wifi_ring_submit(d, BCMPCIE_RING_CTRL_SUB, it);
	for (unsigned ms = 0; ms < 3000 && !d->ioctlDone; ms++) {
		b2l_wifi_poll(d);
		if (!d->ioctlDone) {
			IODelay(1000);
		}
	}
	if (!d->ioctlDone) {
		b2l_diag_record("B2L WIFI IOCTL: cmd %u timeout ctrl_cpl w=%u rd=%u ctrl_sub r=%u\n", cmd,
		    D_TCM32(d, d->ring[BCMPCIE_RING_CTRL_CPL].widx), d->ring[BCMPCIE_RING_CTRL_CPL].rd,
		    D_TCM32(d, d->ring[BCMPCIE_RING_CTRL_SUB].ridx));
		return -999;
	}
	uint32_t n = d->ioctlLen < outMax ? d->ioctlLen : outMax;
	FlushPoC_DcacheRegion((vm_offset_t)d->resp, B2L_WIFI_IOC_LEN);
	for (uint32_t i = 0; i < n && out; i++) {
		((uint8_t *)out)[i] = d->resp[i];
	}
	if (outLen) {
		*outLen = n;
	}
	return d->ioctlStatus;
}

/* "name\0" + value, as bcm_mkiovar */
static int
b2l_wifi_iovar(b2l_wifi_dongle *d, bool set, const char *name, const void *val, uint32_t valLen,
    void *out, uint32_t outMax)
{
	static uint8_t buf[B2L_WIFI_IOC_LEN - 64];
	uint32_t nl = (uint32_t)strlen(name) + 1;
	if (nl + valLen > sizeof(buf)) {
		return -997;
	}
	memcpy(buf, name, nl);
	if (val && valLen) {
		memcpy(buf + nl, val, valLen);
	}
	return b2l_wifi_ioctl(d, set ? 263u : 262u, buf, nl + valLen, out, set ? 0 : outMax, nullptr);
}

static int
b2l_wifi_set_int(b2l_wifi_dongle *d, uint32_t cmd, uint32_t v)
{
	return b2l_wifi_ioctl(d, cmd, &v, 4, nullptr, 0, nullptr);
}

static int
b2l_wifi_iovar_int(b2l_wifi_dongle *d, const char *name, uint32_t v)
{
	return b2l_wifi_iovar(d, true, name, &v, 4, nullptr, 0);
}

/* key=value from wifi.conf; returns the value length or 0 */
static unsigned
b2l_wifi_conf_get(const char *conf, size_t len, const char *key, char *out, unsigned cap)
{
	size_t kl = strlen(key);
	for (size_t i = 0; i < len;) {
		size_t e = i;
		while (e < len && conf[e] != '\n') {
			e++;
		}
		if (e - i > kl && conf[i + kl] == '=' && !strncmp(conf + i, key, kl)) {
			unsigned n = 0;
			for (size_t j = i + kl + 1; j < e && conf[j] != '\r' && n + 1 < cap; j++) {
				out[n++] = conf[j];
			}
			out[n] = 0;
			return n;
		}
		i = e + 1;
	}
	out[0] = 0;
	return 0;
}

static bool
b2l_wifi_parse_mac(const char *s, uint8_t mac[6])
{
	for (unsigned i = 0; i < 6; i++) {
		unsigned v = 0;
		for (unsigned k = 0; k < 2; k++) {
			char c = s[i * 3 + k];
			v <<= 4;
			if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
			else return false;
		}
		if (i < 5 && s[i * 3 + 2] != ':') return false;
		mac[i] = (uint8_t)v;
	}
	return true;
}

/* ---- build 152: the BCM4375 behind HakiOS's station boundary (sys/hakios.sta.h) ---- */
static b2l_wifi_dongle *g_wifi;
static IOLock *g_wifi_lock;

static void b2l_unjoin(b2l_wifi_dongle *d, bool disassoc);

static int
b2l_sta_power(bool on)
{
	if (!on) {
		b2l_unjoin(g_wifi, false);    /* build 163: the flowring and the link go with the radio */
	}
	IOLockLock(g_wifi_lock);
	int r = b2l_wifi_set_int(g_wifi, on ? 2u : 3u, 0);    /* WLC_UP / WLC_DOWN */
	g_wifi->up = on && r == 0;
	if (!on) {
		g_wifi->linkUp = 0;
	}
	IOLockUnlock(g_wifi_lock);
	b2l_diag_record("B2L WIFI STA: power %s status=%d\n", on ? "on" : "off", r);
	return r == 0 ? 0 : EIO;
}

static int
b2l_sta_scan(const struct hakios_sta_scan *what)
{
	uint8_t p[84];                    /* wl_escan_params_v2_t: 8 + wl_scan_params_v2_t (76) */
	bzero(p, sizeof(p));
	IOLockLock(g_wifi_lock);
	b2l_wifi_dongle *d = g_wifi;
	if (!d->up) {
		IOLockUnlock(g_wifi_lock);
		return ENETDOWN;
	}
	d->heardCount = 0;
	d->scanDone = false;
	d->scanning = true;
	d->syncId++;
	/* build 163: firmware 18.41 answers v1 with BCME_VERSION (-37, build 162): wl_escan_params_v2 */
	*(uint32_t *)(p + 0) = 2;         /* ESCAN_REQ_VERSION_V2 */
	*(uint16_t *)(p + 4) = 1;         /* WL_SCAN_ACTION_START */
	*(uint16_t *)(p + 6) = d->syncId;
	*(uint16_t *)(p + 8) = 2;         /* WL_SCAN_PARAMS_VERSION_V2 */
	*(uint16_t *)(p + 10) = 76;       /* sizeof(wl_scan_params_v2_t) */
	unsigned sl = what && what->ssid_length <= 32 ? what->ssid_length : 0;
	*(uint32_t *)(p + 12) = sl;
	if (sl) {
		memcpy(p + 16, what->ssid, sl);
	}
	memset(p + 48, 0xff, 6);          /* any BSSID */
	p[54] = 2;                        /* DOT11_BSSTYPE_ANY */
	*(uint32_t *)(p + 56) = what && what->passive ? 1u : 0u;   /* WL_SCANFLAGS_PASSIVE */
	*(int32_t *)(p + 60) = -1;        /* nprobes, active, passive, home: firmware defaults */
	*(int32_t *)(p + 64) = -1;
	*(int32_t *)(p + 68) = -1;
	*(int32_t *)(p + 72) = -1;
	int r = b2l_wifi_iovar(d, true, "escan", p, sizeof(p), nullptr, 0);
	IOLockUnlock(g_wifi_lock);
	if (r) {
		d->scanning = false;
		b2l_diag_record("B2L WIFI SCAN: escan status=%d\n", r);
		return EIO;
	}
	for (unsigned ms = 0; ms < 15000 && !d->scanDone; ms += 100) {
		IOSleep(100);
	}
	d->scanning = false;
	return d->scanDone ? (d->scanStatus == 0 ? 0 : EIO) : ETIMEDOUT;
}

static void
b2l_sta_heard(void (*each)(const struct hakios_sta_bss *bss, void *context), void *context)
{
	IOLockLock(g_wifi_lock);
	for (unsigned i = 0; i < g_wifi->heardCount; i++) {
		each(&g_wifi->heard[i], context);
	}
	IOLockUnlock(g_wifi_lock);
}

/* ---------------------------------------------------------------------
 * Build 163: the data path and the join (bcmdhd_101_16 dhd_msgbuf.c,
 * wl_cfg80211.c). The firmware has no in-dongle supplicant (sup_wpa
 * BCME_UNSUPPORTED, build 151), so the WPA2 four-way handshake runs here
 * (b2l_wpa.c, from ios-pipa's lx_wpa.c, MIT). One TX flowring (tid 0) per
 * network joined; RX buffers on the common RX post ring. Frames are 802.3.
 * --------------------------------------------------------------------- */
#define B2L_DATA_SIZE             0x91000u
#define B2L_DATA_FLOW_OFF         0x0000u        /* 64 x 48-byte TX post items */
#define B2L_DATA_RX_OFF           0x1000u        /* 256 x 2 KiB */
#define B2L_DATA_TX_OFF           0x81000u       /* 32 x 2 KiB */
#define B2L_DATA_BUF              2048u
#define B2L_RX_BUF                2048u          /* the firmware's rxbufpost_sz (1670) fits */
#define B2L_DATA_NRX              256u           /* build 172: dhd posts max_host_rxbufs (256 default) */
#define B2L_DATA_NTX              32u
#define B2L_RX_ID0                0x3000u
#define B2L_TX_ID0                0x4000u
#define B2L_FLOW_ID               2u             /* BCMPCIE_H2D_COMMON_MSGRINGS: the first flowring */
static IOLock *g_data_lock;
static IOBufferMemoryDescriptor *g_data_md;

/* build 176: 175 got through messages 1-4; the freeze follows message 4 (177: and the keys). The steps after it, 6 s apart
 * (the log writer flushes every 5 s), the first time only */
static unsigned g_steps = 12;         /* build 184: the steps and the heartbeat are off (budget spent) */
/* build 184: the join completes for wifid, but iOS's network stack is not told of the link and gets no
 * frames. A freeze then comes from wifid's side or the driver; none means it comes from the stack's reaction */
static const bool g_link_to_ios = true;   /* build 198: iOS sees the link (197: hidden, no freeze in 8 joins) */
/* build 198: no more stages (194-196); g_up_at dates the first link up for the T+ lines */
static uint64_t g_up_at;
static IOLock *g_unjoin_lock;   /* build 198: wifid's leave and the poll thread's "link lost" one at a time */
static void
b2l_step(const char *what, int a, int b)
{
	if (g_steps < 12) {               /* build 177: a budget, so the first frames in and out get steps too */
		g_steps++;
		b2l_diag_record("B2L WIFI STEP: %s (%d, %d)\n", what, a, b);
		IOSleep(6000);
	}
}

/* build 170: the bus address of a byte of the data block, page by page (never base + offset on trust) */
static uint64_t
b2l_data_pa(b2l_wifi_dongle *d, uint32_t off)
{
	IOByteCount len = 0;
	addr64_t pa = g_data_md ? g_data_md->getPhysicalSegment(off, &len, kIOMemoryMapperNone) : 0;
	return pa ? (uint64_t)pa : d->dataPa + off;
}

/* build 170: does a block's physical address follow its virtual one? */
static unsigned
b2l_dma_check(IOBufferMemoryDescriptor *md, uint64_t base, uint32_t size, const char *what)
{
	unsigned bad = 0;
	for (uint32_t off = 0; off < size; off += 0x1000u) {
		IOByteCount len = 0;
		addr64_t pa = md->getPhysicalSegment(off, &len, kIOMemoryMapperNone);
		if ((uint64_t)pa != base + off) {
			if (!bad) {
				b2l_diag_record("B2L WIFI DMA: %s not contiguous: offset %#x at %#llx, expected %#llx\n", what, off,
				    (uint64_t)pa, base + off);
			}
			bad++;
		}
	}
	b2l_diag_record("B2L WIFI DMA: %s %#x bytes from %#llx, %u pages out of place\n", what, size, base, bad);
	return bad;
}

/* build 170: the endpoint's view of PCIe errors (status, device status, AER) */
static void
b2l_pcie_errs(b2l_wifi_dongle *d, const char *when)
{
	volatile uint8_t *c = d->cfg;
	uint32_t cmdsta = *(volatile uint32_t *)(c + 0x04), devsta = 0, unc = 0, cor = 0;
	uint32_t p = *(volatile uint32_t *)(c + 0x34) & 0xfcu;
	for (unsigned n = 0; p && n < 16; n++) {
		uint32_t h = *(volatile uint32_t *)(c + p);
		if ((h & 0xffu) == 0x10u) {
			devsta = *(volatile uint32_t *)(c + p + 0x08) >> 16;
		}
		p = (h >> 8) & 0xfcu;
	}
	for (uint32_t off = 0x100, n = 0; off >= 0x100 && off < B2L_WIFI_CFG0_SIZE && n < 16; n++) {
		uint32_t h = *(volatile uint32_t *)(c + off);
		if ((h & 0xffffu) == 1u) {
			unc = *(volatile uint32_t *)(c + off + 0x04);
			cor = *(volatile uint32_t *)(c + off + 0x10);
		}
		off = h >> 20;
	}
	b2l_diag_record("B2L WIFI PCIERR[%s]: status=%#x devsta=%#x aer unc=%#x cor=%#x\n", when, cmdsta >> 16, devsta, unc, cor);
}

static void
b2l_data_post_rx(b2l_wifi_dongle *d, unsigned i)
{
	uint64_t pa = b2l_data_pa(d, B2L_DATA_RX_OFF + i * B2L_RX_BUF);
	uint32_t it[8] = { 0 };
	it[0] = 0x11;                     /* MSG_TYPE_RXBUF_POST */
	it[1] = B2L_RX_ID0 + i;
	it[2] = d->rxLen << 16;           /* metadata_buf_len 0, data_buf_len: what the firmware asked (dhd) */
	it[6] = (uint32_t)pa;
	it[7] = (uint32_t)(pa >> 32);
	FlushPoC_DcacheRegion((vm_offset_t)(d->data + B2L_DATA_RX_OFF + i * B2L_RX_BUF), B2L_RX_BUF);
	b2l_wifi_ring_submit(d, BCMPCIE_RING_RXPOST, it);
	g_repost++;
}

/* One Ethernet frame to the flowring; returns the TX buffer used, or -1. Under g_data_lock. */
static int
b2l_data_send_locked(b2l_wifi_dongle *d, const uint8_t *frame, size_t length, mbuf_t m)
{
	if (!d->flowReady || length < 14 || length > HAKIOS_NET_FRAME_MAX) {
		return -1;
	}
	unsigned i = d->txNext, n = 0;
	while (d->txBusy[i] && n++ < B2L_DATA_NTX) {
		i = (i + 1) % B2L_DATA_NTX;
	}
	if (d->txBusy[i]) {
		return -1;
	}
	d->txNext = (i + 1) % B2L_DATA_NTX;
	uint8_t *buf = (uint8_t *)d->data + B2L_DATA_TX_OFF + i * B2L_DATA_BUF;
	if (m) {
		mbuf_copydata(m, 0, length, buf);
	} else {
		memcpy(buf, frame, length);
	}
	FlushPoC_DcacheRegion((vm_offset_t)buf, length);
	uint64_t pa = b2l_data_pa(d, B2L_DATA_TX_OFF + i * B2L_DATA_BUF) + 14;
	uint32_t it[12] = { 0 };
	static unsigned shown;
	it[0] = 0x0f;                     /* MSG_TYPE_TX_POST, if_id 0 */
	it[1] = B2L_TX_ID0 + i;
	memcpy((uint8_t *)it + 8, buf, 14);   /* txhdr: the Ethernet header */
	((uint8_t *)it)[22] = 0x01;       /* BCMPCIE_PKT_FLAGS_FRAME_802_3, priority 0 */
	((uint8_t *)it)[23] = 1;          /* seg_cnt */
	it[8] = (uint32_t)pa;             /* data_buf_addr: the payload after the header */
	it[9] = (uint32_t)(pa >> 32);
	it[10] = (uint32_t)(length - 14) << 16;   /* metadata_buf_len 0, data_len */
	d->txBusy[i] = true;
	if (shown < 4) {
		shown++;
		b2l_diag_record("B2L WIFI TXP: wr=%u %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
		    d->ring[BCMPCIE_RING_FLOW].wr, it[0], it[1], it[2], it[3], it[4], it[5], it[6], it[7], it[8], it[9], it[10], it[11]);
	}
	b2l_wifi_ring_submit(d, BCMPCIE_RING_FLOW, it);
	return (int)i;
}

/* Drain TX_STATUS and RX completions. Frames are handed to Darwin outside the lock. */
static unsigned
b2l_data_poll(b2l_wifi_dongle *d, bool wait)
{
	uint16_t ready[B2L_DATA_NRX], readyOff[B2L_DATA_NRX], readyLen[B2L_DATA_NRX];
	static unsigned said, seen;
	unsigned n = 0;
	if (!d || !d->data) {
		return 0;
	}
	if (wait) {
		IOLockLock(g_data_lock);
	} else if (!IOLockTryLock(g_data_lock)) {
		return 0;
	}
	b2l_wifi_ring *g = &d->ring[BCMPCIE_RING_TX_CPL];
	uint16_t w = (uint16_t)D_TCM32(d, g->widx);
	while (w < g->max && g->rd != w) {
		volatile uint32_t *it = (volatile uint32_t *)(g->va + (uint32_t)g->rd * g->len);
		bool okT = b2l_wifi_d2h_ok(g, it);
		if (seen < 60) {                  /* build 168: what the TX completions look like */
			seen++;
			b2l_diag_record("B2L WIFI TXC: ok=%d w0=%#x id=%#x w2=%#x w3=%#x\n", okT, it[0], it[1], it[2], it[3]);
		}
		if (okT && (uint8_t)it[0] == 0x10) {     /* MSG_TYPE_TX_STATUS */
			uint32_t id = it[1] - B2L_TX_ID0;
			if (id < B2L_DATA_NTX) {
				d->txBusy[id] = false;
			}
		}
		g->rd = (uint16_t)((g->rd + 1) % g->max);
		D_TCM32(d, g->ridx) = g->rd;
	}
	g = &d->ring[BCMPCIE_RING_RX_CPL];
	w = (uint16_t)D_TCM32(d, g->widx);
	while (w < g->max && g->rd != w && n < B2L_DATA_NRX) {
		volatile uint32_t *it = (volatile uint32_t *)(g->va + (uint32_t)g->rd * g->len);
		bool okR = b2l_wifi_d2h_ok(g, it);
		g_rxn++;
		g_rxbad += !okR;
		if (seen < 60) {                  /* build 168: what the RX completions look like */
			seen++;
			b2l_diag_record("B2L WIFI RXC: ok=%d w0=%#x id=%#x st=%#x len/meta=%#x off/flags=%#x\n", okR, it[0], it[1], it[2],
			    it[3], it[4]);
		}
		if (okR && (uint8_t)it[0] == 0x12) {     /* MSG_TYPE_RX_CMPLT */
			uint32_t id = it[1] - B2L_RX_ID0;
			uint16_t len = (uint16_t)(it[3] >> 16), off = (uint16_t)(it[4] & 0xffffu);
			if (!off) {
				off = (uint16_t)d->rxOffset;
			}
			if (id < B2L_DATA_NRX) {
				if ((int16_t)(it[2] & 0xffffu) == 0 && len >= 14 && (uint32_t)off + len <= B2L_RX_BUF) {
					ready[n] = (uint16_t)id;
					readyOff[n] = off;
					readyLen[n++] = len;
				} else {
					b2l_data_post_rx(d, id);
				}
			}
		}
		g->rd = (uint16_t)((g->rd + 1) % g->max);
		D_TCM32(d, g->ridx) = g->rd;
	}
	IOLockUnlock(g_data_lock);
	for (unsigned k = 0; k < n; k++) {
		uint8_t *f = (uint8_t *)d->data + B2L_DATA_RX_OFF + ready[k] * B2L_RX_BUF + readyOff[k];
		FlushPoC_DcacheRegion((vm_offset_t)f, readyLen[k]);
		if (f[12] == 0x88 && f[13] == 0x8e) {              /* EAPOL: for the handshake, never the stack */
			IOLockLock(g_data_lock);
			if (d->wantEapol && !d->eapolHave && readyLen[k] - 14u <= sizeof(d->eapol)) {
				memcpy(d->eapol, f + 14, readyLen[k] - 14u);
				d->eapolLen = (uint16_t)(readyLen[k] - 14u);
				d->eapolHave = true;
			}
			IOLockUnlock(g_data_lock);
			if (said++ < 40) {
				b2l_diag_record("B2L WIFI EAPOL: in, %u bytes\n", readyLen[k] - 14u);
			}
		} else if (d->joinedUp && g_link_to_ios) {
			b2l_step("a frame to the stack next", readyLen[k], f[12] << 8 | f[13]);
			hakios_net_input(f, readyLen[k]);
			b2l_step("the stack took it", readyLen[k], 0);
		}
	}
	if (n) {
		IOLockLock(g_data_lock);
		for (unsigned k = 0; k < n; k++) {
			b2l_data_post_rx(d, ready[k]);
		}
		IOLockUnlock(g_data_lock);
	}
	return n;
}

static unsigned
b2l_link_poll(void)
{
	return b2l_data_poll(g_wifi, false);
}

/* An EAPOL frame to the access point; waits (<= 1 s) for its TX_STATUS when asked. */
static bool
b2l_eapol_send(b2l_wifi_dongle *d, const uint8_t *body, unsigned length, bool wait)
{
	uint8_t f[14 + LX_WPA_REPLY_MOST];
	if (length > LX_WPA_REPLY_MOST) {
		return false;
	}
	memcpy(f, d->joined.bssid, 6);
	memcpy(f + 6, d->mac, 6);
	f[12] = 0x88;
	f[13] = 0x8e;
	memcpy(f + 14, body, length);
	IOLockLock(g_data_lock);
	int i = b2l_data_send_locked(d, f, 14 + length, nullptr);
	IOLockUnlock(g_data_lock);
	memset(f, 0, sizeof(f));
	b2l_diag_record("B2L WIFI EAPOL: out, %u bytes, buffer %d\n", length, i);
	for (unsigned ms = 0; i >= 0 && wait && ms < 1000 && d->txBusy[i]; ms += 5) {
		b2l_data_poll(d, true);
		IOSleep(5);
	}
	return i >= 0 && !(wait && d->txBusy[i]);
}

/* A key into the firmware ("wsec_key", wl_wsec_key_t, 164 bytes). Under g_wifi_lock. */
static int
b2l_wifi_set_key(b2l_wifi_dongle *d, unsigned index, const uint8_t *key, unsigned length, uint32_t algo,
    const uint8_t *peer, const uint8_t *rsc)
{
	uint8_t k[164];
	bzero(k, sizeof(k));
	*(uint32_t *)(k + 0) = index;
	*(uint32_t *)(k + 4) = length;
	memcpy(k + 8, key, length);
	*(uint32_t *)(k + 112) = algo;
	*(uint32_t *)(k + 116) = peer ? 0 : 2u;          /* group: WL_PRIMARY_KEY */
	if (rsc) {
		*(uint32_t *)(k + 132) = 1;                  /* iv_initialized */
		*(uint32_t *)(k + 140) = (uint32_t)rsc[5] << 24 | (uint32_t)rsc[4] << 16 | (uint32_t)rsc[3] << 8 | rsc[2];
		*(uint16_t *)(k + 144) = (uint16_t)(rsc[1] << 8 | rsc[0]);
	}
	if (peer) {
		memcpy(k + 156, peer, 6);                    /* ea */
	}
	int r = b2l_wifi_iovar(d, true, "wsec_key", k, sizeof(k), nullptr, 0);
	memset(k, 0, sizeof(k));
	return r;
}

/* The flowring for the network joined: created / deleted through the control ring. */
static int
b2l_flow(b2l_wifi_dongle *d, bool create)
{
	uint32_t it[10] = { 0 };
	b2l_wifi_ring *g = &d->ring[BCMPCIE_RING_FLOW];
	IOLockLock(g_wifi_lock);
	if (create) {
		g->wr = 0;
		g->seq = B2L_WIFI_EPOCH_INIT;
		D_TCM32(d, g->widx) = 0;
		D_TCM32(d, g->ridx) = 0;
		bzero((void *)g->va, (size_t)g->max * g->len);
		FlushPoC_DcacheRegion((vm_offset_t)g->va, (size_t)g->max * g->len);
		it[0] = 0x03;                 /* MSG_TYPE_FLOW_RING_CREATE */
		memcpy((uint8_t *)it + 8, d->joined.bssid, 6);    /* da */
		memcpy((uint8_t *)it + 14, d->mac, 6);            /* sa */
		it[5] = B2L_FLOW_ID << 16;    /* tid 0, if_flags 0, flow_ring_id */
		it[7] = g->max | ((uint32_t)g->len << 16);
		it[8] = (uint32_t)g->pa;
		it[9] = (uint32_t)(g->pa >> 32);
	} else {
		it[0] = 0x05;                 /* MSG_TYPE_FLOW_RING_DELETE */
		it[2] = B2L_FLOW_ID;
	}
	d->flowDone = false;
	b2l_wifi_ring_submit(d, BCMPCIE_RING_CTRL_SUB, it);
	for (unsigned ms = 0; ms < 2000 && !d->flowDone; ms++) {
		b2l_wifi_poll(d);
		if (!d->flowDone) {
			IODelay(1000);
		}
	}
	int r = d->flowDone ? d->flowStatus : -999;
	IOLockUnlock(g_wifi_lock);
	return r;
}

static void
b2l_unjoin(b2l_wifi_dongle *d, bool disassoc)
{
	if (g_unjoin_lock) {
		IOLockLock(g_unjoin_lock);
	}
	bool was = d->joinedUp || d->flowReady;
	d->joinedUp = false;
	d->wantEapol = false;
	if (was) {
		hakios_net_link_state(false);
	}
	if (disassoc) {
		IOLockLock(g_wifi_lock);
		b2l_wifi_set_int(d, 52, 0);   /* WLC_DISASSOC */
		d->linkUp = 0;
		IOLockUnlock(g_wifi_lock);
	}
	if (d->flowReady) {
		IOLockLock(g_data_lock);
		d->flowReady = false;
		IOLockUnlock(g_data_lock);
		b2l_flow(d, false);
		for (unsigned i = 0; i < B2L_DATA_NTX; i++) {
			d->txBusy[i] = false;
		}
	}
	if (g_unjoin_lock) {
		IOLockUnlock(g_unjoin_lock);
	}
}

/* The RSN element we ask with (and put in message 2): CCMP pairwise, PSK, the access point's group cipher. */
static unsigned
b2l_rsn_ie(const struct hakios_sta_bss *bss, uint8_t ie[22], bool *tkipGroup)
{
	static const uint8_t base[22] = { 0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
		                          0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00 };
	memcpy(ie, base, sizeof(base));
	*tkipGroup = false;
	for (unsigned at = 0; at + 2 <= bss->elements_length && at + 2 + bss->elements[at + 1] <= bss->elements_length;
	    at += 2 + bss->elements[at + 1]) {
		const uint8_t *e = bss->elements + at;
		if (e[0] == 0x30 && e[1] >= 6 && e[4] == 0x00 && e[5] == 0x0f && e[6] == 0xac) {
			*tkipGroup = e[7] == 2;
			ie[7] = e[7] == 2 ? 2 : 4;
		}
	}
	return sizeof(base);
}

static int
b2l_sta_join(const struct hakios_sta_join *what)
{
	b2l_wifi_dongle *d = g_wifi;
	if (!d || !d->up || !d->data || !what || !what->ssid_length) {
		return ENETDOWN;
	}
	b2l_unjoin(d, d->linkUp != 0);
	/* the access point: the one of that name heard best (or the one asked for) */
	struct hakios_sta_bss best;
	bzero(&best, sizeof(best));
	bool found = false;
	for (unsigned pass = 0; pass < 2 && !found; pass++) {
		if (pass) {
			struct hakios_sta_scan s;
			bzero(&s, sizeof(s));
			memcpy(s.ssid, what->ssid, what->ssid_length);
			s.ssid_length = what->ssid_length;
			b2l_sta_scan(&s);
		}
		IOLockLock(g_wifi_lock);
		for (unsigned i = 0; i < d->heardCount; i++) {
			const struct hakios_sta_bss *b = &d->heard[i];
			static const uint8_t zero[6] = { 0 };
			if (b->ssid_length == what->ssid_length && !memcmp(b->ssid, what->ssid, b->ssid_length) &&
			    (!memcmp(what->bssid, zero, 6) || !memcmp(what->bssid, b->bssid, 6)) && (!found || b->rssi > best.rssi)) {
				best = *b;
				found = true;
			}
		}
		IOLockUnlock(g_wifi_lock);
	}
	if (!found) {
		b2l_diag_record("B2L WIFI JOIN: no access point of that name heard\n");
		return ENXIO;
	}
	bool psk = what->security == HAKIOS_STA_WPA2_PSK, tkipGroup = false;
	uint8_t ie[22];
	unsigned ieLen = b2l_rsn_ie(&best, ie, &tkipGroup);
	struct lx_wpa *wpa = d->wpa;
	bzero(wpa, sizeof(*wpa));
	if (psk) {
		if (what->passphrase_length) {
			lx_wpa_pmk(what->passphrase, what->passphrase_length, what->ssid, what->ssid_length, wpa->pmk);
		} else {
			memcpy(wpa->pmk, what->pmk, 32);
		}
		memcpy(wpa->own, d->mac, 6);
		read_random(wpa->own_nonce, sizeof(wpa->own_nonce));
		wpa->rsn = ie;
		wpa->rsn_length = ieLen;
		wpa->pairwise_length = 16;
		wpa->descriptor_version = 2;
	}
	d->joined = best;
	d->joining = true;
	d->eapolHave = false;
	d->wantEapol = psk;

	/* the firmware's side of the join (wl_cfg80211_connect, host supplicant) */
	struct {
		uint32_t ssidLen;
		uint8_t ssid[32];
		uint8_t bssid[6];
		uint16_t bssidCnt;
		int32_t chanspecNum;
		uint16_t chanspec[2];
	} join;
	bzero(&join, sizeof(join));
	join.ssidLen = what->ssid_length;
	memcpy(join.ssid, what->ssid, what->ssid_length);
	memcpy(join.bssid, best.bssid, 6);
	IOLockLock(g_wifi_lock);
	int rInfra = b2l_wifi_set_int(d, 20, 1);                          /* WLC_SET_INFRA */
	int rAuth = b2l_wifi_set_int(d, 22, 0);                           /* WLC_SET_AUTH: open system */
	int rWsec = b2l_wifi_set_int(d, 134, psk ? (tkipGroup ? 6u : 4u) : 0u);   /* WLC_SET_WSEC */
	int rWpa = b2l_wifi_set_int(d, 165, psk ? 0x80u : 0u);            /* WLC_SET_WPA_AUTH: WPA2_AUTH_PSK */
	int rIe = b2l_wifi_iovar(d, true, "wpaie", psk ? ie : nullptr, psk ? ieLen : 0, nullptr, 0);
	d->linkUp = 0;
	int rSsid = b2l_wifi_ioctl(d, 26, &join, 52, nullptr, 0, nullptr);   /* WLC_SET_SSID with join params */
	IOLockUnlock(g_wifi_lock);
	b2l_diag_record("B2L WIFI JOIN: %s, %u MHz, rssi %d: infra=%d auth=%d wsec=%d wpa_auth=%d wpaie=%d set_ssid=%d\n",
	    psk ? (tkipGroup ? "WPA2-PSK (TKIP group)" : "WPA2-PSK") : "open", best.frequency, best.rssi, rInfra, rAuth, rWsec,
	    rWpa, rIe, rSsid);
	int error = rSsid ? EIO : 0;
	for (unsigned ms = 0; !error && ms < 10000 && !d->linkUp; ms += 20) {
		IOLockLock(g_wifi_lock);
		b2l_wifi_poll(d);
		IOLockUnlock(g_wifi_lock);
		IOSleep(20);
	}
	if (!error && !d->linkUp) {
		error = ETIMEDOUT;
	}
	if (!error) {
		uint8_t bssid[6];
		IOLockLock(g_wifi_lock);
		if (b2l_wifi_ioctl(d, 23, nullptr, 0, bssid, 6, nullptr) == 0) {   /* WLC_GET_BSSID */
			memcpy(d->joined.bssid, bssid, 6);
		}
		IOLockUnlock(g_wifi_lock);
		memcpy(wpa->peer, d->joined.bssid, 6);
		int r = b2l_flow(d, true);
		if (r) {
			b2l_diag_record("B2L WIFI JOIN: flowring create status=%d\n", r);
			error = EIO;
		} else {
			IOLockLock(g_data_lock);
			d->flowReady = true;
			IOLockUnlock(g_data_lock);
		}
	}
	/* the four-way handshake */
	bool keys = !psk;
	for (unsigned ms = 0; !error && !keys && ms < 8000; ms += 5) {
		if (ms % 1000 == 0) {             /* build 168: does the dongle take the RX buffers, and answer? */
			b2l_diag_record("B2L WIFI RINGS: rxpost w/r=%u/%u flow w/r=%u/%u txcpl w/r=%u/%u rxcpl w/r=%u/%u ctrlcpl w=%u\n",
			    D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].widx), D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].ridx),
			    D_TCM32(d, d->ring[BCMPCIE_RING_FLOW].widx), D_TCM32(d, d->ring[BCMPCIE_RING_FLOW].ridx),
			    D_TCM32(d, d->ring[BCMPCIE_RING_TX_CPL].widx), D_TCM32(d, d->ring[BCMPCIE_RING_TX_CPL].ridx),
			    D_TCM32(d, d->ring[BCMPCIE_RING_RX_CPL].widx), D_TCM32(d, d->ring[BCMPCIE_RING_RX_CPL].ridx),
			    D_TCM32(d, d->ring[BCMPCIE_RING_CTRL_CPL].widx));
			b2l_pcie_errs(d, "join");
			IOLockLock(g_wifi_lock);
			b2l_wifi_cons_dump(d, "join");    /* the firmware's own words while it waits */
			IOLockUnlock(g_wifi_lock);
		}
		b2l_data_poll(d, true);
		if (!d->eapolHave) {
			IOSleep(5);
			continue;
		}
		uint8_t reply[LX_WPA_REPLY_MOST];
		unsigned replyLen = 0;
		const char *why = "";
		int r = lx_wpa_message(wpa, d->eapol, d->eapolLen, reply, &replyLen, &why);
		d->eapolHave = false;
		b2l_diag_record("B2L WIFI WPA: message -> %d %s\n", r, why);
		if (r == LX_WPA_BAD) {
			error = !strncmp(why, "its signature", 13) ? EAUTH : EPROTO;   /* a wrong passphrase */
		} else if (r == LX_WPA_ANSWER) {
			b2l_eapol_send(d, reply, replyLen, false);
		} else if (r == LX_WPA_KEYS || r == LX_WPA_GROUP) {
			bool sent = b2l_eapol_send(d, reply, replyLen, true);     /* message 4 out before the keys are in */
			b2l_step("message 4 out, pairwise key next", sent, d->txBusy[1]);
			IOLockLock(g_wifi_lock);
			int rp = r == LX_WPA_KEYS ? b2l_wifi_set_key(d, 0, wpa->pairwise, 16, 4, wpa->peer, nullptr) : 0;
			IOLockUnlock(g_wifi_lock);
			b2l_step("pairwise key in, group key next", rp, 0);
			IOLockLock(g_wifi_lock);
			int rg = b2l_wifi_set_key(d, wpa->group_index, wpa->group, wpa->group_length, wpa->group_length == 32 ? 2u : 4u,
			    nullptr, wpa->group_counter);
			IOLockUnlock(g_wifi_lock);
			b2l_step("group key in", rg, (int)wpa->group_index);
			for (int k = 0; k < 10 && g_steps < 12; k++) {   /* build 179: a heartbeat while frames flow, keys in, link not up */
				b2l_diag_record("B2L WIFI BEAT %d: rx=%u bad=%u repost=%u rxpost w/r=%u/%u rxcpl w/r=%u/%u ctrlcpl w/r=%u/%u events=%u\n", k,
				    g_rxn, g_rxbad, g_repost, D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].widx), D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].ridx),
				    D_TCM32(d, d->ring[BCMPCIE_RING_RX_CPL].widx), d->ring[BCMPCIE_RING_RX_CPL].rd,
				    D_TCM32(d, d->ring[BCMPCIE_RING_CTRL_CPL].widx), d->ring[BCMPCIE_RING_CTRL_CPL].rd, d->events);
				IOLockLock(g_wifi_lock);
				b2l_wifi_cons_dump(d, "beat");
				IOLockUnlock(g_wifi_lock);
				IOSleep(6000);
			}
			b2l_diag_record("B2L WIFI WPA: keys in: pairwise=%d group=%d (index %u)\n", rp, rg, wpa->group_index);
			keys = rp == 0 && rg == 0;
			error = keys ? 0 : EIO;
		}
		memset(reply, 0, sizeof(reply));
	}
	if (!error && !keys) {
		error = ETIMEDOUT;
	}
	d->joining = false;
	if (error) {
		b2l_diag_record("B2L WIFI JOIN: failed, error %d\n", error);
		b2l_unjoin(d, true);
		bzero(wpa, sizeof(*wpa));
		return error;
	}
	/* rekeys of the group key keep arriving: the poll thread answers them with the same state */
	wpa->rsn = nullptr;
	wpa->rsn_length = 0;   /* a message 1 after the join copies no IE from nullptr */
	b2l_step("joined, link up next", 0, 0);
	IOLockLock(g_wifi_lock);
	int pm = b2l_wifi_set_int(d, 86, 0);   /* build 198: WLC_SET_PM PM_OFF, the PCIe link never sleeps */
	IOLockUnlock(g_wifi_lock);
	b2l_diag_record("B2L WIFI JOIN: power save off: %d\n", pm);
	d->lost = false;
	if (g_link_to_ios) {
		if (!g_up_at) {
			g_up_at = mach_absolute_time();
		}
		hakios_net_link_state(true);  /* build 180: before frames go up, so the two are told apart */
	} else {
		b2l_diag_record("B2L WIFI JOIN: link kept from iOS's network stack (build 184 test)\n");
	}
	b2l_step("link up done, frames to the stack next", 0, 0);
	d->joinedUp = true;
	b2l_diag_record("B2L WIFI JOIN: joined, frames pass\n");
	return 0;
}

static int
b2l_sta_leave(void)
{
	b2l_unjoin(g_wifi, true);
	return 0;
}

static void
b2l_sta_state(struct hakios_sta_state *state)
{
	bzero(state, sizeof(*state));
	b2l_wifi_dongle *d = g_wifi;
	state->phase = !d || !d->up ? HAKIOS_STA_OFF : d->joinedUp ? HAKIOS_STA_JOINED : d->joining ? HAKIOS_STA_JOINING :
	    d->scanning ? HAKIOS_STA_SCANNING : HAKIOS_STA_IDLE;
	if (d && (d->joinedUp || d->joining)) {
		state->bss = d->joined;
	}
}

/* shortcut: a fixed list for country FR (2.4 GHz 1-13, 5 GHz 36-64 and 100-140); the
 * firmware's "chanspecs" would be exact */
static unsigned
b2l_sta_channels(struct hakios_sta_channel *channels, unsigned most)
{
	static const uint8_t five[] = { 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140 };
	unsigned n = 0;
	for (unsigned c = 1; c <= 13 && n < most; c++) {
		channels[n].frequency = (uint16_t)(2407 + 5 * c);
		channels[n++].flags = 0;
	}
	for (unsigned i = 0; i < sizeof(five) && n < most; i++) {
		channels[n].frequency = (uint16_t)(5000 + 5 * five[i]);
		channels[n++].flags = five[i] >= 52 ? (HAKIOS_STA_CHANNEL_PASSIVE | HAKIOS_STA_CHANNEL_RADAR) : 0;
	}
	return n;
}

static struct hakios_sta_ops b2l_wifi_sta_ops = {
	b2l_sta_power, b2l_sta_scan, b2l_sta_heard, b2l_sta_join, b2l_sta_leave, b2l_sta_state, b2l_sta_channels, { 0 },
};

/* build 163: frames go out on the flowring once a network is joined (and the keys are in) */
static bool
b2l_link_transmit(mbuf_t packet, size_t length)
{
	b2l_wifi_dongle *d = g_wifi;
	if (!d || !d->joinedUp || !g_link_to_ios) {
		return false;
	}
	b2l_step("a frame from the stack, sending next", (int)length, 0);
	IOLockLock(g_data_lock);
	int i = b2l_data_send_locked(d, nullptr, length, packet);
	IOLockUnlock(g_data_lock);
	b2l_step("sent", i, 0);
	if (i < 0) {
		b2l_data_poll(d, true);       /* all TX buffers out: take back what is done, the stack retries */
	}
	return i >= 0;
}

static struct hakios_net_link b2l_wifi_link = {
	"bcm4375", 0, { 0 }, true, false, b2l_link_transmit, b2l_link_poll,
};

bool
B2LWifi::runDongle(volatile uint8_t *cfg, volatile uint8_t *bar0, volatile uint8_t *tcm,
    uint32_t ramtop, uint32_t sh)
{
	static b2l_wifi_dongle dd;
	b2l_wifi_dongle *d = &dd;
	bzero(d, sizeof(*d));
	d->cfg = cfg;
	d->bar0 = bar0;
	d->tcm = tcm;
	d->ramtop = ramtop;
	d->cons = D_TCM32(d, sh + 20);
	d->consIdx = d->cons >= B2L_WIFI_RAM_BASE && d->cons < ramtop ? D_TCM32(d, d->cons + 16) : 0;
	d->respId = 0x1000;

	/* 1. ring_info (dhdpcie_readshared, dhd_fillup_ring_sharedptr_info) */
	uint32_t ri = D_TCM32(d, sh + 48);
	if (ri < B2L_WIFI_RAM_BASE || ri >= ramtop) {
		b2l_diag_record("B2L WIFI MSGBUF: bad rings_info %#x\n", ri);
		return false;
	}
	uint32_t ringmem = D_TCM32(d, ri + 0), h2dw = D_TCM32(d, ri + 4), h2dr = D_TCM32(d, ri + 8);
	uint32_t d2hw = D_TCM32(d, ri + 12), d2hr = D_TCM32(d, ri + 16);
	uint32_t m0 = D_TCM32(d, ri + 52), m1 = D_TCM32(d, ri + 56);
	b2l_diag_record("B2L WIFI MSGBUF: ringmem=%#x h2d w/r=%#x/%#x d2h w/r=%#x/%#x flowrings=%u sub=%u cpl=%u\n",
	    ringmem, h2dw, h2dr, d2hw, d2hr, m0 & 0xffffu, m0 >> 16, m1 & 0xffffu);
	if (ringmem < B2L_WIFI_RAM_BASE || ringmem >= ramtop || h2dw < B2L_WIFI_RAM_BASE || d2hw < B2L_WIFI_RAM_BASE) {
		return false;
	}

	/* 2. one DMA block, cacheable, with explicit maintenance around every
	 *    exchange (build 151: an uncached alias of cacheable memory read
	 *    zeros both ways; pcie0 is cache-coherent in the DT) */
	IOBufferMemoryDescriptor *bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, B2L_WIFI_DMA_SIZE, 0x00000000fffff000ULL);
	IOMemoryMap *map = nullptr;
	if (!bmd || bmd->prepare() != kIOReturnSuccess) {
		b2l_diag_record("B2L WIFI MSGBUF: DMA alloc failed\n");
		if (bmd) {
			bmd->release();
		}
		return false;
	}
	bzero(bmd->getBytesNoCopy(), B2L_WIFI_DMA_SIZE);
	FlushPoC_DcacheRegion((vm_offset_t)bmd->getBytesNoCopy(), B2L_WIFI_DMA_SIZE);
	uint64_t pa = bmd->getPhysicalSegment(0, nullptr, kIOMemoryMapperNone);
	volatile uint8_t *va = (volatile uint8_t *)bmd->getBytesNoCopy();
	if (!va || !pa) {
		b2l_diag_record("B2L WIFI MSGBUF: DMA map failed pa=%#llx\n", pa);
		goto out;
	}
	{
	/* ring layout: ctrl sub 64x40, rxpost 64x32, ctrl cpl 64x24, tx cpl 64x24, rx cpl 64x40 */
	static const uint16_t lens[5] = { 40, 32, 24, 24, 40 };
	uint32_t off = 0;
	for (unsigned i = 0; i < 5; i++) {
		b2l_wifi_ring *g = &d->ring[i];
		g->va = va + off;
		g->pa = pa + off;
		g->max = 64;
		g->len = lens[i];
		g->widx = i < 2 ? h2dw + i * 4 : d2hw + (i - 2) * 4;
		g->ridx = i < 2 ? h2dr + i * 4 : d2hr + (i - 2) * 4;
		g->seq = B2L_WIFI_EPOCH_INIT;
		off += (uint32_t)g->max * g->len;
	}
	/* build 172: RX post and RX completion as deep as dhd's (512), past the event buffers */
	d->ring[BCMPCIE_RING_RXPOST].va = va + 0x10000;
	d->ring[BCMPCIE_RING_RXPOST].pa = pa + 0x10000;
	d->ring[BCMPCIE_RING_RXPOST].max = 512;
	d->ring[BCMPCIE_RING_RX_CPL].va = va + 0x14000;
	d->ring[BCMPCIE_RING_RX_CPL].pa = pa + 0x14000;
	d->ring[BCMPCIE_RING_RX_CPL].max = 512;
	uint64_t scratchPa = pa + 0x2800;
	d->ioc = va + 0x3000;  d->iocPa = pa + 0x3000;
	d->resp = va + 0x5000; d->respPa = pa + 0x5000;
	d->evt = va + 0x7000;  d->evtPa = pa + 0x7000;   /* 8 x 4 KiB, to 0xf000 */

	/* 3. host_cap (dhd_set_host_cap): version 7, host ready, no OOB DW, no trap on UR */
	D_TCM32(d, sh + 84) = 7u | 0x400u | 0x1000u | 0x800000u;
	/* build 171: host SCB offload (dhd_alloc_host_scbs). A firmware with PCIE_SHARED2_HSCB keeps its
	 * station records in host memory at host_scb_addr; without it, data frames have nowhere to go */
	{
		uint32_t flags2 = D_TCM32(d, sh + 80), scbSize = D_TCM32(d, sh + 72);
		bool scbOk = false;
		if ((flags2 & 0x800u) && scbSize && scbSize <= 0x100000u) {
			IOBufferMemoryDescriptor *sm = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
			    kIODirectionInOut | kIOMemoryPhysicallyContiguous, scbSize, 0x00000000fffff000ULL);
			if (sm && sm->prepare() == kIOReturnSuccess) {
				uint64_t spa = sm->getPhysicalSegment(0, nullptr, kIOMemoryMapperNone);
				bzero(sm->getBytesNoCopy(), scbSize);
				FlushPoC_DcacheRegion((vm_offset_t)sm->getBytesNoCopy(), scbSize);
				D_TCM32(d, sh + 64) = (uint32_t)spa;
				D_TCM32(d, sh + 68) = (uint32_t)(spa >> 32);
				D_TCM32(d, sh + 84) |= 0x02000000u;   /* HOSTCAP_HSCB */
				scbOk = spa != 0;
				b2l_diag_record("B2L WIFI HSCB: %#x bytes at %#llx\n", scbSize, spa);
			}
		}
		b2l_diag_record("B2L WIFI SHARED2: flags2=%#x host_scb_size=%#x hscb=%s\n", flags2, scbSize,
		    scbOk ? "given" : (flags2 & 0x800u) ? "FAILED" : "not asked");
	}
	/* 4. ring descriptors (RING_MAX_ITEMS, RING_ITEM_LEN, RING_BUF_ADDR) and indices */
	for (unsigned i = 0; i < 5; i++) {
		b2l_wifi_ring *g = &d->ring[i];
		uint32_t rm = ringmem + i * 16;
		D_TCM32(d, rm + 4) = (uint32_t)g->max | ((uint32_t)g->len << 16);
		D_TCM32(d, rm + 8) = (uint32_t)g->pa;
		D_TCM32(d, rm + 12) = (uint32_t)(g->pa >> 32);
		D_TCM32(d, g->widx) = 0;
		D_TCM32(d, g->ridx) = 0;
	}
	/* 5. D2H DMA scratch buffer (8 bytes) */
	D_TCM32(d, sh + 52) = 8;
	D_TCM32(d, sh + 56) = (uint32_t)scratchPa;
	D_TCM32(d, sh + 60) = (uint32_t)(scratchPa >> 32);
	__asm__ volatile("dsb sy" ::: "memory");
	/* 6. host ready doorbell, then event buffers */
	b2l_wifi_bp_write(cfg, bar0, B2L_WIFI_PCIE2_REGS + B2L_WIFI_PCIE2_DB1, B2L_WIFI_DB_VALUE);
	b2l_diag_record("B2L WIFI MSGBUF: rings at %#llx host_cap=%#x, host ready\n", pa, D_TCM32(d, sh + 84));
	IOSleep(50);
	for (unsigned i = 0; i < B2L_WIFI_NEVT; i++) {
		b2l_wifi_post_buf(d, 0x0d, 0x2000 + i, d->evtPa + i * B2L_WIFI_EVT_LEN, B2L_WIFI_EVT_LEN);
	}

	/* 7. first ioctls */
	uint32_t ver = 0;
	int r = b2l_wifi_ioctl(d, 1, &ver, 4, &ver, 4, nullptr);       /* WLC_GET_VERSION */
	b2l_diag_record("B2L WIFI IOCTL: WLC_GET_VERSION status=%d version=%u\n", r, ver);
	b2l_wifi_cons_dump(d, "rings");
	if (r != 0) {
		goto out;
	}
	char text[256];
	bzero(text, sizeof(text));
	r = b2l_wifi_iovar(d, false, "ver", nullptr, 0, text, sizeof(text) - 1);
	for (unsigned i = 0; i < sizeof(text) - 1; i++) {
		if (text[i] == '\n' || text[i] == '\r') text[i] = ' ';
	}
	b2l_diag_record("B2L WIFI IOCTL: ver status=%d %s\n", r, text);

	/* 8. CLM blob in 1408-byte chunks (dhd_download_blob) */
	if (b2l_wifi_clm_len) {
		static uint8_t chunk[12 + 1408 + 8];
		const uint8_t *clm = (const uint8_t *)b2l_wifi_clm;
		uint32_t left = (uint32_t)b2l_wifi_clm_len, done = 0;
		uint16_t flag = 0x0002;                   /* DL_BEGIN */
		r = 0;
		while (left && r == 0) {
			uint32_t n = left > 1408 ? 1408 : left;
			if (left == n) flag |= 0x0004;            /* DL_END */
			bzero(chunk, sizeof(chunk));
			*(uint16_t *)(chunk + 0) = (uint16_t)((1u << 12) | flag);   /* DLOAD_HANDLER_VER */
			*(uint16_t *)(chunk + 2) = 2;             /* DL_TYPE_CLM */
			*(uint32_t *)(chunk + 4) = n;
			memcpy(chunk + 12, clm + done, n);
			r = b2l_wifi_iovar(d, true, "clmload", chunk, (12 + n + 7) & ~7u, nullptr, 0);
			flag &= ~0x0002;
			done += n;
			left -= n;
		}
		uint32_t st = 0xffffffffu;
		b2l_wifi_iovar(d, false, "clmload_status", nullptr, 0, &st, 4);
		b2l_diag_record("B2L WIFI CLM: %u bytes status=%d clmload_status=%d\n", done, r, (int)st);
	}

	/* 9. MAC and country */
	char val[80];
	const char *conf = (const char *)b2l_wifi_conf;
	size_t confLen = b2l_wifi_conf_len;
	if (conf && b2l_wifi_conf_get(conf, confLen, "mac", val, sizeof(val)) == 17 && b2l_wifi_parse_mac(val, d->mac)) {
		r = b2l_wifi_iovar(d, true, "cur_etheraddr", d->mac, 6, nullptr, 0);
		b2l_diag_record("B2L WIFI IOCTL: cur_etheraddr status=%d\n", r);
	} else {
		b2l_wifi_iovar(d, false, "cur_etheraddr", nullptr, 0, d->mac, 6);
	}
	struct { char abbrev[4]; int32_t rev; char ccode[4]; } cc;
	bzero(&cc, sizeof(cc));
	if (!conf || b2l_wifi_conf_get(conf, confLen, "country", val, sizeof(val)) != 2) {
		strlcpy(val, "FR", sizeof(val));
	}
	memcpy(cc.abbrev, val, 2);
	memcpy(cc.ccode, val, 2);
	cc.rev = -1;
	r = b2l_wifi_iovar(d, true, "country", &cc, sizeof(cc), nullptr, 0);
	b2l_diag_record("B2L WIFI IOCTL: country %s status=%d\n", val, r);
	/* S10e build 31: no roaming in the firmware. The WPA2 4-way handshake is ours (b2l_wpa.c), done once per join;
	 * a firmware roam (WLC_E_ROAM_PREP 32, then WLC_E_REASSOC 9) needs a new handshake nobody does, and ~10 s later
	 * the AP deauthenticates (WLC_E_DEAUTH_IND 6, reason 15 "4-way handshake timeout"): from then on nothing is
	 * received (build 30: rx frozen at 146 frames 13 s after the join, so Safari loaded nothing). As dhd does for a
	 * host supplicant without roam offload: roam_off 1, a lost link then goes through a full join again.
	 * Boot-arg b2l_wifi_roam=1 leaves the firmware's roaming on. */
	{
		uint32_t roam = 0;
		PE_parse_boot_argn("b2l_wifi_roam", &roam, sizeof(roam));
		r = b2l_wifi_iovar_int(d, "roam_off", roam ? 0u : 1u);
		b2l_diag_record("B2L WIFI IOCTL: roam_off %u status=%d\n", roam ? 0u : 1u, r);
	}
	bzero(text, sizeof(text));
	b2l_wifi_iovar(d, false, "cap", nullptr, 0, text, sizeof(text) - 1);
	b2l_diag_record("B2L WIFI CAP: %s\n", text);
	b2l_wifi_cons_dump(d, "init");
	b2l_wifi_fw_release();            /* firmware, NVRAM, CLM and wifi.conf are no longer needed */

	/* 10. Darwin's side (ported from HakiOS): the station, then en0 */
	d->heard = (struct hakios_sta_bss *)IOMallocZeroData(B2L_WIFI_HEARD_MOST * sizeof(struct hakios_sta_bss));
	g_wifi_lock = IOLockAlloc();
	if (!d->heard || !g_wifi_lock) {
		b2l_diag_record("B2L WIFI STA: no memory\n");
		goto out;
	}
	/* build 163: the data path's DMA block (flowring, RX and TX buffers), cacheable like the first */
	{
		IOBufferMemoryDescriptor *db = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, B2L_DATA_SIZE, 0x00000000fffff000ULL);
		d->wpa = (struct lx_wpa *)IOMallocZeroData(sizeof(struct lx_wpa));
		g_data_lock = IOLockAlloc();
		g_unjoin_lock = IOLockAlloc();
		if (db && db->prepare() == kIOReturnSuccess && d->wpa && g_data_lock) {
			d->data = (volatile uint8_t *)db->getBytesNoCopy();
			d->dataPa = db->getPhysicalSegment(0, nullptr, kIOMemoryMapperNone);
			g_data_md = db;
			b2l_dma_check(db, d->dataPa, B2L_DATA_SIZE, "data block");
			b2l_dma_check(bmd, pa, B2L_WIFI_DMA_SIZE, "ring block");
			b2l_pcie_errs(d, "boot");
			bzero((void *)d->data, B2L_DATA_SIZE);
			FlushPoC_DcacheRegion((vm_offset_t)d->data, B2L_DATA_SIZE);
			d->h2dw = h2dw;
			d->h2dr = h2dr;
			d->rxOffset = D_TCM32(d, sh + 36);            /* dma_rxoffset */
			/* build 169: dhd sizes its RX buffers by the firmware's "rxbufpost_sz" (dhd_sync_with_dongle) */
			uint16_t want = 0;
			int rq = b2l_wifi_iovar(d, false, "rxbufpost_sz", nullptr, 0, &want, sizeof(want));
			d->rxLen = rq == 0 && want >= 1024 && want <= B2L_RX_BUF ? want : B2L_DATA_BUF;
			b2l_diag_record("B2L WIFI DATA: rxbufpost_sz status=%d value=%u, posting %u\n", rq, want, d->rxLen);
			b2l_wifi_ring *f = &d->ring[BCMPCIE_RING_FLOW];
			f->va = d->data + B2L_DATA_FLOW_OFF;
			f->pa = b2l_data_pa(d, B2L_DATA_FLOW_OFF);
			f->max = 64;
			f->len = 48;                                  /* host_txbuf_post_v1 */
			f->widx = h2dw + B2L_FLOW_ID * 4;
			f->ridx = h2dr + B2L_FLOW_ID * 4;
			f->seq = B2L_WIFI_EPOCH_INIT;
			/* build 172: as many as the firmware takes (max_host_rxbufs), as dhd does */
			uint32_t lf = D_TCM32(d, sh + 32);
			d->rxCount = (lf >> 16) ? ((lf >> 16) < B2L_DATA_NRX ? (lf >> 16) : B2L_DATA_NRX) : B2L_DATA_NRX;
			b2l_diag_record("B2L WIFI DATA: total_lfrag_pkt_cnt=%u max_host_rxbufs=%u, posting %u\n", lf & 0xffffu, lf >> 16,
			    d->rxCount);
			for (unsigned i = 0; i < d->rxCount; i++) {
				b2l_data_post_rx(d, i);
			}
		} else {
			d->data = nullptr;
		}
		b2l_diag_record("B2L WIFI DATA: block at %#llx (%s), %u RX buffers posted, rx offset %u\n", d->dataPa,
		    d->data ? "ok" : "FAILED", d->data ? d->rxCount : 0, d->rxOffset);
	}
	/* build 153: HakiOS's station counts the radio as on from the attach (sta_on = true);
	 * make that true, so power, state and the answers to wifid agree (build 152: radio down) */
	r = b2l_wifi_set_int(d, 2, 0);    /* WLC_UP */
	d->up = r == 0;
	b2l_diag_record("B2L WIFI STA: radio up before attach status=%d\n", r);
	g_wifi = d;
	memcpy(b2l_wifi_sta_ops.address, d->mac, 6);
	memcpy(b2l_wifi_link.address, d->mac, 6);
	hakios_sta_attach(&b2l_wifi_sta_ops);
	bool attached = hakios_net_attach(&b2l_wifi_link);
	b2l_diag_record("B2L WIFI STA: station attached, en0 %s\n", attached ? "attached" : "NOT attached");

	/* 11. this thread now drains the completion ring for good */
	for (unsigned tick = 0;; tick++) {
		IOLockLock(g_wifi_lock);
		b2l_wifi_poll(d);
		if (tick % 1200 == 0) {
			b2l_wifi_cons_dump(d, "run");
		}
		IOLockUnlock(g_wifi_lock);
		if (g_up_at && tick % 20 == 0) {    /* build 204: about every 1 s, 5 min: was the radio still busy at a freeze */
			uint64_t ns = 0;
			absolutetime_to_nanoseconds(mach_absolute_time() - g_up_at, &ns);
			if (ns < 300000000000ull) {
				unsigned busy = 0;
				for (unsigned k = 0; k < B2L_DATA_NTX; k++) {
					busy += d->txBusy[k];
				}
				b2l_diag_record("B2L WIFI T+%llus: rx %u bad %u txbusy %u rxpost w/r %u/%u rxcpl w/r %u/%u flow w/r %u/%u txcpl w/r %u/%u\n",
				    ns / 1000000000ull, g_rxn, g_rxbad, busy,
				    D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].widx), D_TCM32(d, d->ring[BCMPCIE_RING_RXPOST].ridx),
				    D_TCM32(d, d->ring[BCMPCIE_RING_RX_CPL].widx), D_TCM32(d, d->ring[BCMPCIE_RING_RX_CPL].ridx),
				    D_TCM32(d, d->ring[BCMPCIE_RING_FLOW].widx), D_TCM32(d, d->ring[BCMPCIE_RING_FLOW].ridx),
				    D_TCM32(d, d->ring[BCMPCIE_RING_TX_CPL].widx), D_TCM32(d, d->ring[BCMPCIE_RING_TX_CPL].ridx));
			}
		}
		/* build 163: frames, a network lost, a new group key */
		b2l_data_poll(d, true);
		if (d->lost) {
			d->lost = false;
			b2l_diag_record("B2L WIFI JOIN: the link went down\n");
			b2l_unjoin(d, false);
			hakios_sta_lost(ECONNRESET);
		}
		if (d->joinedUp && d->eapolHave) {
			uint8_t reply[LX_WPA_REPLY_MOST];
			unsigned replyLen = 0;
			const char *why = "";
			int r = lx_wpa_message(d->wpa, d->eapol, d->eapolLen, reply, &replyLen, &why);
			d->eapolHave = false;
			b2l_diag_record("B2L WIFI WPA: rekey message -> %d %s\n", r, why);
			if (r == LX_WPA_GROUP) {
				b2l_eapol_send(d, reply, replyLen, true);
				IOLockLock(g_wifi_lock);
				int rg = b2l_wifi_set_key(d, d->wpa->group_index, d->wpa->group, d->wpa->group_length,
				    d->wpa->group_length == 32 ? 2u : 4u, nullptr, d->wpa->group_counter);
				IOLockUnlock(g_wifi_lock);
				b2l_diag_record("B2L WIFI WPA: new group key in: %d\n", rg);
			}
			memset(reply, 0, sizeof(reply));
		}
		IOSleep(50);
	}
	}
out:
	/* The DMA block stays allocated: the firmware keeps the rings. */
	(void)map;
	return false;
}

void
B2LWifi::stop(IOService *provider)
{
	if (fsysPinMap) {
		fsysPinMap->release();
		fsysPinMap = nullptr;
	}
	if (fsysPinDesc) {
		fsysPinDesc->release();
		fsysPinDesc = nullptr;
	}
	if (periMap) {
		periMap->release();
		periMap = nullptr;
	}
	if (aliveMap) {
		aliveMap->release();
		aliveMap = nullptr;
	}
	if (cmuMap) {
		cmuMap->release();
		cmuMap = nullptr;
	}
	if (periDesc) {
		periDesc->release();
		periDesc = nullptr;
	}
	if (aliveDesc) {
		aliveDesc->release();
		aliveDesc = nullptr;
	}
	if (cmuDesc) {
		cmuDesc->release();
		cmuDesc = nullptr;
	}
	IOService::stop(provider);
}

void
B2LWifi::free()
{
	IOService::free();
}
#endif /* B2L */

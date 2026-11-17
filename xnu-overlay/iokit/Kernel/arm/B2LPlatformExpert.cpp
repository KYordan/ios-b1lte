/* SPDX-License-Identifier: BSD-2-Clause
 * Minimal non-Apple device-tree platform for the Exynos 9820 first boot.
 */
#include <pexpert/arm64/board_config.h>
#if defined(B2L)
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/c++/OSData.h>
#include <pexpert/pexpert.h>

class B2LPlatformExpert : public IODTPlatformExpert {
	OSDeclareDefaultStructors(B2LPlatformExpert);
public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void processTopLevel(IORegistryEntry *root) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	long getGMTTimeOfDay(void) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(B2LPlatformExpert, IODTPlatformExpert);

bool
B2LPlatformExpert::start(IOService *provider)
{
	IOLog("B2L: starting device-tree platform expert\n");
	if (!IODTPlatformExpert::start(provider)) {
		return false;
	}
    IORegistryEntry *chosen=IORegistryEntry::fromPath("/chosen",gIODTPlane);
    OSData *wifi=chosen?OSDynamicCast(OSData,chosen->getProperty("mac-address-wifi0")):nullptr;
    IOLog("B2L: WiFi MAC metadata %s (address redacted)\n",wifi&&wifi->getLength()==6?"present, 6 bytes":"absent");
    /* B1L-0025: MobileGestalt derives the UDID from /chosen mac-address-*;
     * without them it never answers and lsd never finishes seeding the
     * application database (ios-a5xelte herolte, tools/mkadt_herolte.py;
     * here "Could not get wifi address" 755 times a boot). Synthetic,
     * locally administered addresses, never the phone's. Kill switch:
     * boot-arg b2l_synth_mac=0. */
    {
        int synth=1;PE_parse_boot_argn("b2l_synth_mac",&synth,sizeof(synth));
        static const char *const names[3]={"mac-address-wifi0","mac-address-bluetooth0","mac-address-ethernet0"};
        unsigned added=0;
        for(unsigned i=0;synth&&chosen&&i<3;i++){
            OSData *have=OSDynamicCast(OSData,chosen->getProperty(names[i]));
            if(have&&have->getLength()==6)continue;
            const uint8_t mac[6]={0x02,0x42,0x32,0x4c,0x10,(uint8_t)(i+1)};
            OSData *d=OSData::withBytes(mac,sizeof(mac));
            if(d){chosen->setProperty(names[i],d);d->release();added++;}
        }
        IOLog("B2L: synthetic MAC properties added to /chosen: %u\n",added);
    }
    if(chosen)chosen->release();
    /* B1L-0161: arm XNU derives IOPlatformUUID from /chosen unique-chip-id +
     * chip-id (absent here), so gethostuuid() waited forever and hung
     * configd's IPConfiguration start(). Fixed synthetic UUID, not the
     * phone's; no unique-chip-id so MobileGestalt identity is unchanged. */
    if(!provider->getProperty(kIOPlatformUUIDKey)){
        OSString *u=OSString::withCString("02423240-4c10-5000-8000-0000000000b2");
        if(u){provider->setProperty(kIOPlatformUUIDKey,u);publishResource(kIOPlatformUUIDKey,u);u->release();}
        IOLog("B2L: synthetic IOPlatformUUID published: %s\n",u?"yes":"no");
    }
    uint32_t sleepDisabled=0;
    if(PE_get_default("sleep-disabled",&sleepDisabled,sizeof(sleepDisabled)) && sleepDisabled)
        IOLog("B2L PM: system sleep disabled until CPU suspend/resume support is available\n");
	/* B1L-0207: we are the time source (getGMTTimeOfDay); without IORTC the calendar waited 30 s, then 1970. */
	publishResource("IORTC");
	registerService();
	IOLog("B2L: platform expert registered; software product iPhone17,2, Samsung B2L hardware\n");
	return true;
}

/* B1L-0207: boot calendar = the PMIC RTC, never earlier than this kernel's build time (written by
 * b1l-build.sh), so TLS certificates are in their validity window from the first second.
 * shortcut: iOS's own time changes are not written back to the RTC; add setGMTTimeOfDay if the RTC drifts. */
#if __has_include("b2l_build_epoch.h")
#include "b2l_build_epoch.h"
#else
#define B2L_BUILD_EPOCH 0
#endif
extern "C" int64_t b2l_pmic_rtc_seconds(void);
long B2LPlatformExpert::getGMTTimeOfDay(void)
{
	/* the PMIC is read once; later callers (IOPMrootDomain) get that time plus the uptime since */
	static int64_t base;
	static uint64_t at;
	if (!base) {
		int64_t rtc = b2l_pmic_rtc_seconds();
		at = mach_absolute_time();
		base = rtc > B2L_BUILD_EPOCH ? rtc : B2L_BUILD_EPOCH;
		IOLog("B2L RTC: boot calendar %lld (%s)\n", base, base == rtc ? "PMIC RTC" : "build time");
	}
	uint64_t ns;
	absolutetime_to_nanoseconds(mach_absolute_time() - at, &ns);
	return (long)(base + (int64_t)(ns / NSEC_PER_SEC));
}

const char *B2LPlatformExpert::deleteList(void) { return "()"; }
const char *B2LPlatformExpert::excludeList(void)
{
	return "('chosen', 'defaults', 'cpus', 'options')";
}

void
B2LPlatformExpert::processTopLevel(IORegistryEntry *root)
{
	/* There is no Apple NVRAM /options service.  Publish the real boot CPU
	 * without fabricating a firmware NVRAM device. */
	IORegistryEntry *cpus = root->childFromPath("cpus", gIODTPlane);
	if (cpus != nullptr) {
		createNubs(this, IODTFindMatchingEntries(cpus, kIODTExclusive, nullptr));
		cpus->release();
	}
	createNubs(this, IODTFindMatchingEntries(root, kIODTExclusive, excludeList()));
    IORegistryEntry *armio=root->childFromPath("arm-io",gIODTPlane);
    if(armio){createNubs(this,IODTFindMatchingEntries(armio,kIODTExclusive,"('gic')"));armio->release();}

}
#endif

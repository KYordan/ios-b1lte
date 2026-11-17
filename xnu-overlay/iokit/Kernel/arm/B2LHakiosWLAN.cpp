/* B1L-0152: ported from HakiOS (github.com/oosexgang/hakios-xnu-kernel, drivers/, APSL 2.0), commit of 2026-11-06. */
/*
 * HakiosWLAN: the registry entries by which Darwin finds the Wi-Fi interface.
 *
 * en0 is made with the BSD interface KPI (drivers/net/hakios.net.c) and has no IOKit object behind
 * it of its own; Darwin looks for two, as IO80211Family would have made them:
 *
 *   - wifid's device is whatever service has the property IO80211InterfaceRole =
 *     "Infrastructure", and IO80211.framework binds to an interface through a service whose
 *     IOInterfaceName is the interface's name. The service has no user client, on purpose:
 *     the framework's attempt to open one fails, it says "Falling back to IOCTL path", and
 *     from then on every request is an ioctl on a socket, which drivers/wifi/hakios.apple80211.c
 *     answers.
 *   - SystemConfiguration (SCNetworkInterfaceCopyAll) lists the network interfaces it finds as
 *     objects of the class IONetworkInterface, and calls one a Wi-Fi interface when its parent
 *     is of the class IO80211Controller; type, unit, name and built-in-ness are properties of
 *     the interface, the hardware address one of the controller. configd makes a network
 *     service (DHCP and the rest) for an interface it has listed, and only an interface with a
 *     service can be the one the system routes through -- which wifid waits for after a join
 *     ("failed to become routable interface") before Settings ticks the network.
 *
 *   - MobileGestalt, which lsd asks for the Wi-Fi address of every bundle it registers, looks the
 *     address up as the property local-mac-address of the device tree's "wlan" node, as a phone
 *     has it; without that it waits, up to 5 s every time, for a service of the class
 *     IONetworkController under that node. With neither, each such question cost 5 s, and a
 *     LaunchServices database rebuilt in full (a first boot on an empty data volume) takes hours,
 *     SpringBoard waiting. (Found by the ios-sapphire port, patches 0157 and 0158, which carries
 *     this file on: github.com/Angelpro09xd/ios-sapphire, APSL 2.0.)
 *
 * So: a controller of the class IO80211Controller -- an IOEthernetController and so an
 * IONetworkController, as in IO80211Family -- under the "wlan" node (the platform when there is
 * none), the radio's address on that node as local-mac-address, and below the controller one
 * object that is both, an IONetworkInterface with wifid's properties. The classes are ours, named
 * as IO80211Family and IONetworkingFamily name theirs; none has anything but properties.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSNumber.h>

extern "C" void HakiosWLANStart(const char * interface, const uint8_t * address);

class IONetworkController : public IOService
{
	OSDeclareDefaultStructors(IONetworkController);
};

OSDefineMetaClassAndStructors(IONetworkController, IOService);

class IOEthernetController : public IONetworkController
{
	OSDeclareDefaultStructors(IOEthernetController);
};

OSDefineMetaClassAndStructors(IOEthernetController, IONetworkController);

class IO80211Controller : public IOEthernetController
{
	OSDeclareDefaultStructors(IO80211Controller);
};

OSDefineMetaClassAndStructors(IO80211Controller, IOEthernetController);

class IONetworkInterface : public IOService
{
	OSDeclareDefaultStructors(IONetworkInterface);
};

OSDefineMetaClassAndStructors(IONetworkInterface, IOService);

class HakiosWLAN : public IONetworkInterface
{
	OSDeclareDefaultStructors(HakiosWLAN);

public:
	virtual IOReturn newUserClient(task_t owningTask, void * securityID, UInt32 type, OSDictionary * properties,
	    IOUserClient ** handler) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(HakiosWLAN, IONetworkInterface);

/* There is none (see the top); that somebody asked shows that the service was found. */
IOReturn
HakiosWLAN::newUserClient(task_t owningTask, void * securityID, UInt32 type, OSDictionary * properties, IOUserClient ** handler)
{
	static unsigned said;

	if (said++ < 8) {
		IOLog("HakiosWLAN: a user client of type %u is asked for: there is none\n", (unsigned) type);
	}
	return kIOReturnUnsupported;
}

/*
 * B1L-0156: IONetworkStack, from HakiOS's MiniIONetworkStack.cpp (APSL 2.0). configd's interface
 * namer opens it before it says the registry is quiet; until then NetworkInterfaces.plist stays
 * empty, en0 never reaches State:/Network/Interface, and wifid logs "en0 does not exist in dynamic
 * store" and makes no device (log of 155). Interfaces here are born named, so naming is refused.
 */
class IONetworkStack : public IOService
{
	OSDeclareDefaultStructors(IONetworkStack);
};

OSDefineMetaClassAndStructors(IONetworkStack, IOService);

class IONetworkStackUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IONetworkStackUserClient);

public:
	virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IONetworkStackUserClient, IOUserClient);

bool
IONetworkStackUserClient::start(IOService * provider)
{
	if (!IOUserClient::start(provider)) {
		return false;
	}
	IOLog("HakiosWLAN: IONetworkStack opened; registry busy state %u\n",
	    (unsigned) IOService::getServiceRoot()->getBusyState());
	return true;
}

IOReturn
IONetworkStackUserClient::clientClose(void)
{
	if (!isInactive()) {
		terminate();
	}
	return kIOReturnSuccess;
}

IOReturn
IONetworkStackUserClient::setProperties(OSObject * properties)
{
	return kIOReturnUnsupported;
}

/* attach, start, register: the service, or NULL */
static IOService *
publish(IOService * service, IOService * provider, const char * name)
{
	if (service == NULL || !service->attach(provider)) {
		OSSafeReleaseNULL(service);
		return NULL;
	}
	service->setName(name);
	if (!service->start(provider)) {
		service->detach(provider);
		service->release();
		return NULL;
	}
	service->registerService();
	return service;
}

/* Once, when the interface of a radio exists. */
void
HakiosWLANStart(const char * interface, const uint8_t * address)
{
	IOService * platform = IOService::getPlatform();
	OSDictionary * match = IOService::nameMatching("wlan");
	IOService * node = match != NULL ? IOService::copyMatchingService(match) : NULL;   /* the device tree's */
	IO80211Controller * controller = OSTypeAlloc(IO80211Controller);
	HakiosWLAN * wlan = OSTypeAlloc(HakiosWLAN);
	OSData * mac;
	OSNumber * number;

	if (platform == NULL || controller == NULL || wlan == NULL || !controller->init() || !wlan->init()) {
		OSSafeReleaseNULL(controller);
		OSSafeReleaseNULL(wlan);
		OSSafeReleaseNULL(node);
		OSSafeReleaseNULL(match);
		IOLog("HakiosWLAN: cannot create the services\n");
		return;
	}
	if ((mac = OSData::withBytes(address, 6)) != NULL) {
		controller->setProperty("IOMACAddress", mac);
		if (node != NULL) {
			node->setProperty("local-mac-address", mac);     /* where MobileGestalt reads it first */
		}
		mac->release();
	}
	controller->setProperty("IOBuiltin", true);
	/* B1L-0154: iOS 26's wifid (strings of /usr/sbin/wifid, 23G90) waits for a service with the
	 * property WiFiHardwareHealthy (IOPropertyExistsMatch, then __WiFiManagerWiFiHardwareHealthCallback
	 * reads it) and watches a service named AppleWLANDriver; without them it never powers the
	 * radio ("wifi power cannot be modified"). iOS 16.5, HakiOS's target, did not ask. */
	controller->setProperty("WiFiHardwareHealthy", true);
	OSSafeReleaseNULL(match);
	controller = (IO80211Controller *) publish(controller, node != NULL ? node : platform, "AppleWLANDriver");
	OSSafeReleaseNULL(node);
	if (controller == NULL) {
		wlan->release();
		IOLog("HakiosWLAN: the controller did not start\n");
		return;
	}

	wlan->setProperty("IOInterfaceName", interface);
	wlan->setProperty("BSD Name", interface);
	wlan->setProperty("IOInterfaceNamePrefix", "en");
	if ((number = OSNumber::withNumber(6ULL, 32)) != NULL) {        /* IFT_ETHER */
		wlan->setProperty("IOInterfaceType", number);
		number->release();
	}
	if ((number = OSNumber::withNumber((unsigned long long) (interface[2] - '0'), 32)) != NULL) {
		wlan->setProperty("IOInterfaceUnit", number);
		number->release();
	}
	wlan->setProperty("IOBuiltin", true);
	wlan->setProperty("IOPrimaryInterface", true);
	wlan->setProperty("IO80211InterfaceRole", "Infrastructure");
	if (publish(wlan, controller, "HakiosWLAN") == NULL) {
		IOLog("HakiosWLAN: the service did not start\n");
	} else {
		wlan->release();
	}
	controller->release();

	/* after en0's services, so the namer's "quiet" comes once they are in the registry */
	IONetworkStack * stack = OSTypeAlloc(IONetworkStack);
	if (stack != NULL && !stack->init()) {
		OSSafeReleaseNULL(stack);
	}
	if (stack != NULL) {
		stack->setProperty(kIOUserClientClassKey, "IONetworkStackUserClient");
		stack = (IONetworkStack *) publish(stack, IOService::getResourceService(), "IONetworkStack");
	}
	if (stack == NULL) {
		IOLog("HakiosWLAN: IONetworkStack did not start\n");
	}
	OSSafeReleaseNULL(stack);
}

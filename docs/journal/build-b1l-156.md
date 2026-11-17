# Build 156 : WiFi, service `IONetworkStack` pour l'`InterfaceNamer` de configd

## Diagnostic

Journaux de logd du 155 (voir `build-b1l-155.md`) : `wifid` veut allumer le Wi-Fi mais écrit `en0 does not exist in dynamic store` et ne crée aucun appareil. `NetworkInterfaces.plist` est vide : l'`InterfaceNamer` de configd attend d'ouvrir le service `IONetworkStack` (IONetworkingFamily) avant de déclarer le registre calme ; sans lui, `en0` n'arrive pas dans `State:/Network/Interface`.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LHakiosWLAN.cpp` (copié dans `xnu/`) : classes `IONetworkStack` et `IONetworkStackUserClient`, reprises de HakiOS `MiniIONetworkStack.cpp` (APSL 2.0). Le service est publié sous `IOResources` à la fin de `HakiosWLANStart`, après les services d'`en0`. Le client s'ouvre (trace `HakiosWLAN: IONetworkStack opened`) et refuse les demandes de nommage (les interfaces naissent nommées). Aucun nouveau fichier de build.

## Installé avec hash BOOT

- Build : **156**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `df835a00a19678f19537b1f96bfc3b23b8fd9718292b739d8323381f00f0849c` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 60 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression**, bouton Wi-Fi toujours grisé (test du propriétaire). Journal `out/logs/0156-boot.txt`.

- `HakiosWLAN: IONetworkStack opened; registry busy state 0` : configd ouvre le service.
- USERDATA relue en lecture seule avec le nouveau `b1l-tools/pull-delta.sh` (308 blocs de 4 Mio sur 8 192, 557 s, dont le hachage unique de l'image 155) : `NetworkInterfaces.plist` contient maintenant `en0`, `SCNetworkInterfaceType = IEEE80211`, `UserDefinedName = Wi-Fi`, chemin `IOService:/B2LPlatformExpert/wlan/AppleWLANDriver/HakiosWLAN`. logd : `*QUIET&NAMED*` publié par l'InterfaceNamer ; `wifid` parcourt `IONetworkInterface` (nouveau).
- **Reste bloquant** : `wifid` écrit toujours `en0 does not exist in dynamic store`. Code de `wifid` (otool) : à l'initialisation il lit `State:/Network/Interface` → `Interfaces` et n'y trouve aucune interface Wi-Fi (pas de `Discovered N airport Interfaces`), puis `__WiFiManagerDeviceAddedCallback` ne trouve pas `en0` dans la même liste ; aucun `Callback reason=DyamicStoreChange` ensuite. Donc le KernelEventMonitor de configd ne publie jamais `en0` dans cette liste.
- `log show` du Mac refuse toujours l'archive, même avec un `timesync` synthétisé : les messages de configd (formats dans `uuidtext`) restent illisibles.

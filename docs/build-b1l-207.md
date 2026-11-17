# B1L-0207 : heure au démarrage (horloge du PMIC + heure du build)

## Pourquoi
Le noyau démarrait en 1970 (`timesync` : heure murale 00:00:47 à chaque boot) : `IOPlatformExpert::getGMTTimeOfDay`
renvoie 0 et personne ne publie `IORTC`, donc `IOKitInitializeTime` attendait 30 s puis gardait 1970. iOS ne corrigeait
l'heure que parfois, plusieurs minutes après, et la page Date et heure ne s'ouvre pas. En 1970, toute vérification TLS
échoue (`TemporalValidity`), donc aucune app ne charge. Le propriétaire a demandé les deux corrections.

## Changements (hors périmètre Wi-Fi, accord du propriétaire)
- `iokit/Kernel/arm/b2l_rtc.h` (nouveau) : registres RTC du S2MPS19 (bloc speedy 0x02, SEC..YEAR 0x05..0x0B, binaire,
  24 h, année 2000 + YEAR, UTC) et conversion en secondes depuis 1970 (testée sur le Mac contre `timegm`,
  `scratchpad/rtctest.c`). Source : LineageOS android_kernel_samsung_exynos9820 (lineage-21) `rtc-s2mps19.c`,
  `rtc-s2mp.h`, `acpm_mfd.c`, `s2mps19_core.c`.
- `iokit/Kernel/arm/B2LACPM.h` : `b2l_acpm_parse_ch(a, ch)` (le parseur DVFS devient un appel avec le canal 5) et champ
  `reply` pour attendre la réponse sur un canal à interruption.
- `iokit/Kernel/arm/B2LCPU.cpp` : `b2l_pmic_rtc_seconds()` sur le canal ACPM 2 (DT `s2mps19mfd` : acpm-ipc-channel 2,
  speedy 0) : lire UPDATE, écrire UPDATE avec RUDR (seule écriture PMIC, la même que le pilote Linux à chaque lecture),
  1 ms, lecture groupée de 7 registres. Log `B2L RTC: AAAA-MM-JJ hh:mm:ss UTC`.
- `iokit/Kernel/arm/B2LPlatformExpert.cpp` : `getGMTTimeOfDay` = max(RTC du PMIC, heure du build), lu une fois puis
  + temps écoulé ; publie `IORTC` (fin de l'attente de 30 s au démarrage). Log `B2L RTC: boot calendar N (source)`.
- `b1l-tools/b1l-build.sh` (sauvegarde `../b1l-build.sh.206bak`) : écrit `xnu/iokit/Kernel/arm/b2l_build_epoch.h`
  (`B2L_BUILD_EPOCH`, `date +%s`) avant le noyau.
- Sauvegardes : `../B2LACPM.h.206bak`, `../B2LCPU.cpp.206bak`, `../B2LPlatformExpert.cpp.206bak`.

Limite : l'heure réglée par iOS n'est pas réécrite dans le RTC.

## Résultat
Démarrage ~30 s plus rapide. `B2L RTC: 2026-11-13 00:29:39 UTC -> 1794529779` puis `boot calendar ... (PMIC RTC)` :
l'horloge du PMIC est lue et juste (écran verrouillé 4:32, fuseau US/Pacific). Plus aucun `TemporalValidity` ni
`ks_crypt` dans les journaux iOS (`out/live/ios26-userdata-net-207.img`). Les apps ne chargent toujours pas :
164 requêtes `NSURLErrorDomain -1001` (délai), 31 connexions TCP fermées en `SYN_SENT`, 23 TLS seulement.
wifid : adresse 192.168.0.31 obtenue, mais ~60 s après la connexion `WiFiDeviceIsInterfaceRoutable: interface is IPv4 not
routable` → `kWiFiUsageFaultReasonDhcpFailure` → DISASSOCIATE (raison 1005), puis reconnexion (aussi vu en 0205).
Le test de wifid : champ IPv4 du device (+0xdf0) à 0 ou 169.254 = non routable ; il est rempli par
`__WiFiDeviceDynamicStoreCallback` (State:/Network/Service/<id>/IPv4 Addresses). Cause pas encore trouvée.
AppleKeyStore : sélecteur 35 refusé 60 fois (NOT_FOUND, handle ≠ 0).

# Build 154 : WiFi, contrôleur `AppleWLANDriver` avec `WiFiHardwareHealthy`

## Diagnostic

La trace IOKit du build 153 montre que `wifid` d'iOS 26 cherche un service nommé `AppleWLANDriver` (absent) ; les chaînes de `/usr/sbin/wifid` (23G90) montrent qu'il attend un service portant `WiFiHardwareHealthy` et qu'il refuse sinon de modifier l'alimentation du Wi-Fi.

## Changement

Un seul changement, dans `xnu-overlay/iokit/Kernel/arm/B2LHakiosWLAN.cpp` (copié dans `xnu/`) : le contrôleur `IO80211Controller` est publié sous le nom `AppleWLANDriver` (au lieu de `HakiosWLANController`) et porte `WiFiHardwareHealthy = true`. La trace IOKit du 153 reste active.

Compilation OK.

## Installé avec hash BOOT

- Build : **154**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `c95b92d475e5b31560651e6ac57f24c381c0a3fcd7dfeb373c0b5bad32fb8c40` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 59 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (3 000 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0154-boot.txt`). Wi-Fi toujours impossible à activer (constaté par le propriétaire).

```
B2L IOKIT wifid service found=1 <dict><key>IONameMatch</key><string>AppleWLANDriver</string>...   (x3)
B2L IOKIT wifid open IO80211Controller type 1   (x2)
apple80211: wifid gets 19 (POWER) ... answered (x5) ; aucun set
```

- `AppleWLANDriver` est maintenant trouvé ; `wifid` y demande deux fois un user client de type 1. D'après les chaînes voisines de `IOServiceOpen` dans `wifid`, il sert à la configuration « ocla » (analyseur logique de la puce, débogage) : probablement pas le blocage.
- La notification `IO80211SkywalkInterface` de `wifid` ne porte que sur `IO80211InterfaceRole = Infrastructure`, que l'interface a déjà.
- Toujours aucun `SIOCSA80211`. La raison n'est visible que dans les messages `os_log` de `wifid`, que le portage ne capture pas.

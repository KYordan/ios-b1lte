# Build 153 : WiFi, radio allumée à l'attache + trace IOKit des démons Wi-Fi

## Diagnostic

Build 152 : `wifid` trouve `en0` et passe aux ioctl Apple80211, mais ne fait que 15 lectures et n'envoie jamais de `SIOCSA80211` (ni alimentation, ni scan), même quand le propriétaire actionne le bouton Wi-Fi. Les journaux de `wifid` ne sont pas capturés. Incohérence relevée : le thread station de HakiOS compte la radio comme allumée dès l'attache, alors que la BCM4375 était restée `WLC_DOWN` (état `OFF`).

## Changement

- `B2LWifi.cpp` : `WLC_UP` juste avant `hakios_sta_attach()`, `up` mis à jour ; alimentation, état et réponses à `wifid` concordent.
- **Diagnostic en lecture seule, hors fichiers Wi-Fi (accord du propriétaire du 11/11/2026)** : `xnu-overlay/iokit/Kernel/IOUserClient.cpp` (copié dans `xnu/`), sous `#if defined(B2L)` : pour les processus dont le nom contient `wifi`, `WiFi` ou `80211`, journalise chaque recherche IOKit (`getMatchingServices`, `copyMatchingService` : dictionnaire et trouvé ou non) et chaque `IOServiceOpen` (classe, type). 400 lignes au plus ; coupure : `b2l_iokit_trace=0`. Rien d'autre ne change dans IOKit.
- Les types 96 (`VIRTUAL_IF_ROLE`) et 97 (`VIRTUAL_IF_PARENT`) restent refusés : une interface principale n'est pas virtuelle ; la trace dira s'il faut y répondre.

## Installé avec hash BOOT

- Build : **153**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `f79bbdb369f75d9baba454918f9bf046be5df2c05299d3d7d374cbb1342d35f3` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 66 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 999 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0153-boot.txt`). Wi-Fi toujours impossible à activer ; **la trace donne la cause**.

```
B2L WIFI STA: radio up before attach status=0
B2L IOKIT wifid service found=1 <dict><key>IOPropertyMatch</key><dict><key>IOInterfaceName</key><string>en0</string></dict>...
B2L IOKIT wifid service found=1 <dict><key>IONameMatch</key><string>wlan</string>...
B2L IOKIT wifid service found=0 <dict><key>IONameMatch</key><string>AppleWLANDriver</string>...   (x2)
B2L IOKIT wifid service found=0 <dict><key>IONameMatch</key><string>centauri</string>...
B2L IOKIT wifid service found=0 <dict><key>IOProviderClass</key><string>ApplePPM</string>...
B2L IOKIT wifid open HakiosWLAN type 0
apple80211: wifid gets 19 (POWER) ... answered (x13) ; aucun set
```

- La radio est bien allumée avant l'attache.
- `wifid` cherche un service nommé **`AppleWLANDriver`** et ne le trouve pas. Les chaînes de `/usr/sbin/wifid` (23G90, copié en lecture seule depuis `out/ipsw/ios26-userdata.hfs.img`) montrent qu'il surveille aussi un service portant la propriété **`WiFiHardwareHealthy`** (`IOPropertyExistsMatch`, `__WiFiManagerWiFiHardwareHealthCallback`, « WiFiHardwareHealthy property not found ») et que sans cela « wifi power cannot be modified ». `centauri` (puce Wi-Fi Apple) et `ApplePPM` sont sans objet ici.
- HakiOS visait iOS 16.5, qui ne demandait pas ces services.

Suite : build 154, contrôleur nommé `AppleWLANDriver` avec `WiFiHardwareHealthy = true`.

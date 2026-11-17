# Build 152 : WiFi, cohérence DMA, escan, et interface Wi-Fi d'iOS (portage HakiOS)

**Build regroupé** (accord du 11/11/2026). Le propriétaire a demandé de travailler directement sur le Wi-Fi dans l'interface d'iOS.

## Diagnostic

- Build 151 : messages à zéro dans les deux sens (alias cache / non caché sur un contrôleur cohérent) ; `sup_wpa` non pris en charge.
- HakiOS (`github.com/oosexgang/hakios-xnu-kernel`, `drivers/`, APSL 2.0) fait apparaître le Wi-Fi dans iOS ainsi : `en0` par l'API `ifnet` en sous-famille Wi-Fi ; des objets IOKit `IO80211Controller` / `IONetworkInterface` avec `IO80211InterfaceRole = "Infrastructure"` et `IOInterfaceName = "en0"`, **sans user client**, pour qu'IO80211.framework passe aux ioctl `SIOCGA80211`/`SIOCSA80211`, auxquels le noyau répond ; des événements noyau de classe 6 pour `wifid`. Le cache dyld d'iOS 26 (23G90) contient toujours « Falling back to IOCTL path » et `Apple80211GetWithIOCTL`.

## Changement

- Portés de HakiOS (commit du 06/11/2026, mention en tête de chaque fichier) : `bsd/sys/hakios.net.h`, `bsd/sys/hakios.sta.h`, `bsd/dev/b2l_hakios_net.c` (`en0`), `bsd/dev/b2l_hakios_sta.c` (thread station), `bsd/dev/b2l_hakios_apple80211.c` (réponses Apple80211), `iokit/Kernel/arm/B2LHakiosWLAN.cpp` (objets IOKit). Adaptations : pas de virtio, journal par `printf`, pas de `hakios.input.h`. Déclarés dans `xnu/bsd/conf/files` et `xnu/iokit/conf/files`.
- `B2LWifi.cpp` :
  - bloc DMA cacheable, `FlushPoC` après chaque écriture H2D et avant chaque lecture D2H, attente de synchro jusqu'à 20 ms ; tampons d'événements de 4 Kio ;
  - événements : `WLC_E_ESCAN_RESULT` (69) analysés (`wl_bss_info_v109_t`) vers la liste des réseaux entendus ;
  - `hakios_sta_ops` pour la BCM4375 : alimentation (`WLC_UP`/`WLC_DOWN`), scan (`escan` v1), liste, départ (`WLC_DISASSOC`), état, canaux FR fixes ; **join non encore pris en charge** (renvoie `EOPNOTSUPP`) ;
  - lien `en0` « bcm4375 », sans transmission pour l'instant ;
  - après l'init (version, CLM, MAC, pays, cap), la radio reste éteinte (c'est `wifid` qui l'allume) ; le thread devient la boucle de scrutation de l'anneau de complétion.

Compilation OK ; `HakiosWLAN`, `net: en%u attached` et `B2L WIFI STA` présents dans le noyau.

## Installé avec hash BOOT

- Build : **152**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `823acc255e6b014b0cb7659241a7115f2d8ec5e640d67307f63b969c8e1bc89c` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 70 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (aucun `B2L CRASH`, journal `out/logs/0152-boot.txt`). **Cohérence DMA corrigée, `wifid` trouve l'interface, mais le Wi-Fi ne s'active pas** (Réglages et Centre de contrôle, constaté par le propriétaire).

```
B2L WIFI IOCTL: ver status=0 wl0: Jul 19 2022 10:52:21 version 18.41.117 (B1 Network/rsdb) FWID 01-1b4772fd
B2L WIFI CLM: 29464 bytes status=0 clmload_status=0
B2L WIFI IOCTL: cur_etheraddr status=0
B2L WIFI IOCTL: country FR status=0
B2L WIFI STA: station attached, en0 attached
net: en0 attached on bcm4375 (a radio), link down
HakiosWLAN: a user client of type 0 is asked for: there is none
apple80211: wifid gets 12 (CARD_CAPABILITIES), value 0, 21 bytes: answered
apple80211: wifid gets 96 (?), value 0, 0 bytes: REFUSED
apple80211: wifid gets 97 (?), value 0, 16 bytes: REFUSED
apple80211: wifid gets 19 (POWER), value 0, 24 bytes: answered   (x6)
apple80211: wifid gets 1 (SSID) / 4 (CHANNEL) / 16 (RSSI): error 6
```

- **Plus aucun `bad sync`** : tous les ioctl aboutissent ; CLM chargée, MAC et pays réglés.
- `en0` est créée et `wifid` la trouve par le service IOKit, demande un user client, n'en reçoit pas, et passe aux ioctl Apple80211 comme sous iOS 16.5 (HakiOS).
- 15 requêtes seulement, **toutes des lectures** : aucun `SIOCSA80211` (pas de demande d'alimentation, pas de scan), même quand le propriétaire actionne le bouton. `wifid` n'utilise donc pas l'interface.
- Table des noms d'iOS 26 (cache dyld .60) : **96 = `VIRTUAL_IF_ROLE`, 97 = `VIRTUAL_IF_PARENT`**, refusés.
- Incohérence : HakiOS déclare la radio allumée dès l'attache (`sta_on = true`) alors que la BCM4375 est restée `WLC_DOWN` (état station `OFF`).
- Les journaux de `wifid` lui-même ne sont pas capturés : la cause exacte n'est pas visible.

# Build 151 : WiFi, anneaux msgbuf, ioctl, CLM, pays, association WPA2 (build regroupé)

**Exception à « un changement par build »** (accord du 11/11/2026) : chaque étape s'exécute seulement si la précédente a réussi.

## Diagnostic

Au build 150, le firmware 18.41.117 démarre et publie `pciedev_shared` version 7, `flags=0x30040007` : indices d'anneaux sur 4 octets en TCM (pas de `DMA_INDEX`), synchro D2H par somme XOR, `HOSTRDY_SUPPORT`. La réception peut donc se faire par scrutation, sans interruption.

## Changement

- Semence : lit aussi `b2l-wifi/clm.blob` (`bcmdhd_clm.blob`) et `b2l-wifi/wifi.conf` (SSID 5 GHz puis 2,4 GHz, mot de passe, MAC d'efs, pays FR ; fichier hors git, droits 600 sur le Mac, jamais journalisé).
- `B2LWifi.cpp`, `runDongle()` après la console :
  1. lecture de `ring_info` ; bloc DMA de 64 Kio contigu sous 4 Gio, vidé du cache puis utilisé seulement par une projection non cachée ;
  2. `host_cap` = 7 | HOSTRDY | NO_OOB_DW | UR_FW_NO_TRAP ; 5 anneaux communs de 64 éléments (40/32/24/24/40 octets) écrits dans `ring_mem`, indices à 0 ; tampon scratch D2H ; sonnette « hôte prêt » (DB1 `0x12345678`) ; 8 tampons d'événements ;
  3. ioctl par l'anneau de contrôle (sonnette DB0, scrutation de l'anneau de complétion avec contrôle d'epoch et de XOR) : `WLC_GET_VERSION`, `ver`, `clmload` (blocs de 1 408 octets), `clmload_status`, `cur_etheraddr`, `country` FR, `WLC_UP`, `cap` ;
  4. association WPA2-PSK par le supplicant du firmware : `INFRA`, `AUTH` ouvert, `WSEC` AES, `WPA_AUTH` PSK, `sup_wpa`, `WSEC_PMK` (passphrase), `SET_SSID` ; suivi 20 s par les événements et `WLC_GET_BSSID` ; puis le second SSID en secours.

Compilation à blanc OK.

## Installé avec hash BOOT

- Build : **151**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `c8286442a2d3ad3a77525efc04a7bd3915a9b36b3e5d74ee0791634fe7b0b3f1` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 58 s ; `/b2l-wifi/` contient `clm.blob`, `fw.bin`, `nvram.txt`, `wifi.conf`.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 997 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0151-boot.txt`). **Canal msgbuf partiellement fonctionnel.**

```
B2L WIFI FW: fetch clm=29464 conf=81
B2L WIFI MSGBUF: ringmem=0x2f4ee8 h2d w/r=0x2f51e8/0x2f5294 d2h w/r=0x2f5340/0x2f5354 flowrings=40 sub=43 cpl=5
B2L WIFI MSGBUF: rings at 0x83f2c000 host_cap=0x801407, host ready
B2L WIFI IOCTL: WLC_GET_VERSION status=0 version=2
B2L WIFI IOCTL: WLC_UP status=0
B2L WIFI ASSOC: ssid=<SSID> infra=-999 auth=-999 wsec=0 wpa_auth=-999 sup_wpa=-23 pmk=-999 set_ssid=-999
B2L WIFI CONS[rings]| unknown message: 8 / 00 00 00 00 00 00 00 00
B2L WIFI MSG: bad sync rd=0 type=0 epoch=0 want=1
```

- Les anneaux sont acceptés et certains ioctl aboutissent (`GET_VERSION`, `UP`, `WSEC`), mais **la plupart des messages arrivent à zéro dans les deux sens** : le firmware lit des zéros dans nos requêtes, nous lisons des zéros dans ses complétions. Cause retenue : le bloc DMA était écrit par une projection non cachée alors que la projection cachée du noyau existe aussi, et pcie0 est cohérent (`use-cache-coherency`) : les accès du contrôleur trouvent des lignes de cache périmées. Correction au 152 : une seule projection cachée, maintenance explicite (`FlushPoC`) autour de chaque échange.
- **`sup_wpa` = -23 (`BCME_UNSUPPORTED`)** : le firmware Android n'a pas de supplicant intégré. La poignée de main WPA2 doit être faite par l'hôte.
- Aucune association.

Suite : build 152 (cohérence + interface Wi-Fi d'iOS selon HakiOS).

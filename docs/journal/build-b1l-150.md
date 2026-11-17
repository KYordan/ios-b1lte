# Build 150 : WiFi, attente du firmware portée à 10 minutes

## Diagnostic

Au build 149, la semence (`b2l_userspace_before_init`, avant launchd) a lu `b2l-wifi/fw.bin` et `nvram.txt` sans erreur, mais plus de 120 s après le `start()` de `B2LWifi` : le thread de chargement avait déjà abandonné.

## Changement

Un seul changement dans `B2LWifi.cpp` : `firmwareThread` attend `b2l_wifi_fw_state` jusqu'à 600 s au lieu de 120 s. Le reste de la séquence du 149 (arrêt du CR4, copie, NVRAM, relance, `pciedev_shared`, console) est inchangé.

Identifiants WiFi du propriétaire enregistrés hors git (`~/Developer/s10-port/device-s10/wifi-bt-fw/wifi-credentials.conf`, droits 600) pour l'association à venir.

## Installé avec hash BOOT

- Build : **150**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `064b0143be5be75fc2f66233bdf16449099fdbef5826b5f55af2622c920744f3` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 56 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Le firmware Broadcom démarre sur la puce.** Boot iOS sans régression (2 999 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0150-boot.txt`).

```
B2L WIFI FW: files state=1 fw=1297036 nvram=12175 after 189750 ms
B2L WIFI FW: halted ioctrl=0x21 resetctrl=0 cap=0x351585 banks=13 ramsize=0x190000
B2L WIFI FW: image 1297036 bytes at 0x170000, readback mismatches 0
B2L WIFI FW: nvram 12175 -> 9424 bytes at 0x2fdb2c token=0xf6cb0934 rnd=0x2fdb24
B2L WIFI FW: reset vector 0xbbdcf186 (was 0xb83ef000) readback 0xbbdcf186, releasing CR4
B2L WIFI FW: shared=0x24f8bc after 310 ms host_wake=0 ioctrl=0x1
B2L WIFI SHARED: flags=0x30040007 ver=7 trap=0 assert=0/0 line=0 cons=0x2acaf0 fwid=0x1b4772fd rxoff=0 h2dmb=0x2f43f4 d2hmb=0x2f43f8 rings=0x2f3f28
B2L WIFI CONS| RTE (SDIO-PCIE-MSGBUF) 18.41.117 (B1 Network/rsdb) on BCM4375 r5 @ 37.4/160.0/400.0MHz
B2L WIFI CONS| 000000.009 pciemsgbuf0: Broadcom PCIE MSGBUF driver
B2L WIFI CONS| 000000.117 wl0: Broadcom BCM4375 802.11 Wireless Controller 18.41.117 (B1 Network/rsdb)
B2L WIFI CONS| 000000.312 wl1: Broadcom BCM4375 802.11 Wireless Controller 18.41.117 (B1 Network/rsdb)
B2L WIFI CONS| 000000.322 ThreadX v5.6 initialized
```

- Les fichiers arrivent ~190 s après `start()` : l'attente de 10 min suffit.
- RAM du CR4 : 13 bancs, `0x190000` octets, exactement la fenêtre EROM.
- Image copiée et relue sans erreur ; NVRAM Semco CS01 : 9 424 octets de variables.
- **`pciedev_shared` publié en 310 ms**, version 7. Firmware **18.41.117**, RSDB : `wl0` 5 GHz et `wl1` 2,4 GHz initialisés, pilote msgbuf du firmware prêt.
- Aucun trap ni assert.

Suite : build 151 (anneaux msgbuf, ioctl, MAC, pays, scan, association WPA2).

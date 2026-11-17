# Build 149 : WiFi, chargement du firmware et démarrage de la puce (build regroupé)

**Exception à « un changement par build »** (accord du 11/11/2026) : chaque étape s'exécute seulement si la précédente a réussi, avec une trace avant chacune.

## Diagnostic

Le build 148 a identifié la puce (BCM4375 rev 5, module Semco `e43_cs53`) et sa carte des cœurs (EROM). Le firmware et la NVRAM doivent être lus depuis SYSTEM : seul `b2l_system_file_copy()` (`bsd/dev/b2l_ufs_disk.c`) lit ce volume, une seule fois, pendant la semence avant launchd.

Séquence de référence : `dhdpcie_bus_download_state` (chemin CR4), `dhdpcie_download_code_file`, `dhdpcie_bus_write_vars`, `dhdpcie_wrt_rnd`, `dhdpcie_readshared` (`dhd_pcie.c`, bcmdhd_101_16), `_ai_core_reset` (`aiutils.c`), `si_tcm_size` (`siutils.c`), `process_nvram_vars` (`bcmutils.c`).

## Changement

- `b1l-tools/system-add-fast.sh` : copie `~/Developer/s10-port/device-s10/wifi-bt-fw/system/` (hors git : `fw.bin` = `bcmdhd_sta.bin_b1`, `nvram.txt` = `nvram.txt_CS01_semco_b1`) dans `/b2l-wifi/` sur SYSTEM.
- `xnu-overlay/bsd/kern/b2l_userspace_seed.h` : `b2l_wifi_fetch()`, appelée après `b2l_metal_publish()`, copie les deux fichiers en mémoire noyau (`b2l_wifi_fw_state` = 1 si OK).
- `B2LWifi.cpp` : un thread attend les fichiers (120 s max), puis :
  1. arrête le CR4 (`_ai_core_reset(CPUHALT)` sur le wrapper `0x18102000`), lit la taille de la TCM par ses bancs ; arrêt si elle sort de la fenêtre EROM `0x190000` ;
  2. BAR2 → backplane 0 (`PCI_BAR1_WIN`), dernier mot à 0, copie du firmware à `0x170000`, relecture complète ;
  3. NVRAM traitée comme `process_nvram_vars`, écrite sous le dernier mot, jeton de longueur dans le dernier mot, graine aléatoire (`0xFEEDC0DE`, 128 octets) ;
  4. vecteur de reset (premier mot du firmware) à l'adresse 0, relu, puis CR4 relancé (`_ai_core_reset(0)`) ;
  5. attente de `pciedev_shared` (5 s), lecture de sa tête et de la console du firmware.

Compilation à blanc OK. Le journal de semence du 148 confirme que le lecteur SYSTEM (`NCACHE ready`) et la semence tournent à chaque démarrage.

## Installé avec hash BOOT

- Build : **149**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `643ff4eab3d30799ffc2b9542b70f357a617c07c16081a9da9d746c1069e9e99` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 68 s ; `/b2l-wifi/fw.bin` (`1676f46c…`) et `/b2l-wifi/nvram.txt` (`3bf1ba59…`) présents.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 997 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0149-boot.txt`). **Firmware non chargé** : problème d'ordre au démarrage.

```
B2L WIFI FW: files state=0 fw=0 nvram=0 after 120000 ms
B2L WIFI FW: fetch fw=1297036 nvram=12175 error=0
```

- La semence a bien lu les deux fichiers de SYSTEM (tailles exactes, aucune erreur), mais **après** les 120 s d'attente du thread : il a abandonné avant qu'ils arrivent. La puce n'a pas été touchée (aucune trace `halting`).
- Les tampons du firmware restent alloués (2 Mio + 64 Kio) jusqu'au prochain démarrage, sans effet.

Suite : build 150, attente portée à 10 min.

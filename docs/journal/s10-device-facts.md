# Faits mesurés sur le S10 (2026-11-03, TWRP root par adb)

Données brutes, hors dépôt : `~/Developer/s10-port/device-s10/dump/` (DTB live, iomem, partitions)
et sauvegardes `~/Developer/s10-port/device-s10/backup-20261103/` (contiennent l'IMEI : à copier hors du Mac, jamais dans git).

## Identité

| Élément | Valeur |
| --- | --- |
| `ro.boot.em.model` | `SM-G973F` |
| `ro.product.device` (TWRP) | `beyond1lte` |
| Bootloader | `G973FXXUHHXJ1` |
| Recovery | TWRP 3.7.1_12 Artisan v2, kernel ExtremeKernel 4.14.356 |
| DTB live `model` | `Samsung BEYOND1LTE EUR OPEN 26 board based on EXYNOS9820` (même révision 26 que la référence S10+) |
| RAM | ~7,1 Go visibles (`MemTotal` 7 421 288 kB), 8 cœurs |

`OEM LOCK`, `KG STATE`, `RMM STATE` : non lisibles depuis TWRP. À lire en Download mode (phase 0).

## Partitions (UFS `sda`)

| Nom | Bloc | Octets |
| --- | --- | --- |
| boot | sda14 | 57 671 680 (55 Mio, identique au S10+) |
| recovery | sda15 | 67 633 152 |
| dtb / dtbo | sda12 / sda13 | 8 Mio chacune |
| system | sda25 | 7 340 032 000 |
| userdata | sda34 | 499 641 200 640 (~465 Gio) |
| efs / sec_efs / cpefs | sda3 / sda7 / sdd1 | 20 / 20 / 6 Mio |

## Sauvegardes (lecture seule, SHA-256 identique téléphone ↔ Mac)

efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param,
keystorage, steady, persistent, misc. Liste des hash : `SHA256SUMS.txt` dans le dossier de sauvegarde.
Restauration par Odin : pas encore testée (critère de la phase 0 toujours ouvert).

## Comparaison avec la référence S10+ (`beyond2lte-ios`)

| Point | Résultat | Conséquence |
| --- | --- | --- |
| Mémoire réservée | 30 propriétés comparées, 0 différence avec `beyond2lte-8g-dt.json` | Aucune |
| Bancs de RAM | `0x80000000-0xBAB00000`, `0xC0000000-0xE0000000`, `0xE1900000-0x100000000`, `0x880000000-0xA00000000` | La liste blanche S10+ (7 044 Mio) tient entièrement dedans : `b2l_ram_ranges.h` valable |
| Tactile | `/hsi2c@104B0000/touchscreen@48`, sec_ts Y771 | Adresses de `B2LInput.cpp` correctes |
| Touches | GPIO identiques au S10+ (seuls les phandles pinctrl changent) | Aucune |
| Écran | s6e3ha9, DDI `0x37 0x4d` (S10+ : `0x38 0x4e`) | Le framebuffer vient du GOP UEFI : rien dans le kernel |
| Firmware tactile | `y771_beyond1_phole.bin` (S10+ : `beyond2`) | Le pilote ne charge pas de firmware |

## Ce qui doit réellement changer pour `beyond1lte`

1. **UEFI** : partir de l'overlay `beyond2ltePkg` (memory map corrigée, UFS, touch DXE), renommé
   `beyond1ltePkg`, avec le DTB du S10. Le `beyond1ltePkg` d'origine a le DXE Heap sur la zone écran.
2. **DTB du boot.img** : l'équivalent de `Resources/DTBs/beyond2lte.dtb` (base + overlay rev 26 + `model`).
   Le DTB live (`dump/live.dtb`) est déjà fusionné par le bootloader `[VERIFY]` : vérifier qu'S-Boot l'accepte.
3. **Identité et gardes** :
   - `package-boot.py` : `--board beyond2lte`, `model SM-G975F`, chemin du DTB.
   - `pull-b2l-log.py` : exige `ro.product.device == beyond2lte`, à remplacer par `ro.boot.em.model == SM-G973F`.
   - `DeviceTree.c` : `compatible`, `board-config` et `samsung,beyond2lte-display` (cosmétique côté iOS `[VERIFY]`).
4. **Kernel XNU** : aucune adresse matérielle à changer d'après ces mesures.

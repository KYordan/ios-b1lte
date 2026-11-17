# Build B1L-0002 : premier démarrage d'iOS 26 sur le S10

Date : 2026-11-03. Statut : **construit et vérifié hors ligne, rien n'est installé.**

Une seule modification par rapport à B1L-0001 : le kernel embarque le vrai payload
userspace (au lieu du payload factice), et SYSTEM et USERDATA reçoivent leurs images.
Firmware UEFI et loader identiques à B1L-0001.

## Artefacts (dans `~/Developer/s10-port/beyond2lte-ios/out/`)

| Cible | Fichier | Octets | SHA-256 |
| --- | --- | --- | --- |
| BOOT `sda14` | `b1l-ios-0002/boot.img` | 25 893 504 | `fbf6b52064039bbd58926ed93a3bed8b2e8a5160bda9fee4e95c068539835008` |
| SYSTEM `sda25` | `system/b1l-system.img` | 7 340 032 000 (= la partition) | `29683049f8da522a657512932c1f9c5bf15a148fa4039e5a45d331aa62403cac` |
| USERDATA `sda34` | `ipsw/ios26-userdata.hfs.img` | 34 359 738 368 (32 Gio, partition de 465 Gio) | `faa01da93b6bc0665c0b21dd783d1cf0d3a60ae0eeaab5628913529689dadf3f` |

Kernel : `362e15c7…`, 21 322 048 octets, 0 instruction RCpc. Firmware : `d57e54d0…` (inchangé).

## Construction

1. IPSW `iPhone17,2_26.6.2_23G90_Restore.ipsw`, vérifié par `BuildManifest.plist` (d94ap).
   OS et SystemOS sont des `.aea` : clé obtenue par `b1l-tools/aea-key.py` (clé publiée par Apple
   sur `wkms-public.apple.com`, HPKE P-256/AES-256-GCM, résultat authentifié), déchiffrement par `/usr/bin/aea`.
2. Racine iOS : `sudo bash b1l-tools/build-ios-root.sh`.
   - Copie : 20,28 Go (OS 13,89 Go, SystemOS 6,38 Go, AppOS 12 Mo), 28 098 hard links, 0 fichier ignoré.
   - `prepare-ios-root.py` : 26.6.2 / 23G90, 117 liens de repli des cryptex, aucun exécutable Apple modifié,
     activation non simulée (pas de conteneur MCM).
   - `fsck_hfs -fn` : « B2LIOSROOT appears to be OK ».
3. Payload : `build-userspace-bootstrap.py` : lanceur arm64e de 90 432 octets ; `launchd.plist` d'origine de
   2 279 863 octets, toutes ses entrées conservées, une tâche ajoutée, taille préservée.
4. Kernel recompilé avec ce payload (incrémental). Kernel de test conservé dans `out/kernel-stub-0001/`.
5. SYSTEM : `bash b1l-tools/build-system-image.sh`.
   - HFS+ journalisé de 7 323 254 784 octets (4 Kio × 1 787 904 blocs), en-têtes primaire et secondaire conformes à `b2l_ufs_aux_hfs`.
   - `b2l-cache.bin` : v2, 5 788 123 136 octets, 235 519 cellules, lié à l'identité de la racine iOS.
   - `scripts/test-native-cache.sh` sur l'image : « Validated contiguous cache file », journal vide.
   - 16 Mio de journal B2L initialisés (`b2l-log-region.py init`) en fin de partition.

## Ce que ça change sur le téléphone

- **Android est effacé** : SYSTEM (ext4) et USERDATA (données) sont remplacés.
- Le kernel trouve enfin une racine HFS valide dans USERDATA : il lance `launchd`, puis les services iOS.
- Le journal persistant et le cache natif deviennent actifs dans SYSTEM : **journal lisible depuis TWRP**
  avec `B2L_BOARD=beyond1lte scripts/pull-b2l-log.py`.
- Premier démarrage très lent : le cache natif apprend pendant ce temps.

## Préalables (à faire avant l'installation)

- [ ] Données personnelles copiées hors du téléphone (USERDATA est effacé).
- [ ] Sauvegarde complète de SYSTEM (7 Go), vérifiée par hash (à faire depuis TWRP).
- [ ] Recommandé, phase 0 du plan : retour au stock testé avec Odin, et `OEM LOCK` / `KG STATE` lus en Download mode.

## Installation proposée (attend l'accord du propriétaire, partition par partition)

Depuis TWRP, à chaque étape : `ro.boot.em.model == SM-G973F`, lien `by-name` attendu, taille exacte, partition non montée.

1. Sauvegarde SYSTEM : `dd` de `sda25` vers le Mac, puis comparaison du hash.
2. USERDATA : image de 32 Gio envoyée directement dans `sda34` (pas de copie dans `/tmp`), puis hash des 32 premiers Gio relus.
3. SYSTEM : image de 7 340 032 000 octets envoyée dans `sda25`, puis hash de la partition entière relue.
4. BOOT : `boot.img` dans `sda14`, puis relecture et hash.
5. Redémarrage. Ensuite, retour en TWRP pour récupérer le journal.

Retour à Android : `boot.img` de `backup-20261103/`, sauvegarde SYSTEM, et formatage de data depuis TWRP (ou Odin).

## Installation (2026-11-04, avec l'accord du propriétaire)

Le propriétaire a refusé la sauvegarde de SYSTEM. Aucune sauvegarde de l'Android SYSTEM n'existe.

- Gardes à chaque partition : root, `SM-G973F`, lien `by-name`, taille exacte, partition non montée
  (USERDATA démonté de `/data` et `/sdcard` avant écriture).
- USERDATA `sda34` : le premier essai par `adb exec-in` s'est bloqué à 5,36 Go et a été interrompu.
  Il a été repris avec `b1l-tools/flash-chunked.sh` : 64 morceaux de 512 Mio, chacun relu et vérifié.
- SYSTEM `sda25` : 14 morceaux vérifiés, puis SHA-256 de la partition entière = `29683049…`, **identique**.
- BOOT `sda14` : poussé dans `/tmp`, hash contrôlé, `dd conv=fsync`, relecture de 25 893 504 octets = `fbf6b520…`, **identique**.
- Redémarrage lancé le 2026-11-04 vers 00:25.

## Résultat sur le téléphone (premier démarrage, 2026-11-04)

Propriétaire : logo Apple pendant environ 40 à 50 min, puis passage en TWRP à la main.
Journal récupéré en lecture seule : `out/logs/0002-boot1.txt` (4,5 Mo, 36 269 lignes, epoch 1, 15 pertes du producteur).

| Étape | Observé | Statut |
| --- | --- | --- |
| RAM / SMP | `owned=6958 MiB ranges=11`, `8/8 CPUs initialized` | **Vérifié** |
| USERDATA | `GPT userdata p34 … signature=HFS+/HFSX USERDATA read-write authorized` | **Vérifié** |
| Racine | `BSD root: b2lufs0p34`, `hfs: mounted B2LIOSROOT` | **Vérifié** |
| SYSTEM | journal persistant actif, `NCACHE: bounded SYSTEM file storage ready (5519 MiB)` | **Vérifié** |
| launchd | `/sbin/launchd` exécuté, tâches de démarrage (data-protection, keybag, mount-phase-2…) | **Vérifié** |
| backboardd | démarré, ouvre le framebuffer ; 3 swaps 1440×3040 BGRA, « first IOSurface frame copied to physical scanout » | **Vérifié** (c'est le logo affiché) |
| SpringBoard | pid 28 démarré, mais ses threads attendent depuis environ 2 000 s (`event=1 a=0x169`, réceptions Mach) | **Bloqué** : pas d'écran de verrouillage |
| Tactile | `B2L TOUCH EFI: init=0x0 … transfers=0`, puis 4 × timeout sur `cmd=0x22` (`fifo=0x1000001`) | **Absent** : puce non alimentée par le firmware |
| Touches | `B2L HID: key … (1 clients)` | **Vérifié**, reçues par backboardd |
| Cache natif | `learned=61599`, `errors=0`, `applied=0` (appliqué au démarrage suivant) | Appris, pas encore appliqué |

Causes et suites :
- **Tactile** : le firmware a été construit sans `B2L_ENABLE_TOUCH=1`. Sans lui, `B2LTouchDxe` n'allume pas
  les alimentations `tsp_io` / `tsp_avdd`, et le kernel ne fait jamais l'initialisation électrique
  (« never infer touch PMIC state »). Voir `beyond2lte-ios/docs/build-0054.md`.
  → B1L-0003 : firmware reconstruit avec `B2L_ENABLE_TOUCH=1`, seule modification.
- **SpringBoard bloqué** : cause non établie. Il faut les journaux unifiés iOS de la racine
  (`pull-b2l-unified-logs.py`, avec `B2L_BOARD=beyond1lte B2L_SERIAL=<SERIAL>`).
- Le démarrage suivant appliquera les 61 599 traductions apprises et devrait être plus rapide.

## Démarrage suivant (2026-11-04)

**Vérifié (propriétaire)** : l'**écran de verrouillage d'iOS 26 s'affiche** sur le S10.
Probablement grâce aux 61 599 traductions apprises au premier démarrage et appliquées à celui-ci
`[VERIFY]` : à confirmer par `B2L NCACHE: pre-init apply applied=…` dans le journal de ce démarrage.
Tactile toujours absent (firmware sans `B2L_ENABLE_TOUCH=1`) : correction prévue en B1L-0003.
Temps jusqu'à l'écran de verrouillage : non mesuré.

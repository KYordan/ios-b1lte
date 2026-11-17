# Build B1L-0001 : console XNU sur le S10, sans iOS

Date : 2026-11-03. Statut : **vérifié sur photos : 8/8 CPU, arrêt attendu faute de racine HFS.**

## Objectif (phase 2)

Premier démarrage du board `beyond1lte` : texte XNU à l'écran, sans racine iOS.
Une seule modification par rapport à la référence S10+ : la plateforme UEFI et le DTB.
Le kernel et le loader EFI sont **identiques au S10+**.

## Contenu

| Composant | Source | SHA-256 |
| --- | --- | --- |
| `boot.img` (23 517 824 octets) | `beyond2lte-ios/out/b1l-console-0001/` | `566c003e235793176b683703800f06a5d6163627ef2a54f7055998afefd006cd` |
| Kernel XNU | `out/xnu-b2l-ref/.../mach.development.b2l`, S10+ non modifié, payload userspace **factice** | `fd9275d2849e39816e781f21d00d7fffa61feacc04c8e22bcc862fd10494d268` |
| Firmware UEFI (4 Mio) | `mu-work-b1l/Build/beyond1ltePkg/.../SILICIUM_UEFI.fd` | `d57e54d0103038e193ebb258a60cf05f941018a8104f7563d8d8ad065fe14360` |
| DTB ajouté | `device-s10/dtb/merged.dtb` = `dtb.img` entrée 0 + `dtbo.img` entrée 8 (rév. `0x1a..0xff`) | — |

## Chaîne de build

- Mu-Silicium `a8ca6f0d` (Mu Basecore `762a65d`) + overlay `beyond2lte-uefi` (master) ;
  `beyond1ltePkg` généré par `scripts/make-board-platform.py --dtb merged.dtb`.
- `B2L_BOARD=beyond1lte scripts/build-boot-firmware.sh`, puis `package-boot.py`.
- Outils : micromamba (`~/Developer/tools/mamba/envs/s10`) : clang 23.1.4, lld, llvm-tools, bash 5.

Modifications de `beyond2lte-ios` (branche locale `b1l`, non commitée) :
- `scripts/b2l_board.py` (nouveau) : table des cartes, identité par `ro.boot.em.model` ;
- `scripts/make-board-platform.py` (nouveau) : dérive `beyond1ltePkg` de l'overlay S10+ ;
- `package-boot.py`, `prepare-boot-firmware.py`, `build-boot-firmware.sh`, `pull-b2l-log.py`,
  `pull-b2l-unified-logs.py` : sélection par `B2L_BOARD` (défaut `beyond2lte`, comportement S10+ inchangé) ;
- `common/memory/beyond1lte-8g-dt.json` : mémoire réservée mesurée sur le S10 (identique au S10+) ;
- `build-boot-firmware.sh` : expansion du tableau vide compatible bash 3.2.

## Vérifications faites

- `package-boot.py --verify` : OK (en-tête Android, trailer `B2LXNU02`, graine 256 octets, loader embarqué).
- DTB du S10 à la fin du champ noyau ; aucune trace de `BEYOND2LTE`.
- Régression S10+ : champ noyau reconstruit **identique octet pour octet** au build précédent.
- Gardes d'identité : S10 accepté, S10+ refusé (et inversement) ; `scripts/test-loader.sh` OK.
- Taille : 23,5 Mo pour BOOT de 55 Mio (`sda14`, 57 671 680 octets).

## Ce que le kernel écrira sur le téléphone : rien

Vérifié dans `bsd/dev/b2l_ufs_disk.c` : l'écriture n'est autorisée que sur la partition racine
choisie (`rd=b2l-userdata`), et seulement si elle porte un en-tête HFS+/HFSX valide.
- USERDATA est l'Android chiffré : pas d'en-tête HFS, donc lecture seule et pas de racine.
- Le journal persistant et le cache natif exigent un SYSTEM HFS provisionné ; SYSTEM est ext4, donc ils sont refusés.
- Résultat attendu : le kernel s'arrête en **attente du périphérique racine**. Android reste intact à part BOOT.

Conséquence : **pas de journal récupérable depuis TWRP** pour ce build. La preuve sera une photo
de l'écran. Un journal persistant demandera de provisionner une zone HFS (décision à part).

## Installation proposée (non faite, attend ton accord)

1. Depuis TWRP, contrôler `ro.boot.em.model == SM-G973F`, que `/dev/block/by-name/boot` est
   `/dev/block/sda14` et que sa taille est 57 671 680.
2. La sauvegarde de BOOT d'origine existe déjà : `backup-20261103/boot.img`, SHA-256 `75ab19a8…`.
3. `adb push boot.img /tmp/`, vérifier son hash, `dd` vers `by-name/boot`, `sync`.
4. Relire les 23 517 824 premiers octets de BOOT et comparer le SHA-256.
5. Redémarrer, puis photographier l'écran.

Retour à Android : TWRP (Power + Vol haut) puis `dd` de `backup-20261103/boot.img`.

## Installation (faite le 2026-11-03, avec l'accord du propriétaire)

- Avant : TWRP root, `ro.boot.em.model=SM-G973F`, `boot → /dev/block/sda14`, 57 671 680 octets, non monté.
  SHA-256 de BOOT égal à la sauvegarde `75ab19a8…`.
- Image poussée dans `/tmp`, SHA-256 `566c003e…` sur le téléphone.
- `dd … of=/dev/block/sda14 bs=1M conv=fsync` : 23 517 824 octets. Aucune autre partition touchée.
- Relecture des 23 517 824 premiers octets de BOOT après vidage du cache : `566c003e…`, **identique**.
- `adb reboot` lancé.

## Résultat sur le téléphone

**Vérifié (propriétaire, 2026-11-03)** : texte XNU visible à l'écran. La chaîne S-Boot → BootShim →
UEFI `beyond1ltePkg` → XnuLoader → XNU fonctionne sur le S10.

Lu sur les 4 photos du propriétaire :

| Point | Observé | Statut |
| --- | --- | --- |
| SMP | `B2L CPU: 8/8 CPUs initialized, per-CPU timer and bidirectional SGI1 verified`, timer + xcall vérifiés CPU0 à CPU7 | **Vérifié** |
| Clusters Edge | pset0 `cpus=0xf` (A55), pset1 `0x30` type 2 (A75), pset2 `0xc0` (M4) ; préférence `M4 > A75 > A55` | **Vérifié** |
| M4 | CPU7 `MPIDR=0x101`, `MIDR=0x531f0030`, `CPU_ON result=0` | **Vérifié** |
| VM / zones | `VM bootstrap done`, zone maps et compresseur initialisés | **Vérifié** |
| Politiques | Sandbox B2L, AMFI développement, vnguard chargés ; platform expert DT démarré | **Vérifié** |
| Touches | Événements `B2L HID` / `B2L INPUT` (keys 0 à 3, down/up) | **Vérifié** (GPIO S10 = S10+) |
| UFS natif | Pas de ligne `GPT …` visible ; la sélection de racine a tourné et refusé USERDATA (non HFS) | Partiel, ligne GPT hors photo |
| Arrêt | `B2L UFS: requested HFS root unavailable: 6 @IOKitBSDInit.cpp:810` → panic, `spinning forever` | **Attendu** : pas de racine HFS (ENXIO) |

Les lignes `B2L TRAP … insn=0xe7ffdeff` / `B2L COMPAT: wrapper entered` sont l'instruction de trap
du panic lui-même, pas un nouveau défaut.

Critères de phase 2 : texte XNU, 8 cœurs et Android intact hors BOOT sont **remplis**.
Reste ouvert : journal récupérable depuis TWRP (il faut une zone HFS provisionnée dans SYSTEM).

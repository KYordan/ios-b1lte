# Build B1L-0024 : apps système à la manière de HakiOS (déplacement par le noyau)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `dab006d5…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0023.

## Pourquoi

Sur le S9, HakiOS (Scion-Project/hackios-xnu-kernel) n'utilise pas l'installateur d'iOS pour les apps système :
- les apps de `staged_system_apps` vont dans `/Applications` ;
- `AppPlaceholders` est retiré ;
- au démarrage, LaunchServices parcourt `/Applications` et enregistre les apps.

Références : `sysimage/select-system.py`, `userland/build.sh`, `userland/src/init.c` (`flash_root`).

Notre essai B1L-0013 n'avait donné que 4 apps sur 50, à cause des `Info.plist` compressés (réglé dans B1L-0022). La voie de l'installateur d'iOS n'a jamais abouti (B1L-0018 à 0023).

## Changement

`xnu-overlay/bsd/kern/b2l_apps_layout.h`, appelé avant launchd depuis `b2l_userspace_before_init()`. Il ne fait que des renommages sur le même volume, une seule fois :

| Source | Destination |
| --- | --- |
| `/private/var/staged_system_apps/X.app` | `/Applications/X.app` |
| `/System/Library/AppPlaceholders/X.app` | `/System/Library/AppPlaceholders.b2l-parked-0024/X.app` |
| Les 2 bases LaunchServices du conteneur de lsd | `*.b2l-before-0024` |

- Aucune cible existante n'est remplacée.
- Le marqueur `/private/var/db/b2l-apps-layout-v1` est écrit quand les 50 apps sont en place.
- Tant que cette disposition est active, le lanceur de l'installateur n'est pas publié.
- `patches/0103-b2l-kernel-rename-excl.patch` ajoute `b2l_kernel_rename_excl()` dans `vfs_syscalls.c`.
- Kill switch : `b2l_apps_layout=0`.
- Lignes de journal `B2L LAYOUT: …`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-applayout-0024/boot.img` (25 913 192 octets) | `dab006d541005b17541949e4050efcd2359eb295c00d2b6e353b7867cdbb950c` |
| Kernel | `8537b4a879377ee91d07affab80f2074a4b48be7e57464037dd6d779762a62ce` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-touchapps-0023/boot.img`. Les renommages se défont à la main depuis l'image (rien n'est supprimé).

# Build B1L-0022 : décompression HFS (decmpfs types 3, 4, 9) dans le noyau

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `fb9c01e0…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0021.

## Cause trouvée

Les boots 0018 à 0020 journalisent des centaines de fois
`decmpfs.c:257:_decmp_get_func: tried to access a compressed file of unregistered type 4` (et `type 9`).
Sur iPhone, ces types sont décodés par l'extension AppleFSCompression, absente ici.

Les 50 apps de `/private/var/staged_system_apps` ont été restaurées depuis OS.dmg avec `ditto`, qui garde la compression.
Résultat : 3 343 fichiers compressés, dont 46 `Info.plist` sur 50. Les 4 apps restantes ont des `Info.plist` non compressés, ce qui explique les « 4 apps sur 50 » de B1L-0013.

Côté installateur, le message était `MIBundle _validateWithError: Info.plist … had none of the keys that we expect`. Aucun autre fichier du système n'est compressé.

Formats relevés sur les 3 343 fichiers :

| Type | Fichiers | Format |
| --- | --- | --- |
| 9 | 1 941 | Données brutes dans l'xattr `com.apple.decmpfs`, après un octet `0xCC` |
| 4 | 1 402 | zlib par blocs de 64 Kio dans la resource fork |

Le type 3 (zlib dans l'xattr) est ajouté par sûreté.

## Changement

- `xnu-overlay/bsd/kern/b2l_decmpfs_core.h` : décodeurs en C pur.
- `xnu-overlay/bsd/kern/b2l_decmpfs.h` : glue noyau (lecture de la resource fork par `vn_getxattr` avec `decmpfs_ctx`, zlib de libkern).
- `patches/0102-b2l-hfs-decmpfs-zlib-stored.patch` : `decmpfs_init()` enregistre les types 3, 4 et 9.
- Kill switch : `b2l_decmpfs=0`. Une ligne `b2l_decmpfs: type N served/FAILED` par type et par résultat.

Test hôte `tests/test_decmpfs.c` : les 3 343 fichiers sont identiques octet par octet à la lecture macOS, plus 700 sous-plages non alignées et une resource fork synthétique de 3 blocs (zlib, bloc stocké, fin de fichier) : **ok**.

Le lanceur v3 fera sa tentative 3 sur 3 (la dernière avec ce reçu).

Référence consultée : Scion-Project/hackios-xnu-kernel (S9, iOS 16.5). Ce projet met les apps de `staged_system_apps` directement dans `/Applications` (`sysimage/select-system.py`, `userland/build.sh`), sur un système extrait sans compression HFS.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-decmpfs-0022/boot.img` (25 913 008 octets) | `fb9c01e04e86306e0baf11c0ccbde2f5949314a021557c2176e9aa6b746c67c8` |
| Kernel | `8f1730dfc1b81ff2c33d463beaf09fb4108c9bd3babdeb288ec8390df86ec203` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-apfsname2-0021/boot.img`.

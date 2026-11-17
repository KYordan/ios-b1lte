# Build B1L-0019 : « apfs » pour les deux démons d'installation (cause racine des apps manquantes)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `8abb2891…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0017 (+ racine B1L-0018).

## Cause racine (journaux iOS décodés)

Décodeur Mandiant réparé (catalogues `dsc` placés à la racine de l'archive : 0 chaîne partagée non résolue).
Premier démarrage (SystemAppMigrator lancé par `b2l-system-app-bootstrap-v2`), pour chacune des 50 apps :
`IXErrorDomain 2 "Failed to create IXPlaceholder …"` ← `IXErrorDomain 1 "Failed to create promise staging root directory"`
← **`MIInstallerErrorDomain 166 "Expected install path file:///private/var/ to be located on an APFS formatted volume; got hfs"`**
(`-[MIStagingManager stagingLocationForURL:withinStagingSubsytem:…]`, ligne 642).
→ aucune app « en attente » ne s'installe ; donc pas de contrôles du Control Center, de widgets ni de fournisseurs de fonds d'écran.

B1L-0018 (installeur natif) : `staged_system_apps` restauré, mais même blocage ; le lanceur B2L n'a pas fini (journal vide).

## Correctif kernel

`xnu-overlay/bsd/vfs/b2l_fstype.h` + `patches/0100-b2l-installd-apfs-fstype.patch` (`vfs_getfstypenameref_locked`) :
pour le montage racine HFS+, le nom de type renvoyé à **`installd` et `installcoordinationd` seulement** est `apfs`
(statfs, getattrlist `ATTR_VOL_FSTYPENAME`). Tous les autres processus et le kernel voient `hfs`.
Coupe-circuit : `b2l_apfs_fstype=0`. Non vérifié : la suite de l'installation (clonefile non pris en charge par HFS ;
`copyfile` revient normalement à une copie).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-apfsname-0019/boot.img` (25 910 232 octets) | `8abb2891f186d079469c4f44d13ea975b3c202ae32e58285c3c982fd49c79fc0` |
| Kernel | `2422e38c579a13df43817ca968ed7a110a6ed892d358510e9df1132cd58c7f01` |

Retour arrière : `out/b1l-yuv-gpujob-0017/boot.img`.

## Résultat observé (B1L-0019)

Toujours Réglages + Téléphone seulement. Journaux iOS (démarrages récents) : plus d'erreur APFS ; à la place, 478 fois
**`MIInstallerErrorDomain 67 "Device capabilities do not match requirements: MismatchedCapabilities = { arm64e = 0 }"`**.
Le lanceur a appelé `SystemAppMigrator` (« invoking … staged=50 ») sans retour, et laissé un reçu `attempted/complete=0`.

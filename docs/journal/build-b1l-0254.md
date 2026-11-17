# B1L-0254 — HFS+ accepte la demande APFS « marquer purgeable » (PosterBoard → style des icônes)

## Cause (journaux iOS du démarrage 0252, voir build-b1l-0253.md)

- SpringBoard : `[IconStyle] _postersStyleStateDidChange nil poster style state … still nil; bailing` (13 fois) ;
  PosterBoard : `-[PBFPosterExtensionStoreCoordinator _correctPermissionsForInternalDirectories]> failed to correct
  purgability … errno=-1` (90 fois), puis aucun poster créé (`initialPosters is nil`).
- Lu dans le code (cache 23G90, `static-work/orig`) : `_correctPermissionsForInternalDirectories` (0x2207b0d94) appelle
  `pbf_setPurgable:error:` ; PosterBoard (0x220651074, après un `open`) et PosterFoundation (0x1c5518840) envoient
  `fsctl`/`ffsctl` avec la requête **0xC0084A44** = `_IOWR('J', 0x44, uint64_t)`, la demande APFS de marquage
  purgeable. Notre racine HFS+ la refuse.

## Changement (`xnu/bsd/vfs/vfs_syscalls.c`, `fsctl_internal` ; `patches/0110-b2l-hfs-purgeable-fsctl.patch`)

Après `VNOP_IOCTL`, si la requête est 0xC0084A44 sur un volume `hfs` et a échoué : réponse 0 (rien n'est changé,
« purgeable » n'est qu'une indication pour libérer de la place). Lignes `B2L PURGEABLE:` (8 au plus).
Interrupteur : boot-arg `b2l_hfs_purgeable=0`. Noyau seul, set v120 inchangé.

## Risques

- PosterBoard peut ensuite échouer plus loin pour une autre raison propre à APFS (non vu tant que celle-ci bloquait).

## Installé

- SYSTEM-OK, BOOT `cc011e66e021e6a223784352dc8f3a13403339c589e57f6e876c9d15ed96c902`, BOOT-OK (noyau contient « B2L PURGEABLE »).

## Résultat

- Testé : icônes toujours vides. Le log noyau 0254 n'a que 8 lignes `B2L PURGEABLE:`, toutes de `dirs_cleaner` (erreur 25) :
  la limite de 8 a masqué PosterBoard. PosterFoundation utilise aussi 4 autres requêtes (voir build-b1l-0255.md).

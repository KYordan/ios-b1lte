# B1L-0255 — HFS+ accepte les 5 requêtes « purgeable » de PosterFoundation

## Cause

- 0254 ne couvrait que 0xC0084A44. Lu dans le code de PosterFoundation (cache 23G90, `static-work/orig`, relevé par la
  session noyau puis vérifié ici) : `-[NSURL pf_isPurgable]` (0x1c5518410) envoie **0x40304A6D** ;
  `-[NSURL pf_setPurgable:afterDate:error:]` (0x1c551846c) envoie **0x40184A03**, puis **0xC1104A71**, puis
  **0xC0304A6F** (le vrai marquage) ; un retour non nul devient l'erreur `errno=-1` vue dans les journaux ;
  `pf_unmarkAsPurgableWithError:` (0x1c55187f8) envoie **0xC0084A44**.
- L'erreur remonte par `ensureFileSystemIntegrity` ; `+[PBFPosterModelStoreCoordinator createNewStoreCoordinatorWithPath:error:]`
  abandonne si elle n'est pas nil (0x2205ef79c) → `initialPosters is nil` → SpringBoard sans style d'icône.
- Aucune de ces requêtes n'a de `case` propre dans `fsctl_internal` (elles passent par `default`, donc `VNOP_IOCTL`).

## Changement (`xnu/bsd/vfs/vfs_syscalls.c`, `patches/0111-b2l-hfs-purgeable-fsctl-all.patch`)

Même réponse 0 sur `hfs` pour les 5 requêtes. Pour les deux en lecture (IOC_OUT), `fsctl_internal` a déjà mis le tampon à
zéro (bzero) et le recopie si error == 0 : « pas purgeable », « pas encore fait ». Log `B2L PURGEABLE: <proc> fsctl 0x…`
(64 au plus). Interrupteur inchangé : `b2l_hfs_purgeable=0`. Noyau seul, set v120 inchangé.

## Installé

- SYSTEM-OK, BOOT `931382665c2149c7d7844683dab888738ec04974fbed9c30cb51ff933ca9ac8b`, BOOT-OK.

## Résultat

- Journaux iOS du démarrage 0254 (USERDATA lu, `ios26-userdata-icons-254.img`) : **0254 suffisait déjà pour PosterBoard** —
  zéro `failed to correct purgability`, « file system integrity assuaged » pour 5 posters, et SpringBoard
  `[IconStyle] … populated` (style `color`). 0255 reste comme filet (les 4 autres requêtes). Icônes toujours vides :
  cause suivante = `renderbox: unable to create heap` (voir build-b1l-0256.md).
- En attente sur 0255 lui-même : lignes `B2L PURGEABLE: PosterBoard …`, plus de `failed to correct purgability`, icônes dessinées.

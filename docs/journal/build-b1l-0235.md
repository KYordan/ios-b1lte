# Build B1L-0235 : ne plus écraser le dessin d'une texture posée sur un tampon

Date : 2026-11-14. Base : B1L-0234. Ensemble `b2l-metal-v106` (noyau reconstruit), Mesa inchangé, pilote recompilé.

## Changement (`hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0235`)

- `-hakiosRefreshLiveCopy` d'une texture posée sur un tampon : si la texture peut être écrite par le GPU
  (`MTLTextureUsageRenderTarget` ou `ShaderWrite`), elle n'est remplie depuis le tampon **qu'une fois, à sa création**,
  et plus à chaque liaison. Ce que RenderBox y dessine n'est donc plus effacé avant d'être relu.
- Les textures en lecture seule sur tampon restent recopiées à chaque liaison (comme en 0231).
- **Limite connue :** le dessin ne revient toujours pas dans le tampon (pour un lecteur du tampon lui-même).

## Installé

- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `f85666a4ba247f34a841891aab02e4d16c7d36e722411a18c65e95674788446f` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat

- **Pas mieux** (propriétaire). Journal récupéré plus tard (`out/logs/0235-boot.txt`), avant le redémarrage sur 0236.

## Piste donnée par le propriétaire (CHANGELOG de hakios, `oosexgang/hakios-xnu-kernel`, `63fc2b4`)

- « HakiosGPU's request `0x7d`: the client's own pages as a buffer … (grey icons and text before) » :
  `HakiosGPUUserClient::userBuffer` dans `drivers/gpu/hakios.gpu.cpp`. Il câble les pages du programme et les place
  dans l'espace d'adresses du GPU, comme `userptr` sous Linux. Il sert aux tampons Metal sans copie et aux backing
  stores de CoreAnimation.
- **Chez nous, `0x7d` est une autre requête** (`HAKIOS_MALI_WAIT`, `hakios.drm.c`). Les
  `request 0x7d … -> 0xe00002d6` de nos journaux sont des délais d'attente de jobs (`kIOReturnTimeout`), pas cette
  fonction.
- Mais l'idée s'applique : notre `newBufferWithBytesNoCopy:` répond par une copie prise une fois (B1L-0186), donc les
  écritures suivantes de RenderBox ne sont jamais vues. Implémenter les pages du programme comme tampon GPU dans
  `B2LMaliService` + panfrost est une piste sérieuse pour les icônes et le texte. Lire la licence de `drivers/`
  avant d'en reprendre quoi que ce soit.

## Piste ios-pipa (`nishant713/ios-pipa`, MIT, `830ec1c`), lue à la demande du propriétaire

- NOTES, boot 39 : il n'y avait « pas d'icônes » ; les surfaces des images étaient créées par CoreAnimation autour de sa
  propre mémoire d'image (clé `IOSurfaceAddress`), et leur noyau donnait une mémoire neuve à zéro, donc des images
  transparentes. Leur correctif 0117 a fait apparaître les icônes.
- Chez nous, aucune surface de ce genre dans les journaux (notre `B2LSurface.cpp` ignore aussi `IOSurfaceAddress`,
  à garder en tête). **L'équivalent de notre chemin Metal, ce sont les tampons « sans copie »** : leurs tailles sont
  exactement celles des textures sur tampon (62 464 = 122 × 512 RGB10A2, 169 728 = 204 × 832 BGRA, 52 224 = 204 × 256 R8).
  RenderBox dessine les icônes dans sa propre mémoire d'image et la relit ; chez nous, le dessin n'y revient jamais.
  Suite : B1L-0236.

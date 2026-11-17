# Build B1L-0122 : un encodeur de copie (blit) pour le Liquid Glass

Date : 2026-11-08. Base : B1L-0121. Ensemble `b2l-metal-v72`, Mesa recompilé.

## Pourquoi

Depuis 0121, plus aucune texture du verre n'est refusée et le GPU ne fait plus de faute, mais le dock reste noir et les tuiles gris foncé. `-blitCommandEncoder` répondait `nil` : en Objective-C, un message à `nil` ne fait rien, sans erreur. Chaque copie de QuartzCore (le fond sous le verre, relu dans l'image en cours) était perdue, et le verre lisait une texture vide (noire).

## Changement

- **Mesa (`apply.py`, patch B1L-0122, `air_metal.c`)** :
  - `rwa_texture_copy` : copie de texture par `pipe->blit` sur le GPU ; formats des vues respectés ; tranches, niveaux, textures 3D ;
  - `rwa_texture_mipmaps` (`util_gen_mipmap`) ;
  - `rwa_buffer_copy` (`resource_copy_region`) ;
  - `rwa_buffer_fill` (`clear_buffer`).
- **Pilote (`hakiosmetal_air.m`)** :
  - classe `HakiosMetalBlitEncoder`. Copies texture→texture (les trois formes de Metal), `generateMipmapsForTexture:`, `copyFromBuffer:…toBuffer:`, `fillBuffer:`. Les copies tampon↔texture ne sont pas encore faites et le disent une fois ; synchronisations et optimisations sans effet ;
  - `HakiosMetalWork` : champs de copie (source et destination retenues comme le reste) ;
  - `-commit` exécute les copies dans l'ordre avec les dessins ; une copie refusée est dite (`B2L BLIT: copy N refused`) sans arrêter le command buffer ;
  - les 8 premières copies de texture sont décrites dans le journal (`B2L BLIT: texture copy …`).

## Installé

- Deux essais arrêtés à la compilation de Mesa, rien d'écrit : `util/u_box.h` s'appelle `util/box.h` dans Mesa 25 ; les fonctions exportées demandent un prototype (`-Wmissing-prototypes`).
- **Vérifié :** les 4 fonctions exportées par `libhakiosair.dylib` (v72) ; `HakiosMetalBlitEncoder` et les messages `B2L BLIT` dans `libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK en 65 s. **BOOT :** relecture `6a9580053418c9dc51935d07a1f6e37bb180bd40a50b41af169e9423d20a91ed` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0121/boot.img` avec SYSTEM v71.

## Résultat (journal `out/logs/0122-boot.txt`)

- **Propriétaire : pareil.** Aucun plantage, 0 faute GPU, 0 refus.
- CoreAnimation n'utilise les copies que pour de petits morceaux (8 copies 8×8 dans une texture 776×24) : le verre ne passe pas par des copies. L'encodeur reste utile (il manquait), mais ce n'était pas la cause.
- Piste suivante : les vues partielles (un niveau, une tranche) refusées (`nil`) et les surfaces de rendu toujours faites au niveau 0 : les passes de flou et de verre qui dessinent dans les niveaux d'une chaîne de mipmaps ne dessinaient nulle part → B1L-0123.

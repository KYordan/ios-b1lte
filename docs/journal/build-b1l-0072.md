# Build B1L-0072 : M5 étape 3, `CAMetalLayer` et ses drawables

Date : 2026-11-07. Base : B1L-0071 (M5 étape 2 réussie, identique au GPU du Mac).

## Changements

- **`scion/hakiosmetal_air.m` :** une texture créée sur une IOSurface garde cette surface (retenue) et la rend par `-iosurface` et `-iosurfacePlane`. Avant, ces deux méthodes répondaient toujours `nil` et `0`. Le code qui demande sa surface à la texture d'un drawable la trouve donc maintenant.
- **Test `m5_layer_test` (`b2l_metal_test.m`)**, comme le ferait une app :
  1. `CAMetalLayer` (device Mali, BGRA8, `framebufferOnly = NO`, 64×64), puis `nextDrawable` ; le test affiche la classe, la texture, l'IOSurface créée par CoreAnimation (ID, taille, format, octets par ligne) et le temps ;
  2. le dessin QuartzCore de l'étape 2 dans la texture du drawable, comparé ligne par ligne à l'image du Mac ;
  3. `presentDrawable:` dans un command buffer, avec son statut ;
  4. un second `nextDrawable`.
- `libb2l-metal-test` est lié à QuartzCore. Ensemble `b2l-metal-v28` (`libb2l-metal-test` `ac52be4d…`, `libhakiosmetal` `f7e08126…`), et le noyau pointe vers v28.

## Installé

- SYSTEM : 6 blocs réécrits, en 73 s.
- BOOT : `out/b1l-metal-0072/boot.img`, relecture `bca73bd2a700aaf2c6c04a4eb7c18df4c110cf2a4050e60a266ad082293325e3` (BOOT-OK).

## Résultat (`out/logs/0072-boot1.txt`)

- Les étapes 1 et 2 sont toujours justes (rouge, vert ; `0 of 64 rows differ`).
- **`CAMetalLayer` marche :**
  - `nextDrawable -> CAMetalDrawable in 44572 us`, texture `HakiosMetalTexture` 64×64 format 80 (BGRA8Unorm) ;
  - IOSurface ID 38, 64×64 `'BGRA'`, 256 octets par ligne, créée par CoreAnimation elle-même ;
  - `presentDrawable committed, status 4` ;
  - un second `nextDrawable` donne une autre surface (ID 39).
- **Le dessin dans le drawable est refusé :** `colour attachment 0 (MTLPixelFormatBGRA8Unorm) was made without MTLTextureUsageRenderTarget`. Le descripteur de CoreAnimation arrive avec un usage que `libhakiosair` ne lit pas comme une cible de rendu. Dans `texture_of_shape`, `PIPE_BIND_RENDER_TARGET` n'est mis que si le bit RenderTarget est présent ; un usage à 0 (Unknown) ne suffit pas.

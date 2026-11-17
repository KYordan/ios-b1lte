# Build B1L-0073 : la texture d'un drawable est une cible de rendu

Date : 2026-11-07. Base : B1L-0072.

## Changement

- `scion/hakiosmetal_air.m`, `newTextureWithDescriptor:iosurface:plane:` : une texture sur une IOSurface reçoit toujours `MTLTextureUsageRenderTarget`.
  - Un usage `MTLTextureUsageUnknown` (0), qui selon Metal autorise tout, devient `ShaderRead | RenderTarget`.
  - `ShaderWrite` n'est pas ajouté : un format que panfrost ne prend pas en image de shader ferait alors refuser l'import.
- Le premier appel journalise l'usage demandé et si le descripteur est celui d'un drawable (`isDrawable`).
- Ensemble `b2l-metal-v29` (`libhakiosmetal` `a1332e26…`), et le noyau pointe vers v29.

## Attendu

- `M5 layer` : le dessin QuartzCore dans le drawable donne `0 of 64 rows differ`, puis `presentDrawable` en statut 4.

## Installé

- SYSTEM : 5 blocs réécrits, en 81 s.
- BOOT : `out/b1l-metal-0073/boot.img`, relecture `f03a5dadd92532a650c7163af7de39404a30562cca1c131ab88b96e852c5d29b` (BOOT-OK).

## Résultat (`out/logs/0073-boot1.txt`) : M5 terminé

- **Drawable de `CAMetalLayer` :** `nextDrawable` en 31,6 ms (IOSurface ID 38, 64×64 `'BGRA'`), puis le dessin QuartzCore dans sa texture : `two draws in one pass, status 4, 15826 us`, `2005 drawn (Mac: 2005)`, **`0 of 64 rows differ from the Mac's picture`**.
- `presentDrawable committed, status 4`, et un second `nextDrawable` donne une autre surface (ID 39).
- Le journal d'usage n'a vu que la première texture IOSurface, celle du test de l'étape 1 (usage `0x5` = ShaderRead | RenderTarget). Il n'est écrit qu'une fois, donc celui du drawable n'apparaît pas. La correction s'applique à toutes les textures IOSurface.
- **M5 est terminé :** IOSurface, cache dans les deux sens, shaders de QuartzCore, plusieurs draws par passe, `CAMetalLayer`, `nextDrawable` et `presentDrawable` fonctionnent sur le Mali.

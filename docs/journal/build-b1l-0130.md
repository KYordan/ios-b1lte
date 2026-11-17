# Build B1L-0130 : inventaire des shaders que QuartzCore dessine

Date : 2026-11-09. Base : B1L-0129. Ensemble `b2l-metal-v79` (Mesa inchangé).

## Pourquoi

- **0129 n'a rien changé à l'écran** (propriétaire : « pareil qu'en 128 », icônes et widgets toujours absents).
- Journal `out/logs/0129-boot.txt` : aucun plantage, **aucun dessin refusé**. Les glyphes ne sont donc pas rejetés : soit ils sont dessinés mais rendent mal, soit ils ne passent jamais par le GPU.
- La trace `B2L ATT` s'arrête à 16 lignes, toutes pareilles : attachement 1 de 776×24 en RGBA16Float (format 115), chargement DontCare ou Clear. Aucune passe à 3 attachements n'y apparaît, alors que `path_blit_frag` (tracés vectoriels) lit l'attachement 2 et écrit les attachements 0, 1 et 2 (`air2nir --interface`).
- Le test Metal embarqué (`libb2l-metal-test`) ne tourne plus : son hôte n'est pas lancé. L'autotest d'un shader à framebuffer fetch aurait demandé le compilateur Metal, qui n'est pas installé sur ce Mac (`xcodebuild -downloadComponent MetalToolchain`).

## Changement (diagnostic seul, `hakiosmetal_air.m`)

- Chaque pipeline de rendu garde le nom de sa fonction fragment.
- Avant chaque dessin, une table de 96 noms compte les dessins par fonction.
- Le journal écrit `B2L FN: <fonction> draw <n>, <k> attachments <i>:f<format> <l>x<h> [clear]` au 1er, 256e et 65 536e dessin de chaque fonction.

On saura ainsi :
- si `path_*` (tracés vectoriels) et les filtres vibrants (`fixed_frag` à framebuffer fetch) sont dessinés ;
- dans quels formats et tailles.

## Installé

- **Vérifié :** `B2L FN` dans `libhakiosmetal.dylib` (v79).
- **SYSTEM :** SYSTEM-OK en 69 s (6 blocs sur 1750). **BOOT :** relecture `3002514a1822168cde34843cf1cb08b8a6c2af34323d0bbcd944f845be963740` (BOOT-OK). Redémarré vers le système par `adb reboot`.

## Résultat (journal `out/logs/0130-boot.txt`)

- **Aucun shader `path_*` n'est dessiné.** QuartzCore d'iOS 26 ne trace pas les formes vectorielles sur le GPU avec eux. Les glyphes passent par des spécialisations de `fixed_frag` aux noms générés (`TimgA2Xhf_Ialp`, `TcimA2Xhf_…`, et `TimgBvcmA2Xhf_Ialp`, `TkfhBvcm…` pour le mélange « vibrant »).
- **Toutes les passes ont 2 attachements de même taille :** 0 en BGRA8 (format 80) et 1 en RGBA16Float (format 115), souvent en plein écran (1440×3040). `fixed_frag` relit les couleurs 0, 1 et 2 (framebuffer fetch, `air2nir`).
- Un plantage de backboardd (`malloc` dans Foundation/BackBoardServices, pas dans le pilote ; probablement la pression mémoire), puis SpringBoard relancé.
- Suite : B1L-0131 (panfrost ne rechargeait pas une cible seulement relue).

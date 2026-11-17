# Build B1L-0079 : M6a, le Mali à l'écran dans l'app Horloge (et le patch de 0078)

Date : 2026-11-07. Base : B1L-0078, qui n'a pas été démarré et que ce build contient.

## M6a

- **`userspace/metal/b2l_metal_app.m`** → `libb2l-metal-app.dylib`, injectée par le noyau dans Horloge (`/Applications/MobileTimer.app/MobileTimer`). Elle fait, dans l'ordre :
  1. 2 s après le lancement, elle cherche la fenêtre de l'app (et réessaie chaque seconde, pendant 30 s au plus) ;
  2. elle crée le device du Mali à la main (`libhakiosmetal`, `HakiosMetalDevice initWithAcceleratorPort:` sur `IOAcceleratorES`), comme `metal_drv_test` ;
  3. elle construit le pipeline QuartzCore validé en M5 (`std_vert0_lpf` + `attachment_clear_frag_lpf`) ;
  4. elle pose un `CAMetalLayer` plein écran, à la résolution native, par-dessus la fenêtre ;
  5. à chaque image (`CADisplayLink`), elle efface avec une couleur qui change et dessine 24 triangles noirs qui tournent (24 dessins par image) ;
  6. elle présente l'image (`[drawable present]`) dans le handler « completed », donc une fois le Mali fini.
- **Journal `b2l-mapp:`** : premier drawable (texture, IOSurface), puis toutes les 120 images le nombre d'images par seconde, le temps de `nextDrawable`, de l'encodage et du commit jusqu'à la fin du GPU, et le pire temps.
- **Noyau :**
  - `b2l_exec_app_inject` (`b2l_exec_graphics_diag.h`) injecte la bibliothèque, avec le coupe-circuit `b2l_metal_app=0` ;
  - `b2l_amfi_constraint.h` laisse dyld accepter l'injection pour ce processus (`b2l_app_dyld_pid`) ;
  - `b2l_userspace_seed.h` publie la bibliothèque.
- **La Sandbox de ce noyau n'applique pas les profils d'iOS**, donc l'app peut ouvrir le Mali.
- **Ensemble `b2l-metal-v35`** : la bibliothèque M6a s'ajoute aux relais non signés de 0078.

## Ce qu'il faut faire au boot

- Ouvrir **Horloge** et la laisser ouverte environ 30 s : l'écran doit montrer un fond qui change de couleur et des triangles noirs qui tournent.
- L'interface d'Horloge reste rendue comme avant, cachée sous ce calque. L'assemblage à l'écran reste fait par backboardd en rendu CPU.

## Installé

- **Noyau :** il contient l'injection (`libb2l-metal-app.dylib` y apparaît 3 fois).
- **SYSTEM :** 5 blocs réécrits, en 72 s.
- **BOOT :** `out/b1l-metal-0079/boot.img`, relecture `46968a0efccb1fae7d199cd33c61425b7a05fb2e4ea07afca5f2a47d3795d67a` (BOOT-OK).
- **Pour revenir en arrière :** démarrer avec `b2l_metal_app=0`, ou reflasher `out/b1l-metal-0077/boot.img`.

## Résultat (`out/logs/0079-boot1.txt`) : M6a réussi, le Mali à l'écran

- **Ce que le propriétaire a vu :** « l'écran change de couleur et les triangles sont affichés et tournent ». C'est pas très fluide, clairement pas 60 images par seconde, avec quelques saccades.
- **Journal :**
  - `libb2l-metal-app.dylib inserted (pid 507)` ;
  - device Mali-G76 ;
  - pipeline QuartzCore construit en 40 ms ;
  - `CAMetalLayer` de 480×1013 points à l'échelle 3, drawable **1440×3040**, IOSurface de 5888 octets par ligne.
- **Cadence :** **20,4 à 22,8 images par seconde** (17,1 sur une fenêtre). Par image :
  - `nextDrawable` : **30 à 47 ms** ;
  - encodage des 24 dessins : **0,15 à 0,19 ms** ;
  - du commit à la fin du GPU : **5,4 à 7,6 ms** ;
  - pire image : 52 à 81 ms.
- **Le goulot est la récupération des drawables :** l'app attend que backboardd rende un drawable. Son compositeur CPU met environ 45 ms à assembler une surface de 1440×3040 (17 Mo). Le Mali et notre pilote ne prennent ensemble qu'environ 7 ms par image. Cela justifie M6b : le compositeur sur le GPU.

## Résultat des relais réécrits (B1L-0078)

- **`M5 PAC` : 0 piège par dessin.** Il en reste 18 dans tout le tour, des `ldapr` dans `util_sparse_array_get` et `panfrost_bo_unreference` de Mesa : une instruction ARMv8.3 que le noyau émule aussi.
- **`M5 bench` : 26 à 56 µs par dessin** dans la plupart des tours, contre 550 µs avant ; quelques tours font 150 à 200 µs, sans doute dérangés par d'autres processus.
- **Toujours juste :** `0 rows differ` partout.
- **Bilan depuis B1L-0073 :** de 8 à 15 ms par dessin, on est passé à environ 0,03 à 0,05 ms.

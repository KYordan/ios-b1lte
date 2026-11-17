# Build B1L-0056 : Metal M3 réussi, M4 (dessin) en test

Date : 2026-11-07. Statut : **installé : SYSTEM (6 blocs, flash rapide en 84 s) et BOOT (`996583dd…`, revérifié sur le téléphone) ; pas encore testé.** Base : B1L-0055.

## M3 réussi (B1L-0055, `out/logs/0055-boot1.txt`)

Dans un thread de `weatherd`, avec le driver Metal Mali de Scion (`libhakiosmetal` + `libhakiosair`, ensemble `b2l-metal-v11`) :

- **Device :**
  - `step 5 gave HakiosMetalDevice`, puis `MTLAddDevice returned` (trouvé par `dlsym`), et `MTLCreateSystemDefaultDevice() = HakiosMetalDevice`, qui s'annonce `Mali-G76 (Panfrost) (Hakios)` ;
  - `of 129 methods MTLDevice requires, 0 are unanswered` ; `of 414 methods MTLDeviceSPI requires, 0 are unanswered`.
- **Bibliothèque :** `PortraitFilters.cifilter/default.metallib` (36 fonctions) lue par `_MTLLibrary`.
- **`image_calcminmax0` :** `compiled: 256 threads per group, width 8`, AIR → NIR → compilateur Bifrost (Mesa panfrost). Puis **`dispatched image_calcminmax0: status 4`**, avec `buffer 0: … first words 7fffffff 80000000`.
- **`MetalFaceMask_InitMinMax` :** compilé, **`status 4`**, `first words 7fffffff 7fffffff 7fffffff 80000000` (INT_MAX et INT_MIN, l'initialisation attendue d'un min/max).

C'est la première exécution de shaders Metal d'Apple sur le Mali-G76.

**Reste à voir :** `B2L COMPAT: rejected result=0 pc=0x1b7125e2c x16=0x952d00011a1120a0` revient à chaque boot, avant le test. Une branche PAC est refusée dans un autre processus, avec un registre porteur de bits hauts.

## B1L-0056 : M4

- `b2l_metal_test.m` lance :
  - `image_calcminmax0` (contrôle) ;
  - `metal_drv_test --draw AppleMetalGLRenderer.bundle/default.metallib` : un triangle avec le fragment shader d'Apple `clearFragment20`, dont le pixel est comparé à la référence softpipe `2458616957 2930221209 3401825461 3873429713` ;
  - `--draw-index` : le même dessin via un buffer d'index.

  Si la bibliothèque n'existe pas sur iOS 26, le journal le dit.
- Ensemble `b2l-metal-v12` (`libb2l-metal-test` `13d400b9…`). Le noyau publie et injecte la v12.

## Constat sur B1L-0056 et B1L-0057

- **M4, premier essai :** `AppleMetalGLRenderer.bundle/default.metallib` (274 fonctions) est lu ; le pipeline `clearFragment20` est fait (3 attachements RGBA32Uint, étage vertex généré) ; l'encodeur de rendu existe. Les 16 texels préremplis se relisent correctement, puis `draw: status 4`.
- **Mais** `the pixel at (0, 0) of attachment 2 is 0 0 0 0`, alors que softpipe attend `2458616957 2930221209 3401825461 3873429713` (`DIFFERENT`). Les dessins indexés couvrent tous 0 pixel sur 16. Les deux refus attendus, eux, ont lieu.
- Les valeurs préremplies sont écrasées par des zéros : le GPU a donc écrit dans l'attachement, mais sans le résultat du shader. Aucune faute Mali dans le journal noyau.
- **B1L-0057 (diagnostic) :** avant que l'écran panfrost soit créé, `PAN_MESA_DEBUG=sync,linear,noafbc,nocrc` :
  - `sync` attend chaque job et signale les fautes GPU ;
  - `linear` et `noafbc` donnent des attachements ligne par ligne, ce qui écarte une question de disposition à la relecture ;
  - `nocrc` désactive l'élimination de transactions.

  Puis `--draw` à nouveau, et `--draw-attrib` (vertex shader de QuartzCore, fragment shader du renderer GL) comme deuxième image. Ensemble `b2l-metal-v13`.
- **Installé :** SYSTEM (flash rapide en 86 s) et BOOT `out/b1l-metal-0057/boot.img` (relecture `46b387632ad70269659cc11c59d54360e873ca56f3fe90aeb4722fc691f39384`).

## Constat sur B1L-0057 (`out/logs/0057-boot1.txt`)

- **Avec `PAN_MESA_DEBUG=sync,linear,noafbc,nocrc`** (aucune faute GPU signalée), `clearFragment20` donne toujours `0 0 0 0` au lieu de la référence softpipe. L'AFBC, la disposition et la CRC ne sont donc pas en cause.
- **Dessin à attributs** (`std_vert0_lpf` de QuartzCore + `texturedFragment2DUInt`) : à chaque variante, **1 pixel couvert sur 16**, le pixel (0,0), qui vaut `103 203 306 400` (le texel (3,3), donc la coordonnée (1,1)). Changer le pas des attributs change bien le résultat (étape 4), donc la lecture des sommets fonctionne en partie.
- **Lecture :** les primitives s'effondrent sur un seul pixel, avec une coordonnée constante. La position en sortie du vertex shader (ou le viewport) et les varyings n'arrivent pas correctement au rasteriseur ni au fragment shader de panfrost/Bifrost. Avec `clearFragment20`, le shader écrit 0, ce qui est cohérent avec des entrées (constantes ou varyings) lues à zéro.
- **Conclusion :** M4 n'est pas validé. M5 (dessin dans une IOSurface) réutilise exactement ce chemin de dessin : il faut d'abord corriger le rendu (vertex, varyings, buffer constant 14) dans `air_metal.c` / `air_to_nir.c` pour panfrost, que Scion n'a jamais exécuté sur un Mali.
- **Charge (495 s) :** `traps=25193778` (environ 51 000 pièges/s) ; `ret=17458785`, `br=6301141`, `ldra=1372186`, `in-shared-cache=24498035`. Le GPU est à 0 % (seulement nos tests).

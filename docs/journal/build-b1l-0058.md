# Build B1L-0058 : M4, panfrost reçoit enfin la description d'entrées et sorties des shaders

Date : 2026-11-07. Statut : **installé : SYSTEM (flash rapide en 94 s) et BOOT (relecture `1f9147d2…`) ; pas encore testé.** Base : B1L-0057.

## Cause trouvée

- `air_metal.c` (Scion, `libhakiosair`) prépare chaque étage pour Gallium dans `finalize_for_gallium` : UBO décalés, sysvals, puis `nir_lower_io`. Il ne lance jamais `nir_shader_gather_info`. Le front-end AIR remplit quelques champs à la main (`inputs_read` des attributs) ; le vertex shader généré (quad) n'en remplit aucun.
- **ir3** (Adreno, testé par Scion) calcule ces informations lui-même, et **softpipe/nir_to_tgsi** n'en a pas besoin.
- **panfrost** décide d'après `nir->info` seul (`pan_shader.c`, `panfrost_create_shader_state`) :
  - quelles cibles de rendu le fragment shader écrit (`outputs_written`) ;
  - quels attributs le vertex shader lit (`inputs_read`, y compris `pan_lower_image_index`) ;
  - quels varyings relient les deux étages (`fixed_varying_mask`).
- Avec des champs vides, la cible de rendu n'est jamais écrite. Il ne reste que l'effacement à 0 : c'est `clearFragment20` = `0 0 0 0` en B1L-0056/0057. Le vertex shader, lui, ne lit rien, et chaque primitive s'effondre sur un pixel (dessin à attributs).

## Correctif

- `userspace/mesa/air-b2l/apply.py`, appliqué par `scripts/build-mesa.sh` après la copie de `tools/mesa/air` : `nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir))` à la fin de `finalize_for_gallium`, après `nir_lower_io`. Le texte remplacé est vérifié, et la construction échoue si le fichier de Scion a changé.
- Mesa et `libhakiosair` recompilés. Ensemble `b2l-metal-v14` (`libhakiosair` `19af6bc7…`).
- Le test lance `--draw-index` (qui comprend le dessin `clearFragment20` comparé à softpipe) puis `--draw-attrib`.

## Attendu

- `the pixel at (0, 0) of attachment 2 is 2458616957 2930221209 3401825461 3873429713` et `SAME`.
- Les dessins indexés couvrent la moitié ou la totalité de l'image selon les indices.
- Le dessin à attributs donne la grille de texels (16 pixels sur 16, éventuellement inversée verticalement).

## Constat sur B1L-0058 et B1L-0059

- **Le correctif `nir_shader_gather_info` ne change rien :** `clearFragment20` donne toujours `0 0 0 0` ; les dessins indexés couvrent 0 pixel sur 16 ; le dessin à attributs couvre seulement le pixel (0,0). La coordonnée, elle, y arrive bien : texel (3,3) avec la coordonnée (1,1), texel (0,0) à l'étape 9. Les varyings passent ; la couverture, non.
- **B1L-0059 (comparaison) :** `RWA_SOFTWARE=1` fait tourner les mêmes dessins sur softpipe, le rasteriseur logiciel de Mesa, dans `libhakiosair`.
  - Images justes sur softpipe → défaut propre au chemin panfrost.
  - Images fausses aussi sur softpipe → défaut dans la traduction Metal → Gallium sur iOS 26.

  Ensemble `b2l-metal-v15`. Installé : SYSTEM (83 s) et BOOT `out/b1l-metal-0059/boot.img` (relecture `948a15a9b88d2d47cf38b8acafb3a0d6ab84114b51e93515a525ae4309b5c569`).
- **Référence S9 (hakios-starlte, `GPU.md`, 5 novembre) :** l'interface y est rendue en OpenGL ES (`eagl.c` → `libOSMesa`/panfrost → `hakios-drm` → HakiosMali). Leur chemin Metal (`hakiosmetal_air.m` + `libhakiosair`) est le même que le nôtre et n'a jamais tourné sur leur téléphone (« Nothing here has run on the phone »).

## Constat sur B1L-0059 : la traduction Metal est saine, le défaut est dans le chemin panfrost

Avec `RWA_SOFTWARE=1` (softpipe), le device est `softpipe (Hakios)` et **tout est juste** :
- `clearFragment20` : `2458616957 2930221209 3401825461 3873429713`, **`THE SAME`** ;
- dessins indexés : bande 16/16, moitiés 10 et 6 pixels, `baseVertex` et `UInt32` respectés, refus par nom ;
- dessin à attributs (`std_vert0_lpf` de QuartzCore + `texturedFragment2DUInt`) : grille de 16 texels sur 16 (orientée `(x, 3-y)`), instances par pas.

Les mêmes dessins sur le Mali : rien, ou le seul pixel (0,0). Le défaut est donc propre à panfrost ou à son interaction avec le NIR produit par `libhakiosair` (le front-end AIR, lui, est validé).

## B1L-0060 (diagnostic)

Un seul dessin (`--draw`, quad plein écran généré + `clearFragment20`) avec `PAN_MESA_DEBUG=…,trace` et `PANDECODE_DUMP_FILE=stderr`. Pandecode décode chaque descripteur envoyé au Mali (viewport, scissor, framebuffer, cibles de rendu, varyings, attributs, shaders) dans le journal noyau. Ensemble `b2l-metal-v16`.
- **B1L-0060 installé :** SYSTEM (82 s) et BOOT `out/b1l-metal-0060/boot.img` (relecture `84de8c3fd8aa92f03298c6cf5dff8920a5a992471335e20343e75b2b81469bc1`).

## Constat sur B1L-0060 (trace pandecode d'un dessin, `out/logs/0060-trace.txt`)

- **Job vertex :** vertex shader généré = `LD_ATTR_IMM.f32.v4 attribute_index:0` puis `ST_CVT.v4` vers l'adresse de position. 1 attribut (`0xa110000`, pas 16, 64 octets = 4 sommets), buffer de positions `0xa230000` (pas 16, 64 octets). `Invocation (1,1,1) x (1,4,1)` : les 4 sommets.
- **Job tiler/fragment :** `Position: 0xa230000`, viewport `-inf..inf`, z `-1..1`, scissor `0..3 × 0..3`, triangle strip de 4 sommets. Framebuffer 4×4, 3 cibles `RAW128` (RGBA32Uint), écriture linéaire.
- **Fragment shader (`clearFragment20`) :** `LOAD.i128.ubo` sur l'UBO 15 (le buffer constant 14 décalé de 1, lié à `memory_a130000`, 256 octets), puis `ATEST` et un seul `BLEND`. Le seul descripteur de blend décodé est celui de la cible 0 (`RG`, registre `F16`).
- **Piste 1 :** le `BLEND` écrit peut-être la cible 0 au lieu de la cible 2, que le test relit et qui reste effacée à 0.
- **Piste 2 :** le dessin à attributs (une seule cible) ne couvre qu'un pixel, donc un problème de position ou de rastérisation avec le vertex shader d'Apple. **B1L-0061** trace ce dessin-là.
- **B1L-0061 installé :** SYSTEM (70 s) et BOOT `out/b1l-metal-0061/boot.img` (relecture `f62dc57ceb572f08eb2aec3c43ef6cdfe7d0d95ab812918d5db84345821ead42`), trace du dessin à attributs.

## Constat sur B1L-0061 (trace du dessin à attributs, `out/logs/0061-trace.txt`)

- **Vertex shader de QuartzCore (`std_vert0_lpf`), compilé pour le Mali :**
  - partie position : `LOAD.i128.ubo` ×4 sur l'UBO 3 (matrice 4×4, `ubuf_3[64] = memory_a150000`), `LD_ATTR_IMM attribute_index:0`, 16 `FMA` (matrice × position) puis `ST_CVT` vers l'adresse de position ;
  - partie varyings : `LD_ATTR attribute_index:1`, `ST_CVT` (la coordonnée).
- **Descripteurs cohérents :** attribut `0xa1b0000` (pas 48), positions `0xa290040` (pas 16), varying RG32F `0xa290000` (pas 8), viewport et scissor 4×4.
- **Point commun avec `clearFragment20` :** les deux shaders lisent leurs valeurs dans un **buffer de constantes** (UBO). La coordonnée, qui n'en dépend pas, arrive juste ; la position (matrice × sommet) et la couleur (constantes) sont nulles. Hypothèse : sur le Mali, les UBO liés par adresse (le buffer `PIPE_USAGE_STAGING` mappé une fois par `libhakiosair`, puis écrit par le CPU) se lisent comme des zéros.

## B1L-0062

- `air-b2l/apply.py` : sur panfrost (`ubo_base != 0`), chaque buffer de constantes Metal est passé en `user_buffer`, que panfrost recopie lui-même à chaque dessin, au lieu de l'adresse GPU du buffer mappé. C'est aussi le seul chemin par lequel panfrost peut pousser des mots d'UBO en uniforms.
- Les 12 premières liaisons sont journalisées (`air-b2l: stage … constant buffer … words …`), pour voir ce que reçoit le shader.
- `PAN_MESA_DEBUG=sync` seul (sans trace). Tests `--draw-index` et `--draw-attrib`. Ensemble `b2l-metal-v18`.
- Installé : SYSTEM (95 s) et BOOT `out/b1l-metal-0062/boot.img` (relecture `625eacb8d64cc725ae5d84d303bf17cbfdfcee10ae7f7a1f1ae6d99e2f2db131`).

## Constat sur B1L-0062 et B1L-0063

- **Côté CPU, les données sont justes** (`air-b2l: … uploaded by the driver; words …`) :
  - `clearFragment20` reçoit son buffer 14 (48 octets, `221b140d 3e373029 …`, le motif attendu) ;
  - `std_vert0_lpf` reçoit ses matrices aux buffers 2 et 3 (`3f800000 0 0 0 …`, l'identité).
- **Résultats inchangés** (0 0 0 0, un pixel) : même recopiés par panfrost, les mots ne parviennent pas au shader.
- **Analyse :** panfrost ne « pousse » dans les uniforms (FAU) que l'UBO 0 et l'UBO des sysvals (`pan_shader.c`, `pushable_ubos`) ; les autres sont lus par `LOAD.ubo`. Le serveur d'affichage GLES (B1L-0041/42) n'utilisait que l'UBO 0, toujours poussé : **`LOAD.ubo` n'a donc jamais été exercé sur ce Mali**, et c'est lui qui rend des zéros.
- **B1L-0063 (expérience) :** `pushable_ubos = ~0` dans `pan_shader.c` quand `B2L_PAN_PUSH_ALL` est défini (GLES garde la règle de Mesa, ses autres UBO n'étant pas des pointeurs CPU). Appliqué par `air-b2l/apply.py`, de façon idempotente. Le test pose `B2L_PAN_PUSH_ALL=1`. Ensemble `b2l-metal-v19`.
  - Images justes → cause confirmée (`LOAD.ubo`), et contournement disponible.
  - Sinon → la lecture des constantes poussées échoue aussi.
- **B1L-0063 installé :** SYSTEM (72 s) et BOOT `out/b1l-metal-0063/boot.img` (relecture `41a77f19b8ca0708c73d31b57645a3c5a61ad5992f07362638d1db219722b005`).

## Constat sur B1L-0063 ; B1L-0064

- Avec `B2L_PAN_PUSH_ALL=1` (vérifié présent dans `libhakiosair` et dans le test), **rien ne change** : `clearFragment20` donne 0 0 0 0, les dessins indexés 0 pixel, le dessin à attributs 1 pixel. L'hypothèse « seul `LOAD.ubo` est fautif » ne suffit pas.
- **B1L-0064 :** trace pandecode du dessin `clearFragment20` avec les constantes poussables. On y verra si le compilateur Bifrost a remplacé les `LOAD.ubo` par des lectures de registres, et quelles valeurs sont poussées. Ensemble `b2l-metal-v20`.
- **B1L-0064 installé :** SYSTEM (67 s) et BOOT `out/b1l-metal-0064/boot.img` (relecture `f79eab7e2b92659010f3acfd30f3b3c80b0169962387fe092fe47c5313f0dca7`).

## Constat sur B1L-0064 (`out/logs/0064-trace.txt`) ; B1L-0065

- **Les constantes arrivent :** avec `B2L_PAN_PUSH_ALL`, le fragment shader compilé pour le Mali n'a plus de `LOAD.ubo`. Il lit ses mots dans les uniforms poussés (`MOV.i32 r0..r3, u0.w0 u0.w1 u1.w0 u1.w1`, `Uniform count: 2`, `Push uniforms: 0xa2102f0`), puis `ATEST` et `BLEND`.
- **Pourtant la cible 2 reste à 0.** Le défaut est donc après le shader : quelle cible le `BLEND` atteint, et avec quelle conversion. Le seul descripteur de blend décodé est celui de la cible 0 (`RG`, registre `F16`).
- **B1L-0065 :** `metal_drv_test` (copie B2L) relit aussi le pixel (0,0) des cibles 0 et 1 (`drvtest: draw: B2L: the pixel at (0, 0) of attachment N`). Pas de trace. Ensemble `b2l-metal-v21`.
- **Installé :** SYSTEM (79 s) et BOOT `out/b1l-metal-0065/boot.img` (relecture `58e76b61c34600bab6fc7a002d655f970053d85450d4e62bbf11ebc0f2c346ca`).

## Constat sur B1L-0065 ; cause trouvée ; B1L-0066

- **B1L-0065 :** le pixel (0,0) des cibles 0, 1 et 2 vaut `0 0 0 0` : rien n'est écrit, nulle part. Avec la trace 0064 (cibles en 16×16 entrelacé, `Row Stride: 4096`, relues correctement par panfrost) et le dessin à attributs (le seul pixel couvert reçoit la bonne valeur), **c'est la couverture qui manque**, pas l'écriture.
- **Cause :** panfrost n'applique pas la transformation viewport en matériel (`viewport_transform_lowered = true`, `pan_screen.c`). `bifrost_compile.c` l'ajoute au vertex shader via `nir_lower_viewport_transform`, qui ne regarde que les `store_deref` de la variable `VARYING_SLOT_POS`. `libhakiosair` livre des shaders dont l'I/O est déjà abaissée en `store_output` (`finalize_for_gallium`). La passe ne trouve rien, et les positions sortent en coordonnées de découpage [-1, 1] interprétées comme des coordonnées fenêtre : chaque primitive tombe sur le pixel (0,0). Les traces le confirment : le vertex shader généré = `LD_ATTR` + `ST_CVT`, sans le moindre calcul de viewport. ir3 (Adreno) et softpipe font cette transformation en matériel, d'où l'absence du problème chez Scion et sur softpipe.
- **Correctif (`air-b2l/apply.py`, idempotent) :** `nir_lower_viewport_transform` prend aussi un `store_output` de `VARYING_SLOT_POS` (écriture vec4 complète depuis la composante 0). Mesa et `libhakiosair` recompilés (`af2764cb…`). Ensemble `b2l-metal-v22`. Tests `--draw-index` et `--draw-attrib`.
- **Installé :** SYSTEM (76 s) et BOOT `out/b1l-metal-0066/boot.img` (relecture `253d04efb042a8f8562cb67fb32d7535226ebef3ed5715def91127f0b2622bda`).
- **Note :** les changements de 0058 (`gather_info`), 0062 (constantes en user buffer) et 0063 (`B2L_PAN_PUSH_ALL`) restent en place. Ils étaient probablement sans effet sur ce défaut ; ils seront réévalués une fois M4 validé.

## Constat sur B1L-0066 : le correctif viewport marche ; B1L-0067

- **Le dessin à attributs** (vertex shader de QuartzCore `std_vert0_lpf` + `texturedFragment2DUInt`, une cible) est **juste sur le Mali** : `coverage ####/####/####/#### 16 of 16; (0,0) = 100 203 303 400, (3,3) = 103 200 303 400`, `16 of 16 hold texel x, 16 hold (x, 3-y)`, exactement comme softpipe (B1L-0059). Encodages (`Short4Normalized`, `UChar4Normalized`), index de buffer 7, index buffer et instances donnent aussi la même image que softpipe.
- **Deux écarts restent :**
  1. **`clearFragment20` + quad généré** (3 cibles RGBA32Uint, seule la cible 2 écrite) : les cibles 0, 1 et 2 restent `0 0 0 0`, et les dessins indexés de ce pipeline couvrent 0 pixel.
  2. **Dessin à attributs :**
     - étape 4 (pas double, triangle fin) : 2 pixels au lieu de 4 ;
     - étape 7 (indices [1 2 3] à l'octet 8) : le Mali couvre l'autre moitié (10 pixels au lieu de 6), comme si le décalage de l'index buffer était ignoré.
- **pandecode** n'imprime sur v6+ qu'un seul descripteur de blend (`decode_jm.c`, `rt_count = 1`) : l'absence des cibles 1 et 2 dans les traces précédentes ne prouvait rien.
- **B1L-0067 :** `B2L_PANDECODE_RTS=3` (patch pandecode, `air-b2l/apply.py`) imprime les trois descripteurs ; trace du seul dessin `--draw`. Ensemble `b2l-metal-v23`. Installé : SYSTEM (71 s) et BOOT `out/b1l-metal-0067/boot.img` (relecture `0b2cb14d959936eff9bb05ac407dbb073545bf8c8716eca7284720ac6568a427`).

## Constat sur B1L-0067 : les trois descripteurs de blend sont valides

- Cibles 0 et 1 : `Mode: Opaque`, registre `F16` (le shader ne les écrit pas). **Cible 2 : `Mode: Opaque`, `RT: 2`, registre `I32`, masque 15** : valide pour une cible RGBA32Uint écrite en `int4`.
- Le vertex shader généré contient maintenant la transformation viewport (`FRCP` de w, 3 `FMA` avec les uniforms `u0..u2` = échelle/décalage).
- Le fragment shader pousse ses 4 mots (`u0.w0..u1.w1`) puis fait `BLEND`.
- Les « RG » des traces précédentes étaient « RGBA… » coupés par le filtre d'extraction (`[^B]`).
- **Reste inexpliqué :** rien n'est écrit quand le shader n'écrit que la cible 2 d'un rendu à trois cibles. Piste : particularité Bifrost quand la cible 0 n'est pas écrite. Ce cas est mis de côté : le rendu à une cible (celui de QuartzCore) est validé.

## B1L-0068 : M5 étape 1 (IOSurface)

- `b2l_metal_test.m`, `m5_surface_test()`, après un passage de `metal_drv_test` (qui ajoute le device de `libhakiosmetal` au processus) :
  - IOSurface 64×64 `BGRA` (256 octets par ligne, globale), remplie par le CPU de `0xa5a5a5a5` ;
  - `newTextureWithDescriptor:iosurface:plane:` (BGRA8Unorm, cible de rendu) ;
  - passe avec effacement rouge, puis relecture CPU par `IOSurfaceLock` (attendu `0xffff0000` sur 4096 pixels) ;
  - nouveau motif CPU, effacement vert, relecture (attendu `0xff00ff00` ; vérifie les deux sens CPU↔GPU).
- Ensemble `b2l-metal-v24` (`libb2l-metal-test` `e29afc30…`, lié à IOSurface.framework).
- **Installé :** SYSTEM (88 s) et BOOT `out/b1l-metal-0068/boot.img` (relecture `9079362c061796b7a8dd44900cb6ab61a7d9110c35ca9aabab1d296ae115961d`).

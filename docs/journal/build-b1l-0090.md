# Build B1L-0090 : trois règles de Bifrost v7 respectées dans Mesa

Date : 2026-11-07. Base : B1L-0089 (assertions signalées).

## Changements (`apply.py`, B1L-0090)

1. **`bifrost_compile.c`, `bifrost_preprocess_nir` :** avant v9, une écriture de varying en 16 bits d'un vertex shader est **élargie à 32 bits** (valeur et `src_type`). Le fragment continue de lire du `half`, et `LD_VAR` convertit.
2. **`pan_nir_lower_sysvals.c` :** l'instruction remplacée par une lecture du tampon des valeurs système est **retirée tout de suite** (`nir_instr_remove`), au lieu d'être laissée à l'élimination du code mort. Plus de `LEA_BUFFER` de Valhall ni d'intrinsèque inconnue au compilateur.
3. **`bifrost_compile.c`, `bi_emit_intrinsic` :** `load_raw_vertex_offset_pan` donne 0 au lieu d'un `unreachable`, par sécurité.

Mesa est toujours compilé avec ses assertions signalées (`B2L_MESA_ASSERTS=1`), pour voir ce qu'il reste. Ensemble `b2l-metal-v46`, plug-in `B2LMetalv46`.

## Installé

- **Vérifié :** les patchs B1L-0090 sont présents dans `bifrost_compile.c` (3) et dans `pan_nir_lower_sysvals.c` (1) de l'arbre compilé.
- **SYSTEM :** 7 blocs réécrits, en 82 s.
- **BOOT :** `out/b1l-metal-0090/boot.img`, relecture `42f418f3de99972242329b61c92d2038b44224f5357447ba5b875aa3ca3385bf` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0088/boot.img`.

## Résultat (boot 1)

- **Repli sur le rendu CPU.** Pendant la compilation de `A2Xghfc` (75 592 instructions NIR), `mesa-assert: 0 (bifrost_compile.c:2135, bi_emit_intrinsic)` : l'intrinsèque `demote` (le `discard_fragment` d'AIR) n'est pas gérée par Bifrost. La continuation après `unreachable()` fait planter backboardd (`exception=6`, `FAULTMSG frame_arg=demote`), 2 fois, puis le noyau repasse au rendu CPU au 3e démarrage.
- **Pourquoi 0088 marchait :** Mesa sans assertions sautait l'intrinsèque en silence.
- **Bon signe :** 0 faute « did not finish », 16 images envoyées avant le plantage.

# Build B1L-0089 : Mesa avec ses assertions, signalées dans le journal

Date : 2026-11-07. Base : B1L-0088 (interface rendue par le Mali, 99 fautes 0x51 restantes).

## Principe

- **Plutôt que de reproduire tout panfrost sur le Mac,** le Mesa du téléphone est compilé avec ses assertions (`B2L_MESA_ASSERTS=1` dans `build-mesa.sh` → `-Db_ndebug=false`).
- **`libhakiosair` définit `__assert_rtn`.** L'éditeur de liens y relie tous les `assert()` de l'image. Une assertion qui échoue écrit `mesa-assert: <condition> (<fichier>:<ligne>, <fonction>)` dans le journal noyau (600 au plus), puis **le programme continue**.
- **panfrost compile à la création du pipeline,** donc une assertion suit la ligne `air-b2l: render pipeline <nom>` du shader fautif, dans le vrai chemin du pilote.
- **`NIR_DEBUG=novalidate` est posé par défaut** (constructeur de `libhakiosair`). On garde les assertions des passes et du compilateur Bifrost, sans revalider tout le shader après chaque passe, ce qui serait beaucoup trop lent sur le fragment principal.

## Risques

- Une assertion de type `unreachable()` qui « continue » peut faire planter backboardd. Le retour automatique au rendu CPU le couvre.
- Mesa est un peu plus lent avec les assertions. C'est un build de diagnostic.

## Build

- `build-mesa.sh` reconfigure le dossier de build quand le choix change, ce qui recompile Mesa en entier.
- Ensemble `b2l-metal-v45`, plug-in `B2LMetalv45`.

## Installé

- **Vérifié :** le dossier de build est passé à `b_ndebug=false`, et `libhakiosair` exporte `__assert_rtn` (`mesa-assert:`).
- **SYSTEM :** 7 blocs réécrits, en 69 s.
- **BOOT :** `out/b1l-metal-0089/boot.img`, relecture `1796d20cac386375be1ab1bd3bc29381f49daf53b7304e9c68cd56e0eb26fc37` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0088/boot.img` (interface sur le GPU), ou `out/b1l-metal-0079/boot.img` (rendu CPU).

## Résultat (`out/logs/0089-boot1.txt`)

- **Trois assertions de Mesa, dans le vrai chemin du pilote :**
  1. `T_size == 32 || (b->shader->arch >= 9 && T_size == 16)` dans `bi_emit_store_vary` (shader `std_vert0_lph` du test M5). **Sur Bifrost v7, un vertex shader n'écrit ses varyings qu'en 32 bits** ; le 16 bits n'existe qu'à partir de Valhall (v9). OpenGL ES produit toujours du 32 bits, mais les shaders `_lph` de QuartzCore écrivent du `half`.
  2. et 3. `0` dans `bi_emit_intrinsic` (intrinsèque non gérée), avec le pipeline `A2Xghfc`/`VfxXgh`, dans deux instances de backboardd. Le programme continue après l'`unreachable`, d'où un abort (`B2L CRASH: backboardd`, exception 6), puis le retour au CPU au 3ᵉ démarrage, ce que le propriétaire a vu.
- **Sur le Mac**, `b2l-bicomp` donne le nom de l'intrinsèque de `VfxXgh` : **`load_raw_vertex_offset_pan`**. Mesa la crée à partir de `load_vertex_id` (`bifrost_preprocess_nir`, v7 et moins). La passe des valeurs système de panfrost en redirige les utilisations, mais **laisse l'instruction** en comptant sur l'élimination du code mort. Elle fait pareil avec `load_ssbo_address`, pour laquelle le compilateur émet `LEA_BUFFER`, une instruction de **Valhall** que le G76 ne sait pas décoder (assertion `arch >= 9`, retirée en release).

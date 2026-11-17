# Build B1L-0087 : modules de backboardd par le journal, et test des shaders 16 bits

Date : 2026-11-07. Base : B1L-0086.

## Changements

1. **`libhakiosair` (`apply.py`, B1L-0086/0087)** : dans backboardd, chaque module de pipeline (fragment `f-…` et vertex `v-…`) passe par le journal noyau :
   - lignes `B2LMOD <nom> <i>/<n> <fnv32> <base64>`, en morceaux de 192 caractères ;
   - une somme de contrôle par morceau, et chaque module envoyé deux fois, car le journal peut mélanger des lignes écrites en même temps ;
   - 40 modules de 400 Ko au plus ;
   - `scripts/extract-b2lmod.py` reconstruit les modules sur le Mac.
2. **Test M5** (`b2l_metal_test.m`) : le dessin sur IOSurface est fait avec `std_vert0_lpf` + `attachment_clear_frag_lpf`, puis avec les variantes **`_lph`** (demi-précision, 16 bits), qu'aucun de nos tests n'avait exercées. Une faute 0x51 juste après la ligne `M5 draw (lph)` accuserait le 16 bits.
3. **Outils Mac :** compilateur Bifrost en mode débogage (assertions actives) compilé dans `build-mesa/build-host-pan` (`libpanfrost_bifrost.a`). `src/panfrost/meson.build` ne prend plus le sous-dossier `tools` sans pilote panfrost.

Ensemble `b2l-metal-v43`, plug-in `B2LMetalv43`.

## Installé

- **SYSTEM :** 7 blocs réécrits, en 153 s.
- **BOOT :** `out/b1l-metal-0087/boot.img`, relecture `7ea2b9181f19d40527a496eec016a844b2688e3fd88f4596d0bcaf024381b90f` (BOOT-OK).

## Résultat (`out/logs/0087-boot1.txt`)

- **Écran :** le logo Apple sur fond vert, puis une panique `Halt/Restart Timed Out @IOPlatformExpert.cpp:884` ([b1l-0087-panique.jpg](b1l-0087-panique.jpg)).
- **backboardd ne plante plus :** un seul démarrage, 4 images envoyées à l'écran, et 3 fautes 0x51.
- **SpringBoard plante 4 fois** (signal 11, `EXC_BAD_ACCESS`), puis launchd redémarre le système (« rebooting due to critical process crashes: SpringBoard »). L'arrêt dépasse le délai, d'où la panique.
  - **Pile :** `-[_MTLDevice dealloc]` ← `-[HakiosMetalDevice dealloc]` (`libhakiosmetal` +0x48ac) ← notre plug-in.
  - Quand SpringBoard cherche un GPU, `B2LMetalUIDevice` refusait avec `[self release]` sur un objet jamais initialisé, et le `dealloc` de Metal plantait sur ses champs vides.
  - Ça arrivait aussi en 0085 et 0086, à 4 plantages par boot.
- **Modules :** 19 sur 20 reconstruits par `scripts/extract-b2lmod.py`, dans `out/qc-check/mods-0087`. Seul `A2Xghfc` est incomplet (38 morceaux sur 623 abîmés dans les deux copies).

## Diagnostic sur le Mac : `b2l-bicomp`

- **L'outil** (`userspace/mesa/b2l-bicomp.c`, lié à `build-mesa/build-host-pan`, Mesa en mode débogage, assertions actives) fait suivre à un module le chemin du téléphone : traducteur AIR, abaissement des entrées et sorties, `bifrost_preprocess_nir`, `bifrost_compile_shader_nir` pour le G76 (`0x7211`), puis désassemblage.
- **Résultat sur les 19 modules :** `A2Xhf` et `TimgA2Xhf_Isrc`, les deux pipelines qui précèdent chaque faute, ainsi que les vertex `_lph`, s'arrêtent sur `src_bit_size == bit_size` dans **`pan_lower_noperspective`** (Mesa panfrost).
  - Cette passe multiplie une varying « no_perspective » par `w`, qui est en 32 bits, quelle que soit la taille de la varying.
  - Les varyings `half` des shaders `_lph` donnent donc un `fmul` 16 × 32 bits. NIR ne le vérifie pas en release, et le Mali s'arrête sur l'instruction produite (0x51).
  - Metal marque ses varyings `no_perspective` en permanence, alors qu'OpenGL ES utilise rarement ce cas.
- **Autres assertions, probablement propres à l'outil :** `deref` non abaissé dans `brim_*`, intrinsèques de valeurs système dans les vertex. Le chemin Gallium du téléphone fait ces abaissements.

# Build B1L-0232 : format de pixels RGB10A2Unorm (90)

Date : 2026-11-14. Base : B1L-0231. Ensemble `b2l-metal-v103` (noyau reconstruit), Mesa recompilé.

## Changement (`apply.py`, bloc B1L-0232)

- `air_metal.c`, table des formats : `90 MTLPixelFormatRGB10A2Unorm` → `PIPE_FORMAT_R10G10B10A2_UNORM`.
- **Source de l'ordre des bits (pas une supposition) :** MoltenVK (Apache-2.0,
  `MoltenVK/GPUObjects/MVKPixelFormats.mm`) associe `VK_FORMAT_A2B10G10R10_UNORM_PACK32` à `RGB10A2Unorm`. En Vulkan, ce
  format met le rouge sur les bits 0–9 et l'alpha sur les bits 30–31, comme `R10G10B10A2_UNORM` de Gallium (premier
  canal sur les bits de poids faible). panfrost sait l'échantillonner et dessiner dedans (`pan_format.c`, `RGB10_A2_UNORM`).
- Les formats 91 à 94 restent refusés.

## Installé

- **Vérifié :** `air-b2l: MTLPixelFormatRGB10A2Unorm (B1L-0232)` dans `out/build-mesa-0232.log` ; le nom dans `libhakiosair.dylib` de v103.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `3d6ee122631ade22bf8894a9d009dcddfb483e0f02e9ae36b9904b6227904a20` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire + journal `out/logs/0232-boot.txt`)

- **Aucune différence visible** (propriétaire).
- Le format 90 n'est plus refusé. Cela fait apparaître la couche suivante : **38 pipelines de SpringBoard refusés**,
  tous des « effets personnalisés » de RenderBox :
  - `custom_effect_vertex` + `ffgea5cia6dhmha4` (19 refus) ;
  - `primitive_custom_vertex` + `fi2a2ga3dna6dhmha4` (14 refus) ;
  - `primitive_custom_vertex` + `fiea2ga3lcba5dhmha4` (5 refus).
  Raison : « a call of custom_fn.MTL_VISIBLE_FN_REF, which the module only declares ». Modules dans
  `out/b2lmod-0232/`.
- Le module porte `air.visible_function_reference custom_fn` : la fonction est fournie à part, au moment de créer le
  pipeline. Les chaînes de RenderBox (`dyld_shared_cache_arm64e.07`) montrent comment il la fabrique :
  - par **assemblage de fonctions** (`newLibraryWithStitchedDescriptor:`, « Function stitching failed », nœuds
    `__RB::C0…`, `__RB::Llayer`…) ;
  - avec peut-être, en plus, du **source MSL compilé à l'exécution** (`#include <metal_stdlib>` / `namespace RB { struct Layer`).
- Le pilote ne gère ni les fonctions liées (`fragmentLinkedFunctions`) ni l'assemblage de fonctions.

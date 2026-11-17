# Build B1L-0083 : M6b, quatrième essai (`get_read_sampler`, pointeurs à décalage calculé)

Date : 2026-11-07. Base : B1L-0082.

## Changements (`userspace/mesa/air-b2l/apply.py`, patch B1L-0083)

1. **`air.get_read_sampler`** (`air_intrinsics.c`, dans `air_translate_call`) : la fonction ajoute au shader, une seule fois, son propre sampler inline, `b2l_read_sampler`. Ses réglages : clamp to edge sur s, t et r, filtrage nearest, sans mipmaps, `coord::pixel`, LOD de 0 à 65504. La fonction rend un pointeur `AIR_PTR_SAMPLER` vers ce sampler. C'est la lecture d'un texel exact, ce que fait `texture.read()`.
2. **Appel avec un pointeur à décalage calculé** (`air_to_nir.c`, `translate_callee` et `air_call_in_module`) :
   - le décalage devient un paramètre de 32 bits de la fonction appelée ;
   - la classe et le binding du pointeur restent ceux résolus par l'appelant, comme pour un argument texture ;
   - l'appel passe la valeur de l'appelant ;
   - `nir_inline_functions` remet ensuite cette valeur à la place du paramètre.
   - La fonction appelée est spécialisée pour cet appel et n'est pas mise en cache.

Ensemble `b2l-metal-v38` remplacé par `b2l-metal-v39`, plug-in `B2LMetalv39` ; Mesa recompilé (seul le traducteur change).

## Installé

- **Chaîne complète par `b1l-build.sh` :** Mesa (avec `air-b2l: air.get_read_sampler; pointers at a computed offset passed to callees`), bibliothèques, noyau, image, SYSTEM et BOOT.
- **SYSTEM :** 5 blocs réécrits, en 75 s.
- **BOOT :** `out/b1l-metal-0083/boot.img`, relecture `667782d6a16ca5280e33cdbfd555b912daaf45391ea32dfd84f55eba06e23448` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Résultat (`out/logs/0083-boot1.txt`)

- **Les deux refus de 0082 ont disparu.**
- **Nouveaux refus dans backboardd** (raison 13, pids 62 et 138, puis retour au CPU) :
  - `insertelement at a run-time index` ;
  - `a pointer where a number belongs`. Le journal ne dit pas quel shader est en cause, d'où l'outil de vérification sur le Mac de B1L-0084.

# Build B1L-0227 : phi de structures et `return` abaissé avant l'incorporation (`fa16dhmha4`)

Date : 2026-11-14. Base : B1L-0226. Ensemble `b2l-metal-v98`, Mesa recompilé, noyau reconstruit (noms seulement).

## Changement (`apply.py`, bloc B1L-0227)

1. La moitié « structures » de 0188 : un phi de structure (`{ <4 x half>, i8 }`) devient un registre par champ.
2. **`nir_lower_returns_impl` avant `nir_inline_functions`**, seulement sur les fonctions qui contiennent un `return`.
   C'est l'ordre que Mesa exige (`nir_inline_function_impl` le vérifie par une assertion, absente du Mesa du téléphone).
   - Sans cela, le `return` d'une fonction appelée devenait un `return` du shader entier : c'était le plantage de 0188
     (`bi_emit_jump`) ; 0190 l'abaissait après l'incorporation, donc le shader sortait trop tôt.
   - `fa16dhmha4` a aussi un `return` dans sa fonction principale : il est abaissé de la même façon.
   - Les fonctions sans `return` ne sont pas touchées, car la passe remanie même une fonction qui n'en a pas.

## Vérifié sur le Mac (modules du téléphone `out/b2lmod-0225`, `b2l-bicomp`, Mesa avec assertions)

- Les 6 modules compilent jusqu'au code Mali : `fa16dhmha4` 93 552 octets et 64 registres, `uber_vertex` 75 520 octets.
- **QuartzCore (backboardd) inchangé** : NIR des 189 fonctions identique octet pour octet à celui de 0226.
- Point de vigilance : `fa16dhmha4` met 8,9 s à compiler sur le Mac (Mesa en mode débogage, donc lent). Sur le
  téléphone, la première compilation peut prendre plusieurs secondes.

## Installé

- **Vérifié :** `air-b2l: phis of structs translated, early returns lowered before inlining (B1L-0227)` dans `out/build-mesa-0227.log`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `5a9ea8f06b411777d6aa8ade360fa2def808721efba182badd166ebedb76ccbc` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0227-boot.txt`)

- Propriétaire : **exactement pareil** à l'écran. Pas de plantage, backboardd démarré une fois.
- **Plus aucun refus de pipeline** : SpringBoard compile et dessine `fa16dhmha4` et tous les shaders de RenderBox
  (`stroke_lines`, `primitive_image`, `plane_image_stretch`, `spill`, tracés…).
- **Nouveau : 34 dessins de SpringBoard refusés par la bibliothèque** : `the draw failed: fa16dhmha4 reads texture 0 and
  0 textures were bound` (aussi texture 1 avec 1 ou 4 liées). `fa16dhmha4` déclare 4 textures (remplissage, forme,
  effet, bruit) et n'en lit que certaines selon ses données. Metal permet de laisser les autres vides ; notre
  bibliothèque refuse tout le dessin. C'est donc ce dessin qui manquait, pas un problème « après le dessin ».
- Suite : B1L-0228.

# Build B1L-0226 : phi de pointeurs dans un même tampon (pipelines de tracé de RenderBox)

Date : 2026-11-14. Base : B1L-0225. Ensemble `b2l-metal-v97`, Mesa recompilé, noyau reconstruit (noms seulement).

## Changement (`apply.py`, bloc B1L-0226)

- La moitié « pointeurs » de 0188, dans sa version révisée en 0190 : un phi de pointeurs qui parcourent **un même
  tampon** (recherche dichotomique dans les segments d'un tracé) garde le tampon fixe et met le décalage dans un registre.
- Rien d'autre : pas de phi de structures, pas de `return` (0227).

## Vérifié sur le Mac

- Modules du téléphone (`out/b2lmod-0225`) : `vkba6pa7dhmha4` (4 960 octets de code Mali), `vjba2ca3la7dhmha4` (4 432),
  `path_distance_fragment`, `path_exterior_fragment` et `uber_vertex` compilent avec assertions. `fa16dhmha4` reste
  refusé (phi de structure), comme attendu.
- **QuartzCore (backboardd) inchangé** : le NIR des 189 fonctions est identique octet pour octet avant et après
  (66 Mo comparés). Le clignotement de 0189 ne peut donc pas revenir par ce chemin.

## Ce qu'on attend

- Les deux pipelines de tracé de SpringBoard et d'iconservicesagent ne sont plus refusés (journal).
- À l'écran : peut-être les traits du cadran de l'Horloge (tracés), à confirmer par photo.

## Installé

- **Vérifié :** `air-b2l: phis of pointers into one buffer translated (B1L-0226)` dans `out/build-mesa-0226.log` ; le nouveau message de refus dans `build-metal/b2l-metal-v97/libhakiosair.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `e961d6f17469322f58eb1da11a8cc1fc130e22397cc793a84eb22c7a57ff25e3` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour à 0225 :** `bash ~/Developer/s10-port/b1l-tools/b1l-build.sh NNNN b2l-metal-vNN --mesa` après retrait du bloc B1L-0226 d'`apply.py`.

## Résultat (journal `out/logs/0226-boot.txt`, photos `b1l-0226-accueil.jpg`, `b1l-0226-centre-controle.jpg`)

- iOS démarre, backboardd une seule fois, aucun plantage.
- **Les deux pipelines de tracé ne sont plus refusés** : SpringBoard et iconservicesagent dessinent `path_distance_fragment`,
  `path_exterior_fragment` et `path_interior_fragment` (cibles 120×120, 256×128, 416×224).
- Seul refus restant : `fa16dhmha4` + `uber_vertex` (phi de structure), 61 fois SpringBoard, 60 fois iconservicesagent.
- À l'écran : **inchangé**. Horloge sans traits, Calendrier vide, plusieurs glyphes du Centre de contrôle absents
  (cercles du bas, Focus, volume). Pas de clignotement signalé.
- Suite : 0227 (phi de structures et `return`).

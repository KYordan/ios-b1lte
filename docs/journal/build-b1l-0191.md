# Build B1L-0191 : retour à l'état de B1L-0187 (dernier build sans problème signalé)

Date : 2026-11-12. Ensemble `b2l-metal-v95`, Mesa recompilé. Demandé par le propriétaire.

## Pourquoi

- 0188 à 0190 ont fait passer les shaders de RenderBox, mais sans amélioration visible, avec un **clignotement en mouvement** (0189) et des **apps lentes à ouvrir** (0190).
- 0187 est le dernier build sans problème signalé : il démarre, la teinte violette est corrigée, pas de clignotement.

## Changement

- `apply.py` : les blocs B1L-0188, 0189 et 0190 sont retirés. Ils sont gardés dans `userspace/mesa/air-b2l/renderbox-0188-0190.py.txt` (à recoller à la fin de `apply.py` pour reprendre).
- `bifrost_compile.c` : la garde 0189 de `bi_emit_jump` est retirée de la copie de travail.
- Le reste est celui de 0187 :
  - mémoire GPU recyclée remise à zéro (0185) ;
  - GPU donné à backboardd, SpringBoard, chronod et iconservicesagent (0185) ;
  - `newBufferWithBytesNoCopy:` par copie hors backboardd (0186/0187) ;
  - journal du pilote pour les autres processus (0187).

## Installé

- **Vérifié :** aucune ligne 0188/0189 dans `out/build-mesa-0191.log` ; les refus d'origine (`which NIR has no register for`, `comes in over a loop edge`) sont de retour dans `libhakiosair.dylib` (v95) ; `chronod` dans le plug-in.
- **SYSTEM :** fait et relu. **BOOT :** relecture `dbb192a5db9b95ee4cfc20d57e7b6fdd2bf79772afbcf86b29d22502147752a7` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat

- En attente de la confirmation du propriétaire.

## Ce qu'on a appris (pour la suite)

- Icônes Horloge et Calendrier, glyphes du Centre de contrôle et widgets sont dessinés par RenderBox (SwiftUI) dans SpringBoard et iconservicesagent.
- Avec les correctifs 0188–0190, tous leurs shaders compilent et dessinent, mais rien n'apparaît de plus à l'écran : le blocage suivant est donc **après** le dessin (comment la surface dessinée arrive à backboardd), pas dans les shaders.
- Le clignotement et la lenteur viennent avec ce dessin réussi. Cause non prouvée.
- Reste aussi : `a texture over a buffer was asked for` (refusé), chronod ne dessine rien.

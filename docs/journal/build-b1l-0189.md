# Build B1L-0189 : un saut que Bifrost ne sait pas émettre devient un refus nommé, plus un plantage

Date : 2026-11-12. Base : B1L-0188. Ensemble `b2l-metal-v93`, Mesa recompilé. Garde et diagnostic.

## Diagnostic (journal `out/logs/0188-boot.txt`)

- SpringBoard plantait dans `bi_emit_jump` (Bifrost) sur une cible de saut nulle, dans un shader de RenderBox débloqué par 0188.
- Le journal ne dit pas lequel, ni quel saut.

## Changement (`apply.py` B1L-0189)

1. **`air_metal.c`, `make_stage_cso` :** avant de donner le NIR au pilote, on cherche un saut que Bifrost ne sait pas émettre (`return`, `halt`, `goto`, `break`/`continue` hors boucle) ou plus d'une fonction restante. Le pipeline est alors refusé avec `B2L: the <étage> stage <nom> has <saut> (N functions), which the Mali's compiler cannot emit`.
2. **`bifrost_compile.c`, `bi_emit_jump` :** si la cible est quand même nulle (saut créé par une passe de panfrost), le saut est retiré au lieu d'être lu à travers un pointeur nul. Le shader serait faux, mais le processus ne plante pas.

## Installé

- **Vérifié :** `air-b2l: jumps the Mali cannot emit refused` dans `out/build-mesa-0189.log` ; le message dans `build-metal/b2l-metal-v93/libhakiosair.dylib`.
- **SYSTEM :** fait et relu. **BOOT :** relecture `70420cf15ff26bc603db5d58807d8719d96bcf0d354e985f0b5a8466fa569414` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0189-boot.txt`, aucun plantage)

- **iOS démarre.** Propriétaire : « horrible », **l'interface clignote dès qu'il y a du mouvement**, et aucune amélioration visible.
- La garde nomme le shader fautif : `the fragment stage fa16dhmha4 has a return in the middle (1 functions)`, 5 fois chez SpringBoard et 5 fois chez iconservicesagent. C'est le seul refus restant.
- Tous les autres shaders de RenderBox compilent et dessinent maintenant (`path_distance_fragment`, `path_interior_fragment`, `path_exterior_fragment` et une vingtaine de spécialisations), vers 3 attachements de 120×120 et 256×128.
- backboardd : aucun refus, aucun dessin refusé. GPU occupé à 8 %.
- **Cause du clignotement non prouvée.** Deux pistes :
  1. 0188 changeait aussi le `select` entre pointeurs pour 6 fonctions de QuartzCore (sur 189) ;
  2. SpringBoard dessine maintenant beaucoup dans ses surfaces, et backboardd pourrait les lire en cours de dessin.
- Suite : B1L-0190.

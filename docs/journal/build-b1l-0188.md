# Build B1L-0188 : le traducteur accepte les phi de structures et de pointeurs dans un même tampon (shaders de RenderBox)

Date : 2026-11-12. Base : B1L-0187. Ensemble `b2l-metal-v92`, Mesa recompilé.

## Diagnostic (journal `out/logs/0187-boot.txt`, bibliothèque `out/rb-check/default.metallib`)

- RenderBox (moteur de dessin de SwiftUI : icônes, glyphes du Centre de contrôle, widgets) voyait ses pipelines refusés par le traducteur AIR → NIR :
  - `a phi of a struct` : `phi { <4 x half>, i8 }` (un échantillon de texture et son drapeau), dans `filter_blur_fragment`, `resolve_sample_fragment`, `uber_fragment` ;
  - `a phi of pointers … over a loop edge` : une recherche par dichotomie dans un tampon de segments de tracé (`uber_vertex`, `path_*_vertex`, dégradés). Le pointeur avance dans **un seul** tampon ; seul son décalage change. La boucle contient aussi un `select` entre deux pointeurs du même tampon.

## Changement (`apply.py` B1L-0188, `air_to_nir.c`)

1. **Phi d'agrégat :** un arbre de registres, un par feuille (`b2l_phi_agg_regs`, `_load`, `_store`).
2. **Phi de pointeurs dans une même mémoire** (UBO, SSBO, partagée, locale, données constantes) : la ressource est connue à la traduction, le décalage en octets va dans un registre de 32 bits. Une ressource différente sur une arête reste refusée.
3. **`select` entre deux endroits d'un même tampon :** un seul pointeur, dont le décalage est un `bcsel`.

## Vérifié sur le Mac

- `air2nir out/rb-check/default.metallib uber_vertex` : se traduit (429 blocs), refusé avant.
- `air2nir --report` : 17 fonctions traduites ; les autres échecs viennent du rapport hors ligne (constantes de spécialisation absentes), pas du téléphone.
- **Non testé hors téléphone :** le phi de structure (ses trois fonctions demandent des valeurs de spécialisation que l'outil n'a pas).

## Installé

- **Vérifié :** `air-b2l: phis of structs and of pointers into one buffer translated` dans `out/build-mesa-0188.log`.
- **SYSTEM :** fait et relu. **BOOT :** relecture `7f720a6688cae9d6b3020dbb6fe18b24af54d0f880067658dc4c433b8d513de7` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0188-boot.txt`)

- **Bloqué sur la pomme : SpringBoard plante en boucle** (pid 28, 342, 352 ; `EXC_BAD_ACCESS` à l'adresse 0x48), à son premier dessin RenderBox.
- Pile dans `libhakiosair` : `emit_block` ← `emit_cf_list` (récursif), compilateur **Bifrost**. C'est `bi_emit_jump` : il ne connaît que `break` et `continue`, avec pour cible les blocs de la boucle en cours. La cible lue est nulle : le shader contient un saut hors boucle, ou un `return`/`halt`.
- Aucun `this render pipeline is refused` avant le plantage : les shaders débloqués par 0188 passent donc le traducteur.
- Sur le Mac, le NIR de `uber_vertex` est sain (4 boucles, 4 `break` dedans). Le shader fautif n'est pas identifié : le journal ne le nomme pas.
- Suite : B1L-0189.

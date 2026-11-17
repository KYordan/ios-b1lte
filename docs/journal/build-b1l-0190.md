# Build B1L-0190 : `return` au milieu d'un shader abaissé ; le `select` de pointeurs redevient celui d'avant pour QuartzCore

Date : 2026-11-12. Base : B1L-0189. Ensemble `b2l-metal-v94`, Mesa recompilé.
Deux changements regroupés (le propriétaire demande de faire passer tous les shaders refusés).

## Diagnostic (journal `out/logs/0189-boot.txt`)

- Dernier shader refusé : `fa16dhmha4` (RenderBox), `a return in the middle`.
- L'interface clignote en mouvement depuis 0188/0189. 0188 avait changé la traduction de tout `select` entre deux pointeurs d'un même tampon, ce qui touchait 6 fonctions de QuartzCore.

## Changement (`apply.py`)

1. **B1L-0190, `air_metal.c` :** `nir_lower_returns` avant la garde des sauts : un `return` au milieu devient du flot structuré que Bifrost sait émettre.
2. **B1L-0188 révisé, `air_to_nir.c` :** le `select` de pointeurs reste traduit comme avant 0188 (`AIR_VAL_PTR_SELECT`). Le décalage n'est choisi par `bcsel` que là où un phi de pointeurs en a besoin (`b2l_ptr_offset`, `b2l_ptr_leaf`). Les shaders de QuartzCore retrouvent donc leur traduction de 0187.

## Vérifié sur le Mac

- `uber_vertex` de RenderBox se traduit toujours ; QuartzCore : 173 fonctions traduites sur 189.

## Installé

- **Vérifié :** les deux lignes `air-b2l:` dans `out/build-mesa-0190.log`.
- **SYSTEM :** fait et relu. **BOOT :** relecture `dc6c458f3b4f60b4abebe22fe2cf3ce33da22fbe53127b36050a2f30989cf6aa` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat

- Propriétaire : **aucune amélioration, et les apps sont lentes à ouvrir.** Il demande de revenir au dernier build sans problème.
- Journal non récupéré.
- Suite : B1L-0191 (retour à l'état de 0187).

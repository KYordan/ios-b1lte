# Build B1L-0120 : retrait de B1L-0118 (régression graphique)

Date : 2026-11-08. Base : B1L-0119. Ensemble `b2l-metal-v71`, Mesa recompilé.

## Pourquoi

0119 (qui contient 0116 à 0118) est lent et cassé à l'écran : 544 fautes MMU du GPU à l'adresse 0 et 265 jobs inachevés, contre 14 et 1 en 0115 (voir `build-b1l-0119.md`).

## Changement

- `apply.py` : le patch B1L-0118 (états de shader faits une fois par shader) est **retiré**. Il est gardé dans `userspace/mesa/air-b2l/apply.py.with-0118`, pour y revenir après vérification.
- Restent : 0115 (constantes bornées), 0116 (fond d'écran), 0117 (`b2l_diag` lu une fois), 0119 (état mémoire au refus).

## Installé

- **Vérifié :** plus de `B2L CSO` dans `libhakiosair.dylib` (v71), `B2L UBO` présent.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `b53c55ae997178c890e87ff7de2f7ab8011504f11274660851f8d2671bebfb11` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0115/boot.img` avec SYSTEM v69.

## Lecture du résultat

- Retour à l'état de 0115 sans les refus d'allocation → 0118 était en cause par lui-même.
- Retour à 0115 avec les refus de nouveau → 0118 n'agissait qu'en libérant de la mémoire ; les passes du verre sont fautives et seront corrigées à part.
- Toujours cassé → chercher dans 0116, 0117 ou 0119.

## Résultat (journal `out/logs/0120-boot.txt`)

- **Propriétaire : comme 0115, plus le fond d'écran.** Reste le fond « bizarre » du Liquid Glass (Centre de contrôle).
- Journal : 0 faute GPU, 0 job inachevé : **0118 était bien la cause de la régression** (il partageait des états de shader entre pipelines ; à revoir avant tout retour).
- Les refus sont revenus : 284 tampons GPU de 36 Mo, 139 textures (RGBA16Float 1152×2496 à 1472×3072, BGRA8 1472×3072). Diagnostic de 0119 au refus : **1,2 à 1,4 Go libres**, 700 à 880 Mo de tampons GPU vivants, 34 à 51 Mo d'IOSurfaces. Ce n'est pas la RAM qui manque, mais la plage d'adresses noyau `kmem_data_shared` (1 930 Mo) d'où viennent tous ces tampons → B1L-0121.

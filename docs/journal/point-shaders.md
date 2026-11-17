# Point sur les shaders (2026-11-12)

État au build **B1L-0191** (retour à l'état de 0187, demandé par le propriétaire). Chaîne : AIR d'Apple → traducteur
`src/compiler/air` (patchs dans `userspace/mesa/air-b2l/apply.py`) → NIR → compilateur Bifrost (panfrost) → Mali-G76.

## En bref

| Bibliothèque | Qui l'utilise | Traduits | Refusés |
|---|---|---|---|
| QuartzCore (`default.metallib`, 189 fonctions) | backboardd : toute l'interface, Liquid Glass | **173** | **16** (imageblocks) |
| RenderBox (`default.metallib`, 82 fonctions) | SwiftUI dans SpringBoard, iconservicesagent, chronod : icônes Horloge/Calendrier, glyphes du Centre de contrôle, widgets | 17 sur le Mac (voir plus bas) | sur le téléphone : 2 sortes de refus |
| RenderBox `archive.metallib` | idem | — | pas du bitcode LLVM : archive de pipelines précompilés d'Apple, non lisible |

## 1. Shaders refusés sur le téléphone (journal 0187)

C'est la liste qui compte : ce sont les pipelines que les apps demandent vraiment.

| Refus (message du pilote) | Processus | Fois | Shaders concernés |
|---|---|---|---|
| `a phi of pointers whose operand comes in over a loop edge` | SpringBoard 5, iconservicesagent 2 | 7 | `uber_vertex`, `path_*_vertex`, dégradés : recherche par dichotomie dans un tampon de segments de tracé |
| `a phi of a struct, which NIR has no register for` | SpringBoard 4, iconservicesagent 3 | 7 | `filter_blur_fragment`, `resolve_sample_fragment`, `uber_fragment` (`phi { <4 x half>, i8 }`) |
| `a return in the middle` (vu en 0189 seulement, une fois les deux refus ci-dessus levés) | SpringBoard 5, iconservicesagent 5 | 10 | `fa16dhmha4` (spécialisation de RenderBox) |
| `a texture over a buffer was asked for` (pas un shader : une texture posée sur un tampon) | SpringBoard 1, iconservicesagent 1 | 2 | `newTextureWithDescriptor:offset:bytesPerRow:` |

backboardd (QuartzCore) : **aucun refus** sur le téléphone. Les 16 noyaux à imageblocks ne sont jamais demandés.

## 2. Ce qu'on a déjà essayé (0188 à 0190, retirés en 0191)

- **0188** : phi de structures (un registre par champ) et phi de pointeurs dans un même tampon (le décalage dans un
  registre). `uber_vertex` se traduit. Résultat : SpringBoard plante dans Bifrost (`bi_emit_jump`, cible de saut nulle).
- **0189** : garde contre les sauts que Bifrost ne sait pas émettre. Résultat : démarre, seul `fa16dhmha4` reste refusé,
  mais **l'interface clignote en mouvement**.
- **0190** : `nir_lower_returns` (le `return` au milieu passe) ; le `select` de pointeurs de QuartzCore remis comme avant.
  Résultat : **aucune amélioration, apps lentes**.
- **Appris :** avec ces correctifs, **tous** les shaders de RenderBox compilent et dessinent, mais rien n'apparaît de plus
  à l'écran. Le blocage des icônes, glyphes et widgets est donc **après le dessin** : le trajet de la surface dessinée par
  SpringBoard/iconservicesagent jusqu'à backboardd. Les correctifs sont gardés dans
  `userspace/mesa/air-b2l/renderbox-0188-0190.py.txt` (à recoller à la fin de `apply.py` pour reprendre).

## 3. Shaders qui ne se traduisent pas hors téléphone (rapport `air2nir`)

Commande (sur le Mac, quelques secondes, sans téléphone) :

```bash
~/Developer/s10-port/build-mesa/build-host/src/compiler/air/air2nir --report ~/Developer/s10-port/beyond2lte-ios/out/rb-check/default.metallib
```

Attention : l'outil du Mac date du 12/11 à 01:32, donc il n'est peut-être pas à l'état exact de 0191 (il traduit
`uber_vertex` avec une note sur le `select` de pointeurs). Le recompiler avant de s'y fier.

**QuartzCore (`out/qc-check/QuartzCore-air64.metallib`) : 173 traduits, 16 refusés**, tous pour `input air.imageblock`
(mémoire de tuile des GPU Apple, sans équivalent direct sur Mali). Jamais demandés par backboardd jusqu'ici.

**RenderBox (`out/rb-check/default.metallib`) : 17 traduits, 65 refusés.** Les refus hors téléphone viennent surtout de
l'outil, qui n'a pas les valeurs de spécialisation que l'app fournit au moment de créer le pipeline :

| Cause | Fonctions | Vrai problème sur le téléphone ? |
|---|---|---|
| `a function constant of 4 components of 32 bits` | 23 (`stroke_*`, `filter_blur_fragment`, `spill_fragment`, `mask_*`, `uber_resolve_fragment`…) | Non vu sur le téléphone : limite de l'outil hors ligne |
| `stage: air.visible, a library function (not yet)` | 22 (`distanceGradient_v1`, `ovalizeGradient_v1`, `displacementMap_v1`…) | **Oui, à faire** : fonctions « visibles » appelées par `uber_fragment` via une table de fonctions (`air.get_function_pointer_visible_function_table`), non prises en charge |
| `an air.render_target input without a target` | 19 (`filter_custom_fragment`, `resolve_*_fragment`, `mask_color/gradient/image_fragment`, `alpha_effect_fragment`, `custom_effect_fragment`…) | Non vu sur le téléphone : la cible vient du pipeline |
| `input air.function_constant` | 1 (`uber_fragment`) | Limite de l'outil hors ligne |

## 4. Ce qui manque encore, par ordre d'utilité

1. **Le trajet de la surface vers backboardd** (pas un shader) : c'est ce qui empêche icônes, glyphes et widgets
   d'apparaître même quand tout compile. Prouver où l'image se perd avant de toucher aux shaders.
2. Reprendre les correctifs 0188–0190 **un par un**, en cherchant la cause du clignotement (0189) et de la lenteur (0190).
3. Fonctions visibles (`air.visible`) et tables de fonctions, pour `uber_fragment`.
4. Texture posée sur un tampon (`newTextureWithDescriptor:offset:bytesPerRow:`).
5. chronod reçoit un GPU mais ne dessine rien (widgets).
6. Imageblocks de QuartzCore (16 noyaux) : seulement si un journal montre qu'on les demande.
7. Archive de pipelines précompilés (`archive.metallib`) : la refuser proprement, sans plantage.

Sources : `docs/journal/build-b1l-0185.md` à `0191.md`, `docs/metal-progress.md`, journaux `out/logs/0186-boot.txt` à `0189-boot.txt`.

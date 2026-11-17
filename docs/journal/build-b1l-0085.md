# Build B1L-0085 : relevé des function constants, profondeur sans cible, taille des pipelines

Date : 2026-11-07. Base : B1L-0084 (premier rendu de backboardd sur le Mali, interrompu par une instruction invalide).

## Changements

1. **Relevé dans le pilote** (`b2l_describe_function`, `hakiosmetal_air.m`) : pour les 6 premiers pipelines de rendu, la classe de la fonction de fragment et ses variables internes liées aux constantes, à la spécialisation ou aux valeurs (`B2L fc: …`). But : trouver où Metal range les valeurs choisies par QuartzCore (`newFunctionWithName:constantValues:`), pour les transmettre au traducteur au build suivant.
2. **Bibliothèque :** une écriture de `[[depth]]` sans cible de profondeur est **supprimée au lieu d'être refusée**, comme le fait Metal (`b2l_drop_depth_writes`).
3. **Bibliothèque :** une ligne de journal par pipeline de rendu, avec le nom du fragment et son nombre d'instructions NIR (`air-b2l: render pipeline …`), pour voir lesquels sont énormes.

Ensemble `b2l-metal-v41`, plug-in `B2LMetalv41`, Mesa recompilé.

## Attendu

- **Le refus de profondeur disparaît.**
- **L'instruction invalide du Mali va sans doute revenir**, tant que les constantes restent à 0.
- **Le relevé dira comment les obtenir.**

## Installé

- **Vérifié dans les binaires :** le relevé (`B2L fc:`) est dans `libhakiosmetal`, et la ligne par pipeline dans `libhakiosair`.
- **SYSTEM :** 7 blocs réécrits, en 76 s.
- **BOOT :** `out/b1l-metal-0085/boot.img`, relecture `e55a157f1b56761edb3a6848a7e7a50950d9c4d3128fdf4470ef23282202df05` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Résultat (`out/logs/0085-boot1.txt`)

- **Écran :** presque le même qu'en 0084 (vert en escalier, quelques carrés de plus) : [b1l-0085-ecran.jpg](b1l-0085-ecran.jpg).
- **Les function constants arrivent bien.** QuartzCore passe des fonctions déjà spécialisées, de classe `_MTLFunctionInternal`, nommées d'après leur configuration : `A2Xghfc`, `brim_init_lph`, `downsample_4_frag_lph`, `narrow_blur_*_frag_lph`, `home_affordance_base_lph`… Elles n'ont pas de dictionnaire de constantes (`_functionConstantDictionary` = nil), mais leur module AIR est déjà spécialisé : `A2Xghfc` (le fragment principal) fait **75 592** instructions NIR, contre 116 047 pour la version non spécialisée. **Ma crainte de 0084 (« toutes les constantes à 0 ») est donc fausse pour ces modules.**
- **La faute 0x51 (`INSTR_INVALID_ENC`) revient deux fois**, juste après le pipeline `A2Xhf` (7 instructions), toujours sur le slot 0 (fragments). Les images 1 et 2 sont envoyées à l'écran.
- **Le refus de profondeur a disparu :** `TimgA2Xhf_Isrc` se compile (102 instructions).
- **Piste :** les shaders de backboardd sont des variantes `_lph` (demi-précision, 16 bits), alors que tous nos tests réussis utilisaient les variantes `_lpf` (32 bits).

# Build B1L-0084 : les shaders de QuartzCore vérifiés sur le Mac, 173 sur 189

Date : 2026-11-07. Base : B1L-0083.

## Outil : `air2nir` sur le Mac (`scripts/build-air-host.sh`)

- **Ce que c'est :** le traducteur AIR → NIR de Scion, en ligne de commande, compilé pour le Mac à partir du même arbre Mesa patché que pour le téléphone (`build-mesa/build-host`). Il utilise les compilateurs de Xcode et le SDK macOS (`SDKROOT`) : le clang de l'environnement mamba apportait un faux `endian.h`.
- **Shaders de QuartzCore :** la tranche AIR64 du `default.metallib` universel (octet 48, 0x17af00 octets), copiée dans `out/qc-check/QuartzCore-air64.metallib`.
- **Commande :** `air2nir --report out/qc-check/QuartzCore-air64.metallib`, qui donne une ligne par fonction : traduite, ou la raison du refus. Toutes les *function constants* valent 0.
- **Rapports :** `out/qc-check/report-0083.txt` (avant), `report-0084.txt` (après).

## Résultat : de 149 à 173 fonctions traduites sur 189

| Refus (nombre de fonctions) | Correctif dans `apply.py` (B1L-0084) |
| --- | --- |
| `a pointer where a number belongs` (16) | **Lectures de texture AIR 2.8** : `air.read_texture_*` prend le sampler de lecture juste après la texture, et un décalage de texel après la coordonnée ; les deux sont reconnus à leur type |
| `insertelement at a run-time index` (backboardd 0083) | `nir_vector_insert` à position calculée |
| `air.get_null_texture_2d` (fragment principal) | texture nulle à l'emplacement 127, jamais échantillonnée ; un phi entre elle et une vraie texture donne la vraie |
| `an aggregate index out of its value` (4) | `insertvalue` dans un membre pas encore construit (bourrage `[8 x i8]` rempli octet par octet) : niveaux vides créés d'après le type |
| pointeur choisi à l'exécution passé à une fonction (fragment principal) | **en quatre parties :**<br>1. un choix dont la condition se calcule à la traduction est fait tout de suite (évaluateur de constantes, y compris via les variables de l'initialiseur statique) ;<br>2. sinon, la condition et les deux décalages sont passés en paramètres ;<br>3. un échantillonnage avec un sampler choisi à l'exécution est fait avec chacun des deux samplers, puis le bon résultat est gardé (`bcsel`) ;<br>4. un choix « pointeur ou nul » passe directement le pointeur |
| `air.fmax3` / `fast_fmax3` (fragment principal) | `fmax3`, `fmin3`, `fmedian3` |
| lecture via un pointeur nul (chemin mort) | rend 0 |
| `an array where a number belongs` (4 flous + fragment principal) | argument structure ou tableau : vide, il ne prend pas de paramètre ; sinon chaque feuille numérique devient un paramètre et l'agrégat est reconstruit dans la fonction appelée |

- **Bug trouvé en route :** le tableau des paramètres d'une fonction appelée avait une place par argument, alors qu'un choix de pointeur en prend trois. Il débordait, ce qui corrompait la mémoire (assertion de NIR sur un opérande « 6 × 17 bits »). Il est maintenant dimensionné en conséquence.
- **Le fragment principal `fixed_frag_lph_cpf/cph` se traduit** : environ 116 000 instructions NIR avant spécialisation. Une fois ses *function constants* fixées par backboardd, il en restera bien moins.

## Ce qui reste (16 fonctions)

- **Les noyaux de calcul qui utilisent la mémoire de tuile des GPU d'Apple** (`air.imageblock`, `implicit_imageblock`) : `tile_simd_blur_*`, `tile_downsample_*`, `tile_average_luma*`, `variable_blur_*_compute`. Ce sont les flous et réductions des effets de verre.
- **À émuler sur le Mali**, par exemple avec la mémoire partagée des groupes de threads. Il faut aussi `simd_shuffle_and_fill_down` et `get_simdgroup_size`.

## Build

- Mesa est recompilé avec tous ces patchs.
- Ensemble `b2l-metal-v40`, plug-in `B2LMetalv40`.

## Installé

- **SYSTEM :** 6 blocs réécrits, en 90 s.
- **BOOT :** `out/b1l-metal-0084/boot.img`, relecture `21a01e1562f27df9021a05d433214c481253c618d849b654a9d95b93903579a8` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Résultat (`out/logs/0084-boot1.txt`) : backboardd dessine sur le Mali pour la première fois

- **Ce que le propriétaire a vu :**
  - un démarrage long, puis un écran vert sur le haut de la dalle, avec un bord **en escalier** et le reste noir (photo : [b1l-0084-ecran.jpg](b1l-0084-ecran.jpg)) ;
  - le retour automatique ne s'est pas déclenché pendant ce temps.
- **Ce que dit le journal :**
  - backboardd (pid 62) obtient le Mali et compile ses pipelines, dont le fragment principal ;
  - une texture IOSurface est créée, et `setResponsibleTaskIDs:count:` est appelé (la réponse de B1L-0081 sert) ;
  - **`mali: a job on slot 0 did not finish: interrupts 0x10000, status 0x51`**. Le code 0x51 est `INSTR_INVALID_ENC` : une instruction de shader invalide arrête le job de fragments en cours de route, d'où l'image écrite tuile par tuile, en escalier ;
  - `B2L FB: swap 1 layer 0 surface 2 1440x3040` : **la première image de backboardd rendue par le GPU arrive à l'écran** ;
  - puis `TimgA2Xhf_Isrc stores to [[depth]] and this pipeline has no depth attachment` : pipeline refusé, sortie raison 13, puis la même chose au 2ᵉ démarrage (pid 645).
- **Cause commune probable : les valeurs des *function constants* ne sont jamais transmises.**
  - Le pilote construit les pipelines à partir de `-bitcodeData`, le module non spécialisé ;
  - `air_module_function_constants` ne lit que la liste des constantes, et toutes valent 0 ;
  - le fragment principal reste donc énorme, puisque rien n'est éliminé (sans doute l'instruction invalide), et des chemins que QuartzCore n'a pas activés écrivent la profondeur.

# B1L-0263 — les tampons Metal deviennent des mappages persistants (icônes recassées « au bout d'un moment »)

## Cause (lue dans le code, Mesa 25.0.7)

- `rwa_buffer_create` (air_metal.c) mappe chaque tampon **une seule fois**, et le pilote Metal écrit ensuite toute sa vie par ce pointeur
  (`-contents`). Mais la ressource est créée par `pipe_buffer_create`, donc **sans** `PIPE_RESOURCE_FLAG_MAP_PERSISTENT`.
- Les copies et remplissages Metal vers un tampon (`rwa_buffer_copy` → `util_resource_copy_region`, `rwa_buffer_fill` →
  `u_default_clear_buffer`) refont un map en écriture. Dans `panfrost_ptr_map` (pan_resource.c), quand un lot en attente lit encore
  la ressource (« Shadow resource creation ») ou que l'écriture couvre tout le tampon, panfrost **remplace le BO** par un neuf.
- Le pointeur gardé désigne alors l'ancien BO. Tout ce que l'app écrit ensuite (sommets, indices, uniformes) n'atteint plus le GPU, qui
  dessine avec des données périmées : traits et triangles, seulement après un certain temps (quand une copie tombe sur un tampon occupé).
- Cohérent avec 0262 : le cache rendu par iconservicesagent est juste, la casse n'apparaît qu'au dessin en direct, sans plantage ni faute GPU.

## Changement (`userspace/mesa/air-b2l/apply.py`, section B1L-0263, sauvegarde `apply.py.before-0263`)

- Tampon créé avec `PIPE_RESOURCE_FLAG_MAP_PERSISTENT | PIPE_RESOURCE_FLAG_MAP_COHERENT` (même gabarit que `pipe_buffer_create`,
  u_inlines.h, sinon), mappé avec `PIPE_MAP_PERSISTENT | PIPE_MAP_COHERENT`. C'est le contrat Gallium des mappages persistants
  (ARB_buffer_storage) : panfrost ne remplace jamais un BO déjà mappé persistant ; il vide les lots et attend (branche « Synchronized write »).
- Vérifié sur une copie : seules ces lignes changent. Set v126 → v127. `hakiosmetal_air.m` inchangé.

## Risques

- Une copie vers un tampon encore lu par le GPU attend désormais le GPU au lieu de doubler le BO : un peu plus lent à ces endroits, mais juste.

## Installé

- SYSTEM-OK, BOOT `1802d91c8f5c102f576a5b03a2958da757e8580f54a0ad64eee0627593a5bdaf`, BOOT-OK ; `libhakiosair.dylib` reconstruite (set v127), air_metal.c porte B1L-0263.

## Résultat

- Testé (photo du propriétaire) : pas d'amélioration visible. Pictogrammes en **rayures horizontales** (FaceTime, Photos, App Store, Plans,
  Réglages, Messages, Safari, Fichiers, News) ; Calendrier vide ; Horloge dessinée avec ses aiguilles mais qui disparaît quand on bouge ;
  haut d'écran beige à bord jaune ondulé ; widgets « Waiting… » ; tout est lent avec des retours en arrière. Le Dock en verre est flouté
  correctement. Le correctif reste juste (contrat Gallium) mais n'était pas la cause principale.
- Indice dans les masques R8 de SpringBoard (dump 0262 `l-img-SpringBoard-204x204-f10-r256-n16`) : dans chaque bloc de 2×2 pixels, les
  deux pixels du haut sont **identiques**, et la ligne du bas porte des pics isolés (≈165 au lieu de ≈100). Une erreur une ligne sur deux
  donne, réduite, exactement des rayures horizontales. Les shaders RenderBox (`primitive_shape`, `alpha_effect`, `filter_distance`) utilisent
  `fwidth`/`dfdx`/`dfdy` (traduits en `nir_ddx`/`nir_ddy`) ; `discard_fragment` est déjà un `nir_demote`. Piste suivante : les dérivées.


# B1L-0269 — GPU réservé à backboardd (comme avant B1L-0185)

## Pourquoi (demande du propriétaire, « GPU : backboardd seul »)

- Avant 0185, seul backboardd recevait le Mali (log 0184 : c'est le seul à demander un appareil). Les icônes d'apps étaient alors
  dessinées sans GPU, par le code d'Apple, et elles étaient justes. L'Horloge, le Calendrier, les widgets et une partie des glyphes
  du Centre de contrôle restaient vides.
- Depuis 0185, puis 0248 (tous les processus), les icônes passent par notre traducteur et notre pilote : elles sont cassées en
  mouvement, et c'est lent (pipelines d'icônes de ~10 s, 8 processus sur le GPU, voir enquete-icones-lenteur.md).
- C'est aussi un test : si les icônes d'apps redeviennent justes, la casse vient bien de notre chemin GPU.

## Changement (`userspace/metal/b2l_metal_plugin.m`, sauvegarde `.before-0269`)

- Par défaut, seul `backboardd` reçoit le Mali.
- Boot-arg `b2l_gpu_icons=1` : liste de 0185 (backboardd, SpringBoard, chronod, iconservicesagent).
- Boot-arg `b2l_gpu_all=1` : tous les processus (comportement de 0248). Un processus qui ne peut pas lire `kern.bootargs` reste sans GPU.
- Le pilote garde 0268 (traduction hors du verrou), qui n'a pas encore été testé. Set v132 → v133.

## Attendu et limites

- Icônes d'apps justes, interface plus rapide.
- Horloge, Calendrier, widgets et une partie du Centre de contrôle vides, comme en 0184.
- L'app Test Metal de Dictaphone n'a plus de GPU : ses tests échoueront (« appareil nil »), c'est normal.

## Installé

- SYSTEM-OK, BOOT `74a1cf8e049e1fda1499ec2110855dbf06896952cb5e8aa16662c89c90763498`, BOOT-OK ; `B2LMetalv133` contient
  `b2l_gpu_icons=1` et `b2l_gpu_all=1`, `libhakiosair.dylib` v133 garde 0268.

## Résultat

- **1er flash (BOOT `74a1cf8e…`) : sans effet.** Le téléphone démarre, les icônes sont toujours cassées. Le log (`out/logs/0269-boot.txt`)
  montre que 9 processus ont reçu le GPU : le noyau publiait encore `MetalPluginName B2LMetalv131`, le plug-in et le pilote de 0267
  (même erreur de changement de set qu'en 0268). Ce premier test ne dit donc rien sur les icônes.
- Correction : `B2LMetalv131` → `B2LMetalv133` dans `B2LMetalAccel.cpp` et `b2l_userspace_seed.h`, puis 2e flash.
- 2e et 3e essais : SYSTEM refusé, image pleine (« No space left on device » ; 133 sets accumulés). Les sets `b2l-metal-v2…v125`
  (sauf v5), tous présents dans `build-metal/` sur le Mac et que le noyau n'utilise plus, ont été retirés de
  `out/live/b1l-system-live.img` (sauvegarde `b1l-system-live.before-prune.img`) : 1,2 Go libres. Gardés : v1, v5, v126–v133, gl-v1…v9.
- 4e essai : SYSTEM-OK, BOOT `276d422ecc09e4a941d2df10d085316941ac24b0d61e35d750a80f6012ebf440`, BOOT-OK. Log du démarrage
  (`out/logs/0269d-boot.txt`) : `MetalPluginName B2LMetalv133`, **seul backboardd** demande et reçoit le Mali. Aucun plantage.
  Retour visuel du propriétaire en attente.
- 0268 enfin actif (backboardd) : le temps de création des pipelines est dans la **compilation panfrost**, pas dans la traduction :
  `rwa_render_create shader compile` 59,5 s en 170 compilations, dont une de **49,2 s**. `-commit` : 40,0 s cumulées, 1,1 s au plus.
  Attente sur le verrou faible ensuite (71 à 167 ms par fenêtre de 5 s). Prochaine cible : la compilation (cache, ou hors du verrou).

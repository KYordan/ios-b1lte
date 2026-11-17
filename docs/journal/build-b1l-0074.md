# Build B1L-0074 : optimisation de M5 (un lot GPU par passe, une seule attente)

Date : 2026-11-07. Base : B1L-0073 (M5 terminé).

## Constat

- **Deux triangles prenaient 16 à 30 ms.** Chaque appel de dessin (`rwa_render_draw`), et l'effacement seul (`rwa_render_clear`), attendait la fin du GPU (`finish_or_say_why`).
- **Chaque dessin était aussi un lot panfrost séparé :**
  - de nouvelles surfaces étaient créées puis posées, et un framebuffer vide était remis à la fin ;
  - panfrost range le travail par framebuffer, donc chaque dessin donnait un job de tuilage et un job de fragments sur toute l'image, avec rechargement du contenu précédent.
- **Le test tournait avec `PAN_MESA_DEBUG=sync`**, un réglage de débogage de M4 qui ajoute une attente après chaque envoi au GPU.
- **L'attente unique existait déjà :** `-waitUntilCompleted` et les handlers « completed » appellent `rwa_finish`.

## Changements

**`userspace/mesa/air-b2l/apply.py` (B1L-0074) :**
- **Framebuffer gardé sur le device** (`b2l_fb`, avec ses propres références) tant que les cibles restent les mêmes ressources. L'effacement et tous les dessins d'une passe forment un seul lot panfrost.
- **Oubli du framebuffer :** `b2l_fb_forget` l'oublie quand une de ses textures est détruite (`rwa_texture_destroy`).
- **Pas d'attente par dessin ni par effacement.** `B2L_METAL_SYNC_DRAWS=1` remet l'ancien comportement.
- **Libération des surfaces** par `pipe_surface_reference`, et plus par `surface_destroy` : le contexte et le device les gardent.
- **Nouvelle fonction `rwa_flush(device)`** : envoie au GPU ce qui est enregistré, sans attendre.

**`scion/hakiosmetal_air.m` :** `-commit` appelle `rwa_flush` après avoir enregistré le travail. Le travail part donc dès le commit (c'est ce dont `presentDrawable` a besoin), et l'attente reste unique.

**`b2l_metal_test.m` :**
- `PAN_MESA_DEBUG=sync` retiré ;
- **nouvelle mesure `M5 bench`** : 200 dessins dans une passe (les deux triangles en alternance), une passe de chauffe puis trois chronométrées ; le test affiche le temps d'encodage et de commit, le temps jusqu'à la fin et le temps par dessin, et vérifie l'image (2005 pixels, lignes identiques au Mac).

**`scripts/build-mesa.sh`** compile `libhakiosair` par son nom (B1L-0070).

**Ensemble `b2l-metal-v30`**, et le noyau pointe vers v30.

## Installé

- **`libhakiosair` (v30)** `a08ca735…` exporte `rwa_flush` et `rwa_render_clear`. `libhakiosmetal` : `e9a84eba…`. `libb2l-metal-test` : `1b68eb0e…`.
- **SYSTEM :** 6 blocs réécrits, en 72 s.
- **BOOT :** `out/b1l-metal-0074/boot.img`, relecture `cb93432b3f35d5addce5ba7fa45142d080588083d1b378ef7c96fd7d5e4df883` (BOOT-OK).

## Résultat (`out/logs/0074-boot1.txt`)

- **Toujours juste partout :**
  - étapes 1 à 3 réussies ;
  - chaque mesure donne `2005 drawn (Mac: 2005), 0 rows differ` ;
  - `presentDrawable` en statut 4.
- **Deux triangles : 7,8 ms**, contre 16 à 30 ms avant (10,8 ms dans le drawable).
- **200 dessins dans une passe :**

| Cible | Temps total | Par dessin |
| --- | --- | --- |
| IOSurface | 240 à 396 ms | 1,2 à 2,0 ms |
| drawable | 465 à 687 ms | 2,3 à 3,4 ms |

- **Presque tout ce temps est côté CPU.** L'encodage et le commit prennent 238 à 683 ms, alors que l'attente du GPU n'en prend que 2 à 11 ms : le regroupement en un seul lot a bien fonctionné.
- **Cause du coût CPU : nos bibliothèques sont compilées en arm64e.**
  - `libhakiosair` contient 4328 `retab` (retours signés) et 3067 `blraaz` (appels signés par pointeur) ; `libhakiosmetal` contient 114 `retab`.
  - L'Exynos 9820 n'a pas PAC, donc chacune de ces instructions est un piège émulé par le noyau (environ 11 µs). Un dessin traverse des centaines de fonctions de Mesa.
  - Le système faisait environ 200 000 pièges par seconde dans cette fenêtre (`B2L LIVE`).

# B1L-0261 — reprise de primitive de Metal (pictogrammes coupés, traits)

## Cause (B1L-0260)

- RenderBox dessine ses rectangles par lots, en une bande de triangles indexée : `1 0 2 3 65535 5 4 6 …` (u16).
- Metal reprend **toujours** une bande à l'indice « tout à 1 » : 0xFFFF en UInt16, 0xFFFFFFFF en UInt32
  (documentation Apple de `-drawIndexedPrimitives:…`).
- `rwa_render_draw` (air_metal.c) dessinait avec `primitive_restart` désactivé, avec un commentaire qui affirmait que Metal n'en a pas.
  Chaque 65535 devenait donc un sommet :
  - des triangles parasites entre rectangles, d'où les **traits** (Raccourcis) ;
  - des moitiés de rectangle fausses, d'où les **pictogrammes coupés en diagonale** du cache d'icônes 0259
    (FaceTime, Calculette, Fichiers, Livres, Photos…).
- La recherche des indices min et max comptait aussi 65535 (max_index = 65535).

## Changement (`userspace/mesa/air-b2l/apply.py`, section « B1L-0261 » ; sauvegarde `apply.py.before-0261`)

- `index_range` ignore l'indice de reprise. S'il n'y a que des reprises, min = max = 0.
- `pipe_draw_info` : `primitive_restart = (index_size != 0)`, `restart_index` = 0xFFFF ou 0xFFFFFFFF.
  Panfrost le fait en matériel (`MALI_PRIMITIVE_RESTART_IMPLICIT`, pan_jm.c).
- Vérifié sur une copie de air_metal.c : seules ces lignes changent.
- La ligne de diagnostic `B2L DRAW` de 0260 reste. Set v124 → v125, build `--mesa`.

## Risques

- Une liste de triangles qui contiendrait 0xFFFF comme vrai sommet serait coupée. Metal fait de même.

## Installé

- SYSTEM-OK, BOOT `829d9e95037ed31877f6b0936837cfb1173cb752efefdbbdc8e59d193e97b3e2`, BOOT-OK ; arbre Mesa compilé avec « B1L-0261 » (3 lignes), `libhakiosair.dylib` v125 refaite.

## Résultat

- Testé par le propriétaire : pictogrammes toujours cassés ; Horloge, Calendrier et Centre de contrôle inchangés ; **affichage très lent**.
- Non concluant pour les pictogrammes : les icônes du cache d'iconservicesagent (`com.apple.IconsCache`) ont été dessinées en 0259
  (48 relues) et sont réutilisées sans être redessinées. La lenteur vient peut-être du verrou unique de 0259 (non mesuré).

## Cache d'icônes effacé (accord du propriétaire, 2026-11-15)

- USERDATA relu (`out/live/ios26-userdata-icons-261.img`). Sauvegarde du cache : `out/icons-cache-261-backup/com.apple.IconsCache.tar` (86 entrées).
- Dans une copie clonée (`ios26-userdata-icons-261-nocache.img`), suppression de
  `systemgroup.com.apple.lsd.iconscache/Library/Caches/com.apple.IconsCache/AC524191-3C64-3264-8E2B-4CC591996336`
  (store.index, maps et 78 `.isdata`). Le dossier `com.apple.IconsCache` et son ACL restent ; les modes 000 de `Library` et `Caches` sont remis.
- `flash-delta.sh … userdata /dev/block/sda34 499641200640` : 4 suites, 6 blocs de 4 Mio écrits et relus ; le reste était identique.

## Résultat après effacement du cache

- Propriétaire : icônes d'abord **dessinées correctement**, puis « au bout d'un moment » à nouveau des traits et des triangles
  (un peu mieux qu'avant). Horloge, Calendrier et Centre de contrôle restent faux. Le verre (liquid glass) est blanc sur un fond marron clair.
  Interface très lente.
- Nouveau cache relu (`ios26-userdata-icons-261b.img`) : **140 icônes, toutes justes**, Horloge avec aiguilles et Calendrier compris.
  0261 corrige donc le rendu d'iconservicesagent. La casse qui revient vient du rendu en direct de SpringBoard.
- Journaux iOS : ils s'arrêtent 2 min après le démarrage, avant la casse. Le log noyau ne montre ni plantage ni faute GPU ; la mémoire GPU reste
  stable (SpringBoard 150 à 240 Mo, backboardd 220 à 390 Mo).

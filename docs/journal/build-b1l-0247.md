# Build B1L-0247 : mémoire GPU par processus (diagnostic)

Date : 2026-11-14. Base : B1L-0246. Noyau seul (`--no-libs`, ensemble `b2l-metal-v117` inchangé).

## Pourquoi

Demande du propriétaire : **tout le système sur le GPU sans consommer trop de mémoire.** Aujourd'hui 4 processus ont le
GPU (backboardd, SpringBoard, chronod, iconservicesagent, liste de `b2l_metal_plugin.m`, B1L-0185), et pourtant 0231/0239
montrent 1,4 à 1,8 Go de tampons GPU vivants, une quinzaine d'allocations refusées, parfois 65 Mo libres. Avant d'ouvrir
le GPU à tous, il faut savoir qui garde quoi.

Constat dans le code : `kPanfrostMadvise` répond toujours « retained » et ne libère rien. Le cache de tampons de Mesa
(panfrost) marque ses tampons libres comme jetables (DONTNEED) en comptant sur le noyau pour les reprendre sous pression :
ici, ils restent tous.

## Changement (`B2LMaliService.cpp`, `b2l_mali_core.c`)

- Chaque tampon note s'il est marqué jetable (DONTNEED) ; rien n'est encore libéré.
- Chaque client note le nom de son processus ; 32 clients suivis au lieu de 8.
- Toutes les 60 s, après la ligne `mali: step …` : une ligne par client
  `mali mem: <processus>: N MB in K buffers, P MB of them marked purgeable; IOSurfaces S MB`.

## Installé

- **Vérifié :** `mali mem:` dans `out/b1l-metal-0247/boot.img`, ensemble v117 toujours nommé.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `b5ee8db655ada7b00ade5e2be738117dae99f20f8c90a67c7f8cab3e0e6b7005` (BOOT-OK). Redémarré par `adb reboot` (dernier build de la série 0243–0247).

## Résultat

- Relevé à ~3 min (3 tours de 60 s avant TWRP), 821 Mo d'adresses GPU utilisées, 7 clients :
  - backboardd 342 Mo (407 tampons, 19 Mo jetables) + IOSurfaces 33 Mo ; SpringBoard 142 Mo (86, 8 jetables) ; chronod 128 Mo (5) ; iconservicesagent 128 Mo (12).
  - Attention : le relevé compte la taille d'adresses ; le tas du tiler (128 Mo d'adresses) n'a que 16 Mo de mémoire derrière (B1L-0037). Mémoire réelle ≈ backboardd 230 Mo, SpringBoard 30 Mo, chronod 16 Mo, iconservicesagent 16 Mo, soit ~290 Mo + 33 Mo d'IOSurfaces.
  - La part « jetable » est petite (27 Mo) : la purge seule gagnerait peu. Le gros poste est backboardd.
  - Coût probable d'une app de plus sur le GPU : ~16 Mo de tas + ses tampons.

## Retour en arrière

- `B2LMaliService.cpp.before-0247`, `b2l_mali_core.c.before-0247`.

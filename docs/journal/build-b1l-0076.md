# Build B1L-0076 : Mesa et le pilote Metal sans appels signés

Date : 2026-11-07. Base : B1L-0075 (0,65 à 0,91 ms par dessin).

## Changement

- **`-fno-ptrauth-calls`**, en plus de `-fno-ptrauth-returns`, pour Mesa (`cross.ini`) et pour le pilote et les tests Metal. Les appels par pointeur de fonction signés (`blraaz` : 3067 dans `libhakiosair`) deviennent des `blr` ordinaires.
- **Pourquoi c'est sûr ici :** l'émulation PAC du noyau est en mode « stripped-pointer ABI ». `pac*` laisse le pointeur tel quel, et `aut*`/`blraa*` acceptent un pointeur non signé. Un pointeur donné au système (rappel) ou reçu de lui (`dlsym`) est donc le même dans les deux cas.
- **Vérifié sur le Mac :** avec les deux options, un petit fichier de test ne contient plus que `ret` et des appels ordinaires.
- **Ensemble `b2l-metal-v32`**, et le noyau pointe vers v32. Mesa est recompilé entièrement, puisque `cross.ini` a changé.

## Attendu

- **`M5 bench` :** encore une baisse du temps par dessin.
- **Ce qui restera :** les relais vers les fonctions du système (`__auth_stubs`, prévus pour B1L-0077) et le code d'Apple (Metal, libobjc).

## Installé

- **Code (`__text`) :** `libhakiosair` contient 0 `retab`, 0 `blraaz`, 0 `blraa` et 0 `braa` ; `libhakiosmetal` n'en contient plus aucun non plus.
- **Ce qui reste :** la section `__auth_stubs` de `libhakiosair` contient **162 relais signés** (`braa`), un par fonction du système appelée (`malloc`, `memcpy`, verrous…). Chaque appel au système passe donc encore par un piège. C'est l'objet de B1L-0077.
- **SHA-256 :** `libhakiosair` `8a980d42…`, `libhakiosmetal` `13d91ce4…`, `libb2l-metal-test` `c8f3f1c0…`.
- **SYSTEM :** 6 blocs réécrits, en 85 s.
- **BOOT :** `out/b1l-metal-0076/boot.img`, relecture `2383d8adaeb1225749501f76b7cb378c8e20a570648ef211b3084126e4dd1782` (BOOT-OK).

## Résultat (`out/logs/0076-boot1.txt`)

- **200 dessins (IOSurface) :** 111 à 115 ms, soit **0,55 à 0,58 ms par dessin**, contre 0,65 à 0,91 ms dans 0075. Dans le drawable : 0,66 à 0,95 ms (mesure plus bruitée).
- **Toujours juste :** `0 rows differ` partout, `presentDrawable` en statut 4.
- **Gain modeste :** les appels signés coûtaient moins que les retours. Il faut maintenant mesurer au lieu de deviner, d'où B1L-0077.

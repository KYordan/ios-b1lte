# B1L-0262 — diagnostic : ce que SpringBoard dessine plus tard, et l'attente sur le verrou de 0259

## Pourquoi

- Après 0261, le cache d'icônes est juste, mais SpringBoard recasse les icônes « au bout d'un moment ». Les images de diagnostic
  ne couvraient que les dessins 1, 4 et 16 de chaque taille, et les journaux iOS s'arrêtent avant la casse.
- L'interface est très lente. Le verrou unique de 0259 est suspect, mais sans mesure.

## Changement (`hakiosmetal_air.m`, sauvegarde `.before-0262`), sans effet sur le rendu

- Images `B2LMOD l-img-…` : aussi les dessins 256 et 4096 des cibles couleur (format 80) ; budget 2,5 → 4 Mo.
- `B2L LOCK: N takes, X ms waited in Y ms` toutes les 5 s (20 lignes par processus, écrites directement dans le log noyau).
- Set v125 → v126 (Mesa inchangé : la reprise de primitive de 0261 reste).

## Installé

- SYSTEM-OK, BOOT `2d47066aa140176b77298efd7e07c6b0d9b5a5285a29779daa917999c0bbc73c`, BOOT-OK ; « B2L LOCK » présent dans `libhakiosmetal.dylib` v126.

## Résultat

- Testé par le propriétaire : icônes justes au début, puis recassées « au bout d'un moment » (traits, triangles) ; Horloge, Calendrier et
  Centre de contrôle toujours faux ; verre liquide tout blanc sur fond marron clair ; interface très lente.
- `B2L LOCK` (log noyau `out/logs/0262-boot.txt`) : la contention est **forte mais brève**, pendant le rendu des icônes au démarrage
  (SpringBoard 4066 prises, 7999 ms attendus en 5088 ms ; iconservicesagent 4431 prises, 8449 ms en 5000 ms ; backboardd 5214 ms en 25 s).
  Ensuite elle tombe presque à zéro (SpringBoard 81 prises, 11 ms en 34 s). Le verrou n'explique donc pas la lenteur durable.
- Aucune image n256/n4096 : les journaux iOS (USERDATA `ios26-userdata-icons-262.img`, lecture seule) s'arrêtent avant. Les 11 images
  SpringBoard du début sont plausibles (masque étoile, carte de relief 122×122).
- Le cache `com.apple.IconsCache` réécrit (82 icônes) est **entièrement juste**, Horloge et Calendrier compris. La casse vient donc
  du dessin en direct (SpringBoard/backboardd), pas du cache.
- Cause trouvée en lisant le code : voir build-b1l-0263.md (tampons Metal non persistants pour panfrost).


# B1L-0266 — la position du fragment au centre du pixel sur le Mali

## Cause

- B1L-0265 : tous les dégradés tirés de `[[position]]` ont un demi-pixel de retard, sur toutes les cibles (R8, RG16F, BGRA8).
- Metal donne le **centre** du pixel (x + 0,5, y + 0,5), et le traducteur le déclare (`pixel_center_integer = false`).
- panfrost sur Bifrost et après annonce `fs_coord_pixel_center_integer` et pas `..._half_integer` (pan_screen.c, `dev->arch >= 6`) :
  son `load_frag_coord` est le coin du pixel, et l'ajout de 0,5 revient à l'appelant (note de `nir_lower_frag_coord_to_pixel_coord.c`).
  Freedreno annonce des centres en demi-entier : c'est pour ça que l'Adreno ne le montrait pas.
- Conséquence probable sur les icônes : RenderBox lit ses masques et textures à partir de la position, donc au coin des texels
  (pixels doublés par paires, lignes manquantes, d'où les rayures). À confirmer à l'écran.

## Changement (`userspace/mesa/air-b2l/apply.py`, section B1L-0266, sauvegarde `apply.py.before-0266`)

- `finalize_for_gallium` (air_metal.c), après `nir_lower_system_values` : pour un fragment shader, sur un écran dont la position est un
  sysval à centre entier, +0,5 sur x et y de chaque `load_frag_coord`. Vérifié sur une copie : seules ces lignes changent.
- Raccourci noté : centre du pixel aussi en ombrage par échantillon.
- Set v129 → v130. L'app Test Metal reste dans Dictaphone : ses tests de cibles doivent passer.

## Installé

- SYSTEM-OK, BOOT `bdc87bb9fcffbbef774f6c8b55fb9fc038e903060cb67c2ad7edaaf903d4ff92`, BOOT-OK ; air_metal.c porte B1L-0266, `libhakiosair.dylib` v130 reconstruite.

## Résultat

- Propriétaire : **toujours le même problème**. Les bonnes icônes reviennent parfois, mais dès qu'il y a du mouvement ça bugue ;
  toujours très lent. Dictaphone pas lancé (pas de ligne `b2l-mtest`).
- Log noyau `out/logs/0266-boot.txt` : pendant l'usage, le verrou global est très disputé : SpringBoard **4142 ms attendus en 5007 ms**,
  puis 3270, 3614 ; backboardd 2455 ms en 5007 ms ; iconservicesagent 8206 ms en 16 s. Aucun plantage.
- Le décalage d'un demi-pixel reste corrigé (juste d'après le code) mais n'était pas la cause visible.


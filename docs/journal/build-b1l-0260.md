# B1L-0260 — diagnostic : la forme de chaque dessin (pictogrammes coupés en diagonale)

## Constat (cache d'icônes réécrit par iconservicesagent en 0259, `com.apple.IconsCache`, 48 icônes 204×204 relues)

- Beaucoup de pictogrammes sont **coupés en diagonale**. D'un côté de la diagonale du rectangle, le pictogramme est juste ; de l'autre, la moitié est
  une couleur unie, ou manque. Exemples : FaceTime, Calculette, Fichiers, Livres, Photos, Podcasts, Astuces, Musique, Courrier.
  Un triangle sur deux du rectangle est donc faux.
- Ces pictogrammes passent par les pipelines `custom_effect_vertex` / `primitive_custom_vertex` de RenderBox, rendus possibles par 0257.
  `custom_effect_vertex` construit son rectangle **à partir de `vid` seul** (sommets 0 à 3, sans `& 3` ni tampon de sommets) :
  x = 0 pour vid 0 et 3, 1 sinon ; y = 1 pour vid > 1. Dessiné comme une bande de 4 sommets, ce rectangle ne serait pas entièrement couvert.
  Il faut donc savoir quelle primitive, quel début et quels indices RenderBox envoie réellement.

## Changement (`hakiosmetal_air.m`, sauvegarde `.before-0260`)

- Ligne `B2L DRAW: <vertex> + <fragment> primitive P start S count C index none|u16|u32 <8 premiers indices> base B instances N from I`,
  une fois par forme de dessin distincte (60 au plus par processus). Rien d'autre ne change.
- Set v123 → v124.

## Installé

- SYSTEM-OK, BOOT `892145ce77a618a179ffd6a694c37766741125cf2d1106eefb470ae2fe0ddd60`, BOOT-OK ; « B2L DRAW » présent dans `libhakiosmetal.dylib` v124.

## Résultat

- Lignes `B2L DRAW` (log noyau `out/logs/0260-boot.txt`) : RenderBox dessine tout en bande de triangles indexée (`primitive 4`, u16),
  rectangle seul `1 0 2 3`, et **par lots** `1 0 2 3 65535 5 4 6 …` (uber_vertex, jusqu'à 64 indices et 34 instances).
  65535 est l'indice de reprise de primitive de Metal (toujours actif, documentation de `drawIndexedPrimitives`), et
  air_metal.c dessinait sans reprise. Correction dans build-b1l-0261.md.

# Build B1L-0116 : un vrai fond d'écran (motifs) à la place du bleu uni

Date : 2026-11-08. Base : B1L-0115. Noyau seulement (la bibliothèque injectée dans SpringBoard y est intégrée), ensemble `b2l-metal-v69` gardé.

## Pourquoi

Le fond bleu uni n'est pas un défaut de rendu : c'est la vue posée par `b2l-springboard` (B1L-0027) dans les fenêtres « Wallpaper », parce que PosterBoard ne dessine rien. Le propriétaire veut une vraie image, avec des motifs, pour voir le flou et le verre.

## Changement

- `userspace/b2l-springboard.m` (v3) : la vue est une `UIImageView` dont l'image est **dessinée une fois avec CoreGraphics** à la taille de l'écran : dégradé diagonal violet → bleu-vert → orange, cinq grandes taches de couleur douces, 14 vagues blanches translucides, grille de points. Plage standard (8 bits par canal) : un rendu en couleurs étendues ferait une image RGBA16Float, que le pilote Mali refuse en plein écran. Le bleu reste dessous, au cas où.
- La bibliothèque passe en **v3** (`/usr/local/lib/b2l-springboard-v3.dylib`) : le noyau ne remplace jamais un fichier déjà publié, une nouvelle version demande un nouveau nom. Renommée dans `b2l_exec_graphics_diag.h`, `b2l_userspace_seed.h`, `b2l_amfi_constraint.h` et `scripts/build-springboard-dylib.py` (qui lie aussi CoreGraphics maintenant), charge utile régénérée (`b2l_springboard_payload.h`, 90 992 octets, sha256 `d9827b0b…`).

## Installé

- **BOOT :** relecture `7e1b885ffae417b5eedf56a3fbb42d1927600ca2d7e3b86132a0e708719df86f` (BOOT-OK). Pas redémarré (0117 et 0118 suivent).

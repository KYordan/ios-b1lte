# Build B1L-0123 : vues d'une partie des niveaux et tranches, et rendu dedans

Date : 2026-11-08. Base : B1L-0122. Ensemble `b2l-metal-v73`, Mesa recompilé.

## Pourquoi

Le verre reste gris ou noir alors que tout est alloué et qu'il n'y a ni faute ni refus. Deux trous dans le pilote :
- `-newTextureViewWithPixelFormat:textureType:levels:slices:` répondait `nil` dès que la vue n'était pas la texture entière (dit une seule fois par boot, on ne voyait pas combien) ;
- toutes les surfaces de rendu étaient faites au niveau 0, tranche 0, quelle que soit la vue ; et le `level`/`slice` des attachements de passe étaient ignorés.

Le flou et le Liquid Glass travaillent sur des chaînes de mipmaps (dessin dans les niveaux 1, 2… puis lecture) : ces passes ne dessinaient nulle part et le verre lisait des niveaux vides.

## Changement

- **Mesa (`apply.py`, patch B1L-0123, `air_metal.c`)** :
  - `struct rwa_texture` porte la plage de niveaux et de couches dont elle est une vue ;
  - nouvelle `rwa_texture_view_range` (format, niveaux, couches ; une tranche de tableau ou une face de cube devient 2D) ;
  - utilisent cette plage : vues d'échantillonnage, surfaces de rendu (format de la vue, niveau, couche ; la taille du framebuffer suit la forme de la vue, aussi pour une passe qui ne fait qu'effacer), vues d'image, copies (niveaux relatifs à la vue) et mipmaps.
- **Pilote (`hakiosmetal_air.m`)** :
  - les vues partielles sont servies (type identique, ou famille 2D) ; les 8 premières sont décrites (`B2L VIEW: …`), les refus restants sont dits à chaque puissance de deux ;
  - un attachement de couleur avec `level`, `slice` ou `depthPlane` non nul est dessiné à travers une vue de ce niveau et de cette tranche (gardée par l'encodeur).

## Installé

- **Vérifié :** `rwa_texture_view_range` exporté (v73), `B2L VIEW` dans `libhakiosmetal.dylib`.
- **BOOT :** relecture `b2d817dd16b711b42f371ada3e56c9c21bc1d38369cf9421f86295a9f6d0386f` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0122/boot.img` avec SYSTEM v72.

## Résultat (journal `out/logs/0123-boot.txt`)

- **Propriétaire : le Centre de contrôle est plus cassé, nouveaux défauts au dock, clignotements de toute l'interface.** Indice : le Liquid Glass marche quand on descend le rideau de l'écran verrouillé, pas dans le Centre de contrôle.
- Les vues partielles sont bien demandées : niveaux 1, 2 et 3 (un seul niveau chacun) d'une texture BGRA8 768×1600 à 4 niveaux (pyramide de mipmaps). Elles étaient refusées jusqu'ici.
- Nouveau : 11 fautes GPU et 10 jobs en échec, contre 0 en 0121 et 0122 (passes `path_blit`/`attachment_clear` + blend shader, `variable_blur` + `glass_background`). Toutes sont des lectures **juste après la fin d'un tampon de 64 Ko ou 16 Ko** : la taille des pools de panfrost, où sont copiées les constantes de chaque dessin.
- Lecture : les passes de flou, qui tournent enfin, lisent des constantes au-delà de la plage copiée depuis 0115 (borne de `nir_unsigned_upper_bound` trop courte pour un décalage calculé) → B1L-0124.

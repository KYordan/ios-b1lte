# Build B1L-0070 : clear seul et clear une seule fois par passe

Date : 2026-11-07. Base : B1L-0069.

## Constat sur B1L-0069 (`out/logs/0069-boot1.txt`)

- **L'import marche :** `HakiosMali: request 0x7e` puis `newTextureWithDescriptor:iosurface:plane: -> HakiosMetalTexture`. La texture Metal sur l'IOSurface existe.
- **Mais rien n'est écrit :**
  - `clear to red (status completed): 0 of 4096 pixels are red (first a5a5a5a5)` ;
  - `clear to green … 0 of 4096 (first 5a5a5a5a)` ;
  - aucune requête de job n'est envoyée au Mali.
- **Cause : le pilote de Scion n'efface que dans un dessin.**
  - `clear_color` voyage dans `rwa_draw`, à chaque draw.
  - Une passe qui efface sans dessiner (load action Clear, aucun draw) n'enregistre aucun travail.
  - C'est exactement le test M5, et c'est aussi ce que fait QuartzCore au début de chaque image.
- **Second bug, trouvé en lisant le même code :** l'effacement n'était jamais remis à zéro après le premier draw. Chaque draw d'une passe réeffaçait donc ce que les précédents avaient dessiné. Les tests M4 n'avaient qu'un draw par passe ; QuartzCore en fait des dizaines.

## Changements

**`userspace/mesa/air-b2l/apply.py` : nouvelle fonction `rwa_render_clear(device, draw)` dans `air_metal.c`.**
- Elle fabrique le framebuffer de la passe, puis appelle `pipe->clear` sur les aspects demandés (couleur par cible, profondeur, stencil).
- Elle attend la fence, puis libère les surfaces.
- La déclaration est ajoutée dans `air_metal.h`.

**`scion/hakiosmetal_air.m` :**
- **Encodeur de rendu :** après le premier draw, les trois clears passent à NO. La load action appartient à la passe et ne s'applique qu'une fois.
- **`-endEncoding` :** une passe sans draw qui a un clear enregistre un travail `clearOnly`.
- **`-commit` :** `runClear:` appelle `rwa_render_clear`, chargé par `dlsym`. Si la bibliothèque ne l'a pas, le refus est explicite.

**Ensemble `b2l-metal-v26`** (Mesa recompilé, `libhakiosair` et `libhakiosmetal` nouveaux), et le noyau pointe vers v26.

## Ce qu'il faut regarder

- Lignes `M5:` : rouge, puis vert, sur 4096 pixels chacun.
- Si les jobs partent et que le CPU voit toujours le motif témoin, le problème vient du cache CPU. Le GPU écrit en RAM, alors que les lignes déjà en cache côté processeur restent anciennes.

## Compilation

- `libhakiosair` n'est pas une cible par défaut de Mesa, et la première compilation l'a laissé ancien, sans `rwa_render_clear`. Il est maintenant compilé explicitement par nom dans `build-mesa.sh`.
- Fichiers v26 :

| Fichier | SHA-256 |
| --- | --- |
| `libhakiosair` | `3085593b…` (exporte `rwa_render_clear`) |
| `libhakiosmetal` | `d1e5107e…` |
| `libb2l-metal-test` | `12966cda…` |

## Installé

- SYSTEM : 6 blocs réécrits, en 71 s.
- BOOT : `out/b1l-metal-0070/boot.img`, relecture `355cbb829ad61c5939aa4fd59089ef9b8222048258f914e3e889cd914af0408c` (BOOT-OK).

## Résultat (`out/logs/0070-boot1.txt`) : M5 étape 1 réussie

- `clear to red (status completed): 4096 of 4096 pixels are red (first ffff0000): M5 step 1 done`
- `processor canary, then clear to green (status completed): 4096 of 4096 pixels are green (first ff00ff00): both directions seen`
- Le Mali écrit dans la mémoire d'une IOSurface au travers d'une texture Metal, et le processeur relit les bonnes valeurs. Les deux sens fonctionnent : le motif écrit par le CPU juste avant est bien remplacé, donc aucune ligne de cache périmée n'est vue.
- À noter pour M6 : le balayage de drvtest signale que `newTextureWithDescriptor:iosurface:plane:slice:` (variante avec `slice`) est encore un stub d'Apple qui fait un abort si on l'appelle.

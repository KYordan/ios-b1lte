# Build B1L-0236 : le dessin revient dans la mémoire d'image de RenderBox

Date : 2026-11-14. Base : B1L-0235. Ensemble `b2l-metal-v107` (noyau reconstruit), Mesa inchangé, pilote recompilé.

## Pourquoi

RenderBox dessine les icônes dans sa propre mémoire d'image : `newBufferWithBytesNoCopy:` sur ses octets, puis
`newTextureWithDescriptor:offset:bytesPerRow:` sur ce tampon, en cible de dessin, et il relit ensuite ces octets. Les
tailles le prouvent (journaux 0230–0234) : 62 464 = 122 × 512 (RGB10A2 122×122), 169 728 = 204 × 832 (BGRA 204×204),
52 224 = 204 × 256 (R8 204×204). Chez nous, les deux étages sont des copies à sens unique (0186 et 0231) : le dessin
du GPU n'arrive jamais dans la mémoire de RenderBox, et l'icône reste vide. C'est le même mécanisme que les « icônes
grises » de hakios (requête `0x7d`, pages du programme) et les « icônes absentes » d'ios-pipa (boot 39, `IOSurfaceAddress`).

## Changement (`hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0236`)

- Un tampon « sans copie » se souvient des octets de l'appelant (`hakiosSetNoCopySource:`).
- **Retour du dessin :** au `-commit`, le command buffer note les cibles de dessin qui sont des textures sur tampon.
  Quand la fence est payée (`-waitUntilCompleted`, donc aussi avant les completed handlers), chacune est relue
  (`texture_read`) dans son tampon, puis le tampon est recopié dans les octets de l'appelant. Lignes
  `B2L WRITEBACK:` (12 au plus).
- **Entrée :** avant de remplir une texture sur tampon depuis le tampon, le tampon est d'abord remis à jour depuis les
  octets de l'appelant.
- **Limite connue :** si personne n'attend le command buffer (ni `waitUntilCompleted`, ni handler), rien n'est
  recopié. La vraie solution reste les pages du programme données au GPU (comme hakios), qui demande un travail côté
  noyau.

## Installé

- **Vérifié :** `B2L WRITEBACK` dans `build-metal/b2l-metal-v107/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `c5dae72434fb57052f8a76336e80f284f78eede344fc670e82a4f98fa7bf4761` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire + journal `out/logs/0236-boot.txt`)

- **Pas mieux à l'écran** (propriétaire).
- Le retour du dessin tourne : `B2L WRITEBACK` pour 122×122 RGB10A2 (512 octets par ligne), 120×120 R8 (128) et
  120×120 BGRA (512) dans SpringBoard. Aucun refus, aucun job en échec.
- Plus rien à deviner dans le journal : il faut voir ce que le GPU dessine. Suite : B1L-0237 (diagnostic).

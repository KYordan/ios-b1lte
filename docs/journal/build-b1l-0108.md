# Build B1L-0108 : retenue de 30 s pour un command buffer non envoyé

Date : 2026-11-08. Base : B1L-0107. Pilote Metal seulement.

## Changement (`hakiosmetal_air.m`)

- Un command buffer jamais envoyé est relâché au bout de **30 s** (au lieu d'une) : QuartzCore envoie parfois plus d'une seconde après la création, à l'ouverture d'une app.
- À ce moment-là, son emplacement dans la file est rendu d'abord (`hakiosReturnSlot`, sans effet s'il l'a déjà été), pour que la file ne bloque pas.

Ensemble `b2l-metal-v64`. Premier build redémarré automatiquement vers le système après le flash (demande du propriétaire).

## Installé

- **SYSTEM :** SYSTEM-OK en 56 s. **BOOT :** relecture `fd27d653a1a580df99eef368aa1030e37ed36ff34de853863097688ea2250543` (BOOT-OK).
- Téléphone redémarré vers le système par `adb reboot` juste après.
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0108-boot.txt`)

- Toujours le même plantage (2 fois, 4 démarrages) : `[self commit]` dans `-commitAndWaitUntilSubmitted` (`libhakiosmetal` +0xa1ec), sur un objet libéré, avec un premier mot qui ressemble au lien obfusqué de la liste libre de l'allocateur. La retenue de 30 s n'y change rien : soit une libération en trop a lieu quelque part, soit l'objet n'est pas celui que l'on croit. → diagnostic en B1L-0109.

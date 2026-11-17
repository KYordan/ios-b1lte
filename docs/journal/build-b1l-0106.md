# Build B1L-0106 : le command buffer reste en vie jusqu'à ses rappels

Date : 2026-11-08. Base : B1L-0105. Pilote Metal seulement.

## Changements (`hakiosmetal_air.m`)

- Un command buffer qui reçoit un rappel (« scheduled » ou « completed ») est **retenu par le pilote** jusqu'à la livraison de ses rappels (`hakiosHold`/`hakiosUnhold`), comme la file de Metal garde le sien. Corrige le message envoyé par QuartzCore à un command buffer libéré.
- `-commitAndWaitUntilSubmitted` renvoie un `BOOL` (celui d'Apple aussi ; QuartzCore range la valeur).

Ensemble `b2l-metal-v62`.

## Installé

- **SYSTEM :** SYSTEM-OK en 81 s. **BOOT :** relecture `3b89c5dccbfb9f7957ac8704a9c40d85dd912cc62c8d91d82aa685c88172a47d` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0106-boot.txt`)

- Propriétaire : backboardd plante toujours à l'ouverture d'une app ; toujours lent ; pas d'amélioration visuelle.
- **Plantage** (2 fois, même pile) : cette fois **dans notre `-commitAndWaitUntilSubmitted`** (`libhakiosmetal` +0xa118, juste après `bl _objc_msgSend$commit`) : `[self commit]` est envoyé à un command buffer libéré entre-temps (le premier mot de l'objet est le pointeur de liste libre de l'allocateur, `0x77509b44c60dc8`). Ce command buffer n'avait pas de rappel : la retenue de 0106 ne le couvrait pas. → B1L-0107.
- **Cadence** : jusqu'à **24 bascules/s** par moments (241 / 10 s), 2 à 13 le reste du temps. Les 10/s fixes de 0105 étaient une animation au repos, pas un plafond.

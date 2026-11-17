# Build B1L-0107 : command buffer retenu dès sa création

Date : 2026-11-08. Base : B1L-0106. Pilote Metal seulement.

## Changement (`hakiosmetal_air.m`)

Chaque command buffer est retenu par le pilote **dès sa création** (`-initWithHakiosQueue:`), comme la file de Metal tient ceux qu'elle donne. Il est relâché :
- après la livraison de ses rappels, une fois envoyé ;
- ou au bout d'une seconde s'il n'est jamais envoyé, pour ne pas garder un emplacement de la file, qui finirait par bloquer.

Ensemble `b2l-metal-v63`.

## Installé

- **SYSTEM :** SYSTEM-OK en 67 s. **BOOT :** relecture `281bd7b11ae2a838d53c6207a5b53508e13c25b51ef1424004a01aa32c72dca9` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0107-boot.txt`)

- Propriétaire : toujours le même problème.
- 1 plantage (2 démarrages), même endroit : `[self commit]` dans notre `-commitAndWaitUntilSubmitted` (`libhakiosmetal` +0xa1c0) sur un command buffer libéré **entre l'entrée de la méthode et l'appel** (par un autre fil). Cause : la retenue d'un buffer jamais envoyé tombait au bout d'**une seconde**, alors qu'à l'ouverture d'une app QuartzCore l'envoie plus d'une seconde après l'avoir créé (shaders compilés entre-temps), et QuartzCore ne le retient pas lui-même. → B1L-0108.

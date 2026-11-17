# Build B1L-0109 : suivre la vie des command buffers

Date : 2026-11-08. Base : B1L-0108. Pilote Metal seulement, diagnostic plus une protection.

## Changements (`hakiosmetal_air.m`)

- À l'entrée de `-commitAndWaitUntilSubmitted` (400 premières fois) : `B2L CB <adresse> commitAndWait: status … held … retainCount … thread …`.
- Dans `-dealloc` (400 premières fois) : `B2L CB <adresse> dealloc: status … held … thread …`.
- Protection : un buffer déjà envoyé n'est pas renvoyé ; et le buffer est retenu le temps de son `-commit` (`retain` avant, `autorelease` après).

Ensemble `b2l-metal-v65`. Redémarré automatiquement vers le système après le flash.

## Installé

- **SYSTEM :** SYSTEM-OK en 63 s. **BOOT :** relecture `9476f5bfb0f4e210c48aa9fd937bb0c932964830379ab784ec1ce5818a986d66` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0109-boot.txt`)

- Toujours un plantage à l'ouverture d'une app (2 sur 4 démarrages : un `abort`, un accès invalide).
- **Suivi des command buffers** : 424 entrées dans `-commitAndWaitUntilSubmitted`, toutes avec `status 1 held 1 retainCount 3` (vivants, retenus) ; 401 destructions à l'état 4 (terminés). La ligne du buffer qui plante n'est pas dans le journal (plafond de 400 atteint avant).
- L'instruction qui plante, dans `objc_msgSend` (libobjc +0x3420), est `ldr x11, [x16, #16]` : la lecture de la classe à partir du premier mot de l'objet (`isa`, masqué puis `autda`). Ce mot contient des bits aléatoires, typique d'un bloc rendu à l'allocateur. Hypothèse retenue : un buffer réutilisé par QuartzCore après la fin de son travail et sa destruction. → B1L-0111.

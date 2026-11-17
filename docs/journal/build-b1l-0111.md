# Build B1L-0111 : B1L-0110 + command buffers gardés 2 s de plus + imagent et dasd bloqués

Date : 2026-11-08. Base : B1L-0110 (jamais flashé seul), qu'il contient : GPU à 650 MHz, diagnostics coupés, bascule d'écran non bloquante.

## Changements

1. **Pilote Metal (`hakiosmetal_air.m`)** : après la livraison de ses rappels, un command buffer est gardé **encore 2 s** avant que le pilote ne relâche sa référence (`dispatch_after`). Ce qu'il contient (`_work` : textures et tampons retenus) est vidé tout de suite. Le suivi de B1L-0109 reste.
2. **Noyau (`b2l_exec_graphics_diag.h`)** : l'exec d'**imagent** (iMessage/FaceTime) et de **dasd** (planificateur des tâches de fond) est refusé (`EPERM`) avant tout chargement, comme celui des synthétiseurs vocaux. Décision du propriétaire. Interrupteur : boot-arg `b2l_block_daemons=0`.

Ensemble `b2l-metal-v67`. Redémarré automatiquement après le flash.

## Installé

- **SYSTEM :** SYSTEM-OK en 74 s. **BOOT :** relecture `65a643912439f24456920ba9bf18efb6d9ab761d139210baedcd2d8dfd2307a4` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0111-boot.txt`)

- Propriétaire : aucune amélioration de vitesse ni de visuel ; les apps font toujours planter backboardd.
- Confirmé dans le journal : GPU à 650 MHz (`B2L GPU DVFS: request 650000 kHz ret=0`) ; imagent et dasd refusés (8 fois) ; journal 20 fois plus léger (≈ 330 Ko de texte).
- Plantage identique (2 démarrages) : `objc_msgSend` (libobjc +0x3420) depuis notre `-commitAndWaitUntilSubmitted`. Suivi : 555 entrées toutes `status 1 held 1`.
- Le propriétaire passe la main à une nouvelle session : prompt dans `docs/prompt-session-metal.md`, outils de symbolisation copiés dans `~/Developer/s10-port/b1l-tools/dsc/`.

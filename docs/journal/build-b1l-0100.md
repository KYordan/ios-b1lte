# Build B1L-0100 : émulation PAC sans recherche dans la carte mémoire

Date : 2026-11-08. Base : B1L-0099 (qu'il contient). Noyau seulement.

## Pourquoi

Le journal de 0098 : 20 000 à 80 000 interruptions PAC par seconde, ≈ 11 µs chacune dans l'émulateur, dont ≈ 3,5 µs de `vm_map_range_sh_lock` (recherche dans la carte mémoire). Chaque interruption en faisait **deux** : l'adresse de l'instruction interrompue, puis la destination (`RETAA`/`BRAA`/instruction suivante), plus une `copyin` de l'instruction.

## Changement (`osfmk/arm64/b2l_arm64e_compat.c`)

`b2l_compat_validate_pc_fast` : alignement sur 4 et adresse dans l'espace utilisateur, rien d'autre. L'émulateur ne fait que poser un PC utilisateur. Ce qui s'y trouve est ensuite lu par le processeur en EL0, qui applique lui-même la permission d'exécution : une cible non exécutable ou absente fait planter le programme, pas le noyau. Utilisée pour l'adresse interrompue et pour les destinations. Boot-arg `b2l_pac_fastcheck=0` : retour à la vérification complète.

## À regarder

`B2L EMU PERF: samples=… wrapper_ns=… vm_ns=…` : `vm_ns` doit tomber à ≈ 0 et `wrapper_ns / samples` baisser (11,2 µs en 0098).

Ensemble `b2l-metal-v56`.

## Installé

- 1er essai : erreur de compilation (`b2l_compat_fastcheck` utilisé avant sa déclaration), rien d'écrit ; déclaration remontée.
- **SYSTEM :** SYSTEM-OK en 81 s. **BOOT :** relecture `feee6748cbe8829f127dcc723fb6f086702b40500ba140c63207913736e0baef` (BOOT-OK).
- **Pas démarré seul :** B1L-0101 est construit juste après et le contient.

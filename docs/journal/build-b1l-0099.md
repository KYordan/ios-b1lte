# Build B1L-0099 : viewport, scissor, lignes de 64 octets

Date : 2026-11-08. Base : B1L-0098.

## Changements

1. **Rectangle du viewport** (`apply.py` B1L-0099 sur `air_metal.c`/`air_metal.h`, `hakiosmetal_air.m`) : `struct rwa_draw` reçoit `b2l_viewport` (x, y, largeur, hauteur), ajouté à la fin dans les deux copies. Gallium reçoit `scale`/`translate` de ce rectangle (y toujours négatif, règle de Metal). Avant, chaque dessin couvrait toute la cible.
2. **Scissor** : `setScissorRect:` est enregistré (`b2l_scissor`) ; la bibliothèque active le scissor dans l'état de rastérisation et le donne à chaque dessin, borné à la cible (toute la cible s'il n'y en a pas).
3. **Lignes de 64 octets** (`B2LSurface.cpp`) : une surface à un plan de backboardd dont la ligne demandée n'est pas un multiple de 64 octets est arrondie (352 → 384 pour la source RG8 de `compute_average_luma`). L'appelant relit la valeur.

Ensemble `b2l-metal-v55`, Mesa recompilé.

## Installé

- **Vérifié :** patch « viewport rectangle and scissor » appliqué au build Mesa.
- **SYSTEM :** SYSTEM-OK en 57 s. **BOOT :** relecture `4396a45a69e86e9a281f3693e1b1e56f6841e5ac614e8672716dbbe0bf0c83b9` (BOOT-OK).
- **Pas démarré seul :** B1L-0100 est construit juste après et le contient.

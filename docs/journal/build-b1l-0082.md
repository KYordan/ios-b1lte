# Build B1L-0082 : M6b, troisième essai (samplers « inline » de QuartzCore)

Date : 2026-11-07. Base : B1L-0081 (4 pipelines de QuartzCore refusés : « an inline sampler whose state is not a constant »).

## Changement

- **`userspace/mesa/air-b2l/apply.py`, patch B1L-0082 de `air_to_nir.c` :** l'état d'un sampler inline est lu dans le premier mot d'une valeur composée (structure, tableau ou vecteur), dans une suite de données, ou vaut 0 si la valeur est nulle. La forme à un mot est inchangée.
- **Le décalage de LOD** (2ᵉ mot) n'est pas appliqué.
- **Une forme encore inconnue** est refusée avec son type dans le message (`B2L: constant kind …, type kind …`).
- **Mesa recompilé** (seul le traducteur change), puis ensemble `b2l-metal-v38` et plug-in `B2LMetalv38`.
- **Premier build lancé avec `b1l-tools/b1l-build.sh`**, et suivi en direct dans l'onglet « suivi build/flash ».

## Installé

- **Mesa :** il contient le patch (`air-b2l: inline samplers in the two-word form`), et `libhakiosair` v38 contient le nouveau message de refus.
- **Fausse alerte du script de build :** au premier passage, `b1l-build.sh` a refusé l'image en disant que le noyau ne nommait pas v38. Il le nommait bien (16 occurrences). C'est `grep -q` dans un tube avec `pipefail` qui faisait échouer le test. Le test est corrigé, et l'option `--no-kernel` permet de reprendre la chaîne après la compilation.
- **SYSTEM :** 6 blocs réécrits, en 62 s.
- **BOOT :** `out/b1l-metal-0082/boot.img`, relecture `d7be2d028ba1a73b8618c26305ffc26eba2905ba442af0570266cf330225fbc4` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Résultat (`out/logs/0082-boot1.txt`) : les samplers passent, deux nouveaux refus

- **Le refus « inline sampler » a disparu :** le correctif marche.
- **Nouveaux refus du traducteur AIR,** qui font de nouveau sortir CoreAnimation (raison 13, sous-code 4, pids 62 et 172), puis déclenchent le retour au CPU au 3ᵉ démarrage :
  1. `intrinsic: air.get_read_sampler` (2 fois) : l'intrinsèque qui donne le sampler implicite de `texture.read()` n'était pas connue.
  2. `a call passing a pointer at a computed offset as argument 1, which would use the caller's own value here` (2 fois) : une fonction appelée reçoit un pointeur à un décalage calculé par l'appelant, par exemple un membre de structure ou un élément de tableau. Le traducteur de Scion refusait ce cas au lieu de transmettre le décalage.

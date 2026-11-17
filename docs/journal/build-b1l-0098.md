# Build B1L-0098 : zéro copie avec les images de backboardd + texture RG8 pour la luminosité du verre

Date : 2026-11-08. Base : B1L-0097.

## Changements

1. **Pilote Metal (`hakiosdrawable.m`) :** une texture `RG8Unorm`/`_sRGB` (30, 31) peut être posée sur une IOSurface de deux octets par pixel (`'2C08'`, ou toute surface dont la ligne fait 2 à 3 fois la largeur). C'est la source de `compute_average_luma`, qui choisit la teinte claire ou sombre du Liquid Glass. Mesa connaissait déjà le format.
2. **Noyau, zéro copie (`B2LFramebuffer.cpp`) :**
   - à chaque bascule, la **largeur de ligne** est écrite dans `IDMA_SRC_SIZE` (+0x10 du bloc IDMA 0 : hauteur << 16 | largeur source en pixels), la taille d'image restant celle du panneau. La valeur d'origine est gardée et remise quand l'écran revient à la mémoire du panneau ;
   - la surface par défaut donnée à backboardd est **contiguë et en écriture combinée**, donc basculable.
3. **Noyau, surfaces (`B2LSurface.cpp`) :** les surfaces BGRA de backboardd à la taille du panneau sont contiguës quand c'est possible, même avec des lignes plus longues (jusqu'à +1024 octets, multiple de 64). Elles sont **toujours en écriture combinée**, contiguës ou non : jamais de nettoyage de cache, et les écritures du processeur, s'il y en a, ne restent pas dans un cache (les blocs bizarres pourraient en venir).

Ensemble `b2l-metal-v54`. Mesa inchangé.

## À regarder au boot

- Lignes `B2L ZC: flip N to 0x… (5888 bytes a row)` : si la demande de mise à jour retombe à 0, la bascule marche. Sinon `zero-copy OFF` et retour à la copie.
- `compute_average_luma` ne devrait plus échouer.

## Installé

- **Vérifié :** le noyau contient « bytes a row ».
- **SYSTEM :** SYSTEM-OK en 71 s. **BOOT :** relecture `89d1c709e300224e58f1516e787cc7f4b826ff7a485d0876f7afdd612367e2b6` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0097/boot.img`.

## Résultat (journal `out/logs/0098-boot.txt`)

- **Le zéro copie fonctionne** : `flip` alterne entre `0x82420000` (surface par défaut, 5760 octets/ligne) et `0xc10c0000` (surface de backboardd, 5888 octets/ligne), demande de mise à jour retombée à 0 et registres d'ombre égaux. **0 copie CPU** (`blit 0`), 0 nettoyage de cache, envoi au GPU 6–19 ms / 10 s. Bascule ≈ 8 ms (attente du panneau).
- Propriétaire : plus de blanc/noir dans les barres du bas ; **toujours un diaporama** ; triangles rouges et colorés par moments ; éléments qui disparaissent puis reviennent ; Liquid Glass du Centre de contrôle cassé ; curseurs luminosité/volume impossibles à régler, avec du rouge/bleu/vert à la place du gris.
- **Cause du rendu cassé :** `a viewport rectangle is not implemented` **1126 fois** (ex. « 384x800 at 0,0 of a 768x1600 attachment » : la chaîne de flou réduit l'image et chaque étape dessinait sur toute la cible) ; le **scissor** était ignoré aussi. → B1L-0099.
- `compute_average_luma` échoue encore (100 fois) : la surface RG8 a **352 octets par ligne**, refusée (le Mali veut un multiple de 64). → B1L-0099.
- **Cause du diaporama :** le GPU n'est occupé que **2 à 7 %**. Le fil de rendu de backboardd brûle 91 % d'un cœur, dont **88,5 s noyau / 1,6 s utilisateur**. Les compteurs `B2L EMU` : **20 000 à 80 000 interruptions PAC par seconde** (surtout `RETAA` et `BRAA`, presque toutes dans le cache partagé), **≈ 11 µs chacune** dans l'émulateur (dont ≈ 3,5 µs de recherche dans la carte mémoire). Le cache natif n'aide plus : `apply mismatch=63523`, `applied=5` (enregistrements périmés), et ce qu'il apprend encore est surtout dans corecrypto (exclu volontairement).

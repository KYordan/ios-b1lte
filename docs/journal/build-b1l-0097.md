# Build B1L-0097 : backboardd ne dessine plus dans la mémoire affichée

Date : 2026-11-08. Base : B1L-0096. Noyau seulement.

## Changements

1. **Surface par défaut en mémoire ordinaire** (`B2LFramebuffer.cpp`, `b2l_surface_create_memory`) : la surface que l'écran donne à backboardd (`GetLayerDefaultSurface`) n'est plus la mémoire du panneau mais une surface ordinaire de même forme. Toutes les images sont montrées par copie, après l'attente du GPU (0096). Corrige le dessin progressif et le clignotement.
2. **Nettoyage de cache une seule fois** (`B2LMaliService.cpp`, `flushBeforeJob`) : les surfaces de la taille du panneau de backboardd (et la nouvelle surface par défaut) ne sont nettoyées qu'avant leur premier job. Le GPU seul les écrit ; la copie vers l'écran les lit après l'invalidation de `HakiosMaliDrawn()`.
3. **Recherche du canal d'écran** (`b2l_zc_init`) : l'adresse du panneau est cherchée dans tout DPP `0x19020000`, DECON `0x19030000` et DMA `0x19070000` (32 Kio chacun). Chaque endroit qui la contient est écrit dans le journal. Le zéro copie ne s'active que sur un registre de base à `+0x40` d'un bloc DMA/DPP. Sinon, copie.
4. Le zéro copie ne peut de toute façon pas basculer avec les surfaces actuelles de backboardd (lignes de 5888 octets) : c'est pour le build suivant, avec la largeur de ligne réglée dans le DMA.

Ensemble `b2l-metal-v53`.

## Installé

- **Vérifié :** le noyau contient « default surface for clients ».
- **SYSTEM :** SYSTEM-OK en 79 s. **BOOT :** relecture `c86617fce5aceecd09d387a6a86f3b12897ca8db7a9eeeff558571207b7ef0f5` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0095/boot.img`.

## Résultat (journal `out/logs/0097-boot.txt`)

- **Les éléments n'apparaissent plus petit à petit.**
- Restent : les éléments Liquid Glass de la barre d'onglets du bas (et leurs icônes) **passent du blanc au noir et inversement** ; **blocs et triangles bizarres** par moments.
- **Envoi au GPU : 2 à 22 ms par 10 s** (jusqu'à 2 s avant) ; un seul nettoyage par image affichée (≈ 3,5 ms) ; copie vers l'écran 15 ms par image.
- **L'adresse du panneau `0xCA000000` est à `0x19071040`** (DMA+0x1040, canal IDMA 0, base +0x40), copie d'ombre à `0x19071840`. Zéro copie actif, mais **0 bascule** : aucune surface de backboardd n'avait la forme acceptée (lignes de 5888 octets, mémoire non contiguë).
- `compute_average_luma` échoue 47 fois (texture 0 vide) : piste, le format `RG8Unorm` (30) refusé par l'import IOSurface du pilote (vu en 0091). C'est ce calcul qui choisit la teinte claire ou sombre du verre : probable cause du blanc/noir.
- 0 faute 0x51, 2 fautes MMU, 0 plantage.

# Build B1L-0096 : l'écran lit directement l'image du GPU (zéro copie)

Date : 2026-11-08. Base : B1L-0095.

## Changements (noyau seulement)

1. **Attendre le GPU avant de montrer** (`B2LFramebuffer.cpp`) : chaque surface d'un swap passe par `HakiosMaliDrawn()` (100 ms au plus) avant d'être montrée. Corrige les images à moitié dessinées.
2. **Zéro copie** (`B2LFramebuffer.cpp`, `b2l_zc_*`) :
   - registres de l'écran projetés : DECON-F `0x19030000`, DMA `0x19070000` (blocs IDMA à `+0x1000 × (n+1)`), d'après `dpu20/cal_9820` de Samsung (LineageOS `android_kernel_samsung_exynos9820`, `lineage-18.1`) ;
   - au démarrage : registres utiles écrits dans le journal (`B2L ZC:`), et recherche du canal DMA dont `IDMA_IN_BASE_ADDR_Y` (+0x40) vaut `0xCA000000` (la mémoire du panneau) ;
   - à chaque swap d'une surface plein écran que l'écran peut lire : son adresse physique écrite dans ce registre, puis demande de prise en compte à DECON (`SHADOW_REG_UPDATE_REQ` +0x60, bits global + fenêtres), et **attente que DECON l'ait prise** (≤ 50 ms) : double tampon au rythme du panneau ;
   - si DECON ne la prend pas : retour à `0xCA000000` et à la copie, pour tout le boot. Boot-arg `b2l_fb_zc=0` : désactivé.
3. **Surfaces d'écran de backboardd** (`B2LSurface.cpp`) : une surface BGRA de la taille du panneau créée par backboardd (4 au plus) reçoit une mémoire **physiquement contiguë sous 4 Gio, en écriture combinée** (`scanoutPhys`). L'écran peut la lire telle quelle.
4. **Plus de nettoyage de cache pour les surfaces en écriture combinée** (`flushSurface`) : le panneau et ces surfaces ne passent jamais par le cache du processeur. C'était jusqu'à 2,9 s de CPU par 10 s.
5. `B2L PERF` compte aussi les bascules (`flip`).

Mesa inchangé. Ensemble `b2l-metal-v52`, plug-in `B2LMetalv52`.

## Risques

- L'écran peut afficher n'importe quoi si une IOMMU est active devant le DMA de l'affichage : le journal dira ce que contiennent les registres, et le retour en arrière est `out/b1l-metal-0095/boot.img`.

## Installé

- 1er essai : erreur de compilation (`b2l_zc_on` utilisé dans `start()` avant sa déclaration), rien d'écrit ; corrigé par `b2l_zc_active()`.
- **Vérifié :** le noyau contient les messages `B2L ZC`.
- **SYSTEM :** SYSTEM-OK en 67 s. **BOOT :** relecture `f9849c5c90c10926f34b9d52509b4eac119efa7c7d42b64de30f8c40ed0898a8` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0095/boot.img`.

## Résultat (journal `out/logs/0096-boot.txt`)

- Toujours des éléments qui **clignotent et apparaissent un par un** (Centre de contrôle, certaines apps) ; l'affichage ne passe visiblement pas en direct.
- **`flip 0` partout : le zéro copie n'a jamais basculé.** Les blocs IDMA lus à `0x19071000 + 0x1000 n` sont à 0 (enable, tailles, base) : ce ne sont pas ceux qui lisent l'image sur ce téléphone. Lignes `B2L ZC` mélangées avec celles des autres cœurs au démarrage.
- Aucune surface d'écran contiguë créée : la surface 2 de backboardd a **5888 octets par ligne** (le panneau 5760), donc refusée, et toujours nettoyée à chaque envoi (jusqu'à 1,9 s / 10 s).
- **Cause du dessin progressif :** backboardd alterne entre la surface 1 et la surface 2, et **la surface 1 est la mémoire même que l'écran affiche**. Il dessine dedans en direct. L'attente du GPU ne pouvait rien y faire.
- 0 faute 0x51, 8 fautes MMU, 0 plantage.

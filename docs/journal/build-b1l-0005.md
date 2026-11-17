# Build B1L-0005 : écran logique 880×1912 en @2x (440×956 points)

Date : 2026-11-04. Statut : **construit et vérifié hors ligne, pas installé.**
Une seule modification par rapport à B1L-0003 : la taille et l'échelle d'écran présentées à iOS.
Firmware tactile, SYSTEM et USERDATA inchangés (BOOT seulement).

## Pourquoi

- Le rendu est fait par le CPU en 1440×3040 à @3x, soit **480×1013,33 points**. Ce n'est la taille d'aucun iPhone, et elle n'est pas entière.
- Les fonds d'écran du système ne prévoient que 393×852 … **440×956** (D94, iPhone 16 Pro Max) et « any ».
- 880×1912 donne **2,6× moins de pixels** que 1440×3040 (1 682 560 contre 4 377 600), à la taille exacte de l'iPhone émulé.

## Modifications

| Fichier | Changement |
| --- | --- |
| `xnu-overlay/iokit/Kernel/arm/B2LBlit.h` | `b2l_blit_scaled` : canevas logique vers le panneau, plus proche voisin, réutilisation des lignes répétées, mêmes contrôles de bornes que `b2l_blit` |
| `xnu-overlay/iokit/Kernel/arm/B2LFramebuffer.cpp` | boot-args `b2l_fb_lw` / `b2l_fb_lh` (au plus ×2), surface par défaut à la taille logique (leçon du S7), `GetDisplaySize` logique, copie mise à l'échelle dans `swap()` |
| `xnu-overlay/bsd/kern/b2l_profile_cache.h` | génération de profil **v3** : MobileGestalt régénère une fois son cache (taille et échelle d'écran) |
| `uefi/XnuLoader/BoardDisplay.h` (nouveau), `DeviceTree.c`, `XnuLoader.c`, `B2LXnuPkg.dsc`, `scripts/build-efi.sh` | `display-scale` et `artwork-scale-factor` réglables (`B2L_DISPLAY_SCALE`), boot-args ajoutés (`B2L_FB_LOGICAL=WxH`). **Par défaut : 1:1 en @3x, S10+ inchangé** |
| `tests/test_blit_scaled.py` (nouveau) | comparaison pixel par pixel avec une référence (ASan/UBSan) |

Construction : `B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 B2L_FB_LOGICAL=880x1912 B2L_DISPLAY_SCALE=2 scripts/build-boot-firmware.sh`.
L'étirement est de 1440/880 = 1,636 à l'horizontale et 3040/1912 = 1,590 à la verticale (environ 3 % d'écart).
Il n'y a pas de bandes noires, et le tactile, normalisé, n'a pas besoin d'être recalculé.

## Vérifications

- `tests/test_blit_scaled.py` : écran entier 880×1912 et 960×2026, rectangle partiel, identité, ratios impairs et refus des bornes invalides.
- `scripts/test-loader.sh` : tous les tests passent (profil par défaut).
- `tests/test_framebuffer_swap.py` échouait **déjà avant** ce changement : il attend des compteurs (`b2l_fb_received`…)
  absents du `B2LFramebuffer.cpp` actuel. À réparer à part.
- Kernel : 0 erreur, 0 instruction RCpc ; loader : contient `b2l_fb_lw=880 b2l_fb_lh=1912`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-display-0005/boot.img` (25 893 504 octets) | `669e35c60b530980858369cff41d4e885551da0ee57aa3256135eee227cdcc62` |
| Kernel | `368f04d8df1dfa773e52275a62e132fdf494d55e730869d5c780fea83de73de9` |
| Firmware | `503dd43ad53e7ce226fe99c0a09c156efc8af0c8aa6547362c1af240d2de14d7` |

## Attendu, risques et retour arrière

- Journal : `B2L FB: AppleCLCD 1440x3040 … logical 880x1912 (scaled)`, `B2L PROFILE: previous MobileGestalt cache backed up`.
- Risques : ressources @2x absentes pour D94 (repli sur @3x réduit, icônes possiblement floues), copie ×1,6 coûteuse en CPU,
  et premier démarrage un peu plus long (régénération MobileGestalt).
- Retour arrière : BOOT `out/b1l-touch-0003/boot.img` (`0fbcf3ba…`).

## Résultat

À remplir.

# B1L-0217 : redémarrage corrigé dans le menu

**Test de 0216 par le propriétaire :** le menu est bien, mais les deux entrées « Reboot » affichent « Reboot failed ».

**Cause :** le reset du firmware Silicium (S5E9820Pkg `ResetSystemLib`) écrit `1` dans PMU SWRESET (`0x15863A00`).
C'est le bit du 9810. Le noyau Samsung exynos9820 (`exynos-reboot.c`) fait autrement :
1. il remet `RST_STAT` (0x0404) à 0 ;
2. il écrit `1 << 1` dans SWRESET.

**Correction :** le chargeur fait lui-même ces deux écritures, puis appelle `ResetSystem` en secours.
Commit `refactor/arm64e-uefi` du 2026-11-14. Le firmware Silicium n'est pas modifié.

| Élément | SHA-256 |
|---|---|
| boot.img | `ff445a94…0402` (`beyond2lte-ios/out/b1l-menu-0217/`) |
| Noyau | `3b937e6c…89526` (le même que 0213) |
| Firmware | `d9506d37…733a` |
| Chargeur | `cd8308b6…11b4` |

Flash : BOOT-OK. Puis redémarrage vers le système.

## Résultat

**Propriétaire, 2026-11-14 : « Ça marche ».** Points testés :
1. « Reboot » redémarre-t-il le téléphone ?
2. « Reboot to recovery » arrive-t-il dans TWRP ?

## Pour revenir en arrière

Reflasher `b1l-menu-0216/boot.img` (`baccf318…`).

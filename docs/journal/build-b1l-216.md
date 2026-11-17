# B1L-0216 : menu de démarrage en grand, en anglais

**Le noyau est celui de 0213, à l'identique.** Seul le chargeur UEFI change par rapport à 0215 : le menu est maintenant
dessiné en grand sur l'écran (police Spleen 32x64), en anglais, au lieu du petit texte de la console.

Aperçu rendu sur le Mac : `docs/boot-menu-0216-apercu.png`.

| Élément | SHA-256 | Origine |
|---|---|---|
| boot.img | `baccf318…812a` (27 031 584 o) | `beyond2lte-ios/out/b1l-menu-0216/` |
| Noyau | `3b937e6c…89526` | le même que 0213 et 0215 |
| Firmware | `4af3f478…45f6` | `B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 scripts/build-boot-firmware.sh` |
| Chargeur | `dcd91e32…f1fb` (76 288 o) | branche `refactor/arm64e-uefi`, commit `0342b61` |

Flash BOOT avec `flash-boot.sh` : relecture `baccf318…` → **BOOT-OK**. Puis redémarrage vers le système.
SYSTEM et USERDATA n'ont pas été touchés.

## L'écran

- Titre « iOS 26 », puis quatre cartes : **Boot iOS**, **Stay here**, **Reboot to recovery** (untested), **Reboot**.
- La carte choisie est en bleu.
- Compte à rebours « Booting iOS in N seconds » avec une barre.
- Aide en bas de l'écran : Volume +/-, Power, Bixby.
- Sans GOP, retour au menu texte (lui aussi en anglais).

## Pour revenir en arrière

Reflasher `beyond2lte-ios/out/b1l-menu-0215/boot.img` (`c58655ab…`) ou `b1l-metal-0213/boot.img` (`3bb1295f…`).

## Résultat

À remplir après le test du propriétaire. Points à vérifier :
- la taille du texte ;
- le rafraîchissement du compte à rebours (pas de scintillement) ;
- le démarrage d'iOS après 5 s.

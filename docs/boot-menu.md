# Menu de démarrage du chargeur XNU

Le chargeur UEFI (`uefi/XnuLoader`) affiche un menu avant de lancer XNU.

L'écran est dessiné en grand, en anglais, par `BootMenuDraw.c` dans un tampon puis copié sur l'écran par le GOP
(`Blt`). Sans GOP ou sans mémoire pour le tampon, le menu retombe sur la console texte du firmware.
Mise en page prévue pour le panneau du S10 (1440 × 3040) ; sur un autre écran, le dessin est seulement rogné.

Police : Spleen 32x64 et 16x32 de Frederic Cambus, licence BSD-2-Clause, commit `cfde412`
(https://github.com/fcambus/spleen). Les en-têtes `FontSpleen*.h` sont générés par `tools/bdf2c.py` et
reproduisent la notice de licence.

| Touche | Effet |
|---|---|
| Volume + / Volume − | monter / descendre dans le menu |
| Power (appui court, à la relâche) | valider |
| Bixby | arrêter le compte à rebours |
| Volume − maintenu au démarrage | menu sans compte à rebours |
| Volume + maintenu au démarrage | menu, curseur sur « recovery » (il faut encore valider avec Power) |

Entrées :

1. **Boot iOS** (démarrer iOS), choisi automatiquement après 5 s sans touche. Option de build : `-D B2L_MENU_TIMEOUT_S=n` ; 0 veut dire pas de démarrage automatique.
2. **Stay here** (ne pas démarrer) : le téléphone attend, sans chien de garde. Une touche ramène au menu.
3. **Reboot to recovery** : écrit `INFORM3 = 0x12345674` et `SYSIP_DAT0 = 0xFF` dans le PMU (`0x15860000`), puis demande un reset. Ce sont les valeurs du noyau Samsung exynos9820 (`drivers/samsung/sec_reboot.c`, `drivers/soc/samsung/exynos-reboot.c`). **Pas encore testé sur le téléphone.**
4. **Reboot** : comme le noyau Samsung (`exynos-reboot.c`), `RST_STAT = 0` puis `SWRESET (0x3A00) = 1 << 1` dans le PMU ; `ResetSystem(EfiResetCold)` en secours. Le reset du firmware Silicium écrit `1` (valeur du 9810) et ne fait rien sur le S10 (vu en B1L-0216).

## Garanties

- **Échec avant la sortie des services UEFI** (noyau introuvable ou invalide, mémoire, vidéo, carte mémoire finale) : les messages restent à l'écran jusqu'à une touche. Le menu revient ensuite sans compte à rebours, donc un noyau cassé n'est jamais relancé en boucle.
- **Échec après la sortie des services UEFI** : seule la barre de progression peut encore parler. Carré 0 rouge = carte mémoire refusée, carré 1 rouge = sortie refusée. Le processeur s'arrête ; maintenir Power pour redémarrer.
- **Option mémorisée** : c'est une variable UEFI qui place seulement le curseur. Le compte à rebours démarre toujours iOS, donc un « redémarrer » mémorisé ne peut pas tourner en boucle. Sur ce firmware les variables vivent en RAM (`PcdEmuVariableNvModeEnable=TRUE`) : le choix survit à un retour au menu, pas à un redémarrage.
- **Aucune écriture de partition.**
- **Sans clavier ni console**, le chargeur démarre directement, comme avant.

## Tests

- `tests/boot_menu_test.c` (lancé par `scripts/test-loader.sh`) : compte à rebours, navigation, validation, anti-rebond, démarrages forcés.
- `tests/boot_menu_draw_test.c` : dessin dans les limites (garde après la dernière ligne, marge de ligne intacte, petit écran). Avec un nom de fichier en argument, il écrit l'écran en PPM pour le regarder.
- Compilation de l'EFI : `scripts/build-efi.sh`, dans une copie de l'espace EDK2 (`cp -c -R` d'un `mu-work-*`), pour ne pas toucher l'EFI qu'utilise `b1l-build.sh`.

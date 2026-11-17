# Build 0138 : diagnostic pré-modification (pilotes S10)

## Diagnostic

Le téléphone est un Galaxy S10 (SM-G973F, beyond1lte, Exynos 9820), série <SERIAL>, TWRP installé, adb disponible.

**Noyau actuel :** arbre xnu patché B2L dans `beyond2lte-ios/xnu/` avec overlay dans `beyond2lte-ios/xnu-overlay/`. Le dernier build (0137) a recompilé, packagé et flashé le BOOT ; le téléphone démarre et atteint iOS. GPU Mali-G76 (HakiosMali), IOSurface (B2LSurface), framebuffer DECON/IDMA, écran, tactile (HID) et stockage UFS fonctionnent.

**Device tree flashé :** `device-s10/dtb/merged.dtb` est bien un DTB beyond1lte (`Samsung BEYOND1LTE EUR OPEN 26 board based on EXYNOS9820`, compatible `samsung,BEYOND1LTE EUR OPEN 26` et `samsung,EXYNOS9820`). Les nœuds suivants y sont présents (`status = "okay"`) :

| Périphérique | Nœud DT | Compatible |
|--------------|---------|------------|
| WiFi | `wlan` | `samsung,brcm-wlan` |
| Bluetooth | `bluetooth` | `samsung,bcm43xx` |
| Audio haut-parleur/micro | `sound` | `samsung,beyond-madera` |
| PMIC audio | `max77705@66` | `maxim,max77705` (`max77705,support-audio`) |
| Prise jack / USB-C | `muic` | — |
| Haptique / vibration | `haptic` | type `LINEAR_1040`, régulateur `VDD_MOTOR_3P3`, `pwm_id = 1` |
| PWM haptique | `vib-pwm` | pins `gpp3-1` |
| Boost moteur | `motor-boost-en` | pins `gpg1-7` |
| Capteurs | `BCM4773@0` | `ssp,BCM4773` (hub SSP sur SPI) |

**Pilotes déjà présents dans l'overlay :** aucun pilote WiFi, Bluetooth, audio ou haptique n'existe dans `xnu-overlay/iokit/Kernel/arm/` ni dans `xnu/bsd/dev/` (recherche par noms `*wifi*`, `*bt*`, `*bluetooth*`, `*audio*`, `*haptic*`, `*madera*`, `*bcm*` : aucun résultat). Seul le GPU Mali, IOSurface, l'écran, le tactile et l'UFS sont couverts.

**Dépôt hakios-xnu-kernel (cloné) :** tous les pilotes existent (`drivers/wifi/`, `drivers/bluetooth/`, `drivers/haptic/`, `drivers/audio/`, `drivers/power/`) mais ciblent le POCO X5 Pro (Qualcomm WCN6750, Awinic AW86927/AW88261, Adreno 642L, SPMI/RPMh/SMEM). Aucun ne se greffe tel quel sur l'Exynos 9820 ; ils servent de référence de structure uniquement (bring-up par argument `N=1..5`, chargement de firmware depuis `/fw`, gestion propre des erreurs).

**Firmware propriétaire :** WiFi (Broadcom), Bluetooth (BCM43xx), audio (Madera/max77705) et haptique (formes d'onde) doivent être extraits du système Android du S10 ou déclarés manquants. Aucun n'est présent dans le dépôt.

## Changement

Aucune modification de code dans ce build. Diagnostic uniquement, avant toute intégration de pilote.

Le périphérique prioritaire à intégrer en premier est le **WiFi** (décision du protocole : wifi → audio/jack → haptique → bluetooth → capteurs), suivi de l'audio haut-parleur, puis microphone/jack/USB-C, puis haptique, puis bluetooth, puis capteurs.

## Installé avec hash BOOT

Aucun nouveau noyau n'est compilé ni flashé pour ce build de diagnostic. Le BOOT actuellement flashé est celui du build 0137 (hash BOOT inchangé, voir `flash-0137-boot.log`).

## Résultat

Diagnostic confirmé : le DTB beyond1lte déclare tous les nœuds nécessaires (WiFi, Bluetooth, audio Madera/max77705, muic, haptique LINEAR_1040, capteurs BCM4773). Aucun pilote correspondant n'est encore intégré dans l'arbre xnu. Le téléphone démarre et atteint iOS sur le build 0137. Aucun risque de régression pour ce build (aucun changement de code).

Prochaine étape : build 0139 — intégration du pilote WiFi Broadcom BCM43xx (nœud `samsung,brcm-wlan`).

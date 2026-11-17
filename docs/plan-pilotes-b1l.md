# Plan d'intégration des pilotes Galaxy S10 (beyond1lte, Exynos 9820)

**Objectif :** intégrer ou adapter depuis https://github.com/oosexgang/hakios-xnu-kernel les pilotes et le support noyau pour le Galaxy S10 (SM-G973F, beyond1lte, Exynos 9820) : WiFi, Bluetooth, audio (haut-parleur, microphone, prise jack/USB-C), vibration/haptique, et éventuellement d'autres périphériques manquants pour que iOS 26 puisse utiliser le matériel du téléphone.

**Protocole :** parler uniquement en français. Un changement par build ; un diagnostic peut accompagner ce changement. Documenter chaque build dans `docs/build-b1l-NNNN.md` et mettre à jour le suivi de build (`progress/`). Raisonner sur des preuves (journal, photos, code lu), préférer un diagnostic décisif à un correctif deviné. Ne pas casser ce qui marche (GPU Mali-G76 / HakiosMali, IOSurface / B2LSurface, écran DECON/IDMA, tactile HID, clavier à l'écran, stockage UFS).

---

## 1. État actuel (diagnostic initial)

### Ce qui marche déjà (à ne pas casser)
- GPU Mali-G76 (HakiosMali, `B2LMali*` dans `xnu-overlay/iokit/Kernel/arm/`)
- IOSurface (B2LSurface)
- Framebuffer DECON/IDMA, écran
- Tactile (HID, `B2LInput.cpp`, `B2LHID.cpp`)
- Clavier à l'écran
- Stockage UFS (`b2l_ufs_*.c` dans `bsd/dev/`)
- `B2LWifi` existe (étage 1 : pinctrl et `WL_EN`) ; aucun pilote audio, Bluetooth ou haptique n'existe encore dans `xnu-overlay/` ni dans `xnu/`

### Ce que déclare déjà le device tree beyond1lte
Fichier de référence : `ref-dts/exynos9820-beyond1lte_eur_open_26.dts` (compatible `samsung,BEYOND1LTE EUR OPEN 26`, `samsung,EXYNOS9820`). Nœuds déclarés avec `status = "okay"` :

| Nœud | Compatible | Rôle |
|------|-----------|------|
| `wlan` | `samsung,brcm-wlan` | WiFi Broadcom |
| `bluetooth` | `samsung,bcm43xx` | Bluetooth Broadcom BCM43xx |
| `haptic` | (moteur `LINEAR_1040`, régulateur `VDD_MOTOR_3P3`, `pwm_id = 1`) | Vibration/haptique |
| `vib-pwm` | pins `gpp3-1` | PWM du moteur |
| `motor-boost-en` | pins `gpg1-7` | Boost du moteur |
| `sound` | `samsung,beyond-madera` | Codec audio Cirrus Logic Madera |
| `max77705` | `maxim,max77705` (`max77705,support-audio`) | PMIC audio |
| `muic` | — | Détection USB-C / prise jack |
| `BCM4773` | `ssp,BCM4773` | Hub de capteurs SSP (SPI) |
| `qbt2000` | `qcom,qbt2000` | — |

⚠️ À vérifier au premier build : que le DTB flashé est bien **beyond1lte** (et non beyond2lte) et que ces nœuds sont présents et activés (`status = "okay"`).

### Firmware propriétaire (à extraire du système Android du S10, absent du dépôt)
- WiFi : firmware Broadcom (`/vendor/firmware`)
- Bluetooth : firmware/HCD BCM43xx
- Audio : firmware/codec Madera + max77705
- Haptique : formes d'onde (waveforms) du moteur linéaire

À déclarer "manquant" si l'utilisateur ne peut pas les extraire.

---

## 2. Inventaire de hakios-xnu-kernel (dépôt cloné)

Tous les pilotes existent mais **ciblent le POCO X5 Pro (Qualcomm SM7325/SC7280)**, pas l'Exynos 9820. Aucun ne se greffe tel quel sur le S10.

| Sous-système | Pilote hakios | Matériel visé | Incompatibilité S10 |
|--------------|--------------|---------------|---------------------|
| `wifi` | `hakios.wifi.c`, `.wlfw`, `.lx`, `.sta`, `.apple80211`, `hakios.wlan.cpp` | WCN6750 (Qualcomm) + ath11k, SMEM/SMP2P/IPCC/RPMh | S10 = Broadcom BCM43xx |
| `bluetooth` | `hakios.bt.c` | WCN6750 BT half (UART/H4, read-only) | S10 = BCM43xx |
| `haptic` | `hakios.haptic.c` | Awinic AW86927 + LRA (I2C POCO X5 Pro) | S10 = moteur `LINEAR_1040` |
| `audio` | `hakios.audio/.sound/.speaker/.mic/.adsp/.guard` | ADSP Qualcomm + amplis AW88261 sur TDM | S10 = codec Cirrus Madera + max77705 |
| `gpu` | `hakios.adreno.c` | Adreno 642L (SC7280) | S10 = Mali-G76 (déjà fait, HakiosMali) |
| `power` | `hakios.pmic/.battery/.cpufreq/.thermal` | SPMI arbiter, RPMh, SMEM (Qualcomm) | S10 = max77705 + domaines Exynos |
| `modem` | `hakios.mpss/.cell/.qmi/.nas/.ipa` | Modem Qualcomm | S10 = Exynos modem, recherche seulement |
| `input` | `hakios.input/.goodix/.buttons` | Goodix GT9916 / FocalTech FT3680 (POCO X5 Pro) | S10 = tactile déjà fait (HID) |
| `core` | `hakios.platform/.socinfo/.version/.stubs` | Identité SoC Qualcomm (SMEM item 137) | À adapter pour Exynos 9820 |

**Usage prévu de hakios :** référence de structure uniquement — pilote `bsd/dev` + objet IOKit, bring-up progressif par argument de démarrage (`N=1..5`), chargement de firmware depuis `/fw`, gestion propre des erreurs, lecture seule aux niveaux bas. Chaque pilote S10 devra être écrit/adapté pour le matériel réel du téléphone.

---

## 3. Plan d'intégration par build

État au 10/11/2026 : les builds **0138** (diagnostic), **0139**, **140** (WiFi étage 1) et **141** (DT synthétique) sont réalisés et testés. La numérotation reprend à **142**. Les livrables utilisent le format `docs/build-b1l-NNNN.md`.

### Build 140 — WiFi, étage 1 : pinctrl et WL_EN (fait)
- Pilote `B2LWifi` : vérification de `/wlan`, lecture de `gpg0`/`gpa0`, assertion de `cfg_wlanen` (`gpg0-5`).
- Résultat : boot iOS sans régression, `WL_EN` asserté (`con=0x100000`, `dat=0x21`), `/wlan` absent du DT synthétique (`dt=0`). Les offsets `gpg0 = 0x080` (PERIC0) et `gpa0 = 0x000` (ALIVE) sont confirmés par `pinctrl-exynos-arm64.c`.
- Limite : aucun pilote PCIe/CMU/PMU/PHY ni firmware Broadcom ; le WiFi fonctionnel reste bloqué à cet étage.
- Livrable : `docs/build-b1l-140.md`.

### Build 141 — Device tree synthétique : `/wlan` et pinctrl (fait)
- Ajouter `/wlan` (`samsung,brcm-wlan`, `gpios`, `pinctrl-0`) et les nœuds `pinctrl@10430000` (`gpg0`, `cfg_wlanen`) et `pinctrl@15850000` (`gpa0`, `wlan_host_wake`) dans `uefi/XnuLoader/DeviceTree.c`.
- Aucun accès matériel : le changement rend le DT cohérent avec le matériel et fournit la base dont les pilotes suivants auront besoin.
- Résultat : `dt=1`, `gpios = <0x8e 5 0x1 0x134 7 0x1>`, boot iOS sans régression.
- Livrable : `docs/build-b1l-141.md`.

### Build 142 — WiFi, audit lecture seule CMU/PMU/pinctrl (FSYS1)
- Pilote `B2LWifi` étendu : audit en lecture seule du CMU FSYS1 (`0x13c00000`) — portes QCH APB/DBI/MSTR/PCS/PHY et registres de gating d'horloge PCIe — plus les registres pinctrl déjà lus.
- Aucun accès au contrôleur PCIe (ELBI/DBI/PHY) tant que les horloges et domaines de puissance FSYS1 ne sont pas maîtrisés.
- Rien n'est écrit : lecture uniquement, pour décider si le build 143 peut démarrer les horloges PCIe.
- Livrable : `docs/build-b1l-142.md`.

### Build 143 — WiFi, activation des portes QCH PCIe GEN2 (CMU FSYS1)
- Écrire ENABLE (bit 0 = 1) sur les 5 portes QCH PCIe GEN2 du CMU FSYS1 (APB `0x3034`, DBI `0x3038`, MSTR `0x303c`, PCS `0x3040`, PHY `0x3044`).
- Lecture après écriture pour confirmer que bit 0 = 1 sur chaque registre.
- Aucun accès au contrôleur PCIe (ELBI/DBI/PHY), rien n'est écrit ailleurs.
- Livrable : `docs/build-b1l-143.md`.

### Build 144 — Audio haut-parleur (décalé)
- Codec Cirrus Logic CS47L93 (`samsung,beyond-madera`, SPI2 à `0x10470000`, reset `gpg1-0`) et amplis CS35L40 (HSI2C38 à `0x109A0000`, adresses `0x40`/`0x41`), sur les voies ABOX (UAIF/SIFS/RDMA).
- Première étape : diagnostic de disponibilité (horloges ABOX, pinmux SPI2/HSI2C38, lecture d'identité), avant toute écriture.
- Firmware nécessaire : `cs47l93-dsp1.wmfw` et firmware des CS35L40 — à extraire ou déclarer manquant.
- **Preuve attendue :** son audible sur le haut-parleur (log + test d'écoute).
- Livrable : `docs/build-b1l-144.md`.

### Build 145 — Audio microphone + prise jack / USB-C (décalé)
- Microphone via le codec Madera ; détection de la prise jack/USB-C par le nœud `muic` et le max77705 (`max77705,support-audio`).
- **Preuve attendue :** détection du micro, bascule jack/USB-C dans les logs.
- Livrable : `docs/build-b1l-145.md`.

### Build 146 — Vibration / haptique (décalé)
- Moteur `LINEAR_1040` : PWM `vib-pwm` (gpp3-1), boost `motor-boost-en` (gpg1-7), régulateur `VDD_MOTOR_3P3`.
- **Preuve attendue :** log noyau (détection du moteur) + vibration physique ressentie.
- Livrable : `docs/build-b1l-146.md`.

### Build 147 — Bluetooth (décalé)
- Contrôleur Broadcom BCM43xx (nœud `samsung,bcm43xx`), UART/H4 + firmware.
- Dépend du WiFi (module radio et lignes d'alimentation communes).
- **Preuve attendue :** détection du contrôleur BT, appairage dans le log noyau.
- Firmware nécessaire : firmware/HCD BCM43xx — à extraire ou déclarer manquant.
- Livrable : `docs/build-b1l-147.md`.

### Build 148+ — Capteurs (recherche, hors périmètre immédiat)
- Hub `BCM4773` (SSP, SPI) — à évaluer.
- Modem/RIL : marqué "recherche" chez hakios (`drivers/modem/`, QMI, IPA) — Qualcomm, non applicable tel quel à l'Exynos 9820.

---

## 4. Règles à respecter pour chaque build (protocole)

1. **Un seul changement par build** ; un diagnostic peut l'accompagner.
2. **Français uniquement** (comptes-rendus, messages, boutons).
3. **Documenter** : `docs/build-NNNN.md` avec les sections *Diagnostic*, *Changement*, *Installé avec hash BOOT*, *Résultat* ; et mettre à jour le suivi de build (`progress/`).
4. **Preuve avant correctif :** raisonner sur le journal, le code lu, ou un test physique ; préférer un diagnostic décisif à un correctif deviné.
5. **Ne pas casser ce qui marche :** GPU Mali, IOSurface, écran, tactile doivent rester fonctionnels.
6. **Flash et test :** noyau recompilé (`build-kernel.sh` avec `KDKROOT` et `B2L_DEPSROOT`), packagé dans le BOOT, flashé (`flash-boot.sh`), le téléphone doit démarrer jusqu'à iOS.
7. **Après le dernier flash** (si aucun autre build prévu) : redémarrer vers iOS avec `adb -s <SERIAL> reboot`.
8. Le propriétaire teste, remet le téléphone en TWRP, et dit ; je lis alors le journal.

---

## 5. Livrables attendus

- Une branche ou copie de travail dans `~/Developer/s10-port/beyond2lte-ios/xnu-overlay/` (et `xnu/`) intégrant les pilotes choisis.
- Un noyau recompilé, packagé, et flashé (BOOT) qui démarre et atteint iOS.
- Si possible, une preuve que le pilote fonctionne (log noyau, détection du périphérique, son, vibration, ou WiFi).
- Documentation : `docs/build-b1l-NNNN.md` (Diagnostic, Changement, Installé avec hash BOOT, Résultat) + mise à jour du suivi de build (`progress/`).

---
*Plan établi le 09/11/2026, révisé le 10/11/2026 après le build 140 (WiFi étage 1).*
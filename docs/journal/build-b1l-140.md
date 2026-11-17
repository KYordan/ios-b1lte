# Build 140 : premier étage du pilote WiFi (pinctrl + WL_EN)

## Diagnostic

Le WiFi du Galaxy S10 (beyond1lte, Exynos 9820) est un Broadcom `samsung,brcm-wlan` sur le bus **PCIe0** (`/pcie0@13ED0000`, `status = "okay"`, 1 lane, max-link-speed 1). Le nœud `/wlan` du DTB flashé déclare :

- `gpios = <0x8e 5 1 0x134 7 1>` : `gpg0-5` = `cfg_wlanen` (enable), `gpa0-7` = `wlan_host_wake` ;
- `pinctrl-0 = <0x17c 0x13a>` : `cfg_wlanen` et `wlan_host_wake`.

Les pinctrl sont les mêmes blocs que ceux déjà utilisés par `B2LInput` :

| Bloc | Adresse | Banque | Offset | Registres |
|------|---------|--------|--------|-----------|
| PERI0 | `0x10430000` | `gpg0` | `0x080` | CON `0x00`, DAT `0x04`, PUD `0x08`, DRV `0x0c`, CON_PDN `0x10`, PUD_PDN `0x14` |
| ALIVE | `0x15850000` | `gpa0` | `0x000` | idem |

Les banques sont espacées de `0x20` ; l'ordre du DT (`gpp0..gpp3`, `gpg0`, `gpg1`, `gpg2`, `gpg4`) donne bien `gpg0` à `0x080`, ce que confirme la table Exynos 9810 PERIC0 de la même génération. `B2LInput` lit déjà `gpa2` à `0x44` (`gpa0` + 2 × `0x20` + DAT `0x04`), ce qui valide la disposition.

Configuration DT de `cfg_wlanen` : fonction `1` (sortie GPIO), pull désactivé, drive `3`. `wlan_host_wake` : fonction `15` (EINT), pull-up. La largeur du champ fonction est de 4 bits par pin (la valeur 15 ne tient que dans 4 bits) ; le pull est sur 2 bits par pin.

Le contrôleur PCIe Exynos n'a **aucun pilote** dans l'arbre XNU (pas de driver PCIe/SDIO/MMC), et ses horloges/domaines de puissance/PHY ne sont pas initialisés. Aucun registre PCIe n'est donc touché dans ce build : une lecture ELBI/DBI sans horloge peut bloquer le bus.

## Changement

Nouveau pilote `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/iokit/Kernel/arm/B2LWifi.cpp`), enregistré comme service IOKit `B2LWifi` sur `IOResources` dans `xnu/iokit/KernelConfigTables.cpp`, et ajouté à `xnu/iokit/conf/files`.

Au démarrage, le pilote :

1. vérifie `/wlan` dans le device tree et journalise ses `gpios` ;
2. mappe `pinctrl@10430000` et `pinctrl@15850000` (4 Kio chacun) ;
3. lit et journalise l'état brut de `gpg0` (CON/DAT/PUD/DRV/CON_PDN/PUD_PDN) et de `gpa0` (CON/DAT/PUD) ;
4. si `cfg_wlanen` n'est pas déjà sortie à l'état haut : désactive le pull, pose le bit DAT 5, puis passe la fonction 5 à `1` (sortie) ; relit l'état et `wlan_host_wake` ;
5. écrit une trace dans le journal persistant B2L (`b2l_diag_record`) et dans `IOLog`.

Interrupteurs : `b2l_wifi=0` ou `b2l_wifi_nowrite=1` pour ne rien écrire (lecture seule). Sans argument, WL_EN est asserté.

Aucun accès PCIe, aucun changement GPU/Mali, IOSurface, framebuffer, tactile ou UFS.

## Installé avec hash BOOT

- Build : **140**, SET `b2l-metal-v88` (libs réutilisées, `--no-libs`).
- Noyau : `out/xnu-b2l-ref/obj/DEVELOPMENT_ARM64_B2L/mach.development.b2l`, compilation `B2LWifi.cpo` confirmée, symboles `B2LWifi::start/stop/free` présents.
- BOOT : `out/b1l-metal-140/boot.img`, 26 895 456 octets.
- Hash BOOT : `35a9679ad4d80092b580e491851dbff1269d586020ca68406b977f7660b64f90`.
- SYSTEM : 6 blocs différents (24 Mio écrits), relecture complète OK.
- BOOT flashé et relu (`BOOT-OK`), le 10/11/2026.

## Résultat

**Boot réussi, aucune régression.** Le téléphone a démarré jusqu'à iOS : `backboardd` et HakiosMali sont actifs dans le journal (`hakiosmetal[backboardd 62]`), l'écran et le tactile restent fonctionnels.

Le journal persistant contient les deux traces attendues :

```
B2L WIFI: dt=0 gpio=<0 0 0 0 0 0> gpg0[con=0 dat=0x1 pud=0x11131113 drv=0] wl_en=0/0/0 wake=0/0/0
B2L WIFI: WL_EN asserted con=0x100000 dat=0x21 host_wake=0
```

Lecture :

- le pilote `B2LWifi` a bien été instancié sur `IOResources` et a exécuté son `start()` ;
- `/wlan` est **absent** du device tree vu par XNU (`dt=0`). XNU ne reçoit pas le DTB Samsung complet, mais l'arbre synthétique construit par `uefi/XnuLoader/DeviceTree.c` (`chosen`, `product`, `cpus`, `arm-io`, `framebuffer`). Les adresses codées en dur étaient donc nécessaires ; le pilote n'a pas dépendu du DT ;
- `gpg0` avant écriture : `con=0` (toutes les pins en fonction 0, entrée), `dat=0x1` (gpg0-0 haut), `pud=0x11131113`. Les 16 bits bas se décodent pin par pin : gpg0-0 = pull-up (3), gpg0-2 et gpg0-4 = pull-down (1), gpg0-5 = flottant (0). Ces valeurs correspondent à celles du noyau TWRP (`pin 143` PUD 3, `pin 148` PUD 0), ce qui valide l'offset `gpg0 = 0x080` et la disposition CON/DAT/PUD/DRV ;
- après écriture : `con=0x100000` (champ de fonction de gpg0-5 = 1, sortie), `dat=0x21` (bit 5 haut ; le bit 0 était déjà haut), `host_wake=0`. `WL_EN` est bien asserté côté SoC, sans blocage du démarrage ;
- `wlan_host_wake` (gpa0-7) reste à 0. C'est cohérent : la puce Broadcom n'est ni alimentée ni horlogée (aucun pilote PCIe, aucune horloge, aucun domaine de puissance, pas de firmware).

### Confirmation des adresses pinctrl

La table `exynos9820_pin_banks5` (PERIC0) de `pinctrl-exynos-arm64.c` (LineageOS 22.2) donne :

| Banque | Offset | Pins |
|--------|--------|------|
| gpp0 | 0x000 | 114–121 |
| gpp1 | 0x020 | 122–129 |
| gpp2 | 0x040 | 130–137 |
| gpp3 | 0x060 | 138–142 |
| **gpg0** | **0x080** | **143–150** |
| gpg1 | 0x0A0 | 151–158 |
| gpg2 | 0x0C0 | 159–165 |
| gpg4 | 0x0E0 | 166–169 |

et `exynos9820_pin_banks0` (ALIVE) place `gpa0` à `0x000` (pins 0–7). Les 56 pins enregistrés par `10430000.pinctrl` dans TWRP confirment la somme. Les constantes du pilote sont donc correctes : aucune correction d'offset n'est nécessaire.

### Limite

Le WiFi ne peut pas aller au-delà de l'assertion de `WL_EN` dans l'état actuel : il n'existe dans l'arbre XNU aucun pilote pour le contrôleur PCIe0 (`13ed0000.pcie0`), ses horloges CMU, ses domaines de puissance et son PHY, ni pour le firmware Broadcom. Le build 140 atteint son objectif (étage 1 : pinctrl et WL_EN) mais ne peut pas produire d'interface réseau.

## Prochaine étape

Build 141 : exposer `/wlan` et les nœuds pinctrl dans le device tree synthétique de `uefi/XnuLoader/DeviceTree.c`, puisque le `dt=0` du build 140 prouve que XNU ne voit que cet arbre. Le changement ne touche aucun registre matériel et fournit la base DT dont les pilotes suivants (audio, haptique, Bluetooth) auront besoin.

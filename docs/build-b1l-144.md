# Build 144 : WiFi, retour des QCH PCIe à l'état de démarrage + audit PCIe en lecture seule

## Diagnostic

Revue du 11/11/2026 des builds 140 à 143, contre le DTB beyond1lte et les sources Samsung (LineageOS `android_kernel_samsung_exynos9820`, branche `lineage-22.2`).

- **QCH (build 143)** : `ra.c` dit « QH ENABLE 1 : Skip, 0 : REQ control ». `0x3f0002` (`ENABLE=0`, `CLOCK_REQ=1`) = horloge forcée allumée par le logiciel, l'état laissé par le chargeur. Le 143 a mis `ENABLE=1` : l'horloge passe au Q-channel matériel et peut être coupée tant que le contrôleur PCIe n'est pas initialisé, ce qui risque de bloquer le bus au premier accès ELBI. Samsung ne touche pas ces registres sur le 9820 (`pcie-clk-num = 0`, `phy-clk-num = 0`).
- **Pinctrl** : sur le 9820, les champs PUD et DRV font **4 bits par broche** (`exynos9810_bank_type_off`, largeurs `{4,1,4,4,2,4}`), pas 2. `pud=0x11131113` donne gpg0-5 (`WL_EN`) = 1 (pull-down), alors que le DT veut 0. L'écriture du build 140 visait les bits 10-11 (broche 2) et n'a rien changé. À corriger au build 145.
- **Nœud `pcie0`** : ELBI `0x13ED0000`, PHY `0x13EF0000`, sysreg `0x13C11044`, DBI `0x13F00000`, PCS `0x13EE0000`, config `0x14FFE000`, IA `0x13EC0000` ; `gpios = <gpf2 2>` → **PERST = gpf2-2**, CLKREQ = gpf2-0 (fonction 2, pull-up) dans le pinctrl FSYS1 `0x13C40000` (`gpf2` à `0x020`). PMU `PCIE_PHY_CONTROL` = `0x15860000 + 0x71C`.
- **Puce** : BCM4375 (`CONFIG_BCM4375=y`, pilote `bcmdhd_101_16`), device ID PCIe attendu `0x4475`.

## Changement

Un seul changement, dans `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`) : `enableCmuQch()` devient `restoreCmuQch()`, qui remet `ENABLE` (bit 0) à 0 sur les 5 QCH si besoin, relit, et journalise `CMU[restore]`. L'appel est déplacé juste après `auditCmu("start")`, avant les sorties anticipées de `start()`.

Diagnostic en lecture seule ajouté (`auditPcieSide()`, mappé, lu puis démappé) : `gpf2` CON/DAT/PUD/DRV, PMU `0x71C`/`0x720`, sysreg `0x13C11044`, `0x13C11050`, `0x13C10700`. Aucun accès ELBI, DBI, PHY ou PCS.

Hors build (lecture seule, depuis TWRP) : firmware, CLM, NVRAM et `.hcd` copiés depuis la partition vendor (EROFS, montée en lecture seule) dans `~/Developer/s10-port/device-s10/wifi-bt-fw/` ; adresses MAC WiFi/BT lues depuis efs (`ro,noload`) dans `mac-addresses.txt` (droits 600). Rien dans git.

## Installé avec hash BOOT

- Build : **144**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `c11c7b09a31325d585f85b8b253142a8e40db54a87e0ea9d9d92699991045332` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 76 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (3 000 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0144-boot.txt`).

```
B2L WIFI CMU[start]: qch=<0x3f0002 ×5> req=1/1/1/1/1 clk=<0x200000 ×7>
B2L WIFI CMU[restore]: before=<0x3f0002 ×5> after=<0x3f0002 ×5> enable=0/0/0/0/0
B2L WIFI PCIE[audit]: gpf2=<0 0 0x111 0> clkreq=f0 perst=f0/0 pmu=<0 0> sysreg=<0x12 0x1d02 0xc00011> map=1/1/1
B2L WIFI: WL_EN asserted con=0x100000 dat=0x21 host_wake=0
```

- Les QCH sont revenus à `0x3f0002` d'eux-mêmes au démarrage à froid (l'écriture du 143 ne survit pas au reset) ; `restoreCmuQch()` n'a donc rien eu à écrire. L'état reste celui du chargeur : horloges forcées allumées.
- `gpf2` : toutes les broches en entrée (fonction 0), pull-down sur gpf2-0, 1 et 2 (`pud=0x111`, champs de 4 bits). PERST est tenu bas par le pull, CLKREQ n'est pas en fonction PCIe.
- PMU `PCIE_PHY_CONTROL` ch0 et ch1 = 0 : le PHY PCIe est isolé. Samsung y écrit 1 dans `exynos_pcie_poweron`.
- sysreg FSYS1 : `0x13C11044 = 0x12`, `0x13C11050 = 0x1d02`, partage `0x13C10700 = 0xc00011`.
- Lecture des trois blocs sans blocage (`map=1/1/1`).

Suite : build 145 (broches PCIe selon le DT, PERST piloté bas, pull et drive de `WL_EN` corrigés).

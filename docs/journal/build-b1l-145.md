# Build 145 : WiFi, broches PCIe et WL_EN conformes au DT

## Diagnostic

Le build 144 a montré `gpf2` (pinctrl FSYS1 `0x13C40000`, banque à `0x020`) entièrement en entrée avec pull-down : PERST (gpf2-2) n'est pas piloté et CLKREQ (gpf2-0) n'est pas en fonction PCIe. Le DT `pcie0` (`pinctrl-0`) veut :

- `pcie0_clkreq` gpf2-0 : fonction 2, pull 3 (pull-up), drive 3 ;
- `pcie0_perst` gpf2-2 : fonction 1 (sortie), drive 3 ; Samsung le demande en sortie basse puis le lève dans `exynos_pcie_establish_link`.

La revue du 144 a aussi montré que PUD et DRV font 4 bits par broche (`exynos9810_bank_type_off`) : `WL_EN` (gpg0-5) garde un pull-down (1) alors que `cfg_wlanen` veut pull 0 et drive 3.

## Changement

Un seul changement (broches WiFi/PCIe selon le DT), dans `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`) :

- nouvelle `configurePciePins()`, appelée **avant** l'assertion de `WL_EN` : DAT gpf2-2 = 0, puis PUD gpf2-0 = 3, DRV gpf2-0/2 = 3, CON gpf2-0 = 2 et gpf2-2 = 1. Relu et journalisé (`PCIE[pins]`). La carte du pinctrl FSYS1 est gardée pour la suite ;
- `WL_EN` : champ PUD de 4 bits mis à 0, DRV à 3 ; décodage des PUD en 4 bits dans les traces.

Aucun accès ELBI, DBI, PHY, PCS ou PMU.

## Installé avec hash BOOT

- Build : **145**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `35cb3fc72d9fda200295fcb3b893aec3fc7754d992c7cdb5c894df6b262323a3` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 63 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 998 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0145-boot.txt`).

```
B2L WIFI PCIE[pins]: before=<0 0 0x111 0> after=<0x102 0x1 0x113 0x303> clkreq=f2 perst=f1/0
B2L WIFI: WL_EN asserted con=0x100000 dat=0x21 pud=0x11031113 drv=0x300000 host_wake=0
```

- `gpf2` conforme au DT : CLKREQ (gpf2-0) en fonction 2 avec pull-up (`pud` champ 0 = 3), PERST (gpf2-2) en sortie basse, drive 3 sur les deux. Le bit DAT 0 lu à 1 est l'entrée CLKREQ (fonction PCIe), sans effet.
- `WL_EN` : pull désactivé (champ 5 de `pud` = 0), drive 3, sortie haute.
- `host_wake` reste à 0 : normal, la puce attend PERST et le lien.

Suite : build 146 (isolation du PHY levée dans la PMU + première lecture ELBI en diagnostic).

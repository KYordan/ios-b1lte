# Build 146 : WiFi, isolation du PHY PCIe levée (PMU) + première lecture ELBI

## Diagnostic

Le build 144 a lu PMU `PCIE_PHY_CONTROL` (`0x15860000 + 0x71C`) = 0 : le PHY PCIe du canal 0 est isolé. `exynos_pcie_poweron` (pcie-exynos-host-v0.c) fait `regmap_update_bits(pmureg, PCIE_PHY_CONTROL + ch * 4, 1, 1)` après avoir sélectionné les broches par défaut, ce que le build 145 a fait (CLKREQ en fonction 2, PERST bas).

Le contrôleur pcie0 (ELBI `0x13ED0000`) n'a encore jamais été lu. Les horloges FSYS1 sont forcées allumées (QCH `0x3f0002`), le domaine FSYS1 est actif (CMU, sysreg et pinctrl lisibles).

## Changement

Un seul changement, dans `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`) : `releasePhyIsolation()` met le bit 0 du mot PMU `0x71C` à 1 (si besoin), relit et journalise `PCIE[pmu]`. Appelée juste après `configurePciePins()`.

Diagnostic en lecture seule (`auditElbi()`) : une trace `ELBI: reading 0x13ed0000` est écrite avant l'accès, puis lecture de `APP_LTSSM_ENABLE` (`0x02c`), `RDLH_LINKUP` (`0x074`), `PM_DSTATE` (`0x088`), `APP_REQ_EXIT_L1_MODE` (`0x0f4`), `SOFT_CORE_RESET` (`0x1d0`), `QCH_SEL` (`0x2c8`). Aucune écriture ELBI, DBI, PHY ou PCS.

## Installé avec hash BOOT

- Build : **146**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `9ab5ed619cdfa4404c5ad9389a8e2c62671af40aa5aa69ad37508ef3be872cde` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 71 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 998 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0146-boot.txt`).

```
B2L WIFI PCIE[pmu]: phy_control before=0 after=0x1
B2L WIFI ELBI: reading 0x13ed0000
B2L WIFI ELBI: ltssm_en=0 linkup=0xc5600 dstate=0x4 exit_l1=0 soft_reset=0x1 qch_sel=0x577 map=1
```

- Isolation du PHY PCIe levée (bit 0 du mot PMU `0x71C` relu à 1).
- **Premier accès au contrôleur pcie0 sans blocage** : ELBI répond. `linkup & 0x1f = 0` (LTSSM en `DETECT_QUIET`, LTSSM non activé), `PM_DSTATE = 4` (`PCIE_D0_UNINIT_STATE`), soft reset relâché (1), `QCH_SEL = 0x577` (gating d'horloge PMU/APB/AXI actif, que Samsung remet à 0 avant le lien).

Suite : build 147 (séquence de lien complète).

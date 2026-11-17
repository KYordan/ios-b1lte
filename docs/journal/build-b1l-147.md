# Build 147 : WiFi, montée du lien PCIe (PHY + LTSSM)

## Diagnostic

Le build 146 a levé l'isolation du PHY (PMU `0x71C` = 1) et lu ELBI sans blocage : LTSSM en `DETECT_QUIET` (`linkup=0xc5600`), D0 non initialisé, `QCH_SEL = 0x577`. Les broches sont prêtes depuis le 145 (CLKREQ fonction 2, PERST bas, `WL_EN` haut).

Séquence de référence : `exynos_pcie_poweron` puis `exynos_pcie_establish_link` (`drivers/pci/dwc/pcie-exynos-host-v0.c`), `exynos_phy_all_pwrdn_clear` et `exynos_pcie_phy_config` (`pci-exynos9820_cal.c`, canal 0, Gen2 26 MHz), `dw_pcie_setup_rc` (`pcie-designware-host.c`). Côté puce, `dhd_wlan_init_gpio` lève `WLAN_REG_ON` puis attend 200 ms (`WIFI_TURNON_DELAY`) avant la montée du lien.

Les accès DBI avant le lien se font, comme dans `exynos_pcie_{rd,wr}_own_conf`, avec `L12ERR_CTRL` (ELBI `0x2F0`) = 0 et `IGNORE_ELECIDLE` (PCS `0xEC` bit 3) = 1.

## Changement

Un seul changement (la séquence de lien), dans `B2LWifi.cpp`, nouvelle `bringUpLink()` appelée après l'assertion de `WL_EN` :

1. attente de 200 ms ;
2. `phy_all_pwrdn_clear` : PCS `0x100` bit 6 = 0, `0x104` bit 7 = 0, PHY `0x80` = `0xC0`, `0x15C` = `0x7E` ;
3. `L12ERR_CTRL` = 0, `IGNORE_ELECIDLE` ;
4. soft reset du cœur (ELBI `0x1D0` 0 puis 1), **PERST haut** (gpf2-2), 20 ms ;
5. ELBI `0xF4` |= `0x11`, `0x1B8` = 2, `QCH_SEL` (`0x2C8`) bits 0-11 à 0 ;
6. `phy_config` : sysreg PCS high speed + lane 0, `pcs_g_rst`, tables `cmn` (48) et `trsv` (80) extraites telles quelles du source Samsung, PCS `0xF8/0x100/0x104/0xC`, resets MAC et PCS/PMA, reset CDR ;
7. `dw_pcie_setup` / `dw_pcie_setup_rc` (1 lane, BAR, INTA, bus 0/1/ff, commande `0x107`, classe pont, `SPEED_CHANGE`) sans ATU ;
8. `exynos_pcie_setup_rc` (LNKCAP x1 5 GT/s, aux 26 MHz, L1.2, PM_CTRL, retrain, cible Gen1 selon `max-link-speed = 1`) ;
9. `ENABLE_ELECIDLE`, LTSSM activé (ELBI `0x2C` = 1), attente de `0x074 & 0x1f` entre `0x0d` et `0x14` pendant 20 ms. En cas d'échec : PERST bas, LTSSM coupé (comme Samsung).

Laissés de côté : réglage OTP du PHY, assistant I/A (sortie de L1.2), tampon d'historique, MSI, L1SS, réécriture des ID Samsung.

Compilation à blanc vérifiée le 11/11/2026 (`out/build-147-check.log`).

## Installé avec hash BOOT

- Build : **147**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `c31ea1301ea3fbc8fe5169707fa89b328350c8852744eb3d6cf62ea38f36fb3a` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 63 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Lien PCIe monté au premier essai, boot iOS complet** (`backboardd`, SpringBoard, aucun `B2L CRASH` ; journal `out/logs/0147-boot.txt`, plus court car le téléphone a été remis en TWRP plus tôt).

```
B2L WIFI LINK: start ltssm=0xb97f00
B2L WIFI LINK: phy configured sysreg=<0x12 0x1d04> pcs_ec=0
B2L WIFI LINK: up after 2 polls linkup=0x32a7711 lnksta=0x7011 speed=1 width=1 host_wake=0
```

- LTSSM = `0x11` (**L0**) après 2 scrutations (~20 µs après l'activation).
- `LNKSTA = 0x7011` : 2,5 GT/s (Gen1, cible `max-link-speed = 1` du DT), largeur x1, horloge de slot commune, lien de données actif.
- sysreg `+0xC` relu `0x1d04` (écrit `0x1d0c`) : le bit 3 ne se relit pas, sans effet sur le lien.
- `host_wake` reste à 0 : la puce attend son firmware.

Suite : build 148 regroupé (configuration de la puce, BAR, identifiant interne, EROM, OTP).

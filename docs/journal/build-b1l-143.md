# Build 143 : WiFi, activation des portes QCH PCIe GEN2 (CMU FSYS1)

## Diagnostic

Le build 142 a prouvé que le CMU FSYS1 (`0x13c00000`) est lisible depuis XNU et que les 5 portes QCH PCIe GEN2 (APB `0x3034`, DBI `0x3038`, MSTR `0x303c`, PCS `0x3040`, PHY `0x3044`) existent. Leur valeur lue était `0x3f0002` : bit 0 (ENABLE) = 0, bit 1 (CLOCK_REQ) = 1, expire = `0x3f`. Les horloges PCIe ne sont donc pas formellement activées par le CMU, même si `CLOCK_REQ` est déjà demandé.

`WL_EN` reste asserté (`con=0x100000`, `dat=0x21`, `host_wake=0`) et `/wlan` est présent (`dt=1`).

## Changement

Un seul changement, dans `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (reporté dans `xnu/iokit/Kernel/arm/B2LWifi.cpp`) :

- Nouvelle fonction `enableCmuQch()` qui écrit **ENABLE = bit 0 = 1** sur les 5 registres QCH PCIe GEN2 (APB, DBI, MSTR, PCS, PHY), en préservant les autres bits.
- Chaque registre est re-lu après l'écriture pour confirmer la prise en compte.
- Appel de `enableCmuQch()` dans `B2LWifi::start`, après `auditCmu("wl_en")` et avant `registerService()`.
- Traces `IOLog` + `b2l_diag_record` avec les valeurs avant/après et l'état de bit 0 sur chaque registre.
- Aucun accès au contrôleur PCIe (`0x13ED0000`), ni ELBI, ni DBI, ni PHY. Rien d'autre n'est écrit.

Le fichier `uefi/XnuLoader/DeviceTree.c` n'est pas modifié.

## Installé avec hash BOOT

- Build : **143**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `6099195feb99f29f3949be7b55291979ce0b4f9c6bb379378dea97be1911fdb5` (`BOOT-OK`), SYSTEM `SYSTEM-OK`.

## Résultat

**Boot iOS sans régression** (2 996 lignes `hakiosmetal[backboardd]`, journal `out/logs/0143-boot.txt`). Trace lue :

```
B2L WIFI CMU[enable]: before=<0x3f0002 0x3f0002 0x3f0002 0x3f0002 0x3f0002> after=<0x3f0003 0x3f0003 0x3f0003 0x3f0003 0x3f0003> enable=1/1/1/1/1
```

L'écriture a pris, mais elle repose sur une lecture inversée du registre (voir la correction du build 142) : `ENABLE=1` confie l'horloge PCIe au Q-channel matériel, qui peut la couper tant que le contrôleur n'est pas initialisé. Le pilote Samsung du 9820 ne touche jamais ces QCH (`pcie-clk-num = 0`, `phy-clk-num = 0` dans le DTB). Le build 144 remet donc la valeur de démarrage.

Trace attendue à l'origine :

```
B2L WIFI CMU[enable]: before=<%#x %#x %#x %#x %#x> after=<%#x %#x %#x %#x %#x> enable=1/1/1/1/1
```

avec `before` = `0x3f0002` et `after` = `0x3f0003` sur chaque registre. Si la confirmation réussit, le build 144 pourra passer à l'étape suivante (reset PCIe / pinctrl `pcie0_perst`, puis premier accès ELBI/DBI pour lire le vendor/device ID Broadcom).

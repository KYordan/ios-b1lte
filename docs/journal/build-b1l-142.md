# Build 142 : WiFi, audit lecture seule CMU/PMU/pinctrl (FSYS1)

## Diagnostic

Le build 141 a validé le device tree synthétique (`/wlan` présent, `dt=1`, gpios corrects) et l'assertion de `WL_EN` (`con=0x100000`, `dat=0x21`, `host_wake=0`). Le WiFi fonctionnel reste cependant bloqué : aucun pilote PCIe host-v0, CMU, PMU, PHY ni firmware Broadcom n'existe dans XNU.

Le contrôleur WiFi est sur **PCIe0 host-v0** (`0x13ED0000`, `samsung,exynos-pcie`, `num-lanes=1`, `max-link-speed=1`, `status="okay"`), générant l'interruption `0xfd`. Le DTB beyond1lte déclare `clocks = <0x2 0x129>` où `0x129` = `GATE_PCIE_GEN2_MSTR` (CLK_FSYS1_BASE 290 + 7) sur le `clock-controller@0x1a240000` (phandle `0x2`). Cependant, d'après `cmucal-sfr.c`, le bloc CMU qui contient les portes QCH et les registres de gating PCIe GEN2 est **CMU_FSYS1 à `0x13c00000`** (taille `0x8000`), et non `0x1a240000`. L'ID d'horloge `0x129` du DT correspond à l'index Linux du CCF, pas à l'adresse du bloc SFR.

Offsets clés trouvés dans `cmucal-sfr.c` (bloc `CMU_FSYS1`, base `0x13c00000`) :
- `QCH_CON_PCIE_GEN2_QCH_MSTR` = `0x303c` (enable bit 0, clock bit 1, expire bits 16–25)
- `QCH_CON_PCIE_GEN2_QCH_PCS` = `0x3040`
- `QCH_CON_PCIE_GEN2_QCH_PHY` = `0x3044`
- `QCH_CON_PCIE_GEN2_QCH_DBI` = `0x3038`
- `QCH_CON_PCIE_GEN2_QCH_APB` = `0x3034`
- Registres de gating d'horloge PCIe GEN2 (base `0x2000` dans le bloc) : PHY_REFCLK `0x2000`, DBI `0x2030`, PHY_APB `0x2034`, MSTR `0x2038`, SUB_CTRL_APB `0x203c`, PIPE2 `0x2040`, SLV `0x2044`

Aucun de ces registres n'a encore été lu depuis XNU. Le build 142 ne fera qu'en prendre une snapshot en lecture seule, sans toucher ELBI/DBI/PHY ni écrire quoi que ce soit.

## Changement

Un seul changement, dans `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (reporté dans `xnu/iokit/Kernel/arm/B2LWifi.cpp`) :

- Ajout d'un mappage lecture seule de `CMU_FSYS1` (`0x13c00000`, `0x4000` octets) avec `kIODirectionIn` / `kIOMapInhibitCache`.
- Nouvelle fonction `auditCmu(const char *when)` qui lit (sans écrire) :
  - les 5 portes QCH PCIe GEN2 (APB, DBI, MSTR, PCS, PHY) aux offsets `0x3034`–`0x3044` ;
  - les 7 registres de gating d'horloge PCIe GEN2 aux offsets `0x2000`–`0x2044`.
- Appels à `auditCmu("start")` et `auditCmu("wl_en")` dans `B2LWifi::start`, avec traces `IOLog` + `b2l_diag_record`.
- Le pinctrl (PERI0 `gpg0` et ALIVE `gpa0`) et l'assertion de `WL_EN` restent inchangés (étage 1 du build 140).
- Aucun accès au contrôleur PCIe (`0x13ED0000`), aucun registre n'est écrit.

Le fichier `uefi/XnuLoader/DeviceTree.c` n'est pas modifié (le DT synthétique du build 141 est conservé).

## Installé avec hash BOOT

- Build : **142**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `bd3327265d8437578f39ec9104deaac5a7f27e1174882e8ff5faf061785a889a` (`BOOT-OK`), SYSTEM `SYSTEM-OK`.

## Résultat

**Boot iOS sans régression.** Les 5 QCH PCIe GEN2 valent `0x3f0002` et les 7 portes d'horloge `0x200000`.

Correction (revue du 11/11/2026, `ra.c` et `cmucal-sfr.c`) : sur un QCH, le bit 0 est `ENABLE` (1 = le Q-channel matériel décide, 0 = le logiciel décide avec le bit 1) et le bit 1 est `CLOCK_REQ`. `0x3f0002` veut donc dire **horloge forcée allumée par le logiciel** : les horloges PCIe tournaient déjà. Sur les portes, `0x200000` = `CG_VAL=1`, `MANUAL=0` (gating laissé au QCH). La conclusion « horloges arrêtées » qui a motivé le build 143 était inversée.

Trace prévue à l'origine :

```
B2L WIFI: dt=1 gpio=<0x8e 5 0x1 0x134 7 0x1> ...
B2L WIFI: WL_EN asserted con=0x100000 dat=0x21 host_wake=0
B2L WIFI CMU[start]: qch=... clk=...
B2L WIFI CMU[wl_en]: qch=... clk=...
```

Le résultat indiquera si les portes QCH PCIe GEN2 sont déjà activées (bit 1 = 1) ou si elles sont toutes à 0 (horloges arrêtées), ce qui décidera du contenu du build 143 (activation des horloges FSYS1 puis premier accès ELBI/DBI).

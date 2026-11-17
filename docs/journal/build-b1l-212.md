# Build B1L-0212 (2026-11-13) — Bluetooth 2 (micrologiciel) + lot audio A2–A4

Build groupé (OK du propriétaire : étapes audio regroupées). Le Wi-Fi est inchangé depuis 0210.

## Bluetooth étape 2
- `iokit/Kernel/arm/B2LBluetooth.cpp` : après HCI_Reset, Download_Minidriver puis les 1261 enregistrements de `bt.hcd` (bcm4375B1_semco.hcd, 176 645 o) et Launch_RAM ; ensuite HCI_Reset, version et nom relus, puis la puce est éteinte.
- `bsd/kern/b2l_userspace_seed.h` : `b2l_bt_fetch()` appelé juste après `b2l_wifi_fetch()`.
- SYSTEM : `b2l-wifi/bt.hcd` ajouté (hors git, dans device-s10/wifi-bt-fw/system).
- Journal : `B2L BLUETOOTH FW`, `THE FIRMWARE RUNS`, `step 2 done`. Coupure : `b2l_bt=2` (sans micrologiciel), 1 ou 0.

## Audio A2–A4 (`iokit/Kernel/arm/B2LAudio.h`)
- A2 : lecture CMU_AUD/ABOX/broches AUD, seulement si PLL_AUD0 est verrouillée.
- A3 : réglage des CS35L40 par I2C, AMP_EN=0, en sourdine.
- A4 : horloge UAIF1, puis les amplis allumés et éteints ; tout est restauré sauf PMU 0x1920 bit 11. Aucun son.
- Journal : `abox-verdict`, `amp-prep`, `amp-power`, `uaif1-verdict`, `amps-off pins-restored=1`.
- Coupure : `b2l_audio` = 4 (sans A4), 3, 2, 1, 0 ; 5 par défaut. Détails : `docs/audio/review-and-plan.md` §8–9.

## Données copiées (lecture seule, hors git)
- `~/Developer/s10-port/vendor-audio/` : calliope*, cs35l40-*.wmfw/.bin/-calib.bin et mixer_paths.xml, depuis vendor (erofs, monté en ro dans TWRP puis démonté).

## Artefacts
- BOOT readback : 9bac69a4e846e0269a52a75e9863e1ac116f9fb1bfabf36764064524779023b9
- `out/mach-0212.unstripped`

## Résultat
- Wi-Fi intact (link up, IP, geod relancé).
- Bluetooth : micrologiciel chargé (1261 enregistrements, 17 s, tous acceptés) ; après le lancement, rev 0 → 0xad et nom « BCM4375B1 Beyond E43 Ant0 [Baseline: 0173] ». Le test a écrit « not confirmed » à tort : sur cette puce, lmp_subver reste 0x1111.
- Audio : PLL_AUD0 verrouillée à 1,18 GHz, ABOX v2.0.1 lu, les deux CS35L40 réglés (`ready`), UAIF1 à 6,144 MHz : `amps-clocked-by-uaif1`, `pins-restored=1`. À vérifier : le registre 0x4820 vaut 0x1 sur bot mais reste à 0x100 sur rcv.

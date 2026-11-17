# Build B1L-0211 (2026-11-13) — Bluetooth 1b + Audio A1

Build groupé ; le Wi-Fi est inchangé depuis 0210 (relance de geod).

## Bluetooth 1b (`iokit/Kernel/arm/B2LBluetooth.cpp`)
- Le nœud /bluetooth n'est plus exigé : le DT iOS de XnuLoader n'en a pas.
- Le mux UART_BT_USER passe de 0x10 à OSCCLK pendant le test, puis revient à 0x10.
- Le reste est inchangé : attente de la graine Wi-Fi + 30 s, HCI_Reset, Read_Local_Version, puis la puce est éteinte.
- Journal : `B2L BLUETOOTH`. Coupure : `b2l_bt=0` ou `b2l_bt=1`.

## Audio A1 (`iokit/Kernel/arm/B2LAudio.h`), accord du propriétaire le 2026-11-13
- Les traces passent par `b2l_diag_record`, plus de mélange avec B2L THERMAL.
- Lecture de DEVID/REVID des deux CS35L40 par I2C simulé sur gph0-6/7 : impulsion de reset sur gph1-1/2, aucune écriture vers les amplis, broches remises à leur état de départ.
- Journal : `B2L AUDIO: amp-id …`. Coupure : `b2l_audio=1` (A0 seul) ou `b2l_audio=0`. Détails : `docs/audio/review-and-plan.md` §7.

## Artefacts
- BOOT readback : 959c76038f0beb70eddd0e4d9bf3f71c6417390b4783c682a6aea26d85ea9eca
- `out/mach-0211.unstripped`

## Résultat
- Wi-Fi intact (link up, IP, geod relancé).
- Bluetooth : THE CHIP ANSWERS. HCI_Reset OK à 115200 ; hci=9 lmp=9 manufacturer=0xf (Broadcom), lmp_subver=0x1111 (ROM). Puce rééteinte, mux remis à 0x10.
- Audio A1 : `both-cs35l40`, id=00035a40 rev=a0 pour les deux amplis (0x40 et 0x41), pins-restored=1.
- Apps : Bourse se connecte en TLS (une réponse 200), puis reste sur son écran d'accueil ; l'abonnement News/entitlements échoue (EntitlementsProviderErrorDomain 3001, pas de storefront). Ce n'est plus le pilote Wi-Fi : le réseau marche.

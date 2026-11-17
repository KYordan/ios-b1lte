# Build B1L-0213 (2026-11-13) — Audio lot 2 : premier bip

Seul changement : `iokit/Kernel/arm/B2LAudio.h`. Bluetooth, Wi-Fi et B2LCPU.cpp sont inchangés depuis 0212.

## Audio
- A3 recopie l'OTP d'usine des deux CS35L40 (cs35l41_otp_unpack).
- A5 : l'AP programme lui-même RDMA0 → SPUS → SIFS0 → UAIF1, sans Calliope. Il joue un bip de 1 kHz à −10 dB pendant 0,7 s sur bot@40 seul (boost coupé, rcv jamais activé), puis il coupe le son et remet tout à sa valeur de départ.
- Journal : `amp-otp … result=ok`, `beep-path … | rdma-moving`, `beep played=1 ms=700 …`, `abox-path-restored=1`, `amps-off pins-restored=1`.
- Coupure : `b2l_audio=5` (sans bip), 4, … ; 6 par défaut.
- Risques : sens du bit 4 de RDMA CTRL0 inconnu ; SysMMU de l'ABOX supposée en contournement ; pas de firmware de protection des amplis, d'où le volume bas et la durée courte.
- Détails : `docs/audio/review-and-plan.md` §10.

## Artefacts
- BOOT readback : 3bb1295f40c24c119311b26f75df667542e2f5105ca047ee268ad540479c51c9
- `out/mach-0213.unstripped`

## Résultat
- **Bip entendu par le propriétaire.** Journal : `amp-otp … result=ok` (71 et 70 écritures), `beep-path … dram=ok … | rdma-moving`, `beep played=1 ms=700 vol_up=1 err=00000000 … rdma-idle-ring-freed=1 abox-path-restored=1`, `amps-off pins-restored=1`.
- Le registre 0x4820 vaut toujours 0x1 sur bot et 0x100 sur rcv (à expliquer, sans effet sur le bip).
- Wi-Fi : IP OK et geod relancé. Le lien tombe puis revient 3 fois (événement 11, raison 8 : départ demandé par le téléphone), suivi chaque fois d'un nouveau join. Probablement wifid qui change de bande ; pas encore vérifié.

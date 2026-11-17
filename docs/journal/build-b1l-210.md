# Build B1L-0210 (2026-11-13) — Wi-Fi + Bluetooth étape 1 + Audio A0

Build groupé (OK du propriétaire) : trois changements indépendants, chacun avec son interrupteur.

## Wi-Fi : relance de geod
- `bsd/dev/b2l_hakios_net.c` : au premier `SIOCSIFADDR` IPv4 sur l'interface Wi-Fi, le noyau tue `geod` une seule fois ; launchd le relance à la demande, avec le réseau prêt.
- Pourquoi : `geod` charge son « resource manifest » une seule fois au boot, avant l'IP (-1009), et ne relance jamais, même après Wi-Fi off/on. Sans manifeste, ses réponses à Météo échouent en 2 ms.
- Journal : `net: first IPv4 address, geod N restarted`.
- Autre piste vue : `weatherd` est tué par jetsam (InactiveHard 8 Mo, 35 Mo utilisés).

## Bluetooth étape 1
- Nouveau `iokit/Kernel/arm/B2LBluetooth.cpp`, plus une ligne dans `iokit/conf/files` et une dans `iokit/KernelConfigTables.cpp`. Voir `docs/bluetooth/plan.md`.
- Journal : `B2L BLUETOOTH`. Coupure : `b2l_bt=0` (rien) ou `b2l_bt=1` (lecture seule).

## Audio A0 (lecture seule)
- Nouveau `iokit/Kernel/arm/B2LAudio.h`, plus deux lignes dans `B2LCPU.cpp`. Voir `docs/audio/review-and-plan.md` §5.
- Journal : `B2L AUDIO`, avec une ligne `verdict` à la fin. Coupure : `b2l_audio=0`.

## Artefacts
- BOOT readback : 3fea141a53bc10487d7c23c3b9a9760c953f70023263d6672428096182758f3c
- `out/mach-0210.unstripped`
- Sauvegardes : `~/Developer/s10-port/b2l_hakios_net.c.209bak`, `iokit-files.209bak`, `KernelConfigTables.209bak`

## Résultat
- Wi-Fi : join et IP OK. `net: first IPv4 address, geod 151 restarted`. geod relancé charge son manifeste (configuration.ls.apple.com 304, puis 200 de 89 Ko) : premier succès de geod sur ce port.
- Météo toujours vide : la recherche de ville via geod (T:778) échoue en 2 ms sans réseau (MKErrorGEOError=-4) ; WeatherKit (« Sending request ») n'ouvre aucune connexion ; weatherd dépasse sa limite mémoire (ActiveSoft 10 Mo, non fatal cette fois).
- Bluetooth : arrêt avant toute écriture (pas de /bluetooth dans le DT iOS, mux UART_BT sur CLKCMU_PERIC1_IP). Étape 1b prête.
- Audio A0 : verdict pd-aud=on amp-bus=i2c-timed scl_fn=0 sda_fn=0 amp_reset bot=fn0/0 rcv=fn0/0 codec_reset=fn1/0 i2c_regs=1 ; 3 lignes mélangées avec B2L THERMAL. Étape A1 prête (écrit des GPIO), en attente de l'accord du propriétaire.

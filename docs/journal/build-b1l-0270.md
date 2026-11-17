# B1L-0270 — Bluetooth branché à iOS (étape 3)

## Pourquoi (demande du propriétaire, « lier le Bluetooth à iOS »)

- Depuis 0212, le noyau charge le micrologiciel de la BCM4375 puis rééteint la puce. iOS ne la voit pas.
- Détail et lecture de bluetoothd : `bluetooth-plan.md` §6, état avant reprise : `bluetooth-etat.md`.

## Changement (étape 3 rangée le 2026-11-14, réappliquée sur l'état 0269)

- `iokit/Kernel/arm/B2LBluetooth.cpp` : critère corrigé (révision HCI), puce **laissée allumée**, adresse synthétique
  (Write_BD_ADDR), kernel control `com.apple.uart.sk.bluetooth`, entrée de registre `bluetooth`, relais UART ↔ bluetoothd.
- `bsd/kern/b2l_exec_bluetooth.h` (nouveau) : `BT_HCI_TRANSPORT=H4:POSIX:com.apple.uart.sk.bluetooth,115200` et
  `BT_BAUDRATE=115200` pour bluetoothd ; `b2l-bluetool-v1.dylib` insérée dans BlueTool.
- `bsd/kern/b2l_bt_seed.h` + `b2l_bluetool_payload.h` (nouveaux) : la graine publie la dylib dans `/usr/local/lib`.
- Fichiers partagés (patch `bluetooth-step3-shared.patch`, passé sans décalage) : `b2l_userspace_seed.h`,
  `b2l_exec_graphics_diag.h`, `b2l_amfi_constraint.h` (pid de BlueTool dans la politique dyld).
- Metal : rien ne change, set **b2l-metal-v133** gardé (`--no-libs`). Sauvegardes : `~/Developer/s10-port/bak-0270-bt/`.

## Coupures

`b2l_bt_ios=0` (rien côté iOS) ; `b2l_bt=3` (comme 0212/0213 : micrologiciel puis extinction) ; `b2l_bt=0` (rien).

## Attendu

`THE FIRMWARE RUNS`, `synthetic address in place`, `THE RADIO IS UP for iOS`, `B2L BLUETOOTH EXEC: …`,
`H4: client connected`, `TOOL: RunBuiltinScript boot -> 0`, `iOS sends HCI …` ; interrupteur Bluetooth dans Réglages.
Wi-Fi, GPU, écran et tactile inchangés.

## Installé

- SYSTEM-OK (7 blocs sur 1750), BOOT `4bf1eebc7904cd5038b802bc15c5248c88635632296a4e7a41b8109d004b033c`, relu BOOT-OK.
- Noyau non strippé : `out/mach-0270.unstripped`. Graine sans l'audio (lignes AUDIO-PARKED de la session Audio).
- Pas de redémarrage : la session Audio enchaîne son build et redémarrera après le sien.

## Résultat

Démarré en 0271 (build Audio, qui contient aussi le Bluetooth ; journal `out/logs/0271-boot.txt`).
- Noyau : tout passe. Micrologiciel en 17 s, `THE FIRMWARE RUNS (… -> 0x111100ad)`, `synthetic address in place`,
  `THE RADIO IS UP for iOS … errno 0, registry "bluetooth" published`, `BlueTool stand-in publication error=0`.
- Le propriétaire voit l'adresse Bluetooth dans Réglages, mais l'interrupteur ne s'allume pas.
- Cause : **bluetoothd n'est jamais lancé** (aucune ligne `B2L BLUETOOTH EXEC`, aucun `bypassed for bluetoothd`, contre 27 en
  0213). Le USERDATA propre de 0250 (`prepare-ios-root.py`, `launchd_disabled()`) le marque désactivé dans
  `/private/var/db/com.apple.xpc.launchd/disabled.plist`, avec la note « à retirer quand le Bluetooth reprend ».
- Correctif préparé (B1L-0272, `b2l_bt_seed.h`) : avant launchd, la clé `com.apple.bluetoothd` y est renommée sur place en
  `com.apple.bluetoothX` (même longueur ; plist binaire ou XML). **Écriture dans USERDATA : accordée** par le propriétaire (« Oui, autorise », 2026-11-17, via la session Wi-Fi) ; part dans le prochain build de la session Audio.
  Boot-arg `b2l_bt_launchd=2` remet la clé (bluetoothd de nouveau désactivé), `=0` ne touche pas au fichier.


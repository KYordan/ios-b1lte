# Bluetooth — état au 2026-11-14 (en pause, demande du propriétaire)

Le propriétaire a demandé de s'arrêter là pour le Bluetooth, sans build. On y reviendra plus tard.

## Ce qui marche (sur le téléphone, build 0213)
- **Étape 1 (0211)** : la partie Bluetooth de la BCM4375 répond en HCI sur l'UART_BT (uart@10840000), à 115200 bauds avec RTS/CTS. HCI_Reset OK ; ROM : hci=9, lmp=9, manufacturer=0xf (Broadcom), lmp_subver=0x1111, nom « BCM4375B1 ».
- **Étape 2 (0212)** : micrologiciel `bt.hcd` (bcm4375B1_semco.hcd, 176 645 o, 1261 enregistrements) chargé en 17 s, tout accepté. Après le lancement : révision HCI 0 → 0xad, nom « BCM4375B1 Beyond E43 Ant0 [Baseline: 0173] ». Le test écrivait « not confirmed » à tort (lmp_subver ne change pas sur cette puce).
- Au boot, le noyau charge le micrologiciel puis **rééteint la puce** (BT_EN=0) ; le mux UART_BT est remis à 0x10.
- Le Wi-Fi n'a jamais été gêné.

## Ce qui est installé (dans 0213)
- `iokit/Kernel/arm/B2LBluetooth.cpp` (étape 2), entrées dans `iokit/conf/files` et `iokit/KernelConfigTables.cpp`.
- `bsd/kern/b2l_userspace_seed.h` : `b2l_bt_fetch()` après `b2l_wifi_fetch()`.
- SYSTEM : `/b2l-wifi/bt.hcd` (copie de `~/Developer/s10-port/device-s10/wifi-bt-fw/system/bt.hcd`, hors git).
- Boot-args : `b2l_bt=0` (rien), `1` (lecture seule), `2` (sans micrologiciel), défaut = étape 2.

## Étape 3 préparée, jamais flashée (branchement à iOS)
- Rangée dans `~/Developer/s10-port/bluetooth-step3-parked/` : `B2LBluetooth.cpp` (sha eab8b5a2…), `b2l_exec_bluetooth.h`, `b2l_bt_seed.h`, `b2l_bluetool_payload.h` et `bluetooth-step3-shared.patch` (pour `b2l_userspace_seed.h`, `b2l_exec_graphics_diag.h` et `b2l_amfi_constraint.h`). Pour l'appliquer : copier les fichiers dans `xnu-overlay/` aux mêmes chemins, puis lancer `patch -p0` depuis `beyond2lte-ios/`.
- Principe : bluetoothd 23G90 ouvre le contrôleur par `BT_HCI_TRANSPORT=H4:POSIX:com.apple.uart.sk.bluetooth,115200`, un kernel control (PF_SYSTEM). Le noyau garde la puce allumée et relaie UART ↔ client. BlueTool est remplacé par une dylib injectée (`/usr/local/lib/b2l-bluetool-v1.dylib`, publiée par la graine, autorisée par la politique dyld AMFI). Adresse synthétique écrite par Write_BD_ADDR.
- Sources hôte : `userspace/b2l-bluetool.c`, `scripts/build-bluetool-dylib.py`. Plan détaillé : `docs/bluetooth-plan.md` §6.
- Risques notés : bac à sable de BlueTool ; plantage de bluetoothd à l'extinction (case +0x60, vue par ios-pipa) ; 115200 bauds trop lents pour l'A2DP ; appairages pas encore persistants.

## Build 0214 interrompu
- Le build 0214 (étape 3) a été arrêté à la demande du propriétaire, pendant le hachage côté Mac de l'étage SYSTEM, avant toute écriture. Ni SYSTEM ni BOOT n'ont été écrits : le téléphone est toujours en 0213.
- `xnu/` et `xnu-overlay/` sont remis à l'état 0213 pour le Bluetooth (copies dans `~/Developer/s10-port/bak-0214/`). Le prochain build réutilisera le numéro 0214 ou le suivant.

## Pour reprendre
1. Remettre l'étape 3 en place, voir plus haut.
2. Construire, puis vérifier dans le journal : « THE FIRMWARE RUNS », « THE RADIO IS UP for iOS », « H4: client connected », « TOOL: RunBuiltinScript boot -> 0 », « iOS sends HCI … ».
3. Dans Réglages : l'interrupteur Bluetooth, puis une recherche d'appareils.

## Mise à jour 2026-11-17 (0270 flashé, démarré en 0271)
Le noyau fait tout jusqu'au point H4. bluetoothd ne démarre pas, car il est désactivé dans le `disabled.plist` de
launchd du USERDATA propre (0250). Correctif prêt dans `xnu-overlay/bsd/kern/b2l_bt_seed.h` (renommage
`com.apple.bluetoothd` → `com.apple.bluetoothX`), en attente de l'accord USERDATA. Détail : `build-b1l-0270.md`.


## Build 0272 (2026-11-17) : bluetoothd lancé, mais plante au démarrage
- La graine a bien renommé la clé : bluetoothd est lancé, avec nos variables (`Searching for transport: "H4"`, `driver: "POSIX"`).
- Il plante en boucle (17 fois, « Too many corpses ») : EXC_BAD_ACCESS à l'adresse 0x31c, à 0x1004040bc sur le fil principal
  (rapports `bluetoothd-*.ips` dans USERDATA, os_log : `Unknown platform 0xffffffff detected`).
- Cause : `MGGetProductType()` renvoie 0xffffffff, car MobileGestalt ne connaît pas le modèle de ce téléphone. La table des
  plateformes de bluetoothd (0x1004312a0, 88 appels à `MGIsDeviceOneOfType`) ne reconnaît rien, l'objet plateforme reste
  nul et bluetoothd lit son champ +0x31c (le numéro de plateforme, utilisé à plus de 400 endroits).
- Correctif (build commun suivant, `b2l-bluetool-v2.dylib`) : la dylib est aussi insérée dans bluetoothd. Elle répond « oui »
  à `MGIsDeviceOneOfType` au seul site d'appel qui crée la plateforme choisie, et laisse passer tous les autres appels.
  Plateforme 0x16 par défaut (famille Broadcom la plus répandue) ; boot-arg `b2l_bt_platform=0x13|0x14|0x17|0x18|0x19`,
  `0` = rien. Garde : UUID de bluetoothd 23G90 (DA7ADF00-…). AMFI autorise aussi le pid de bluetoothd (`b2l_btd_dyld_pid`).
- Pas encore traité : crash possible quand on éteint le Bluetooth (case +0x60 de la table H4, vue par ios-pipa), commandes
  Broadcom refusées par la BCM4375 (le noyau les journalise : `refused by the chip`), 115200 bauds pour l'A2DP.

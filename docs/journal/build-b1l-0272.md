# B1L-0272 — Build commun audio + Bluetooth

## Pourquoi

- 0271 : le propriétaire dit « iOS me laisse changer le volume mais pas de son ».
- Journal 0271 (`out/logs/0271-boot.txt`) :
  - côté noyau, tout est prêt (chemin gardé, amplis en veille sans erreur) ;
  - le plug-in est chargé, `/dev/hakios-audio` est ouvert, le volume arrive au noyau ;
  - mais à chaque son, le plug-in écrit « playing » puis « stopped after 0.00 s » : iOS démarre l'IO et l'arrête sans un seul cycle d'écriture. Aucun `ios-play`.
- os_log d'audiomxd (USERDATA tiré en lecture seule, `out/live/ios26-userdata-audio-271.img`) :
  - la route est bien le haut-parleur (`Route changed. New device routes = [Speaker~Speaker~]`) ;
  - le plug-in Bluetooth audio d'iOS (BTAudioHALPlugin) tourne en boucle dans audiomxd, faute de bluetoothd : environ 8 000 lignes/s. logd coupe alors le journal d'audiomxd, et les essais du propriétaire n'y sont pas ;
  - iOS se plaint aussi d'un domaine d'horloge nul : « We expect this device to report a correct clock domain ».
- ios-pipa (même plug-in HakiOS, identité D22) avait du son avec le même plug-in, et son bluetoothd tournait.

## Changements

- **Bluetooth** (session Bluetooth, accord du propriétaire « Oui, autorise ») : `b2l_bt_seed.h` réactive bluetoothd dans `disabled.plist`. Retour arrière : `b2l_bt_launchd=2`.
- **Plug-in audio v2** (`audio/plugin/audio_plugin.c`, v1 gardé en `.v1`) :
  - domaine d'horloge `'hkck'` au lieu de 0 ;
  - trace de l'IO dans le journal noyau pour les 8 premiers démarrages : `hakios: trace: start_io / zero-time-stamp / will-do / do-io / stop_io` (durée et compteurs).
- **`b2l_audio_seed.h`** :
  - installe `SYSTEM:/b2l-audio-v2` ;
  - l'ancien bundle est mis de côté dans `Plug-Ins/HakiosAudio.driver.before-b2l-audio-v2` ;
  - un dossier marqueur `.b2l-audio-v2` est créé dans le bundle ;
  - la limite de lignes du plug-in dans le journal passe de 400 à 1500.
- `B2LAudio.h` inchangé. Set Metal v133 inchangé (`--no-libs`).

## À lire après le démarrage

```bash
grep -a "B2L AUDIO" out/logs/0272-boot.txt | grep -v "does not have"
```

- `ios-seed … b2l-audio-v2 older-moved-aside=1 publication error=0` ;
- les lignes `hakios: trace:` ;
- `ios-play`.

## Installé

- SYSTEM : `/b2l-audio-v2` (SYSTEM-OK). Build : SYSTEM-OK, BOOT `94b1399ef0bb7c6a0a7905b3f17346638f23942520f8c0612a18c76ea6f37450`, BOOT-OK.
- Noyau non strippé : `out/mach-0272.unstripped`. Téléphone redémarré vers le système.

## Résultat

- Le propriétaire : « l'audio ne marche toujours pas ». Le mode silencieux était désactivé.
- Le plug-in v2 est installé, mais aucun `start_io` : iOS ne démarre même plus la lecture. Le volume arrive toujours au noyau.
- os_log d'audiomxd (pull USERDATA de la session Bluetooth) :
  - `Product with Acoustic ID '0' is not handled` ;
  - `Unknown product ID (0): unable to return a valid iOS RoutingDatabase` ;
  - `Unable to locate category 'cspr' in the routing database` ;
  - `Hardware unknown! default config!`.
- **Cause : VirtualAudio identifie le téléphone par `IODeviceTree:/product/audio` `acoustic-id`.** Ce nœud est absent de notre arbre, qui vient du DTB Samsung.
  - Sur le vrai iPhone17,2 (`Firmware/all_flash/DeviceTree.d94ap.im4p` de 23G90), on trouve `acoustic-id = 8017` (0x1f51), d'où le dossier de réglages `/Library/Audio/Tunings/AID8017`.
  - Correctif dans B1L-0273.

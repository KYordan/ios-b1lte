# Build B1L-0004 : chiens de garde SpringBoard désactivés (racine)

Date : 2026-11-04. Une seule modification par rapport à B1L-0003 : une clé de préférence dans la racine iOS.
BOOT (`0fbcf3ba…`, B1L-0003) et SYSTEM sont inchangés.

## Cause

Les 3 rapports `Preferences-*.ips` (`out/logs/0003.logarchive`) montrent `0x8BADF00D`, « scene-create watchdog
transgression … exhausted real (wall clock) time allowance of 20.00 seconds ». Le processus avait consommé 167 s de CPU,
dont 113 s en système, à cause de l'émulation arm64e du code pas encore traduit. Réglages n'a pas de bug : il est trop lent pour FrontBoard.

## Modification

`private/var/mobile/Library/Preferences/com.apple.springboard.plist` : `BKNoWatchdogs = YES`.
C'est la clé utilisée par le S7 (`sweet-root-tweaks.py`, `herolte-tune-ios-root.sh`). Propriétaire `501:501` et mode `0600` conservés.

## Méthode (préserve les traductions du cache natif)

1. Relecture vérifiée de USERDATA (`b1l-tools/pull-chunked.sh`), soit `out/live/ios26-userdata-live.img`.
   51 morceaux transférés, 13 identiques à l'image d'origine. Clone de retour : `ios26-userdata-live-0003.img`.
2. Contrôle avant édition : morceau 1 du téléphone identique à la copie, donc pas de démarrage iOS entre-temps.
3. `sudo bash b1l-tools/set-root-prefs.sh springboard-watchdogs` : `fsck_hfs` indique « appears to be OK ».
4. Morceaux modifiés par rapport au clone : **1, 2, 3 et 45** sur 64.
5. `flash-chunked.sh` : ces 4 morceaux sont écrits, relus et vérifiés, et les 60 autres sont confirmés identiques sur le téléphone.

Retour arrière : réécrire `ios26-userdata-live-0003.img` avec `flash-chunked.sh` (seuls les mêmes 4 morceaux changeront).

## Attendu

Réglages s'ouvre, éventuellement lentement au premier lancement, sans kill `0x8BADF00D`.
Le cache natif apprend son code, et les ouvertures suivantes sont plus rapides.

## Résultat

- 2026-11-04, premier démarrage : **Réglages s'ouvre** (plus de kill immédiat), mais l'app reste figée au début.
  Cause probable : code de Réglages et SwiftUI encore émulés (pas traduits). Durée et état final à confirmer dans le journal.

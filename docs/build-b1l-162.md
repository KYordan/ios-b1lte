# Build 162 : scan Wi-Fi au format iOS 26

## Diagnostic

Build 161 : configd est débloqué, le Wi-Fi s'allume dans l'interface (constaté par le propriétaire), mais la liste reste vide. wifid envoie `SCAN_REQ` (10) avec **5528 octets**, et le pilote (porté de HakiOS / iOS 16.5) n'accepte que 5456 octets : erreur 22 à chaque scan.

Format iOS 26, lu dans IO80211 (23G90) :
- constructeur de la requête : 0x25d4780b8 (non glissé) ;
- `bzero` de 0x1598 octets, puis les champs aux décalages 16.5 : 0, 4, 8/12 (BSSID), 16 (longueur du SSID), canaux à 0x48, `flags` à 0x1308 (4872), `name_count` à 0x1314 (4884), 5448 ;
- conclusion : même disposition, avec 72 octets ajoutés à la fin.

Les résultats (`SCAN_RESULT` 11 / `CURRENT_NETWORK` 103) passent de 1164 à **2272 octets** (0x25d464334) :
- `elements_length` reste à 0x8a ; il arrive à 2048 (la place disponible) ;
- les éléments commencent toujours à 0x8c ;
- 84 octets sont ajoutés à la fin.

Le tampon est mis à zéro par l'appelant, et notre réponse de 1164 octets reste lisible.

ios-pipa n'aide pas sur ce point : son code attend aussi 5456 octets, car son interface Wi-Fi vient d'iOS 16 (binaires 20H392).

## Changement

`xnu-overlay/bsd/dev/b2l_hakios_apple80211.c` (fichier Wi-Fi, copié dans `xnu/`) :
- `SCAN_REQ` accepte une requête d'au moins 5456 octets et lit la partie 16.5 ;
- `RSSI_BOUNDS` (106) répond {-90, -40, -80} dBm, comme ios-pipa 0132 (refusé, wifid n'a pas d'échelle pour la force du signal).

## Attendu

`apple80211: wifid sets 10 (SCAN_REQ) ... answered`, `B2L WIFI SCAN`, `the station says: a scan is over`, puis des `gets 11 (SCAN_RESULT)` qui renvoient des réseaux. La liste doit apparaître dans Réglages. La connexion n'est pas encore possible (`join` non écrit, pas de chemin de données).

## Installé avec hash BOOT

- Build : **162**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `f87b14cc8de7ddb7228d217320e1bbecbd984ba601d65fce1ea5c97eff6f4d95` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 75 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Partiel : `SCAN_REQ` de 5528 octets est accepté, mais la liste reste vide (constaté par le propriétaire). Le firmware refuse chaque scan :
- `B2L WIFI SCAN: escan status=-37` (17 fois) ;
- -37 = `BCME_VERSION` : le firmware 18.41.117 n'accepte plus `wl_escan_params` v1, il faut la v2 (`ESCAN_REQ_VERSION_V2`, `wl_scan_params_v2_t` de 76 octets ; bcmdhd_101_16 `wl_cfgscan.c`).

wifid lit bien `SCAN_RESULT` sur 2272 octets ; il reçoit EIO (5), car rien n'a été entendu. Corrigé au 163.

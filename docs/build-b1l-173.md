# Build 173 : la première trame émise, tracée avant d'être envoyée

## Diagnostic

Build 172 : avec 256 tampons RX, la connexion WPA2 gèle le téléphone comme le réseau ouvert. Le point commun des deux gels est la première trame émise par la flowring (DHCP pour le réseau ouvert, message 2 du WPA2 maintenant que le message 1 arrive). L'élément TX_POST correspond champ par champ à `host_txbuf_post_v1` (48 octets). Hypothèse : le dongle plante sur cette émission, le lien PCIe tombe, et la lecture suivante de la TCM bloque le processeur. Le journal perd les 5 dernières secondes.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp`, traces seulement :
- `B2L WIFI TXP` : les 4 premiers éléments TX_POST, en mots bruts, avec l'index d'écriture de la flowring ;
- avant la toute première trame émise, `B2L WIFI TX: first frame (N bytes) waits 6 s for the log` puis 6 s d'attente, pour que le journal soit sur le disque avant que le dongle voie la trame.

La première poignée de main échouera sans doute (le point d'accès renvoie le message 1 pendant l'attente). Le gel, s'il vient de l'émission, arrive après « goes now ».

## Attendu

Si `EAPOL in`, `WPA: message` et `TX: first frame` apparaissent, la réception fonctionne et le gel est confirmé côté émission. Si le journal s'arrête avant, le gel vient de la réception.

## Installé avec hash BOOT

`98e7e77649f2d0b4d7c7760d4f2fc87733bfcf4691594c9bd9d9cdf658c694a1` (relu, BOOT-OK ; SYSTEM-OK en 66 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Gel à nouveau. Le journal montre `EAPOL: in, 99 bytes` (la réception marche depuis 172), puis `WPA: message -> 1`, la TXP du message 2 (format identique à `dhd_prot_txdata`) et « waits 6 s ». Le journal s'arrête environ 5 s plus tard, sans « goes now » : le gel tombe au moment de l'envoi vers la puce ou juste après..

# Build 169 : tampons RX à la taille demandée par le firmware

## Diagnostic

Build 168 : le dongle prend nos 32 tampons RX mais ne rend aucune complétion. À chaque message 1 du point d'accès, sa console écrit `EL: 30 6c50`, soit `EVENT_LOG_TAG_PCI_ERROR` (0x30). Le format de `host_rxbuf_post` est identique à celui de dhd (`dhd_prot_rxbuf_post`). Une différence reste : dhd lit l'iovar `rxbufpost_sz` du firmware (`dhd_sync_with_dongle`) et poste des tampons de cette taille, alors que nous postions 2048 octets fixes.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` :
- lecture de `rxbufpost_sz` avant de poster les tampons RX (`B2L WIFI DATA: rxbufpost_sz status=.. value=..`) ; la valeur est utilisée si elle est entre 1024 et 4096, sinon 2048 ;
- emplacements RX de 4 Kio (bloc de données 0x31000 : flowring, RX 0x1000–0x21000, TX 0x21000–0x31000) ;
- la console du firmware est relevée chaque seconde pendant la poignée de main.

## Attendu

Si la taille était la cause : `RXC ok=1` avec l'EAPOL du point d'accès, `EAPOL in`, `WPA: message`, puis les clés. Sinon, la console du firmware et la valeur `rxbufpost_sz` orientent la suite.

## Installé avec hash BOOT

`6b8013182853c9d978a0727fe7e1232e9376b0affc8cb874a2d6b3505415b51d` (relu, BOOT-OK ; SYSTEM-OK en 120 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Le firmware demande `rxbufpost_sz` = 1670, et nous postons maintenant cette taille. Rien ne change : `rxpost 32/32`, `rxcpl 0/0`, `EL: 30 6c50` à chaque message 1 du point d'accès, puis désauthentification (raison 15). Le réseau ouvert gèle encore le téléphone, sans trace écrite. La taille des tampons n'était pas en cause. Voir le build 170.

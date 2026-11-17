# Build 168 : traces du chemin de données Wi-Fi

## Diagnostic

Au build 167, l'association réussit (LINK up, flowring 2 créée), mais aucune complétion RX n'est vue en 8 s : pas d'EAPOL, donc la poignée de main expire (error 60). Le firmware annonce `flags=0x30040007` : synchro D2H XOR (0x40000), pas de numéro de séquence ni d'index DMA, ce qui correspond au contrôle `b2l_wifi_d2h_ok`. Les tailles d'éléments rev7 (RXPOST 32, TXCPL 24, RXCPL 40, TXPOST 48) sont vérifiées contre `bcmmsgbuf.h`.

Un réseau ouvert a gelé le téléphone. Le journal s'arrête avant la requête, parce que l'écrivain ne vide que toutes les 5 s.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp`, traces seulement :
- `B2L WIFI TXC` / `B2L WIFI RXC` : les 60 premières complétions des anneaux TX et RX, en mots bruts, avec le résultat du contrôle de synchro ;
- `B2L WIFI RINGS` : chaque seconde de la poignée de main, les index écriture/lecture de RXPOST, de la flowring, de TXCPL, de RXCPL et de CTRLCPL, pour savoir si le dongle consomme nos tampons RX et s'il répond.

## Attendu

Tester un réseau WPA2, pas le réseau ouvert. Si `rxpost r` reste à 0, le dongle ne prend pas les tampons RX. Si `rxcpl w` avance sans `RXC ok=1`, le problème est dans la synchro ou le format.

## Installé avec hash BOOT

`612b62486cdf7d9f14c228d07d42bd1fdf0bc5139c2a7117b9416874e96ddc8d` (relu, BOOT-OK ; SYSTEM-OK en 112 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Réseau WPA2 en 2,4 GHz : association OK et flowring créée. `rxpost w/r=32/32` : le dongle a pris nos 32 tampons RX, mais `rxcpl w` reste à 0 et rien ne part (`flow 0/0`). Le point d'accès renvoie le message 1 chaque seconde, puis désauthentifie : événement 6 (DEAUTH_IND), raison 15, expiration de la poignée de main. À chaque renvoi, la console du firmware affiche `EL: 30 6c50` : l'étiquette 0x30 vaut 48 = `EVENT_LOG_TAG_PCI_ERROR`. Le firmware reçoit donc la trame mais échoue côté PCIe en la remontant. Piste : la taille des tampons RX (dhd lit `rxbufpost_sz`), voir le build 169.

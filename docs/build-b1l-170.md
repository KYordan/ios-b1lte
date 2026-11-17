# Build 170 : adresses physiques du bloc de données et erreurs PCIe

## Diagnostic

Le dongle accède sans problème au bloc des anneaux (64 Kio) : ioctls, événements et lecture des RXPOST. En revanche, il n'a jamais rien écrit dans le bloc de données (tampons RX) ni émis depuis lui. À chaque trame entrante, il signale `EVENT_LOG_TAG_PCI_ERROR`. Nous lui demandons de ne pas planter sur une « Unsupported Request » (`HOSTCAP_UR_FW_NO_TRAP`), ce qui correspond à un accès refusé côté hôte.

Le bloc de données (196 Kio, plusieurs pages de 16 Kio) était adressé « base + décalage » sans vérifier qu'il est physiquement contigu. Si ce n'est pas le cas, le dongle écrit hors de nos tampons. Cela expliquerait aussi le gel avec un réseau ouvert : une écriture dans une mémoire étrangère, ou la perte du lien PCIe pendant qu'on lit la TCM.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` :
- `b2l_data_pa` : l'adresse de chaque tampon RX et TX, et de la flowring, vient de `getPhysicalSegment` à son propre décalage ;
- `B2L WIFI DMA: ... pages out of place` : contrôle page par page des deux blocs DMA au démarrage ;
- `B2L WIFI PCIERR[boot|join]` : état PCIe du dongle (status, device status, AER), relevé au démarrage et chaque seconde de la poignée de main.

## Attendu

Si des pages sont « out of place », la correction d'adresse doit faire arriver l'EAPOL (`RXC ok=1`, `EAPOL in`). Sinon, `PCIERR` indique le type d'erreur (bit 13 du status : Received Master Abort, ou AER).

## Installé avec hash BOOT

`f687f138bc30ab19ab2873a90576fe6ccc40ce7c56fba875202262ba03a1b729` (relu, BOOT-OK ; SYSTEM-OK en 128 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Les deux blocs sont contigus (`0 pages out of place`). Côté dongle, le status vaut 0x10 et devsta 0x10 ; AER à 0 avant et pendant la poignée de main : aucune erreur PCIe matérielle. `EL: 30 6c50` est donc une erreur interne de la couche PCIe du firmware, pas un accès refusé par l'hôte. Même échec (raison 15). Voir le build 171.

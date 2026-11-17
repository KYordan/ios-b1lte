# Build 172 : autant de tampons RX que dhd

## Diagnostic

Tout ce qui précède est écarté : format des postes RX, taille (1670), adresses, contiguïté, erreurs PCIe matérielles, HSCB. Il reste une différence nette avec dhd : le nombre de tampons. dhd poste `max_host_rxbufs` tampons (`dhd_prot_init`, `MAX_HOST_RXBUFS` lu dans pciedev_shared + 34, 256 par défaut) sur des anneaux RXPOST et RXCPL de 512 éléments. Nous en postions 32, sur des anneaux de 64. Le firmware fonctionne en `SPLITRX_MODE: 4` : il confie des tampons hôte au MAC. À chaque trame, il doit sans doute en reprendre un neuf ; s'il n'en a plus, il jette la trame et signale l'erreur dans sa couche PCIe (`EL: 30`).

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` :
- le bloc des anneaux passe à 128 Kio ; RXPOST (512 × 32) à 0x10000 et RXCPL (512 × 40) à 0x14000 ;
- le bloc de données passe à 0x91000 : 256 tampons RX de 2 Kio, puis 32 tampons TX ;
- le nombre posté vient de `max_host_rxbufs` (au plus 256). Trace : `B2L WIFI DATA: total_lfrag_pkt_cnt=.. max_host_rxbufs=.., posting ..`.

## Attendu

`RXC ok=1`, `EAPOL in`, `WPA: message`, puis les clés.

## Installé avec hash BOOT

`ad97eae71cf6538b9efffe6dbe0a1ba966ec2710c6700b64d501a453a23ae55f` (relu, BOOT-OK ; SYSTEM-OK en 90 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

`max_host_rxbufs=511`, `total_lfrag_pkt_cnt=225` ; 256 tampons postés, blocs contigus. Le réseau WPA2 ne renvoie plus d'échec : il gèle désormais le téléphone dès la connexion, comme le réseau ouvert. Les dernières secondes ne sont pas dans le journal (écriture toutes les 5 s). La réception semble donc débloquée, et c'est la première émission qui gèle (réseau ouvert : DHCP ; WPA2 : message 2). Voir le build 173.

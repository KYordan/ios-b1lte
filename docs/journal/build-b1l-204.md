# B1L-0204 — données de fin de join d'iOS 26, battement Wi-Fi par seconde

Base : `b2l-metal-v95`, `--no-libs`. Le client IOKit de 0203 est retiré (`B2LHakiosWLAN.cpp` revient à sa version 0202).

## Cause de « échec du join » (IO80211 23G90, désassemblé)
- `__eventRead`, pour l'événement 9 pendant un join asynchrone, appelle `__dispatchAsyncAssocEvent` avec les données de
  l'événement :
  - 10 entrées de 44 octets ;
  - +448 : statut 1000..1010 ;
  - +452 : résultat, comme `ASSOCIATE_RESULT` (1 = rejoint ; `__errorForAssociateResult` le change en 0) ;
  - +456 : « résultat présent ».
- Si +456 vaut 0, le framework demande le résultat par un chemin IOKit qu'on n'a pas, d'où « échec ». Notre ASSOC_DONE
  était vide.

## Changements
- `bsd/dev/b2l_hakios_apple80211.c` : ASSOC_DONE porte 464 octets, avec +452 = `join_result` et +456 = 1.
- `bsd/kern/kern_event.c` (sauvegarde `../kern_event.c.203`) : `kev_post_msg` prend un cluster mbuf jusqu'à
  `MCLBYTES` au-delà de `MLEN` (sinon l'événement est refusé : EMSGSIZE). Seul cet événement dépasse `MLEN`.
- `iokit/Kernel/arm/B2LWifi.cpp` : `B2L WIFI T+Ns` chaque seconde pendant 5 min après le join, avec RX reçus et
  mauvais, buffers TX occupés, indices des anneaux rxpost, rxcpl, flow et txcpl. But : savoir si la radio et les anneaux
  tournaient encore juste avant un gel (App Store, Safari).

## Résultat
Le join depuis Réglages marche enfin : la feuille de mot de passe se ferme. Ouvrir Stocks fait tout geler en moins de 1 s. Battement juste avant (`out/logs/0204-boot.txt`) : `T+11s: rx 653 bad 0 txbusy 0`, tous les anneaux à jour (r = w ou presque), 115 trames reçues dans la dernière seconde, ce qui est déjà arrivé sans gel à T+1s. La radio et les anneaux sont sains au moment du gel. Safari ouvert sans réseau (0151) n’a pas gelé.

### Cause du gel (journal `out/logs/0204b-boot.txt`, Stocks ouvert puis Wi-Fi connecté)
Ce n'est pas un gel mais une panique noyau : `Kernel data abort, far=0x8` dans `OSKext::foreachKextInBacktrace`
(OSKext.cpp:15718), appelée par `telemetry_backtrace_add_kexts` ← `drain_record_submission_buffer_thread_call`
(trap_telemetry.c:896). `gLoadedKextSummaries` vaut NULL sur ce port (aucun kext chargé), donc le premier
enregistrement de télémétrie de trap fait paniquer le noyau. Corrigé en 0205.

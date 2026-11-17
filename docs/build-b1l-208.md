# B1L-0208 : compteurs de trafic + trousseau sélecteur 35

## Pourquoi
En 0207, l'heure et le trousseau sont bons mais les apps expirent (`-1001`), des connexions TCP restent en `SYN_SENT`,
et wifid déconnecte ~60 s après chaque connexion (« IPv4 not routable »). Il faut savoir où les paquets s'arrêtent.

## Changements
- `bsd/dev/b2l_hakios_net.c` (sauvegarde `../b2l_hakios_net.c.207bak`) : compteurs par type de trame dans les deux sens
  (TCP total, SYN envoyés, SYN+ACK reçus, RST ; DNS ; DHCP ; ARP ; ICMP reçus ; IPv6), ligne
  `net: en0 kinds: ...` avec la ligne `so far`, maintenant toutes les ~4 s d'activité (2048 tours au lieu de 8192).
  Aucun nom ni adresse n'est journalisé.
- `iokit/Kernel/arm/b2l_aks_core.c` (sauvegarde `../b2l_aks_core.c.207bak`) : sélecteurs 17/35 répondent « sans code »
  pour tout handle (comme ios-pipa 0055), au lieu de NOT_FOUND hors handle 0.

## Résultat
- Le chemin des données marche : SYN 30 / SYN+ACK 30, DNS 51/51, 1,4 Mo reçus, 9 trames sortantes jetées
  (32 tampons TX pleins). Le Wi-Fi tombe ~60 s après la connexion (wifid DISASSOCIATE, raison « not routable »).
- `log show` du Mac lit enfin l'archive iOS : il fallait un `Info.plist` avec `EndTimeRef`/`OldestTimeRef` pris dans
  `timesync` (script `mkinfo.py`, voir mémoire). Les requêtes HTTP des démons réussissent (200, TLS 1-4 s).
- Cause : `WiFiDeviceScheduleWithQueue` s'arrête sur « Apple80211FilterMonitoringEvent failed » (ioctl SET 564
  `APPLE80211_IOC_FILTER_MONITOR_EVENTS` refusé par le pilote) **avant** de créer le store configd du device
  (`device+0x98`, rappel `__WiFiDeviceDynamicStoreCallback`). wifid ne reçoit jamais l'IPv4 (`device+0xdf0` reste 0),
  conclut « not routable » et coupe ; geod passe alors en `net_down` et Météo échoue (« Missing URL »). Corrigé en 0209.

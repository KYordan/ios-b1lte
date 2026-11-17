# B1L-0209 : wifid voit l'adresse IPv4 (ioctl 564 accepté)

## Pourquoi
En 0207/0208, wifid coupe le Wi-Fi ~60 s après chaque connexion (« IPv4 not routable »), ce qui casse les apps
(geod `net_down`, Météo « Missing URL »). Le journal iOS complet montre au démarrage de wifid :
`WiFiDeviceScheduleWithQueue: Error: monitoring Apple80211FilterMonitoringEvent failed` — l'ioctl SET 564
(`APPLE80211_IOC_FILTER_MONITOR_EVENTS`) est refusé (-3903). La fonction sort alors avant `SCDynamicStoreCreate`
du device ; son rappel ne reçoit jamais `State:/Network/Service/*/IPv4`, donc l'adresse du device reste 0.

## Changement
- `bsd/dev/b2l_hakios_apple80211.c` (sauvegarde `../b2l_hakios_apple80211.c.208bak`) : `set()` accepte le type 564
  (retour 0). Le pilote envoie déjà tous les événements ; le filtre n'a rien à faire.

## À vérifier
- Plus de `WiFiDeviceScheduleWithQueue: Error`, présence de `{IP+} IPV4 State change` et `interface is IPv4 routable`.
- Plus de DISASSOCIATE ~60 s après la connexion ; Météo/Safari chargent.

## Résultat
- 564 accepté : wifid voit l'IPv4 (« IPV4 State change for en0 », « interface is IPv4 routable »), plus de coupure à 60 s. Les démons (identityservicesd, mobileassetd, translationd…) ont des chemins `satisfied` sur en0 et se connectent.
- Météo vide : geod charge son « resource manifest » une seule fois au boot, 12 s avant l'IP (-1009). Il passe en mode « relancer au changement de joignabilité » (octet +48), mais ne relance jamais, même au passage 2,4→5 GHz ; sans manifeste, ses requêtes Météo échouent tout de suite. Cause côté noyau non trouvée (le réseau marche pour les autres).
- Safari : boucle de plantage de WebContent (~16 relances), problème JIT suivi par la session Metal, pas le réseau.
- Test proposé sans build : couper puis rallumer le Wi-Fi après le boot, puis ouvrir Météo.

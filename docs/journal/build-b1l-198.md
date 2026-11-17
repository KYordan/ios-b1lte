# B1L-0198 — patch : lien normal, économie d'énergie coupée, déconnexions sérialisées, test de l'événement seul

Base : `b2l-metal-v95`, `--no-libs`. Seul `B2LWifi.cpp` change (journal 1 s et traces en0 du 196 gardés).
- `g_link_to_ios = true`, plus d'étapes : lien, trames reçues et émises dès le join (repère `T+Ns` toutes les ~2 s jusqu'à 90 s).
- Après les clés : `WLC_SET_PM` (86) = 0, PM_OFF (la liaison PCIe ne s'endort pas) ; `B2L WIFI JOIN: power save off: N`.
- `b2l_unjoin` sous `g_unjoin_lock` : la déconnexion de wifid et celle du thread du pilote (« link lost ») une à la fois.
- Test : ~60 s après l'offre de la station, sans réseau joint, `hakios_net_link_state(true)` puis ~10 s après `(false)` :
  lignes `B2L WIFI EVENT TEST`. Un gel là = la réponse d'iOS à l'événement de lien, pas la radio.

## Résultat
Bloqué au logo Apple : le test de l'événement seul (tick 1200, pendant le démarrage, aucun réseau) a suffi. `EVENT TEST: link up sent`, configd lit SIOCGIFMEDIA (lien actif), retrouve en0 dans IOKit, `CARD_CAPABILITIES`, `SSID` (error 6), puis plus rien (≤1 s). La radio n'y est pour rien : c'est ce que le système fait sur en0 quand son lien passe actif.

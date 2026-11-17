# B1L-0194 — Wi-Fi : annonce du lien découpée en temps

Base : `b2l-metal-v95`, `--no-libs`. Change `B2LWifi.cpp` et `b2l_hakios_net.c`.

193 a gelé dans les 5 s du join, sans trame : c'est l'annonce du lien. Après le premier join :
- 0–20 s : en0 marqué up (les envois d'iOS atteignent le pilote et sont refusés), **sans** événement noyau ;
- 20 s : `KEV_DL_LINK_ON` envoyé seul (`hakios_net_link_announce`, depuis le thread du pilote) ;
- 40 s : trames reçues vers iOS ;
- 60 s : iOS émet.
`B2L WIFI T+Ns` toutes les ~2 s jusqu'à 90 s date le gel.

## Résultat
Mot de passe tapé, « connecter » : échec affiché (« Join network » de nouveau) puis gel immédiat. Aucune ligne `ASSOCIATE`/`JOIN`/`STAGE`/`T+` écrite : connexion, échec et gel tiennent dans les ≤5 s jamais écrites. Le propriétaire précise que le gel arrive aussi sur réseau ouvert (sans fenêtre de mot de passe). Le journal (écrit toutes les 5 s, `b2l_persistent_log.c`) ne permet plus de situer le gel.

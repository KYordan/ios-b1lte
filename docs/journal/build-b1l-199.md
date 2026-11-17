# B1L-0199 — protocoles d'en0 tracés, IPv6 refusé

Base : `b2l-metal-v95`, `--no-libs`. Comme 0198 (test de l'événement au démarrage, PM off, déconnexions sérialisées), plus
dans `b2l_hakios_net.c` : `add_proto` et `check_multi` d'en0 enveloppés (`hakios_add_proto`, `hakios_check_multi`) qui
écrivent chaque protocole attaché et chaque groupe multicast ; `PF_INET6` refusé (EOPNOTSUPP).
But : 198 gèle ≤1 s après que configd voit le lien actif, sans toucher au pilote ; la configuration IP qui suit
(IPv4/DHCP, IPv6 lien-local, groupes multicast) est la suite logique. Sans gel, IPv6 est en cause.

## Résultat
Démarrage complet. **Test de l'événement au démarrage passé sans gel** (lien up puis down, `EVENT TEST: link up/down sent`),
alors que 198 bloquait au logo au même test : l'IPv6 d'en0 explique ce gel-là. configd a tenté 5 fois d'attacher
IPv6 (refusé, `failed to attach v1 protocol 30 (err=102)`) ; IPv4 (protocole 2) attaché au démarrage sans souci.
Puis un join WPA2 : gel ≤1 s après `B2L WIFI JOIN: ... set_ssid=0`, avant le moindre événement du firmware (au 196,
les événements 54/87/3 suivaient aussitôt). Aucun protocole attaché ni lien annoncé à ce moment : autre déclencheur.

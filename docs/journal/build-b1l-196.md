# B1L-0196 — chaque appel à en0 tracé une fois le lien up

Base : `b2l-metal-v95`, `--no-libs`. Comme 0195 (journal 1 s, étapes 0/20/40/60 s), plus dans `b2l_hakios_net.c` :
- une fois en0 up, chaque ioctl qui n'est pas apple80211 est écrit avant (`asked`, 400 au plus) et après (`answered`) ;
- les 60 premières trames sortantes : `out N: taille, type, par qui`, puis `bpf done`, puis `done`.
But : la dernière ligne avant le gel (qui suit `KEV_DL_LINK_ON` à ≤1 s) dit quel appel ne revient pas.

## Résultat
Gel **à l'étape 0**, 1–2 s après un join WPA2 réussi (`keys in`, `joined, frames pass`, `T+0s`, pas de `T+2s`) :
aucun `KEV_DL_LINK_ON`, aucune ligne `net: out`, aucun ioctl hors apple80211. Dernières lignes : wifid cherche
AppleKeyStore, sondages CURRENT_NETWORK/POWER. L'utilisateur voit l'échec de connexion puis le gel.
Donc en0 n'était pas sollicité : l'annonce du lien n'est pas forcément la cause (195 avait tenu 20 s à l'étape 0).

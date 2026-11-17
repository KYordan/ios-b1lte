# B1L-0182 — la première trame reçue donnée à iOS, découpée

Fichier : `xnu-overlay/bsd/dev/b2l_hakios_net.c` (copié dans `xnu/`).

Dans `hakios_net_input_unit`, à la première trame seulement, une ligne `net: in step` puis 6 s de pause à chaque étape :
- avant le mbuf ;
- mbuf fait, `bpf_tap_in` ensuite ;
- `bpf_tap_in` fait, `ifnet_input` ensuite ;
- `ifnet_input` rendu ;
- vivant 6 s après.

Pendant ce temps, les autres trames sont jetées (`dropped_in`), pour qu'aucun autre fil ne passe au même endroit.

Flashé : SYSTEM 63 s, BOOT relu 444fa3eab8dc7d3d0e191ef71c0c31e505082ac67e0344244859dc90cf7f6a32.

Résultat : gel, et **aucune** ligne `net: in step`. On n'est jamais entré dans `hakios_net_input_unit`, alors que deux fils avaient écrit « a frame to the stack next (42, 2054) » et dormaient 6 s avant l'appel. Le gel tombe donc ailleurs, environ 6 à 12 s après KEV_DL_LINK_ON, que des trames montent ou non (au 180, aucune ne montait). Les 8 surveillances de `net_watch` étaient toutes prises par les requêtes apple80211 de wifid (qui passent), donc rien d'autre n'a été vu.

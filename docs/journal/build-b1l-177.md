# B1L-0177 — montée du lien et premières trames, pas à pas

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

`b2l_step` fonctionne sur un budget de 12 étapes, au lieu de s'arrêter à la première connexion. Nouvelles étapes :
- avant et après la première remise d'une trame à la pile (`hakios_net_input`), avec sa taille et son ethertype ;
- avant et après le premier envoi venant de la pile (`b2l_link_transmit`).

Les étapes du 176 (clés, montée du lien) restent.

Flashé : SYSTEM 59 s, BOOT relu 86f1a8cfb278bc137d590630808305cc5af627142bbb87178dc5b00efad8cbd2.

Résultat : gel **avant** la montée du lien. Clés posées, étape « joined, link up next » écrite, puis quelques trames de 42 octets jetées (lien pas encore monté). Le journal s'arrête pendant la pause, sans « link up done ». Ce n'est donc ni `hakios_net_link_state` ni `hakios_net_input`. Pendant ce temps, seuls tournent les fils de sondage (`b2l_wifi_poll`, `b2l_data_poll`) et wifid (`sets 481`, `COUNTRY_CODE`, refusés). Hypothèse : un accès au bus qui bloque (puce en veille après l'association) ou un panic qui ne laisse pas de trace (le journal n'est écrit que toutes les 5 s).

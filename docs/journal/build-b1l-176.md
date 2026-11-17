# B1L-0176 — ce qui suit le message 4, pas à pas

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

Les étapes TXW du 175 sont retirées (elles cassaient la poignée de main). `b2l_step` écrit une ligne `B2L WIFI STEP` puis attend 6 s, à la première connexion seulement :
- message 4 envoyé (résultat, tampon encore occupé), clé pairwise ensuite ;
- clé pairwise posée (statut), clé de groupe ensuite ;
- clé de groupe posée (statut, index) ;
- connecté, lien à monter ;
- lien monté (`hakios_net_link_state(true)`).

Les deux dernières étapes concernent aussi le réseau ouvert.

Flashé : SYSTEM 59 s, BOOT relu 64da1e942b13849c151da4a6959106fb660aafc3cc13e1e61dc3c73e25ca695e.

Résultat : gel. Les messages 1 à 4 passent, les deux clés sont posées (`pairwise=0 group=0 (index 1)`), l'étape « joined, link up next » est écrite. Pendant la pause qui suit, les premières trames de données arrivent (42 octets, data_offset 0x52) et sont jetées, car le lien n'est pas encore monté. Le journal s'arrête là. Restent : la montée du lien (`hakios_net_link_state`), la remise des trames à la pile (`hakios_net_input`), ou le premier envoi depuis la pile.

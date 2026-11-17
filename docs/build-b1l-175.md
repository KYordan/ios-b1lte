# B1L-0175 — ce qui suit la première trame, pas à pas

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

Les étapes TX1–TX5 du 174 sont retirées : l'envoi redevient normal. Après le message 2 (première fois seulement), dans la boucle de poignée de main, avec 6 s entre chaque étape :
- TXW1 : état des anneaux txcpl et rxcpl, drapeaux partagés ;
- TXW2 : après `b2l_data_poll` (accusé d'envoi et éventuel message 3) ;
- TXW3 : après `b2l_wifi_poll` (événements) ;
- TXW4 : toujours vivant.

Flashé : SYSTEM 65 s, BOOT relu 220789da964c966aad2925ab4a7a0c00e5d8e5174b44e90f2643061c244d4cea.

Résultat : gel, mais la poignée de main WPA2 passe entièrement :
- message 1 reçu, message 2 envoyé (TX_STATUS ok) ;
- message 3 reçu (155 octets, répété par le point d'accès pendant nos pauses) ;
- TXW1 à TXW4 présents ; les pauses provoquent un deauth (raison 15) puis une réassociation du firmware ;
- le message 3 en attente est ensuite traité (`message -> 2`) et le message 4 envoyé (`EAPOL: out, 99 bytes`).

Le journal s'arrête là : le gel suit le message 4 (attente de son accusé, installation des clés ou mise en route du lien).

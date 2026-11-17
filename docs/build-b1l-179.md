# B1L-0179 — battement de cœur pendant le flux de trames

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

Le silence du 178 est retiré. Compteurs ajoutés : `g_rxn` (complétions RX lues), `g_rxbad` (synchronisation XOR ratée) et `g_repost` (tampons RX remis). Après la clé de groupe, 10 battements de 6 s (`B2L WIFI BEAT`) donnent :
- les compteurs ;
- les index de rxpost (TCM w/r), de rxcpl (TCM w et notre rd) et de ctrlcpl ;
- le nombre d'événements ;
- la console du firmware.

Le dernier battement présent dira dans quel état étaient les anneaux juste avant le gel (débordement, retour à zéro).

Flashé : SYSTEM 60 s, BOOT relu cc4b6f7c0eec2e6a5417720acef018e87fd492b9f091d68d4828b90bba8ce1d2.

Résultat : gel, mais le flux de trames tient 60 s sans geler. Dix battements de `rx=11` à `rx=112`, `bad=0`, rxpost et rxcpl avancent normalement (pas de retour à zéro, pas de file pleine), 50 événements. Le gel revient juste après « joined, link up next », comme dans les 176 à 178. Ce n'est donc pas la réception : c'est la montée du lien ou ce qu'elle déclenche. La perte des 5 dernières secondes du journal empêchait de voir que « link up done » manquait parce que `hakios_net_link_state` ne rendait pas la main.

# B1L-0184 — connexion terminée sans annoncer le lien à iOS

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

- `g_steps = 12` : toutes les étapes de 6 s et le battement sont coupés.
- `g_link_to_ios = false` : à la fin de la connexion, pas de `hakios_net_link_state(true)` (pas de KEV_DL_LINK_ON) et aucune trame remise à la pile (`hakios_net_input`). La connexion se termine quand même pour wifid (JOIN_DONE, « the link, 1 »).

Lecture :
- pas de gel en 2 à 3 min : le gel vient de la réaction de la pile réseau (configd, IPConfiguration, mDNSResponder...) au lien monté ;
- gel : il vient du côté wifid ou du pilote, après la fin de la connexion.

`b2l_hakios_net.c` garde les instruments du 183, inactifs sans montée du lien.

Flashé : SYSTEM 65 s, BOOT relu 6afb95f791d94cdf14bb4260be5af1ea49322be2d1bc6a6fae9d8eedcfad892e.

Résultat : **pas de gel** en plusieurs minutes, et plus de clignotement (il venait sans doute des pauses de 6 s). Trois connexions WPA2 complètes et rapides, sans pauses : messages 1 à 4, clés posées, JOIN_DONE 0, « the link, 1 ». Après chaque connexion, wifid fait un scan, puis se déconnecte (WLC_E_DISASSOC raison 8, puis LINK raison 2) et se reconnecte. C'est attendu, puisque en0 n'a ni lien ni IP côté iOS dans ce test.

Conclusion : le gel vient de la réaction de la pile réseau d'iOS au lien monté. Le propriétaire a vu une ligne s'afficher à l'écran lors du dernier gel (183). C'est la bande de panic de `b2l_early_panic` (B2L FIRST PANIC, B2L BT, FAULT ou PANIC FORMAT) : c'est donc un panic noyau, pas un blocage.

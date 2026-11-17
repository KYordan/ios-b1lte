# B1L-0180 — montée du lien découpée

Fichiers : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` et `xnu-overlay/bsd/dev/b2l_hakios_net.c` (copiés dans `xnu/`).

- B2LWifi : `hakios_net_link_state(true)` est appelé avant `joinedUp = true`, avec une étape de 6 s entre les deux. Les trames ne montent vers la pile qu'après.
- hakios net : à la première montée du lien, une ligne `net: link step` et 6 s de pause avant puis après `net_link_event` (`ifnet_event` KEV_DL_LINK_ON).

Le battement du 179 reste.

Flashé : SYSTEM 62 s, BOOT relu 9341f602e1cde5ae4a62abf514c425025eb7a02ed58796c477896b1309b0806f.

Résultat : gel **juste après l'événement noyau**. Dans l'ordre : `net: en0's link (bcm4375) is up`, `link step: kernel event next`, `kernel event sent`, puis configd ouvre HakiosWLAN (pas de user client), demande CARD_CAPABILITIES (répondu) et SSID (erreur 6). Le journal s'arrête là ; « link up done » n'apparaît jamais. Aucune trame n'était encore remise à la pile (`joinedUp` faux), et `b2l_link_transmit` jette tout envoi tant que `joinedUp` est faux. Le gel vient donc de la réaction d'iOS à KEV_DL_LINK_ON (configd, DHCP, IPv6) et de ce qu'elle appelle sur en0.

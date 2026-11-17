# B1L-0181 — les premiers appels sur en0 après la montée du lien

Fichier : `xnu-overlay/bsd/dev/b2l_hakios_net.c` (copié dans `xnu/`).

Après l'événement KEV_DL_LINK_ON de la première montée du lien, les 8 premiers appels à `net_ioctl` et `net_output` sont encadrés :
- avant : `net: watch N: ioctl|output <commande|taille> by <processus>, next`, puis 6 s ;
- après : `net: watch: ... done, <erreur|envoyé>`, puis 6 s.

La pause de 6 s après l'événement est retirée. B2LWifi est inchangé depuis le 180 (lien monté avant `joinedUp`).

Flashé : SYSTEM 61 s, BOOT relu f491d082d03f803e6ee2da13a35920501715b8ab612f26da33a2680a5a3e2cbb.

Résultat : gel, mais plus tard, et **à la première trame donnée à iOS**.
- `watch 1` (ioctl 0xc02c6938 de configd) passe, et la connexion se termine : `joined, frames pass`, puis « a join is over, 0 » et « the link, 1 » côté apple80211.
- wifid lit POWER, CHANNEL, CURRENT_NETWORK (×5), SSID et BSSID : tout est répondu.
- `STEP: a frame to the stack next (42, 2054)` (un ARP), mais « the stack took it » ne vient jamais. Le journal s'arrête environ 5 s plus tard.

Le gel est dans `hakios_net_input` ou dans ce qu'il déclenche.

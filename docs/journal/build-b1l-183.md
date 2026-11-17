# B1L-0183 — surveillance de en0 sans les requêtes apple80211

Fichier : `xnu-overlay/bsd/dev/b2l_hakios_net.c` (copié dans `xnu/`).

`net_watch` ignore SIOCGA80211/SIOCSA80211 (0xc02869c9 et 0x802869c8, toutes répondues sans souci au 181) et suit 16 appels au lieu de 8 : les autres ioctl (configd, IPConfiguration, IPv6, BPF) et les envois `net_output`. Chacun est entouré d'une ligne et de 6 s de pause avant et après.

Flashé : SYSTEM 69 s, BOOT relu a4ce7787428a7feacb337c658df589d0ea2dce1cdcdcfab8ed207bf96a968407.

Résultat : gel, encore plus tard. `watch 1-3` (SIOCGIFMEDIA 0xc02c6938 de configd) passent. Puis `in step: 42 bytes, type 0x806: mbuf next`, `mbuf made, bpf next`, et le journal s'arrête environ 1 s après. Aucun autre appel d'iOS sur en0 (pas de SIOCSIFFLAGS, pas d'adresse, pas d'envoi) en environ 40 s.

Mesure du temps entre la montée du lien et la fin du journal, sur les 180 à 183 : environ 18 à 45 s, et non quelques secondes comme je le croyais. Le gel n'est donc lié à aucun de nos appels de façon immédiate.

Remarques du propriétaire : l'interface clignote pendant les mouvements depuis ce build, avant même la connexion. Le gel n'est pas venu à la première tentative de connexion, mais à la deuxième. L'époque 186 du journal est un démarrage plus ancien.

# B1L-0178 — silence sur le bus après les clés

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

Quand `g_hush` est vrai, personne ne touche la puce : `b2l_wifi_poll` et `b2l_data_poll` sortent tout de suite, la console n'est pas lue et `b2l_wifi_ioctl` rend -997. Après la clé de groupe, à la première connexion :
- `HUSH: 0/6/12/18 s` : 24 s de silence ;
- `HUSH: one read next` puis `read back flags=` : un seul accès à la TCM ;
- ensuite, le sondage reprend.

Lecture :
- gel pendant le silence : la cause n'est pas nos accès (DMA de la puce, interruption, autre fil) ;
- gel à la relecture : c'est l'accès au bus qui bloque (veille de la puce, état du lien) ;
- gel plus tard seulement : c'est le traitement qui suit.

Flashé : SYSTEM 75 s, BOOT relu 935046e2a122555e935b2ad78ea49f07ef4c8c784a4501f2a0e8e0421ec56999.

Résultat : gel, mais **pas pendant le silence**. Les 24 s sans accès passent (HUSH 0 à 18 s), et la relecture rend `flags=0x30040007` sans bloquer. Le sondage reprend : les trames accumulées (42, 86, 92 et 231 octets, data_offset 0x52) sont traitées et jetées, car le lien n'est pas monté. On voit aussi « joined, link up next ». Le journal s'arrête quelques secondes plus tard. Le gel suit donc le traitement des trames de données reçues, pas un accès isolé au bus ni la puce seule.

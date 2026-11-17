# B1L-0219 : correctif du gel de Safari de 0218

- Identique à 0218, sauf `patches/0108` : la maintenance du cache d'instructions passe par une fenêtre de copie du
  processeur (`pmap_map_cpu_windows_copy`), comme `b2l_unowned_cache_op` (0105). Elle ne passe plus par
  `phystokv()`, qui panique sur la RAM hors de sa table.
- BOOT `e67feb5f…e102` : BOOT-OK. SYSTEM-OK (7 blocs).

## Résultat

À remplir : Safari s'ouvre-t-il sans geler ? WebKit meurt-il encore ?
Résultat du propriétaire : le téléphone démarre, mais :
- Safari gèle encore tout le système (retour à TWRP) ;
- Musique plante dès l'ouverture, Fichiers plante dans « Partagé » ;
- Notes plante quand on prend le pinceau.

Analyse (`beyond2lte-ios/out/logs/0219-apps.txt`) :
- **Safari.** En 0212, WebContent mourait 65 fois de suite (instruction invalide, cache d'instructions périmé) sans
  geler le système, et n'ouvrait jamais IOSurface. Depuis 0218, il va plus loin : il ouvre IOSurface
  (`SURF OPEN pid=513`), puis tout gèle en moins d'une seconde. La dernière seconde n'arrive pas sur le disque, donc la
  cause n'est pas visible. Le gel peut venir du trap de cache, du GPU ou d'IOSurface. Voir 0220.
- **Musique et Fichiers.** Les deux s'arrêtent sur `_assertionFailure` de libswiftCore (pc 0x185d70878, glissement du
  cache 0x5988000 : 0x1803e8878). Il s'agit d'une erreur Swift volontaire. MessagesBlastDoorService meurt au même
  endroit depuis 0211, donc ce n'est pas l'émulation arm64e de 0218. Le code appelant reste à trouver, par le rapport
  `.ips` sur USERDATA.
- **Notes.** `B2L SURF: MobileNotes unsupported 40` et `41` : les sélecteurs IOSurface 40 et 41 ne sont pas
  implémentés, puis Notes sort. C'est un problème graphique, pas arm64e.

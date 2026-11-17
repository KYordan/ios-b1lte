# B1L-0220 : trace du gel de Safari (diagnostic)

Identique à 0219, plus `b2l_wk_trace()` (`osfmk/arm64/b2l_persistent_log.c`). Pour les processus
`com.apple.WebKit.*`, elle journalise :
- chaque appel aux user clients Mali, IOSurface et framebuffer (`call`/`ret` dans `B2LCallTimer`) ;
- chaque trap de cache EL0 (`ic`/`dc` : crm, va, pa) ;
- chaque émulation arm64e (`emu` : pc, instruction).

Pour les 300 premières lignes, la fonction réveille le fil du log et attend 40 ms qu'il écrive. Ainsi, la ligne juste
avant le gel atteint le disque. Il y a 4000 lignes au plus. Safari démarre donc environ 12 s plus lentement. C'est
voulu, et ce sera retiré une fois le gel compris.

- BOOT `9268b3a7…d5f7` : BOOT-OK. SYSTEM-OK (6 blocs).
- Noyau `ce8b94d3…f674`. Chaîne `B2L WK %u` vérifiée dans `mach.development.b2l`.

## Test demandé

Ouvrir Safari, attendre le gel, puis revenir dans TWRP. Ensuite, lire le log :
`B2L_BOARD=beyond1lte python3 scripts/pull-b2l-log.py --serial <SERIAL> --output out/logs/0220-safari.txt`.
La dernière ligne `B2L WK` montre la dernière action de WebKit avant le gel.

## Résultat

Propriétaire : Safari démarre comme avant. Au lieu d'afficher « une erreur s'est produite », la page ne charge pas,
puis le système gèle (retour à TWRP). Log : `beyond2lte-ios/out/logs/0220-safari.txt`.

- Les 300 lignes écrites d'avance sont **toutes** des émulations PAC (RETAB, BRAA, AUTIB…) dans **libcorecrypto**.
  Glissement du cache 0x1400c000, sous-cache .31. Il s'agit surtout de `ccdrbg_generate_internal`, `update` et
  `ccrng_process_init` : le générateur aléatoire, appelé par chaque processus WebKit. corecrypto n'est pas traduit,
  voir `docs/plans/corecrypto.md`, d'où un trap par instruction PAC. C'est lent, mais ce n'est pas le gel.
- Aucun appel GPU ou IOSurface n'a été journalisé, ni aucun trap de cache. Le gel est arrivé juste après la 300e
  ligne, donc sans trace écrite d'avance.
- WebContent 380 a été tué par libxpc (namespace 7) : il était probablement trop lent, à cause des attentes de 40 ms.

Suite : 0221.

# B1L-0166 : démarrage plus court — purge des enregistrements périmés du cache natif

Ensemble : `b2l-metal-v88` (noyau seul, `--no-libs`). Part de 0165.

## Pourquoi

Journal 0162 : launchd ne démarre qu'à ~200 s. Le cache de traduction natif (NCACHE, `b2l_native_cache.c`) lit son index
(~35 s, 69 575 enregistrements de 12 Kio sur SYSTEM), puis les relit tous pour les réappliquer (~95 s) :
`applied=0 already=6052 skipped=63523`, `mismatch=63523`. L'archive d'annulation est indisponible (`undo archive
unavailable (5)`), donc `reclaim_disabled=1` : les enregistrements périmés ne sont jamais libérés et sont relus à chaque démarrage.

## Changement

`xnu-overlay/bsd/kern/b2l_native_cache.c`, dans `b2l_native_cache_before_init` : un enregistrement dont l'inode a disparu
ou dont le fichier/la page ne correspond ni à l'original ni à la traduction (`SKIP(8)`/`SKIP(9)`) n'a rien appliqué ; il est
libéré par `b2l_ncs_reclaim` (sans copie d'annulation, aucun fichier d'iOS n'est modifié), puis la plage de lecture est
raccourcie par `b2l_ncs_trim` si la fin est libre.

- Budget : 60 s de purge par démarrage, le reste au démarrage suivant.
- Première erreur d'écriture : purge arrêtée pour ce démarrage.
- Pas en mode `b2l_cache_restore=1`. Désactivable : `b2l_cache_purge=0`.
- Journal : `B2L NCACHE PURGE: stale records freed=… kept=… used=… live=…`.

Gain attendu une fois la purge finie : ~1 min 30 (la relecture ne porte plus que sur ~6 000 enregistrements valides). Les
~35 s de lecture de l'index restent tant que des trous subsistent avant le dernier enregistrement valide.

## Résultat

(à remplir après le test)

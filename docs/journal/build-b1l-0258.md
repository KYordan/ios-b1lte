# B1L-0258 — le noyau reprend la mémoire GPU que Mesa a marquée purgeable

## Cause (log noyau du démarrage 0257)

- La mémoire s'épuise : `no memory for a buffer of 9043968 bytes (free 159 MB, wired 1629 MB …)`, puis une faute MMU du
  Mali et SpringBoard qui plante sur une adresse presque nulle ; le noyau panique ensuite au redémarrage forcé.
  Déjà vu en 0254 (14 allocations refusées).
- Rapport `mali mem:` (toutes les 60 s) juste avant/après : backboardd 452 Mo dont 228 purgeables, iconservicesagent
  372 Mo dont 213, SpringBoard 349 Mo dont 176. « Purgeable » = tampons rangés dans le cache de Mesa
  (`PANFROST_MADV_DONTNEED`), que notre noyau gardait toujours (`retained = 1`, B1L-0247 : « nothing is purged yet »).
- Mesa sait déjà les perdre : `panfrost_bo_cache_fetch` (pan_bo.c) demande `PANFROST_MADV_WILLNEED` et, si la réponse
  est `retained == 0` (panfrost_kmod.c), libère le tampon et en prend un autre. Mesa démappe un tampon avant de le mettre en
  cache (`panfrost_bo_unreference` → `panfrost_bo_munmap`, notre `IOConnectUnmapMemory64`) : plus de vue CPU dessus.

## Changement (`xnu-overlay/iokit/Kernel/arm/B2LMaliService.cpp`, sauvegarde `.before-0258`, copié dans `xnu/`)

- `b2l_mali_purge(want)` : parcourt les tampons de tous les clients ; un tampon purgeable, pas une IOSurface, qu'aucun
  travail n'utilise encore (`hakios_mali_wait(seq, 0)`) perd ses pages et ses adresses GPU ; la poignée reste réservée
  (`purged`) jusqu'au `GEM_CLOSE`. Comme le « shrinker » de panfrost sous Linux.
- Appelé avant une allocation quand il reste moins de 256 Mo libres (2 × la taille demandée + 32 Mo), et après un échec
  d'allocation (tout ce qui est purgeable), avec un second essai.
- `PANFROST_MADVISE` sur une poignée purgée → `retained = 0` ; `GEM_CLOSE` d'une poignée purgée → libérée.
- Log : `HakiosMali: B1L-0258: N purgeable buffers taken back, X MB (free now Y MB)` (40 au plus).
- Noyau seul, set v122 inchangé.

## Installé

- SYSTEM-OK, BOOT `9ba922e44213e0424c5c0d90d693bf9976d623514caec27657892d8ec87dfaa7`, BOOT-OK. Téléphone redémarré vers iOS.
- Aucun rapport de crash SpringBoard sur USERDATA (le panic est venu avant l'écriture) : la preuve reste le log noyau.

## Résultat

- Testé : bloqué sur le logo Apple. SpringBoard plante 3 fois (pids 28, 236, 291) sur `ctx->prog[VS]` NULL, sur des fils secondaires, puis « MACH Reboot ». La purge n'a pas été déclenchée. Voir build-b1l-0259.md.

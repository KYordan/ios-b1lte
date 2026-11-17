# Build B1L-0038 : vidage de cache physique hors RAM gérée, pile d'appels au panic

Date : 2026-11-06. Statut : **installé : BOOT (relecture `0554ce9e…`) ; pas encore testé.** Base : B1L-0037 (SYSTEM inchangé : `b2l-gl-v6`).

## Constat sur B1L-0037 (vidéo et photo de la console)

- **Progrès :** pour la première fois, `[208.53] b2l-gl: renderer asked: EAGLContext…` s'affiche. Le serveur d'affichage d'iOS 26 appelle notre crochet GLES. Mesa ouvre HakiosMali, enchaîne les `GET_PARAM` et lit `THREAD_TLS_ALLOC 0, THREAD_MAX_THREADS 0x300, SHADER_PRESENT 0x770077`.
- **Ensuite, panic sur le cœur MPIDR 0x101 :**
  `phystokv_range: illegal PA: 0xf05a4000; phys base 0x900000000, size 0xfb800000 @arm_vm_init.c:425`.
- `0xf05a4000` ne fait partie d'aucune plage de RAM B2L : la plage 8 s'arrête à `0xefffc000`, la plage 9 commence à `0x88ad00000`. C'est donc une page réservée.

## Cause

- `IOMemoryDescriptor::writeBytes()` appelle `copypv(cppvPsnk | cppvFsnk …)`.
- `bcopy_phys` copie correctement vers une page non gérée en passant par une fenêtre de copie du CPU (`pmap_valid_address()` faux).
- Mais sur cette plateforme non « coherent IO », `copypv` vide ensuite le cache de la destination avec `flush_dcache64(sink, phys=TRUE)`. `flush_dcache()` et `clean_dcache()` traduisent alors l'adresse par `phystokv_range()` sans vérification, ce qui provoque le panic.
- `dcache_incoherent_io_flush64()`, lui, utilisait déjà une fenêtre de copie pour ce cas.

## Changements

- `patches/0105-b2l-phys-cache-op-unowned-page.patch` (`osfmk/arm/caches.c`) : sous `B2L`, `flush_dcache` et `clean_dcache` passent par une fenêtre de copie (`pmap_map_cpu_windows_copy`) pour une page que `pmap_valid_address()` refuse. Les 8 premiers cas sont journalisés : `B2L CACHE: flush of PA …; caller 0x… (unslid)`.
- `xnu-overlay/osfmk/arm64/b2l_early.c` : l'écran de panic affiche maintenant `B2L BT:`, les adresses de retour (sans le slide) du cœur qui panique, en suivant la chaîne des pointeurs de pile (14 au plus). Une photo suffit alors pour retrouver la fonction en cause.
- Le noyau est copié à côté de l'image (`out/b1l-cache-0038/kernel.macho`) pour symboliser ces adresses.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-cache-0038/boot.img` | `0554ce9e2ce3515a5f3b5165a8c08f976137fba1602fe90511f0d166835599d8` |
| Kernel (`kernel.macho`) | `154439718993459506437ab30ef40d11e67fc12c6e3d3fe424601b848d1bd11f` |

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

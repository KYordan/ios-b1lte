# Build B1L-0119 : l'état de la mémoire quand un tampon GPU est refusé

Date : 2026-11-08. Base : B1L-0118. Noyau seulement (diagnostic), ensemble `b2l-metal-v70`.

## Pourquoi

Le verre (dock, dossiers, Centre de contrôle) reste noir ou gris. Le journal de 0115 montre 143 refus `HakiosMali: no memory for a buffer` (24 à 36 Mo) et des textures RGBA16Float et BGRA8 plein écran refusées : ce sont typiquement les images hors écran du flou et du verre. Les deux allocations du noyau échouent : celle par pages avec masque 40 bits, qui passe en fait par `kernel_memory_allocate` dans la plage `kmem_data_shared` (1 930 Mo), et le repli dans le même tas. Aucune fuite (`still in a job's hands` : 0). Hypothèse : la plage `kmem_data_shared`, partagée par toutes les IOSurfaces et tous les tampons GPU, est épuisée ou fragmentée.

## Changement

- `B2LMaliService.cpp` : total des tampons GPU vivants (`gMaliLiveBytes`). Le message de refus donne aussi la mémoire libre, la mémoire câblée, les tampons GPU vivants et les IOSurfaces (Mo).
- `B2LSurface.cpp` : `b2l_surface_wired_bytes()` pour ce message.

## Installé

- **BOOT :** relecture `125712e88b336c9ce49638adc8c51edac3897ed54b71d616ea74eea2e1afd02b` (BOOT-OK). Redémarré vers le système (0116 à 0119 testés ensemble).

## Résultat (journal `out/logs/0119-boot.txt`, photos du propriétaire) : régression

- **Propriétaire : lent, déchirures, carrés, couleurs cassées ; « rien à voir avec 0115 ».** Le fond d'écran de 0116 s'affiche bien (dégradé, vagues, points).
- Journal, 0115 → 0119 : tampons GPU refusés 143 → **0**, textures refusées 67 → **0**, fautes MMU 14 → **544**, jobs « did not finish » 1 → **265**, « THE GPU DID NOT FINISH a command buffer within two seconds » 0 → 19.
- Toutes les fautes sont des lectures du GPU **à l'adresse 0** (« the fault is in no buffer »), dans les passes du verre, du flou et des tracés (`variable_blur_vert`, `glass_background_no_bleed`, `sdf_filter`, `path_blit` + `attachment_clear`).
- Aucune ligne `B2L CSO` dans le journal (le cache de 0118 n'a rien dit).
- Suspect : B1L-0118 (cache des états de shader), directement, ou indirectement si les passes du verre, qui n'avaient plus leurs textures refusées, sont elles-mêmes fautives. → B1L-0120 retire 0118.

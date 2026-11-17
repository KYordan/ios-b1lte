# Build B1L-0121 : plus de place pour les tampons GPU (plage kmem_data_shared agrandie)

Date : 2026-11-08. Base : B1L-0120. Noyau seulement, ensemble `b2l-metal-v71`.

## Pourquoi

Le Liquid Glass (Centre de contrôle, dock, dossiers) est gris ou noir parce que ses images de travail (cibles RGBA16Float et BGRA8 plein écran, 24 à 36 Mo) sont refusées : 284 tampons GPU et 139 textures en 0120. Au moment du refus, 1,2 à 1,4 Go de RAM sont libres. Tous les tampons GPU (≈ 880 Mo vivants) et les IOSurfaces sont de la mémoire `kIOMemoryKernelUserShared`, prise dans la plage d'adresses noyau `kmem_data_shared`, qui ne recevait que la moitié de l'espace des données (1 930 Mo).

## Changement (`xnu/osfmk/vm/vm_kern.c`, `kmem_range_startup_init`, sous `#if defined(B2L)`)

Les deux tiers de l'espace des données vont à la plage partagée (≈ 2,6 Go), un tiers à la plage privée (≈ 1,3 Go, aucun signe de pression dans les journaux). Fichier modifié directement dans `xnu/` (il n'est pas dans l'overlay ; original dans le scratchpad de la session).

## À surveiller

- La ligne `kmem_data_shared_range` du début du journal (taille).
- `HakiosMali: no memory` et `would not allocate` : doivent baisser.
- Fautes GPU (`did not finish`) : si les passes du verre, qui tourneront enfin, sont fautives par elles-mêmes, elles le montreront.

## Installé

- `vm_kern.c` recompilé (journal de build).
- **BOOT :** relecture `8024100e9e8889cca217460c4e359ff98430b669e750d4173b50908130004273` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0120/boot.img`.

## Résultat (journal `out/logs/0121-boot.txt`, photos du propriétaire)

- `kmem_data_shared_range` : **2 999 Mo** (privée : 1 499 Mo).
- **0 tampon GPU refusé, 0 texture refusée, 0 faute GPU, 0 job inachevé** : les textures du verre sont toutes allouées, et ses passes tournent sans faute (les fautes de 0119 venaient bien de 0118).
- **Propriétaire : même aspect.** Tuiles du Centre de contrôle gris foncé et opaques avec un liseré clair, dock noir uni. Le flou du fond, lui, marche.
- Lecture : le verre ne voit rien derrière lui. `-[HakiosMetalCommandBuffer blitCommandEncoder]` répondait `nil` : chaque copie demandée par QuartzCore (la zone sous le verre, relue dans ce qui est déjà dessiné) disparaissait sans un mot, et le verre échantillonnait une texture restée à zéro. → B1L-0122.

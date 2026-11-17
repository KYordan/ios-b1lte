# Build B1L-0037 : réglages Mali-G76 de Samsung, tas du tiler réduit, fermeture sûre

Date : 2026-11-06. Statut : **installé : BOOT (relecture `70133773…`) ; pas encore testé.** Base : B1L-0036 (SYSTEM inchangé : `b2l-gl-v6`).

## Constat sur B1L-0036

- Panic « B2L FIRST PANIC: MPIDR=0x100 » (cœur 1), affiché pendant un « B2L CS: signature… » (un exec).
- Le journal persistant (`out/logs/0036-boot1.txt`) s'arrête vers 200 s, peu après l'injection de `libb2l-gl` dans backboardd. Pas de texte de panic.
- **Ce n'est pas propre à 0036.**
  - Dans les boots GL (0032 à 0036), l'écrivain du journal s'arrête peu après le démarrage de backboardd. Sa dernière salve couvre seulement 128 enregistrements (0036).
  - Les boots sans GL (0029 à 0031) journalisent normalement jusqu'à 315–556 s (4,4 Mo).
  - Quelque chose dans le noyau se fige quand Mesa commence à utiliser le GPU.
- Hypothèse principale : le watchdog de 120 s arrête backboardd, `clientClose` libère des buffers (`IOBufferMemoryDescriptor`) alors qu'un job GPU peut encore y écrire, et la mémoire noyau est corrompue. Cela donnerait un panic à un endroit quelconque, par exemple pendant un exec.

## Sources de référence

- **Noyau Samsung exynos9820** (LineageOS, `lineage-20`, extrait dans `~/Developer/s10-port/ref-samsung/k9820`), `drivers/gpu/arm/b_r26p0`. Pour le G76 (tNOx, GPU_ID 0x7211), kbase écrit après chaque reset :
  - `JM_CONFIG |= 0xf << 16` (taille de groupe IDVS) ;
  - `SHADER_CONFIG |= 1 << 17` (`SC_TLS_HASH_ENABLE`) ;
  - `L2_MMU_CONFIG` : champs 3 bits 12–14 et 15–17 à 0.

  Notre pilote, porté du G72, écrivait 0 dans les deux premiers registres et effaçait les bits 24–27, qui sont les champs du G72.
- **Linux panfrost** fait les mêmes réglages (`panfrost_gpu_init_quirks`).
- **hakios-metal** (oosexgang/hakios-xnu-kernel) : un pilote Metal complet, mais pour l'Adreno (A6xx, PM4 et compilateur ir3), donc pas réutilisable tel quel sur le Mali.
  - À retenir pour l'option 2 : Metal.framework trouve le GPU grâce aux propriétés `MetalPluginName` et `MetalPluginClassName` du service IOAcceleratorES, puis charge un bundle dont la classe hérite de `_MTLDevice`, voir `userland/src/HakiosMetalCA.m`.
  - Pour servir le serveur d'affichage, il faut en plus un `default.metallib` pour QuartzCore et un compilateur AIR → NIR → (ici Bifrost).

## Changements

- `xnu-overlay/bsd/dev/b2l_mali_core.c`, `gpu_reset()` : pour le 0x7211, ajout des trois réglages de kbase ci-dessus. Les autres GPU gardent l'ancien chemin. La trace de l'étape 5 affiche aussi JM_CONFIG et SHADER_CONFIG.
- `xnu-overlay/iokit/Kernel/arm/B2LMaliService.cpp` :
  - **Tas du tiler** (`PANFROST_BO_HEAP`, 128 Mo chez Mesa) : les 128 Mo d'adresses sont réservés, mais seuls 16 Mo de mémoire réelle sont alloués et mappés. Avant, 128 Mo de mémoire noyau bloquée (wired) étaient alloués en une fois. Si le tiler dépasse, la MMU du GPU signale une faute et le job est réinitialisé au bout d'une seconde.
  - **`freeBuffer()`** : si le dernier job du buffer n'est pas terminé après 3 s, la mémoire est conservée (fuite volontaire) et reste mappée. Elle n'est jamais rendue au noyau tant que le GPU peut y écrire.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-g76-0037/boot.img` | `701337730c059f569cf8d38ad975621ea275dd587d3272c4e84e519abb1cedf4` |
| Kernel | `7a649dbe4ffd224a4078f5b726624d41a36e916e561829f807cde5a7fe1fab1f` |

Flash : `bash ~/Developer/s10-port/b1l-tools/flash-boot.sh ~/Developer/s10-port/beyond2lte-ios/out/b1l-g76-0037/boot.img 701337730c059f569cf8d38ad975621ea275dd587d3272c4e84e519abb1cedf4`

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

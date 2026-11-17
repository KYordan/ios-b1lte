# Build B1L-0034 : traces de la création du contexte GLES, filet de sécurité

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs relus, copie `d7a22dad…`) et BOOT (relecture `bac89a46…`) ; pas encore testé.** Base : B1L-0033.

## Constat sur B1L-0033

- Crochet appliqué : « new_metal_context patch 0, renderer cbz patch 0 ».
- backboardd s'est figé au premier dessin (logo), juste après la lecture des propriétés IOMFB (`IOMFBBlendDisabledKey`). Il n'a pas planté (pas de rapport).
- Notre fonction n'a jamais signalé « renderer #1 », aucun message de `initWithAPI` du pont, aucun client sur `HakiosMali`. Le blocage est donc entre l'appel du crochet et l'ouverture du GPU par Mesa.
- Écran noir avec « 0x44 » en haut.

## Changements

- **Traces dans le journal noyau (`kern.hakios.say`) :**
  - `userspace/b2l_accel_gles.c` : rendu demandé, `alloc`, `initWithAPI:3`, `gles_new_context` ;
  - `gl_osmesa.c` (patché par `scripts/build-gl.sh`) : avant et après `OSMesaCreateContextAttribs` ;
  - `userspace/mesa/hakios-drm/hakios.drm.c` (copie locale de HakiOS, utilisée par `scripts/build-mesa.sh`) : `drmOpenWithType`, service, `IOServiceOpen`, les 40 premières requêtes et toutes celles en erreur.
- **Filet de sécurité :**
  - sans contexte GLES au bout de 8 s, ou en cas d'échec, le marqueur `/private/var/mobile/Library/Logs/b2l-gl-fallback` reçoit l'UUID de la session de boot, puis backboardd s'arrête (`abort`) ;
  - relancé par launchd, il voit le marqueur de ce boot et ne pose pas le crochet : retour au rendu CPU ;
  - au boot suivant, le GPU est retenté.
- Bibliothèques `b2l-gl-v4` : `libOSMesa.8.dylib` `f0491be5…`, `libb2l-gl.dylib` `9bef48d7…`.
- Outil `b1l-tools/system-add-gl.sh` : copie SYSTEM vérifiée, ajout du dossier, flash-delta.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glhook3-0034/boot.img` | `bac89a46a52c46dfaa0988ad9bfb55e5edc2111092ce58b30d0fe77a7fabf53f` |
| Kernel | `19017e5ebe659ec521aa844d3eb22b89ddc52b1350b439f6c5411b48e0190aee` |

Retour arrière : `out/b1l-thermgpu-0030/boot.img`.

# Build B1L-0043 : rendu CPU à nouveau par défaut (GLES en secours)

Date : 2026-11-06. Statut : **installé : BOOT (relecture `f478d903…`) ; SYSTEM inchangé (`b2l-gl-v9` reste disponible).** Base : B1L-0042.

## Constat sur B1L-0042 (journal `out/logs/0042-boot1.txt`)

- **Stable :** aucun plantage de backboardd sur 2880 images rendues par le GPU.
- **Framebuffer partagé :** l'écran est dessiné sur place (`0 pictures copied out, 1 drawn in place`).
- **Images simples :** 24 à 33 ms, dont environ 13 ms d'attente du GPU.
- **Images complexes :** environ 2 s (`frame 2880: 816 GL calls, 94 draws … 1957141 us`).
- **GPU occupé à 1–2 % :** la limite est le CPU (Mesa, pont EAGL, compilation des shaders).
- Le propriétaire constate un rendu plus cassé et moins fluide que le rendu CPU. Il choisit de passer à Metal (`docs/metal-plan.md`).

## Changement

`xnu-overlay/bsd/kern/b2l_exec_graphics_diag.h`, `b2l_exec_bb_inject()` : `libb2l-gl` n'est plus injectée dans backboardd par défaut (`enabled = 0`). Le boot-arg `b2l_gl=1` la réactive.

Le reste de 0040–0042 est conservé :
- correctifs pmap ;
- framebuffer mappable par le GPU ;
- `CA_BACKDROP_SCALE` désactivé.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-cpudefault-0043/boot.img` | `f478d903678d6e6d5c3a0429fadbd7ff447f4810a0ea514b9108399202ae2bcf` |
| Kernel (`kernel.macho`) | `23bbc3561168018decff18087376d94e92a002bae3e47c39a6e6b4b2e405aa81` |

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

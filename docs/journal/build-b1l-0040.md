# Build B1L-0040 : RAM des banques basses traitée comme de la DRAM, adresse de vidage non tronquée

Date : 2026-11-06. Statut : **installé : BOOT (relecture `30a5c549…`) ; pas encore testé.** Base : B1L-0039 (SYSTEM inchangé : `b2l-gl-v7`).

## Constat sur B1L-0039 (journal `out/logs/0039-boot1.txt`)

- **Mesa est bien branché :**
  `an OpenGL ES 3 context for CoreAnimation … OpenGL ES 3.1 Mesa 25.0.7; Mali-G76 (Panfrost); shaders OpenGL ES GLSL ES 3.10`.
  Extensions complètes, `GL_MAX_TEXTURE_SIZE` 16384.
- **L'IOSurface d'affichage est attachée :** `a new kind of texture: IOSurface attached … 1440 x 3040`.
- **Puis backboardd meurt :** `B2L CRASH: backboardd[62] exception=1 code=0x101 sub=0x100fec054` (`EXC_ARM_DA_ALIGN`), avec `pc=0x102832c3c`, `x19=0x100fec040`.
- **Le repli marche :** au relancement, `GLES failed earlier this boot: window server left on the software renderer`. Plus de boucle.

## Analyse

- L'instruction fautive est `stur d0, [x19, #0x14]` à `libOSMesa+0x752c3c`, dans une fonction genxml (assemblage d'un descripteur GPU). C'est une écriture de 8 octets à une adresse alignée sur 4 seulement, dans un buffer Mali mappé côté CPU. Sur de la mémoire normale, c'est permis ; sur de la mémoire device, c'est une faute d'alignement.
- **`pmap.c`, `VM_WIMG_WCOMB`** : le mapping n'est « Normal non-cacheable » que si `is_dram_addr(pa)`. `is_dram_addr` ne connaît que `gDramBase/gDramSize`, c'est-à-dire la plage principale donnée par le chargeur (`0x900000000`, plage 10). Les buffers alloués dans une autre plage possédée (ici la plage 9, `0x88ad00000–0x8fe000000`) devenaient de la mémoire « Device GRE ».
- **`pmap_sync_page_attributes_phys()`** fait `flush_dcache((vm_offset_t)(pp << PAGE_SHIFT), …)` avec un `ppnum_t` sur 32 bits. Le décalage est fait en 32 bits, donc `0x8f1e1c000` devient `0xf1e1c000`.
  - C'est l'adresse « illégale » du panic de 0037.
  - Le correctif de 0038 évitait le panic, mais vidait le cache d'une page sans rapport au lieu du buffer.

## Changements

`patches/0106-b2l-pmap-owned-ram-is-dram-and-ptoa.patch` (`osfmk/arm/pmap/pmap.c`) :
- `pmap_sync_page_attributes_phys` utilise `ptoa(pp)`, qui calcule l'adresse sur 64 bits ;
- `VM_WIMG_WCOMB` et `VM_WIMG_POSTED_REORDERED` traitent toute RAM possédée par B2L (`b2l_managed_pa`) comme de la DRAM.

Le garde-fou de 0038 dans `caches.c` reste en place pour une vraie page hors RAM.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-wcomb-0040/boot.img` | `30a5c549ce2173432584ecf479ca89d58035a4a774dd1681ada5bfe6de0dcdfc` |
| Kernel (`kernel.macho`) | `b8521f0fee23a3c0bf45b38f567865f784388f4d4c8577b14f173a5b6a3e85b4` |

Un premier essai de compilation a échoué : la macro avait été placée dans un bloc `#if HIBERNATION`. Elle a été déplacée et l'image refaite.

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

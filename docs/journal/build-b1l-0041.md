# Build B1L-0041 : la surface d'affichage GLES est suivie par le contexte (première image)

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs, copie `3d243310…`) et BOOT (relecture `169ba65e…`) ; pas encore testé.** Base : B1L-0040.

## Constat sur B1L-0040 (journal `out/logs/0040-boot1.txt`)

- **Plus de faute d'alignement.** Le pont attache l'écran au GPU :
  `attachImage: texture 1 is IOSurface 2 ('BGRA' 1440 x 3040, 5888 bytes a row): a texture, copied into its memory by the GPU`.
- **Puis backboardd s'arrête (SIGABRT)** sur une assertion de QuartzCore : `B2L FAULTMSG: … ogl-context.cpp`.
- Pile d'appels, symbolisée avec un slide de `0xb77c000` :
  - `CA::OGL::Context::push_surface + 0x250` (assertion `found != slist->end ()`, `ogl-context.cpp` 0x4a2) ;
  - `CA::OGL::GLESContext::make_buffer_current + 0x2f0` ;
  - `CA::WindowServer::Display::render_display` ;
  - `AccelServer::render_update` ;
  - `Server::render_for_time` ;
  - `IOMFBServer::vsync_callback`.

  C'est donc la toute première image rendue.
- Les plantages répétés de KonaSynthesizer, SiriAUSP, MauiAUSP et MacinTalkAUSP (synthèse vocale) n'ont rien à voir avec le GPU.

## Cause : un oubli d'Apple dans le chemin GLES d'iOS 26

- `make_buffer_current` alloue lui-même la `OGL::Surface` de l'écran (calloc de 0xc0 octets) et ne l'ajoute qu'à la liste propre au contexte GLES (`+0xb00`).
- Dans iOS 26, `Context::push_surface` exige que la surface fasse partie des surfaces suivies du contexte (listes à `+0x58` ou `+0x60`, remplies par `Context::track_surface`). Sinon, il fait un abort.
- Les autres chemins GLES appellent bien `track_surface` (`create_null_surface`, `create_surface_with_properties`), mais pas celui-ci : Apple ne l'exécute plus.

## Changements

- `userspace/b2l_accel_gles.c` :
  - les 5 instructions de `0x1840d2620` (`ldr x0,[x19,#0xb00]; mov x1,x21; bl _x_list_prepend; str x0,[x19,#0xb00]; b 0x1840d2660`) deviennent `ldr x16,#12; blr x16; b 0x1840d2660; .quad b2l_mbc_tramp` ;
  - le trampoline appelle `track_surface(ctx, surface)` (`0x1840d8c58`), puis fait le même `x_list_prepend` (`0x183f6ec78`) ;
  - les mots d'origine sont vérifiés avant l'écriture (relus dans le QuartzCore 23G90 extrait) ;
  - nouvelle trace `b2l-gl: make_buffer_current tracks its surface: 0`.
- Bibliothèques `b2l-gl-v8` : `libOSMesa.8.dylib` `36524d8c…`, `libb2l-gl.dylib` `a7e62560…`. Chemins `b2l-gl-v8` dans le noyau.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-track-0041/boot.img` | `169ba65e63c444f8096f13368b34af6108c1770bf7eaa40700a4bad92d7e1e24` |
| Kernel (`kernel.macho`) | `7c42948725943d49fbcca868d4a8216f9785f3888cca07d77d7938ffb82d8259` |

Installation : `system-add-gl.sh b2l-gl-v8`, puis `flash-boot.sh out/b1l-track-0041/boot.img 169ba65e…`.

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

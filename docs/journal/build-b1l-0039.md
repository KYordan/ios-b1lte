# Build B1L-0039 : les appels GL du pont EAGL vont enfin à Mesa

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs, copie `9b285b7f…`) et BOOT (relecture `d8bafccf…`) ; pas encore testé.** Base : B1L-0038.

## Constat sur B1L-0038 (journal `out/logs/0038-boot1.txt`, 5,3 Mo)

- **Le panic de 0037 a disparu.** Le vidage de cache passe par une fenêtre de copie (8 pages `0xf1df0000–0xf1e2c000`), appelé depuis `pmap_set_cache_attributes_internal` pendant la création des buffers Mesa.
- **Le journal persistant n'est plus figé :** il continue jusqu'au redémarrage.
- **Le contexte GLES de QuartzCore est créé :** `[210.89] b2l-gl: window server renderer #1: GLESContext … over EAGLContext …`. Le tas du tiler est bien limité à 16 Mo.
- **Puis backboardd s'arrête sur SIGABRT** (raison de sortie, espace 2, sous-code 6), juste après :
  `hakios-gl: -attachImage: target 0xde1, texture 0, an IOSurface of 'BGRA': not done`.
- launchd le relance, il plante pareil (244 s, 277 s, …). Le marqueur de repli n'était écrit que par le watchdog, qui ne se déclenche plus puisque le rendu est créé. D'où plusieurs minutes de journaux identiques, puis un redémarrage au bout d'environ 7 min.

## Cause

`eagl.c` interroge le contexte par `glGetString` et `glGetIntegerv(GL_TEXTURE_BINDING_2D)`. Au lien, ces symboles étaient résolus vers **Apple OpenGLES** (`-framework OpenGLES` était placé avant `libOSMesa` ; `dyld_info -imports` affichait `_glActiveTexture (from OpenGLES)`, etc.). Apple renvoie NULL ou 0, ce qui explique :
- les chaînes `(null)` ;
- `GL_MAX_TEXTURE_SIZE` à 0 ;
- `texture 0` au moment d'`attachImage`, donc le refus, puis l'abort de QuartzCore.

## Changements

- `scripts/build-gl.sh` :
  - `libOSMesa.8.dylib` est passé avant les frameworks au lien ;
  - le build échoue si un `gl*` est encore importé d'OpenGLES. Résultat : `_glGetString`, `_glGetIntegerv` et `_glBindTexture` viennent de `libOSMesa`.
- `userspace/b2l_accel_gles.c` : un plantage de backboardd (SIGABRT, SEGV, BUS, ILL, TRAP) après l'installation du crochet écrit le marqueur de repli du boot. Le backboardd suivant reste alors en rendu logiciel, sans boucle de plantages.
- Bibliothèques `b2l-gl-v7` (le noyau ne remplace pas un fichier existant) : `libOSMesa.8.dylib` `d794d038…`, `libb2l-gl.dylib` `53bd90ce…`. Chemins `b2l-gl-v7` dans `b2l_userspace_seed.h` et `b2l_exec_graphics_diag.h`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glink-0039/boot.img` | `d8bafccfdd0fcbfa1534530f5f8426a0d3081fd32958a29328bb5f5863cb878a` |
| Kernel (`kernel.macho`) | `84f843848ae3aa9d9444ad5b5dbd398612032d86b826dc5253aed5feea59c2ca` |

Installation : `system-add-gl.sh b2l-gl-v7`, puis `flash-boot.sh out/b1l-glink-0039/boot.img d8bafccf…`.

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

# Build B1L-0036 : délai GPU de 120 s, marqueur de repli fiable, console allégée

Date : 2026-11-06. Statut : **installé : SYSTEM (6 blocs, copie `9d7b693a…`) et BOOT (relecture `f140adba…`) ; pas encore testé.** Base : B1L-0035.

## Constat sur B1L-0035 (photo de la console)

- **Mesa parle au GPU :** `drmOpen`, une suite de `GET_PARAM` (0x44) qui réussissent, `mali: THREAD_TLS_ALLOC 0, THREAD_MAX_THREADS 0x300, SHADER_PRESENT 0x770077`, `CREATE_BO` (0x42) puis `MMAP_BO` (0x43) réussis.
- **Plusieurs séquences de `GET_PARAM`, chacune sur une nouvelle connexion, avec des `xpcproxy` entre elles :** backboardd redémarrait en boucle. Le chien de garde de 20 s l'arrêtait pendant la création du contexte Mesa, trop lente sur ce CPU, et le marqueur de repli ne prenait pas effet (`kern.bootsessionuuid` probablement vide, ou dossier non inscriptible).
- Un premier essai s'est terminé en panic, sans texte récupéré. Le journal persistant s'arrête vers 197–209 s.

## Changements

- `userspace/b2l_accel_gles.c` :
  - délai de 120 s ;
  - identifiant de boot tiré de `kern.boottime` ;
  - marqueur `/private/var/tmp/b2l-gl-fallback` ;
  - chaque trace préfixée par `[uptime]`.
- `userspace/mesa/hakios-drm/hakios.drm.c` : 6 traces, plus les erreurs.
- `B2LMaliService.cpp` : 4 lignes « request » par connexion au lieu de 32, toutes visibles sur la console.
- Bibliothèques `b2l-gl-v6` : `libOSMesa.8.dylib` `e537850f…`, `libb2l-gl.dylib` `5b02c1d3…`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glhook5-0036/boot.img` | `f140adbaabf3243c2f4c449a7ba726ba662d8956994dd69505f2b17e222cc395` |
| Kernel | `dc7da35f9eac46c87a9ca338f8e4669f3bc78f29b369be3263d79032c8e6f96f` |

Retour arrière : `out/b1l-thermgpu-0030/boot.img`.

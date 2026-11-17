# Build B1L-0031 : GPU G5 + G6, Mesa Panfrost et pont EAGL dans backboardd

Date : 2026-11-05. Statut : **installé et testé : chaîne noyau OK, mais CoreAnimation reste sur le CPU** (voir `docs/gpu/plan.md` §4c). BOOT + SYSTEM (ajout de fichiers). Base : B1L-0030. Plan : `docs/gpu/plan.md`.

## G5 : Mesa pour iOS 26

- `scripts/build-mesa.sh` (dans beyond2lte-ios), adapté de HakiOS `tools/mk-mesa.sh` :
  - Mesa 25.0.7 vérifié par SHA-256 ;
  - les 11 patches de HakiOS (`tools/mesa/*.patch`) et leur libdrm `hakios-drm`, inchangés ;
  - Xcode, SDK iPhoneOS 26.5, cible `arm64e-apple-ios26.0` ;
  - pilotes `softpipe,panfrost`, sans LLVM.
- Résultat : `libOSMesa.8.dylib`, arm64e (`0x100000c`/`0x80000002`), signé ad hoc, 15 385 824 octets, 1 296 fonctions `gl*`. Il contient Panfrost avec le Mali-G76 et le libdrm vers « HakiosMali » (IOKit).

## G6 : pont EAGL (HakiOS `userland/src/eagl.c`, licence MIT)

- **Table de dispatch :** `tools/derive-eagl-slots.py` montre que les **457 positions de la table d'OpenGLES d'iOS 26 (23G90) sont identiques** à `eagl_slots.txt` d'iOS 16.5. `eagl_gen.h` est généré par leur `mkeagl.py` : 409 entrées, 356 transmises, 30 écrites à la main, 23 sans fonction.
- **`libb2l-gl.dylib`** (`eagl.c` + `gl_osmesa.c`) : 577 648 octets, arm64e, ad hoc, lié à `/usr/local/lib/b2l-gl-v1/libOSMesa.8.dylib`. Son constructeur remplace les méthodes d'`EAGLContext` (`initWithAPI:properties:`, `getMacroContextPrivate`, `texImageIOSurface:…`, `attachImage:toCoreSurface:…`).
- **Pourquoi CoreAnimation devrait passer sur OpenGL ES sans adresse codée en dur :** sans périphérique Metal, `renderer()` prend `[EAGLContext initWithAPI:3]` puis `CA::OGL::GLESContext` (constaté par HakiOS ; nous n'exposons pas de Metal). Les adresses iOS 16.5 de leur `inject.c` ne servaient qu'à retirer un périphérique Metal ; elles ne sont pas reprises.

## Livraison et noyau

- **SYSTEM :** les deux bibliothèques vont dans `SYSTEM:/b2l-gl-v1/`, sans compression HFS. On part de l'image SYSTEM actuelle du téléphone (pour garder le cache natif appris), puis seuls les blocs modifiés sont flashés.
- **`b2l_ufs_disk.c` :** `b2l_system_file_copy()` lit un fichier de SYSTEM par le lecteur HFS déjà ouvert pour le cache natif.
- **`b2l_userspace_seed.h` :** `b2l_gl_publish()` recopie, une seule fois, vers `/usr/local/lib/b2l-gl-v1/`. Un fichier présent n'est jamais remplacé.
- **`b2l_exec_graphics_diag.h` :** `DYLD_INSERT_LIBRARIES=/usr/local/lib/b2l-gl-v1/libb2l-gl.dylib` pour `/usr/libexec/backboardd`, si le fichier existe.
- **`b2l_amfi_constraint.h` :** l'appel AMFI 90 (politique dyld) est accordé au pid de backboardd, comme à celui de SpringBoard.
- **Arrêt :** `b2l_gl` (défaut 1). Les boot-args sont compilés dans le loader du firmware, donc le vrai retour arrière est un BOOT reconstruit avec la valeur par défaut à 0, ou `out/b1l-thermgpu-0030/boot.img`, qui n'injecte rien.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-gl-0031/boot.img` (26 054 000 octets) | `b8f51a06a16d615ac44c61f6e3d67a1a8ff07ad680210568df6129aaad1d55d1` |
| Kernel | `a947c743d99a474512458475d50cd2ba80bc8da1834588bea21754345ff8d8bf` |
| `b2l-gl-v1/libOSMesa.8.dylib` | `39818693077d6dd493d4d44a44a38853551bf164b8ba763885b462a174e12aa0` |
| `b2l-gl-v1/libb2l-gl.dylib` | `b1c16e2c774c8c219a67ee982b9b417acbcdd5e022b7d77088667542635836dc` |

## Image SYSTEM préparée

- Copie du téléphone en lecture seule : `out/live/b1l-system-live.img`, 7 340 032 000 octets, SHA-256 `1c8c9c2c…` identique des deux côtés.
- Clone `out/live/b1l-system-0031.img` + `/b2l-gl-v1/` (`ditto --nohfsCompression`). Les hash des fichiers sont conformes, et `b2l-cache.bin` n'est pas touché.
- `fsck_hfs` sur la partition entière dit « Volume header needs minor repair » **aussi sur l'image d'origine** : l'en-tête de secours est cherché à la fin de la partition, alors que le volume HFS s'arrête 16 Mo avant (zone de journal). Ce n'est pas causé par l'ajout.
- Flash prévu : `b1l-tools/flash-delta.sh out/live/b1l-system-0031.img system /dev/block/sda25 7340032000`, qui n'écrit que les blocs de 4 Mio modifiés.

## Critères de réussite

- Noyau : `B2L GL: b2l-gl-v1/... -> /usr/local/lib/b2l-gl-v1/...`, puis `B2L GL: ... inserted in backboardd`, `B2L AMFI: dyld policy for pid <backboardd>`.
- Mali : `HakiosMali: a client opened the GPU`, puis les requêtes 0x44/0x42/0x40, et le rapport par minute avec des « job chains » qui augmentent.
- iOS : l'interface s'affiche par le GPU (plus fluide) ; dire si Liquid Glass apparaît.
- **En cas d'écran noir :** reflasher `out/b1l-thermgpu-0030/boot.img`.

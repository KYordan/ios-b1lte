# Build B1L-0033 : crochet GLES, tolérance à l'instruction PAC traduite

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs relus, copie `2f8a8e2e…`) et BOOT (relecture `ef43bd17…`) ; pas encore testé.** Base : B1L-0032.

## Constat sur B1L-0032

- Le pont s'est installé : « hakios-gl: 8 of 8 methods of EAGLContext are ours ».
- Le crochet a refusé : « QuartzCore is not 23G90 as expected (cbz b4000a20, gles b9400008, metal d503201f): nothing patched ».
- Cause : en mémoire, le premier mot de `new_metal_context` est `d503201f` (nop) au lieu de `d503237f` (pacibsp). C'est la traduction PAC du noyau qui réécrit `pacibsp` dans le cache partagé.

## Changement

- `userspace/b2l_accel_gles.c` accepte les deux formes de ce seul mot ; les autres vérifications restent strictes.
- Bibliothèques en `b2l-gl-v3` : `libOSMesa.8.dylib` `60e63862…`, `libb2l-gl.dylib` `4373a32e…`.
- Chemins du noyau en `b2l-gl-v3`.
- `b1l-tools/flash-boot.sh` (nouveau) : écriture de BOOT avec contrôles, tous les appels `adb` en `</dev/null`. Le flash enchaîné SYSTEM + BOOT s'était arrêté après SYSTEM, parce qu'un `adb` lisait la suite du script sur l'entrée standard.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glhook2-0033/boot.img` | `ef43bd1771564173e289c842461c3b8703807fd1fd3c47213916e9cfd3a7be85` |
| Kernel | `91533667cdf873510bcd4b73603f1ca5342b9d60277ed9df1bd6e1c2776916ba` |

Retour arrière : `out/b1l-thermgpu-0030/boot.img`.

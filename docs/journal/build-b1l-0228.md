# Build B1L-0228 : textures et samplers déclarés mais non liés acceptés

Date : 2026-11-14. Base : B1L-0227. Ensemble `b2l-metal-v99`, Mesa recompilé, noyau reconstruit (noms seulement).

## Diagnostic (journal 0227)

- 34 dessins de SpringBoard échouent : `the draw failed: fa16dhmha4 reads texture 0 and 0 textures were bound`.
- `fa16dhmha4` (le shader général de RenderBox) déclare `fill_tex`, `shape_tex`, `custom_tex`, `noise_tex` et ne lit
  que ceux que ses données choisissent. Metal laisse l'application ne pas lier les autres. `check_bindings`
  (`air_metal.c`) refusait le dessin entier.

## Changement (`apply.py`, bloc B1L-0228)

- Texture **lue** et non liée : le dessin passe ; Gallium reçoit une vue nulle, que panfrost émet comme sa texture
  nulle (`panfrost_emit_null_texture`).
- Sampler non lié : le sampler par défaut du pilote (`default_sampler`, déjà utilisé pour les emplacements vides).
- Image **écrite** non liée : toujours refusée.
- Lectures des tableaux de liaisons bornées par leur longueur.
- backboardd n'a aucun dessin refusé (0 dans le journal 0227) : son chemin ne change pas.

## Installé

- **Vérifié :** `air-b2l: unbound sampled textures and samplers bound as null/default (B1L-0228)` dans `out/build-mesa-0228.log`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `85a6ddb5db1191eda831c30ecc84e904e41d9f58f5a7e5b826dd9afa83112b0c` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat

- **Gel à l'écran verrouillé** (propriétaire). Journal `out/logs/0228-boot.txt` : SpringBoard plante en boucle (SIGSEGV,
  FAULTREG pc=0x13dec4f68). Pile symbolisée (base libhakiosair 0x13dc00000) : `panfrost_emit_const_buf+0x1f4` ←
  `panfrost_update_shader_state+0x400` ← `panfrost_draw_vbo+0xb94` ← `rwa_render_draw+0x129c`.
- **Cause :** `panfrost_upload_txs_sysval` (`pan_cmdstream.c`) lit la vue de texture sans la vérifier. Depuis 0228, une
  texture non liée a une vue NULL, et `fa16dhmha4` demande la taille de ses textures. Les descripteurs, eux, gèrent déjà
  une vue NULL.
- Suite : B1L-0229.

# B1L-0268 — optimisation : la traduction des shaders d'un pipeline sort du verrou

## Pourquoi (mesures de 0267)

- `newRenderPipelineStateWithDescriptor:` tient le verrou global du pilote (un seul `pipe_context` par processus, B1L-0259) pendant
  toute la création : jusqu'à 18,3 s d'un seul tenant dans iconservicesagent, 0,8 s dans backboardd, 0,6 s dans SpringBoard.
  Pendant ce temps, les autres fils du processus ne peuvent ni dessiner ni valider de command buffer : saccades et retours en arrière.
- Lu dans `air_metal.c` (`rwa_render_create`) : la traduction AIR → NIR (`air_to_nir`, `build_vertex_state`, `quad_vertex_shader`,
  `slot_shapes`, `count_resources`…) n'utilise que l'écran (`is_format_supported`), pas le contexte. Le frontal n'a pas de variable
  statique modifiable ; les types GLSL ont leur propre mutex (`glsl_type_cache_mutex`) ; les modules liés (B1L-0234) sont par fil
  (`__thread`). La compilation panfrost, elle, a besoin du contexte (`panfrost_create_shader_state` utilise `ctx->shaders` et `ctx->descs`).

## Changement

- Mesa (`apply.py`, section B1L-0268, sauvegarde `.before-0268`) : `rwa_set_context_lock(drop, take)`. `rwa_render_create` lâche
  le verrou juste après l'allocation du pipeline (toutes les sorties suivantes passent par `fail:` ou par `make_stage_cso`) et le
  reprend avant `make_stage_cso`, ou à `fail:` avant `rwa_render_destroy`.
- Pilote (`hakiosmetal_air.m`, sauvegarde `.before-0268`) : `b2l_mesa_drop` / `b2l_mesa_take`, branchés par `dlsym` après l'ouverture
  de Mesa. Seule une prise extérieure (profondeur 1) est lâchée : un pipeline créé sous un appelant qui tient déjà le verrou le garde.
  La reprise est comptée à part dans `B2L HOLD` (« rwa_render_create shader compile ») : on saura si le temps est dans la traduction
  ou dans la compilation panfrost.
- Set v131 → v132.

## Risques

- Si une étape de la traduction touchait le contexte malgré la lecture, ce serait une course (plantage dans Mesa). Vérifié : aucun
  appel `pipe->` ni `device->pipe` entre le lâcher et la reprise.

## Installé

- SYSTEM-OK, BOOT `a9bff31f4714ab9d3573f9c192f794fa7d6c6c0d384c865f0502e415cc1a5c19`, BOOT-OK ; `libhakiosair.dylib` v132 exporte
  `rwa_set_context_lock`, `libhakiosmetal.dylib` v132 contient « shader compile ».

## Résultat

- **Jamais chargé sur le téléphone.** Le changement de set n'a remplacé que `b2l-metal-v131`, pas le nom du plug-in `B2LMetalv131`
  (`B2LMetalAccel.cpp`, `b2l_userspace_seed.h`). Le noyau a donc continué de charger le plug-in de 0267, qui est lié à
  `/usr/local/lib/b2l-metal-v131/libhakiosmetal.dylib`, donc au pilote de 0267. Preuve : aucune ligne `HOLD … shader compile`
  dans le log 0269. Le changement 0268 est repris dans 0269 (2e flash).

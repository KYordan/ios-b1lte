# Build B1L-0225 : diagnostic de 0224 dans un nouvel ensemble (`b2l-metal-v96`)

Date : 2026-11-14. Base : B1L-0224. Noyau reconstruit (seuls les noms `b2l-metal-v96` / `B2LMetalv96` changent dans
`B2LMetalAccel.cpp`, `b2l_userspace_seed.h`, `b2l_exec_graphics_diag.h`) ; l'état noyau de 0223 (traps de cache EL0,
émulation arm64e, `b2l_wk_trace`) reste tel quel. Mesa identique à 0224.

## Pourquoi

- 0224 n'a rien changé sur le téléphone : le noyau ne remplace jamais une bibliothèque déjà publiée sur USERDATA sous
  le même ensemble (voir `build-b1l-0224.md`).

## Changement

- Aucun, en dehors de l'ensemble : le diagnostic B1L-0224 (pipelines refusés de RenderBox nommés, modules AIR envoyés
  dans le journal) entre enfin en service.

## Installé

- **Vérifié :** `B2L REFUSED` dans `build-metal/b2l-metal-v96/libhakiosair.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `dba3024cf9bfbe488154f1388339eafc138b53e8c9ccc91ff7a2e34115d5426c` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0225-boot.txt`)

- 224 refus, mais **3 pipelines distincts seulement**, redemandés sans cesse :
  | Processus | Fragment | Vertex | Refus |
  |---|---|---|---|
  | SpringBoard 75, iconservicesagent 20 | `fa16dhmha4` (66 Ko) | `uber_vertex` (52 Ko) | phi de structure |
  | SpringBoard 63, iconservicesagent 4 | `path_distance_fragment` | `vkba6pa7dhmha4` | phi de pointeurs (boucle) |
  | SpringBoard 62 | `path_exterior_fragment` | `vjba2ca3la7dhmha4` | phi de pointeurs (boucle) |
- Les 6 modules sont rebâtis sans perte : `out/b2lmod-0225/` (`scripts/extract-b2lmod.py`).

## Rejoués sur le Mac (`b2l-bicomp`, Mesa avec assertions)

- Correctif 0188 seul : les deux pipelines de tracé et `uber_vertex` passent. **`fa16dhmha4` fait échouer une assertion
  de Mesa** : `nir_inline_function_impl` reçoit une fonction qui contient encore un `return`. Sur le téléphone (sans
  assertions), ce `return` devient un saut dans le shader principal : c'est le **plantage de 0188** (`bi_emit_jump`,
  cible nulle). La garde 0189 supprimait ce saut (shader faux). Le `nir_lower_returns` de 0190 tournait après
  l'incorporation, donc le shader sortait trop tôt sans écrire sa couleur (« aucune amélioration »).
- **Bon ordre** (celui de Mesa) : `nir_lower_returns` **avant** `nir_inline_functions`. Avec lui, les 6 modules
  compilent jusqu'au code Mali. Le Mac demandait aussi un remplacement de `txs` (taille de texture), que panfrost fait
  sur le téléphone (`panfrost_nir_lower_sysvals`) ; il est ajouté dans l'outil.
- Taille et temps (Mac, Mesa en mode débogage) : `fa16dhmha4` 93 Ko de code et 8,9 s ; `uber_vertex` 75 Ko et 0,7 s ;
  shaders de tracé 4–5 Ko et 0,04 s. `fa16dhmha4` n'a aucune constante de spécialisation : il est gros par nature.
  C'est le suspect principal de la lenteur de 0190.
- Suite : 0226 = phi de pointeurs seuls (pipelines de tracé), 0227 = phi de structures + ordre des `return`.

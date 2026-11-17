# Build B1L-0234 : les fonctions liées d'un pipeline (effets personnalisés de RenderBox)

Date : 2026-11-14. Base : B1L-0233. Ensemble `b2l-metal-v105` (noyau reconstruit), Mesa et pilote recompilés.

## Changement

- **Traducteur** (`apply.py`, bloc B1L-0234 ; sauvegarde `apply.py.before-0234`) :
  - `air_to_nir_options` reçoit des modules liés ;
  - `air_to_nir` les lit avec le shader, et leurs variables globales et fonctions définies rejoignent celles du shader ;
  - un appel d'une déclaration nommée `<f>.MTL_VISIBLE_FN_REF` devient un appel de la fonction `<f>` définie dans un
    module lié. C'est la liaison statique que fait le compilateur de Metal.
  - `rwa_b2l_set_linked(étage, …)` (nouveau, dans `air_metal.c`) donne au prochain `rwa_render_create` les modules à
    lier.
- **Pilote** (`hakiosmetal_air.m` ; sauvegarde `hakiosmetal_air.m.before-0234`) : avant `render_create`, il passe le
  bitcode de `functions` et `privateFunctions` de `vertexLinkedFunctions` et `fragmentLinkedFunctions`, puis il les
  efface. Le diagnostic `B2L LINKED` de 0233 reste.
- **Outil Mac :** `b2l-bicomp --link MODULE.air`.

## Reproduit sur le Mac (avant flash)

- `b2l-bicomp f-ffgea5cia6dhmha4-795eda35.air` : refusé comme sur le téléphone (« only declares »).
- Avec `--link l-custom_fn.air` : **passe Bifrost, 608 octets de code**. Les deux autres shaders d'effet passent aussi
  (832 et 1072 octets ; testés avec le même `custom_fn`, le leur n'a pas été récupéré).
- Aucun changement pour `fa16dhmha4`, `uber_vertex`, les tracés et les vertex des effets (mêmes tailles qu'avant).

## Installé

- **Vérifié :** `air-b2l: functions a render pipeline links (B1L-0234)` dans `out/build-mesa-0234.log` ; `_rwa_b2l_set_linked` exporté par `libhakiosair.dylib` de v105, et appelé par nom dans `libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `6f52d428e474cc5ad06dc0747b837cf494c8573d4ef775cc605efd15165e4557` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire + journal `out/logs/0234-boot.txt`)

- **Pas mieux à l'écran** (propriétaire).
- Journal **propre** : plus aucun pipeline refusé, aucun job GPU en échec, aucun plantage. Les pipelines d'effet se
  créent maintenant.
- Ce que le journal montre encore : RenderBox **dessine dans des textures posées sur un tampon** (122×122 RGB10A2 en
  usage 0x7, 120×120 R8 cible de `fa16kamha4`). La copie tenue à jour de 0231 est recopiée depuis le tampon à chaque
  liaison, donc elle **écrase ce que le GPU vient d'y dessiner**.
- Suite : B1L-0235.

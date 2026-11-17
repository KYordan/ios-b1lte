# Build B1L-0245 : résolution MSAA (et bip de démarrage coupé, demande du propriétaire)

Date : 2026-11-14. Base : B1L-0244. Ensemble `b2l-metal-v116`, Mesa recompilé.

## Cause (échec 3 de 0242 : MSAA 4x + résolution donne `0xff808393` au lieu du rouge)

Le pilote dessinait bien dans la cible à 4 échantillons, mais **ne résolvait jamais** vers la `resolveTexture`
(« this driver resolves nothing »), et `rwa_texture_copy` refusait toute texture multi-échantillons.

## Changement

- `apply.py` (bloc B1L-0245) : `rwa_texture_copy` accepte une source multi-échantillons ; le blit de Gallium fait la
  résolution. Une destination multi-échantillons reste refusée.
- `hakiosmetal_air.m` : une attache couleur avec `MultisampleResolve` et une `resolveTexture` est résolue par une copie
  enregistrée à `-endEncoding`, après les dessins de la passe. Limite notée : niveau 0, tranche 0.
- Non changé : `supportsTextureSampleCount:4` (réponse d'Apple d'après le profil) ; à revoir avec les familles.

## Autre changement, demandé par le propriétaire (hors GPU)

- `B2LAudio.h` : niveau audio par défaut **5 au lieu de 6** ⇒ **plus de bip au démarrage** (étape A5). Tout le reste de
  l'audio (A0–A4) est gardé. `b2l_audio=6` en argument de démarrage remet le bip.

## Installé

- **Vérifié :** « rwa_texture_copy resolves a multisample source (B1L-0245) » dans `out/build-mesa-0245.log` ; B1L-0245 dans `libhakiosmetal.dylib` (v116).
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `f062d363ef9e44ba14108be94d756129557f1ca76a6959955cb3421a384e897d` (BOOT-OK). Pas redémarré : 0246 suit.

## Résultat

- OK : « MSAA 4x + résolution » centre `0xffff0000` (rouge). Bip de démarrage : pas joué (à confirmer à l'oreille). « MSAA 4x annoncé » reste en échec (l'annonce n'a pas été touchée, voulu).

## Retour en arrière

- `apply.py.before-0245`, `hakiosmetal_air.m.before-0245`, `B2LAudio.h.before-0245`.

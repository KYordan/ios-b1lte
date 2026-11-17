# Build B1L-0244 : un calcul qui écrit une texture écrit le bon type

Date : 2026-11-14. Base : B1L-0243. Ensemble `b2l-metal-v115`, Mesa recompilé (`--mesa`).

## Cause (échec 6 de 0242 : texel `0xffffffff` au lieu de `~0xff801133`)

`air.write_texture_*` devient `nir_image_store` (`air_intrinsics.c`), dont le `src_type` était pris du type de retour
de l'instruction. Une écriture ne renvoie rien (`void`), donc `tex_dest_type` retombait sur « uint » : un `float4`
écrit dans une texture RGBA8Unorm était converti comme un entier, chaque canal saturé à 1.

## Changement (`apply.py`, bloc B1L-0244)

- Le `src_type` vient du type de la **valeur écrite** (float, half, ou entier signé/non signé selon le nom `.s`/`.u`).
- Vérifié sur une copie de `air_intrinsics.c` d'origine : le motif est trouvé et remplacé une fois.

## Installé

- **Vérifié :** « write_texture stores with the type of the value written (B1L-0244) » dans `out/build-mesa-0244.log`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `791ecd3f02477194c2e5683ce72bf250231db5590782450829252421971c70e9` (BOOT-OK). Pas redémarré : 0245 suit.

## Résultat

- OK : « compute écrit une texture » texel `0xff801133` (attendu ~`0xff801133`). Échec 6 corrigé.

## Retour en arrière

- `apply.py.before-0244`.

# Build B1L-0094 : l'instruction invalide de `A2Xghfc` (fautes 0x51)

Date : 2026-11-08. Base : B1L-0093.

## Cause

B1L-0093 a nommé `A2Xghfc` dans chaque lot échoué de backboardd. Sur le Mac, `b2l-bicomp` (Mesa avec ses assertions), avec en plus les valeurs système de Gallium mises à zéro (outil seulement), s'arrête dans l'empaquetage des instructions. `bi_lower_swizzle` (Mesa) remplace un choix d'octets ou de moitiés qu'une instruction ne sait pas encoder par un `SWZ` placé avant. Trois cas étaient faux, et une version sans assertions emballe alors des bits sans sens (INSTR_INVALID_ENC, 0x51) :

1. instruction 8 bits avec un choix de **moitiés** (`H11`) → `SWZ.v4i8`, qui ne l'encode pas ;
2. instruction 16 bits avec un choix d'**octets** (`B0000`) → `SWZ.v2i16`, qui ne l'encode pas ;
3. `IADD`/`ISUB` `.v4s8`/`.v4u8` : aucun cas, donc le choix restait sur la 1re source, qui n'en accepte aucun.

## Changements (`apply.py`, B1L-0094 et B1L-0094b, `bi_lower_swizzle.c`)

- Le `SWZ` est choisi selon le genre du choix : octets → `SWZ.v4i8`, moitiés → `SWZ.v2i16`.
- `IADD`/`ISUB` v4 : la 1re source est toujours abaissée, la 2e garde l'identité ou un octet répliqué.

## Vérifié sur le Mac

- `A2Xghfc` : **363 856 octets de code Bifrost, 64 registres**, sans assertion.
- Les **41 modules** récupérés (0088 + 0090) compilent tous jusqu'au bout.
- `b2l-bicomp` met à zéro les valeurs système de Gallium (`load_rt_conversion_pan`, `load_viewport_*`, `load_base_*`, `load_ssbo_address`, `load_noperspective_varyings_pan`) pour vérifier l'encodage.

Ensemble `b2l-metal-v50`, plug-in `B2LMetalv50`. Mesa toujours avec ses assertions signalées.

## Installé

- **SYSTEM :** SYSTEM-OK en 76 s. **BOOT :** relecture `ff55dbbb68042b922dbb3d2b063ea11c2b7d3a796bd74aafe6dddea8f0bfca98` (BOOT-OK).
- **Pas démarré seul :** B1L-0095 est construit et flashé juste après, et le contient (les builds s'additionnent).

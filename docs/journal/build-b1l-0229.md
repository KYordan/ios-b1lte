# Build B1L-0229 : taille nulle pour une texture non liée (plantage de SpringBoard en 0228)

Date : 2026-11-14. Base : B1L-0228. Ensemble `b2l-metal-v100` (noyau reconstruit pour la publication des bibliothèques), Mesa recompilé.

## Changement (`apply.py`, bloc B1L-0229)

- Dans `panfrost_upload_txs_sysval` (`gallium/drivers/panfrost/pan_cmdstream.c`) : si la vue de la texture est NULL
  (texture non liée, cas ouvert par 0228), la taille rendue est 0, 0, 0, au lieu d'une lecture par un pointeur nul. C'est
  ce que Metal rend pour une texture absente.

## Installé

- **Vérifié :** `air-b2l: the size of an unbound texture is zero (B1L-0229)` dans `out/build-mesa-0229.log`.
- **SYSTEM :** 7 blocs sur 1750, SYSTEM-OK. **BOOT :** relecture `7b8e7cbe7c842cf39985904231f16d68a84253a15d27593ff3e70058b722db5c` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire + journal `out/logs/0229-boot.txt`)

- **Le gel de 0228 est réglé** : plus aucun plantage de SpringBoard. **Mais toujours pas d'icônes** : Horloge et
  Calendrier apparaissent une fois puis disparaissent en changeant de page (comme avant). Gels, lenteur, et l'écran
  revient en arrière quand on bouge.
- Journal : **70 jobs du GPU échouent avec le statut `0x5A`**, tous dans des chaînes qui contiennent `fa16dhmha4` (le
  shader des icônes, qui atteint le GPU pour la première fois). 12 « THE GPU DID NOT FINISH a command buffer within two
  seconds » (SpringBoard, iconservicesagent, backboardd). C'est la source des gels.
- `0x5A` = `ADDR_RANGE_FAULT` (`mali_kbase_mmu.c`, pilote Mali b_r16p0 de Samsung).
- **Cause, trouvée sur le Mac** (`B2L_NIR=1 b2l-bicomp out/b2lmod-0225/f-fa16dhmha4-de27b680.air`) : `fa16dhmha4` lit
  4 textures (0 à 3) **à travers les samplers 0 à 13**. `pan_shader.c` déclare au GPU
  `sampler_count = texture_count = 4`, donc tout sampler n° 4 à 13 sort des limites.
- Suite : B1L-0230.

## Retour en arrière

- Si 0229 échoue : retirer les blocs 0228 et 0229 de `apply.py` (état 0227), nouvel ensemble.

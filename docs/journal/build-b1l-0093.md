# Build B1L-0093 : nommer les shaders sur lesquels le Mali s'arrête

Date : 2026-11-08. Base : B1L-0092. Build de diagnostic (aucun changement de rendu attendu).

## Changements

1. **Noyau, `b2l_mali_core.c` :** les 32 dernières chaînes de jobs que le GPU n'a pas finies (numéro, adresse GPU, statut, fragment ou non), lisibles par `sysctl kern.hakios.mali_failures`.
2. **Mesa (`apply.py`, B1L-0093) :**
   - `pan_job.h` : chaque lot retient les noms de ses shaders (`b2l_label`, noms des fonctions AIR) ;
   - `pan_jm.c` : noms ajoutés à chaque dessin (vertex et fragment) et à chaque calcul ; les 512 dernières chaînes envoyées sont retenues avec leurs noms ; à chaque envoi, les nouveaux échecs du noyau sont dits dans le journal : `b2l-batch: job N (fragment chain 0x…) failed with status 0x51: NOMS` ;
   - `pan_cmdstream.c` : `+blend-shader` quand le lot utilise un blend shader.
3. **Pilote Metal (`hakiosmetal_air.m`) :** quand une texture liée à un dispatch n'a pas de texture de la bibliothèque dessous, le journal dit sa classe, son format, sa taille et son IOSurface (pour `compute_average_luma`).

Ensemble `b2l-metal-v49`, plug-in `B2LMetalv49`. Mesa toujours avec ses assertions signalées.

## Installé

- **Vérifié :** `b2l-batch` dans `libhakiosair.dylib` ; `mali_failures` (B1L-0093) dans le noyau.
- **SYSTEM :** SYSTEM-OK en 73 s.
- **BOOT :** `out/b1l-metal-0093/boot.img`, relecture `42808aacab5b8a173d8f613321d3525a89847bffc4266e89f5bb2ab76d0b035d` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0092/boot.img`.

## Résultat (boot 1, photo `b1l-0093-calculatrice.jpg`)

- Rendu comme 0092, « un peu mieux » selon le propriétaire, toujours lent ; **plus de 10 minutes à chaque redémarrage**.
- **Calculatrice :** touches d'opérations jaunes avec des taches rouges (au lieu d'orange) ; chiffres 1 à 6 et leurs touches absents (disques blancs vides ou rien).

## Journal (`out/logs/0093-boot1.txt`)

- 116 lignes `b2l-batch` : **chaque lot échoué (0x51) qui appartient à backboardd contient `A2Xghfc`**, le grand shader fragment (avec son vertex `VfxXgh`). 21 autres sont d'autres processus.
- Sur le Mac, `b2l-bicomp` compile `A2Xghfc` en entier en **33 s** (sur le Mac !) et s'arrête sur `swz0 < 8` dans `bi_pack_add_swz_v4i8` : `SWZ.v4i8 r11.h11`. Ce sont des bugs de `bi_lower_swizzle` de Mesa, corrigés en B1L-0094.
- `compute_average_luma` : 4 échecs « texture 0 », mais la ligne de diagnostic n'apparaît pas : l'index 0 est donc **vide** (`nil`) dans le travail enregistré, pas un objet étranger. À suivre.

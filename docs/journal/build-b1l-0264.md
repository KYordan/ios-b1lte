# B1L-0264 — test Metal complet dans Dictaphone, avec les shaders des icônes

## Pourquoi

- Demande du propriétaire : un test Metal complet via Dictaphone, pour diagnostiquer les shaders et le reste sur ce build.
- 0263 : pictogrammes en rayures horizontales ; les masques R8 de SpringBoard sont faux sur la ligne du bas des blocs 2×2.
  Les shaders RenderBox utilisent `dfdx`/`dfdy`/`fwidth`, parfois dans un `switch` ; SpringBoard écrit R8 + RG16F + BGRA8 dans une même passe.

## Changement

- `userspace/metal/b2l_metal_suite.m` (sauvegarde `.before-0264`) : nouveau groupe `testIconShaders`, après le rendu :
  - « dérivées dfdx/dfdy/fwidth » : quad 64×64, valeurs attendues exactes ;
  - « dérivées dans un if » : la même chose dans une branche sur une valeur uniforme (comme `primitive_shape`) ;
  - « cible R8 (masque) », « cible RG16F », « cible BGRA8 » : un dégradé de la position du pixel écrit dans les trois cibles en une passe.
  - Chaque ligne compte les pixels faux selon la parité de la ligne et de la colonne : une faute « bloc 2×2 » se voit telle quelle.
- `b2l_exec_graphics_diag.h` : injection de l'app Test Metal dans Dictaphone **réactivée par défaut** (boot-arg `b2l_metal_app=0` pour la couper).
- Set v127 → v128. Mesa inchangé (0263 garde les tampons persistants).

## Installé

- SYSTEM-OK, BOOT `7ec1cdadac94e9572dde8ac02495f16a1d1e8609ee1e315bb5f554d12ac90e0b`, BOOT-OK.

## Résultat (log noyau `out/logs/0264-boot.txt`, lignes `b2l-mtest:`)

- **35 OK, 3 ÉCHEC, 1 SAUTÉ** en 6,5 s ; CAMetalLayer 10,9 images/s. `newLibraryWithSource` 5,2 s.
- ÉCHEC : MSAA 4x annoncé (connu) ; **cible R8 (masque)** 4032 faux et **cible RG16F** 4096 faux : dans la passe R8 + RG16F + BGRA8
  (celle de SpringBoard), seule la cible BGRA8 semblait écrite. **Corrigé par 0265 : fausse conclusion**, les cibles sont écrites mais la position est décalée d'un demi-pixel (tolérance du test BGRA8 plus large).
- OK : dérivées dfdx/dfdy/fwidth, et dans un `if` : 0 pixel faux. Les dérivées sont hors de cause.
- Lien avec les icônes : `primitive_image_fragment` / `primitive_custom_fragment` de RenderBox rendent `<{ half4 color, half2 coverage }>`
  et **relisent** la cible 1 (`air.render_target` en entrée : `coverage` accumulée en RG16F). Si RG16F n'est jamais écrite, la couverture
  des icônes est fausse dans SpringBoard (iconservicesagent, juste, utilise c0/c2 en RGBA16F).
- Lu dans le code : Bifrost complète bien une sortie à 1/2 composantes (`bi_src_color_vec4`), l'état de mélange est commun à toutes
  les cibles. Cause pas encore isolée → tests de 0265.


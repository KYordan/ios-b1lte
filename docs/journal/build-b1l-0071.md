# Build B1L-0071 : M5 étape 2, shaders de QuartzCore dessinant dans l'IOSurface

Date : 2026-11-07. Base : B1L-0070 (M5 étape 1 réussie).

## Référence prise sur le Mac

- `default.metallib` de QuartzCore est extrait de l'IPSW 23G90 (`SystemOS.dmg`), puis chargé par le Metal du Mac (Intel UHD 617, macOS 26), qui l'accepte.
- **Réflexion :**
  - `std_vert0_lpf` : position à l'attribut 0, couleur à l'attribut 3 (le `texcoord0` de l'attribut 1 n'est pas utilisé), `mvp_matrix` à `[[buffer(2)]]`, `texmat` à 3 (inutilisé), flux de sommets à l'index 5 ;
  - `attachment_clear_frag_lpf` n'a aucune entrée et écrit 0 (noir transparent) ;
  - `fixed_frag_lph_cpf`, le fragment général de QuartzCore, a une soixantaine de *function constants* (pour M6).
- **Image de référence :**
  - même passe, 64×64 BGRA8 effacé en vert, puis deux triangles, deux draws dans la même passe ;
  - **2005 pixels dessinés**, le nombre de pixels de chaque ligne étant intégré au test.
- Outils : `scratchpad/qcinfo.m` et `scratchpad/qcdraw.m` (temporaires).

## Test (`b2l_metal_test.m`, `m5_draw_test`)

- Après l'étape 1, sur la même texture IOSurface :
  - bibliothèque QuartzCore chargée sur le téléphone ;
  - pipeline `std_vert0_lpf` + `attachment_clear_frag_lpf` ;
  - même passe et mêmes sommets que sur le Mac.
- Le test compte les pixels dessinés, verts et autres, et compare chaque ligne avec le Mac. Il affiche les lignes qui diffèrent, le temps du commit et le statut.
- **Il vérifie aussi la correction de 0070 :** si le second draw réeffaçait la passe, seul le second triangle resterait.
- Ensemble `b2l-metal-v27` (`libb2l-metal-test` `e557c075…`), et le noyau pointe vers v27.

## Installé

- SYSTEM : 5 blocs réécrits, en 69 s.
- BOOT : `out/b1l-metal-0071/boot.img`, relecture `97cc2009fd96ffaa580b7c5e3d45bb73050eae8f199ba391fadb78c265787aa2` (BOOT-OK).

## Résultat (`out/logs/0071-boot1.txt`) : M5 étape 2 réussie

- `std_vert0_lpf found, attachment_clear_frag_lpf found`, puis `pipeline made` (0,6 s de compilation AIR → Bifrost pour la paire).
- `two draws in one pass, status 4, 20955 us`.
- `2005 drawn (Mac: 2005), 2091 green, 0 other` ; **`0 of 64 rows differ from the Mac's picture`**.
- `QuartzCore's shaders drew into the IOSurface exactly as the Mac's GPU did: M5 step 2 done`.
- **Ce que cela valide :**
  - le vertex shader de QuartzCore, avec sa matrice et son flux de sommets ;
  - la couverture, identique pixel près à un vrai GPU ;
  - deux draws dans une passe, sans réeffacement (correctif 0070) ;
  - l'écriture dans une IOSurface relue par le CPU.
- **À améliorer plus tard :** 21 ms pour deux triangles, car chaque draw attend sa propre fence (`finish_or_say_why` dans `rwa_render_draw`). Pour l'interface, il faudra une seule attente par command buffer.

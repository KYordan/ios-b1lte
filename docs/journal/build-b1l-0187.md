# Build B1L-0187 : le pilote journalise aussi SpringBoard, chronod et iconservicesagent

Date : 2026-11-12. Base : B1L-0186. Ensemble `b2l-metal-v91` (Mesa inchangé). Diagnostic, plus un retour prudent.

## Diagnostic (journal `out/logs/0186-boot.txt`)

- Les processus qui ont maintenant un GPU dessinent (82 surfaces `RGhA`), mais icônes, glyphes et widgets manquent toujours.
- `rw_print` n'envoyait au journal du noyau que les lignes de backboardd : aucune trace des refus du pilote dans les autres processus.

## Changement (`hakiosmetal_air.m`)

1. `rw_print` envoie aussi au journal les 600 premières lignes de chaque autre processus (backboardd garde 3000).
2. `newBufferWithBytesNoCopy:` redonne `nil` à backboardd, comme avant 0186 : QuartzCore s'en accommode, et une copie pourrait y devenir périmée. Les autres processus gardent la copie.

## Installé

- **Vérifié :** `answered with a copy` dans `build-metal/b2l-metal-v91/libhakiosmetal.dylib`.
- **SYSTEM** et **BOOT** : voir `flash-0187-system.log` et `flash-0187-boot.log` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0187-boot.txt`, aucun plantage)

**Cause trouvée : notre traducteur de shaders (AIR → NIR) refuse des shaders de RenderBox**, le moteur de dessin de SwiftUI. SpringBoard et iconservicesagent l'utilisent pour les icônes, les glyphes et les widgets.

- Refus dans SpringBoard (8) et iconservicesagent (5), répétés des centaines de fois :
  - `this render pipeline is refused: instruction: a phi of pointers whose operand comes in over a loop edge, so which resource it is depends on the iteration` ;
  - `this render pipeline is refused: instruction: a phi of a struct, which NIR has no register for`.
- Reproduit sur le Mac avec la bibliothèque de RenderBox, copiée depuis `ipsw-23G90/OS.dmg` dans `out/rb-check/default.metallib` (82 fonctions) et `archive.metallib` :
  ```bash
  ~/Developer/s10-port/build-mesa/build-host/src/compiler/air/air2nir --report out/rb-check/default.metallib
  ```
  - `uber_vertex` échoue sur le phi de pointeurs (bloc 4, instruction 54) ;
  - `uber_fragment` utilise des tables de fonctions visibles (`air.get_function_pointer_visible_function_table`) ;
  - les autres échecs du rapport (`function constant of 4 components`, `render_target input without a target`) viennent du rapport hors ligne, qui n'a pas les valeurs de spécialisation. Ils ne sont pas dans le journal du téléphone.
- Le reste marche : les passes de RenderBox qui compilent dessinent bien (`B2L PASS: fhea6ca7dhmha4 c0 120x120 f115 c1 120x120 f65 c2 120x120 f115`), vers 3 attachements (RGBA16Float, format 65, RGBA16Float).
- Autres refus, une fois chacun : `a texture over a buffer was asked for` (`newTextureWithDescriptor:offset:bytesPerRow:` sur un tampon répond `nil`).
- chronod reçoit un GPU mais ne dessine rien (4 lignes, aucune passe).

## Suite

1. Dans `src/compiler/air` (traducteur) : accepter un phi de structure (le décomposer champ par champ) et un phi de pointeurs sur une arête de boucle. Tester hors du téléphone avec `air2nir` sur `out/rb-check/default.metallib` (`uber_vertex`, puis les fonctions que le journal refuse).
2. Ensuite : texture posée sur un tampon, puis chronod.

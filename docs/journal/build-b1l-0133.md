# Build B1L-0133 : quelles images arrivent au GPU

Date : 2026-11-09. Base : B1L-0132. Ensemble `b2l-metal-v82` (Mesa inchangé). Diagnostic seul.

## Pourquoi

- 0131 a fait apparaître une partie des glyphes du Centre de contrôle, mais 0132 n'a rien changé.
- Manquent toujours : chiffres de l'Horloge, icône Calendrier, glyphes des boutons ronds, contenu des widgets.
- On ne sait pas par quelle image ces dessins arrivent au GPU : le noyau ne journalise que les 64 premières surfaces créées.

## Changement

- Noyau (`B2LSurface.cpp`) : en plus des 64 premières, toute surface qui n'est pas en `BGRA` (masques, glyphes, YCbCr) est journalisée, jusqu'à 600 par démarrage.
- Pilote (`hakiosmetal_air.m`) : chaque forme d'IOSurface transformée en texture est dite une fois, avec numéro, taille, format, octets par ligne, plan et résultat (`B2L IMPORT: …`). 256 formes au plus.

## Installé

- **Vérifié :** `B2L IMPORT` dans `libhakiosmetal.dylib` (v82) ; `B2LSurface.cpp` recopié dans `xnu/` avant la compilation.
- **SYSTEM :** SYSTEM-OK en 59 s. **BOOT :** relecture `081de5fbf68f3515870991f55e03bdfb4f770eacb0d9363b730a542a8edcd55c` (BOOT-OK). Redémarré vers le système par `adb reboot`.

## Résultat (journal `out/logs/0133-boot.txt`)

- **Propriétaire :** rien de changé dans le Centre de contrôle, mais les icônes du menu d'extinction s'affichent maintenant. C'est la surface `LA08` 298×286 d'InCallService, copiée depuis 0132.
- **backboardd n'importe que 3 IOSurface :**
  - le fond d'écran (1440×3040, BGRA, 5888 et 5760 octets par ligne) ;
  - la surface de luminosité (168×124, RG8).

  Les icônes et les glyphes arrivent donc par d'autres chemins.
- **`chronod` (widgets et contrôles WidgetKit du Centre de contrôle)** appelle des sélecteurs IOSurface que le noyau ne gère pas : 11, 20, 21 et 54. Il recrée 15 fois ses paires de surfaces YCbCr (`xf20` 128×96, `xf44` 88×88).
- **A8 :** fausse piste, panfrost gère bien `A8_UNORM` (`pan_format.c`, `000R`).

Suite : B1L-0134.

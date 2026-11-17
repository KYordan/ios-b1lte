# Build B1L-0129 : chaque attachement de couleur a sa propre action de chargement

Date : 2026-11-08. Base : B1L-0128 (« ce build marche très bien »). Ensemble `b2l-metal-v78`, Mesa recompilé.

## Diagnostic (journal `out/logs/0128-boot.txt` et photos 0128 du propriétaire)

- **Le haut « blanc » du fond d'écran n'est pas un bogue.** La photo du propriétaire (`fond-proprietaire.jpg`) a vraiment un ciel crème texturé façon papier au-dessus des montagnes, bordé de jaune. Le téléphone l'affiche tel quel : mêmes couleurs, mêmes taches, même bord. Rien à corriger.
- **Ce qui manque, ce sont les dessins vectoriels :** glyphes du Centre de contrôle (lune de Concentration, boutons ronds, Wi-Fi), aiguilles d'Horloge et chiffres de Calendrier, contenu des widgets. Le texte dessiné par le CPU (étiquettes, « Focus », « Not Playing ») s'affiche bien.
- QuartzCore dessine les formes vectorielles sur le GPU en une passe à plusieurs attachements :
  - `path_cubic`, `path_rect` et `path_stroke` cumulent la couverture dans un attachement à part ;
  - `path_blit_frag` la relit (framebuffer fetch, `[[color(2)]]` : il lit la cible 2 et écrit les cibles 0, 1 et 2) pour dessiner la forme (`out/qc-check/report-0084.txt`).
- **Défaut du pilote :**
  - `-[HakiosMetalRenderCommandEncoder initWithHakiosCommandBuffer:descriptor:]` ne lisait que l'action de chargement de **l'attachement 0** (`if (i == 0 && loadAction == Clear)`) ;
  - `rwa_render_draw` effaçait ensuite **tous** les attachements avec cette seule couleur (`PIPE_CLEAR_COLOR`) ;
  - une passe de tracé garde l'image (attachement 0 : Load) et efface sa couverture (Clear), donc la couverture n'était jamais effacée ;
  - à l'inverse, une passe qui efface l'attachement 0 effaçait aussi les attachements à garder.
- Autres constats, non traités dans ce build :
  - 11 refus d'une texture RGBA16Float de 1472×3072 (36 Mo) : `no memory` avec 370 Mo libres et 800 Mo de tampons GPU vivants ;
  - au moins 128 imports refusés de surfaces `LA08` de 240×240 venant de Réglages et de Spotlight. Leurs lignes de 480 octets ne sont pas alignées sur 64, et le noyau n'arrondit que pour backboardd et SpringBoard.

## Changement

- `hakiosmetal_air.m` :
  - l'encodeur note, pour chaque attachement, s'il s'efface (`_clearMask`) et avec quelle couleur (`_clearColors[8][4]`) ;
  - l'élément de travail les transporte ;
  - `struct rwa_draw` gagne `b2l_clear_colors` et `b2l_clear_mask`, ajoutés en fin de structure.
- `apply.py` (B1L-0129) :
  - `b2l_clear_colors()` efface chaque attachement seul, avec sa propre couleur (`PIPE_CLEAR_COLOR0 << i`) ;
  - elle sert dans `rwa_render_draw` (premier dessin de la passe) et dans `rwa_render_clear` (passe sans dessin) ;
  - profondeur et stencil sont inchangés.
- La trace `B2L ATT` dit aussi `load` (action de chargement) et `format`.

## Installé

- **Vérifié :** patch Mesa appliqué (`air-b2l: each colour attachment its own load action and clear colour`), `b2l_clear_colors` dans libhakiosair (v78), noyau nommant `b2l-metal-v78`.
- **SYSTEM :** SYSTEM-OK en 67 s (7 blocs sur 1750). **BOOT :** relecture `54892b2e9d10691d4ef7b8d712cf7cf56499522905c3fe76f3e7e33ccced62f9` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128` (ou le dossier `flash-0128/`).

## Résultat

- **Propriétaire : « pareil qu'en 128 »**, icônes du Centre de contrôle et widgets toujours absents.
- Journal `out/logs/0129-boot.txt` : aucun plantage, aucun dessin refusé. Suite : inventaire des shaders dessinés (B1L-0130).

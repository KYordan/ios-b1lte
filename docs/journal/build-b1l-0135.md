# Build B1L-0135 : où arrivent les glyphes, les icônes et le contenu des widgets

Date : 2026-11-09. Base : B1L-0134. Ensemble `b2l-metal-v84` (Mesa inchangé, noyau inchangé). Diagnostic seul.

## Diagnostic

- **Journal 0134 récupéré** (`out/logs/0134-boot.txt`, 1,45 Mo) : aucun plantage, aucun dessin refusé.
- backboardd importe 7 surfaces, toutes grandes :
  - 2 et 3 en 1440×3040 BGRA (fond d'écran et destination) ;
  - 18 en 1440×156 BGRA (bande de 156 lignes, le dock) ;
  - 12 en 168×124 RG8 (luminosité) ;
  - 27 en 1152×72 RG8 (InCallService) ;
  - 15 en 240×240 RG8 (Preferences) et 25 en 298×286 RG8 (InCallService), copiées car leurs lignes ne sont pas alignées sur 64 octets.
- **Aucune IOSurface de glyphe, d'icône de Calendrier/Horloge ni de widget.** Les seuls autres dessins sont 8 copies texture→texture de 8×8 sur une 776×24 RGBA16Float.
- Les deux chemins par lesquels ce contenu pourrait encore arriver ne sont pas instrumentés :
  1. `-replaceRegion:…` dans une texture ordinaire (téléversement par le processeur) ;
  2. une passe de rendu qui dessine le masque ou la vignette par le GPU.
- 0130 n'avait montré aucune cible à un seul canal : les masques sont donc plus probablement téléversés que dessinés, mais il faut le prouver.

## Changement (`hakiosmetal_air.m`, diagnostic seul)

1. **`B2L TEX`** : chaque forme distincte de texture ordinaire demandée (format, taille, type, niveaux, tranches, échantillons, usage), 160 formes au plus, puis compteur au 1er, 2e, 4e… appel.
2. **`B2L PUT`** : chaque téléversement distinct (`replaceRegion:`), avec niveau, tranche, région, taille et format de la texture, **ligne reçue et ligne minimale attendue**, et le pourcentage d'octets non nuls de la source. Un contenu vide et une image lue à la mauvaise largeur de ligne se ressemblent à l'écran et diffèrent ici.
3. **`B2L PASS`** : au premier dessin de chaque couple (fonction fragment, forme des cibles, formes des textures échantillonnées), une ligne qui nomme le shader et dit quel format et quelle taille il lit (`c0 … t0 …`). C'est ce qui reliera le glyphe hachuré à une texture précise.
4. **`B2L IMPORT`** : compteur des imports demandés et refusés (les lignes par forme ne disent pas si un import a eu lieu).

## Installé

- **Vérifié :** `B2L TEX`, `B2L PUT`, `B2L PASS`, `B2L IMPORT: %u asked` dans `build-metal/b2l-metal-v84/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK en 73 s (7 blocs sur 1750). **BOOT :** relecture `00d97925aceb0cf0299942df00f68c2896501b4f16aec09e3a476d4cff71059e` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0135-boot.txt`, 999 Ko, sans plantage)

- **Les textures de glyphes et d'icônes arrivent bien aux shaders.** 23 formes distinctes en RGBA8 (format 70) sont téléversées — de 20×20 à 204×204 — et **chacune est ensuite échantillonnée par une passe** (`TimgA2Xhf_Ired`, `TcimA2Xhf_Ired`, `TmuaA2Xhf_Ired_Isqr`, `TimgA2Xhf_Isrc`), toujours vers les cibles 1440×3040 `f80` + `f115`. 59 formes en A8 (format 1) également. Ce n'est donc pas le chemin des textures ordinaires qui manque.
- **Trois imports IOSurface seulement** : surface 3 et surface 2 (1440×3040 BGRA), surface 14 (168×124 LA08), 0 refus. Aucun import de glyphe ni de widget.
- **Surfaces d'instantané d'icône** : `MobileCal`, `MobileTimer`, `Spotlight` et `Preferences` créent chacun une surface `240x240 LA08` à 480 octets par ligne. En 0134, celle de Preferences était importée puis copiée **immédiatement après sa création** ; en 0135, aucune ne l'est dans la fenêtre capturée (le journal se termine au geste de retour à l'accueil).
- **Premiers téléversements vides** : 798×466 BGRA, 417×186 A8, 102×146 RGBA8, 908×51 A8, 444×51 BGRA à 0 % non nul. `B2L PUT` n'imprime qu'une fois par forme : impossible de dire depuis ce journal si un second téléversement les remplit.
- **Conclusion** : la copie de 0132 est prise une seule fois, au moment de l'import. Pour un instantané d'icône vivant (Horloge, Calendrier), l'application dessine **après** cet import : la copie reste vide, la plaque de verre reste seule à l'écran. C'est l'objet de 0136.


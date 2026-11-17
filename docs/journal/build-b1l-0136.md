# Build B1L-0136 : l'instantané d'icône vivant est relu avant chaque dessin

Date : 2026-11-09. Base : B1L-0135. Ensemble `b2l-metal-v85` (Mesa inchangé ; noyau inchangé, seule l'étiquette de l'ensemble change). Un changement de comportement, avec son diagnostic.

## Diagnostic (journal `out/logs/0135-boot.txt`)

- Les textures de glyphes et d'icônes **arrivent aux shaders** : 23 formes RGBA8 de 20×20 à 204×204, toutes téléversées puis échantillonnées vers les cibles plein écran. Ce n'est pas le chemin des textures ordinaires qui manque.
- `MobileCal` et `MobileTimer` créent chacun une surface `240x240 LA08` à 480 octets par ligne : c'est leur **instantané d'icône vivant** (chiffre du Calendrier, aiguilles de l'Horloge).
- En 0134, l'instantané de Preferences était importé et copié **juste après sa création** (`B2L IMPORT … refused`, puis `surface with 480-byte rows copied into a texture`).
- Or la copie de 0132 est prise **une seule fois** ; le commentaire du code le disait déjà : « later CPU writes to the surface are not seen ». L'application dessine son instantané après cet import : la copie reste vide.
- Cela explique l'observation du propriétaire : l'icône se voit pendant l'ouverture de l'application (l'animation utilise un instantané frais), puis la plaque de verre reste seule sur l'accueil (la copie périmée est transparente).
- 0135 n'a importé que 3 surfaces et aucune `240x240` dans la fenêtre capturée : le journal se termine au geste de retour à l'accueil. La mesure manquante (contenu de la copie au moment de l'import, puis après le dessin de l'application) est ajoutée ici.

## Changement (`hakiosmetal_air.m`)

1. Une copie d'IOSurface à lignes non alignées (0132) est désormais **marquée vivante** : elle retient sa surface (`hakiosSetLiveCopy`) et une vue sur cette copie hérite de la source (`hakiosInheritLiveCopyFrom:`).
2. **`hakiosRefreshLiveCopy`** recopie la surface dans la texture, au rythme de la surface, **à chaque fois que le moteur de rendu lie la texture** (`setFragmentTexture:atIndex:`), donc après que l'application a dessiné et avant l'envoi du tampon de commandes. Verrou `IOSurfaceLock` en lecture pendant la copie.
3. Diagnostic joint : `B2L LIVE` dit, à l'import, le pourcentage de luminance et d'alpha déjà dessiné (`L x% A y%`), puis aux rafraîchissements 1 à 3, 64 et 1024. Si l'import est vide et le rafraîchissement plein, la cause est prouvée.

## Installé

- **Vérifié :** `B2L LIVE: surface`, `brought up to date before each draw`, `refreshed %u times` dans `build-metal/b2l-metal-v85/libhakiosmetal.dylib`. Ensemble `b2l-metal-v85` dans `xnu-overlay/bsd/kern/b2l_userspace_seed.h`, `b2l_exec_graphics_diag.h` et `B2LMetalAccel.cpp` (recopié dans `xnu/`).
- **SYSTEM :** SYSTEM-OK en 61 s (8 blocs sur 1750). **BOOT :** relecture `e7da0d4410df7850d94c921c5ebed9813112998d7deb05126ff8d18267b5725c` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0136-boot.txt`, 1,61 Mo, 580 enregistrements)

- **Aucune amélioration visuelle** : l'icône Horloge garde ses aiguilles à l'ouverture de l'app, puis redevient une plaque grise sur l'accueil ; Calendrier reste vide.
- **La correction 0136 n'a jamais été sollicitée :** aucune ligne `B2L LIVE: surface` ni `B2L LIVE: copy of surface` ; aucun `copied into a texture`. Les seules lignes `B2L LIVE:` sont l'ancien compteur PAC (collision de nom).
- Aucun import de surface `240x240` : seuls 4 imports (surfaces 2 et 3 en 1440×3040 BGRA). `MobileTimer id 16`, `MobileCal id 26/30`, `Spotlight id 14` créent bien des surfaces `240x240 LA08` (480 octets/ligne) mais aucune n'est jamais importée ni échantillonnée.
- Aucun `B2L PUT` de 240×240 (les seuls PUT format 30 sont 23×23 et 96×96). L'instantané d'icône vivant n'arrive donc ni par IOSurface, ni par upload texture.
- **Diagnostic décisif :** la piste « copie périmée » n'est pas la cause actuelle — le chemin de copie n'est jamais exécuté car la surface `240x240` n'est jamais consommée par backboardd.
- Suite : il faut identifier qui consomme la surface `240x240 LA08` (IconServices, SpringBoard, cache d'icônes) et pourquoi backboardd ne la voit pas — build 0137 (instrumentation des accès IOSurface avec ID de surface).

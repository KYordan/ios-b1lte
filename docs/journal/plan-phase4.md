# Plan des correctifs après B1L-0003 (phase 4)

État au 2026-11-04, observé par le propriétaire sur le S10 :

| Élément | État |
| --- | --- |
| Écran de verrouillage, tactile, clavier | **Fonctionnent** |
| Réglages | **Plante** à l'ouverture |
| Centre de contrôle | **Ne s'affiche pas** |
| Fond d'écran | Noir |
| Horloge | Rouge |
| Fluidité | Faible : rendu CPU en 1440×3040, @3x |

Règle du projet : **une seule modification par build**, avec un journal `docs/journal/build-b1l-XXXX.md`.
Les correctifs côté **kernel ou loader** passent par BOOT (~26 Mo, quelques secondes).
Les correctifs côté **iOS** touchent la racine (USERDATA). `[HYPOTHÈSE]` = à prouver par les journaux.

## Constats de l'étape 0 (2026-11-04, journaux `out/logs/0003.logarchive`)

- **Réglages n'a pas de bug : il est tué par le chien de garde.** Les 3 rapports `Preferences-*.ips` donnent
  `0x8BADF00D`, « scene-create watchdog transgression … exhausted real (wall clock) time allowance of 20.00 seconds ».
  Au moment du kill : 167 s de CPU, dont 113 s en système, ce qui correspond à l'émulation arm64e du code pas encore traduit.
  Le thread principal est dans SwiftUI (`SidebarRow_iOS`, `ScaledMetric`).
  Correction : `BKNoWatchdogs = YES` dans `com.apple.springboard`, comme sur le S7 (`sweet-root-tweaks.py`, `herolte-tune-ios-root.sh`),
  puis le cache natif apprend le code de Réglages au fil des démarrages.
- Autres plantages : services vocaux (KonaSynthesizer, SiriAUSP, MacinTalkAUSP, MauiAUSP), MomentsUIService et BlastDoor.
  Ils sont sans effet visible, mais candidats au nettoyage des démons (performances).
- **Taille d'écran non standard** : 1440×3040 en @3x donne 480×1013,33 points. Les fonds du système ne prévoient que
  393×852 … **440×956** (D94) et « any ». Le S7 avait ce problème (WallpaperKit filtre par `logicalScreenClass`).

## Ordre révisé

1. **B1L-0004 (racine)** : `BKNoWatchdogs`. Un seul fichier de préférences, préparé sur la copie à jour de USERDATA.
2. **B1L-0005 (BOOT)** : écran logique **880×1912 en @2x = 440×956 points**, la taille réelle de l'iPhone 16 Pro Max,
   au lieu de 960×2026. On a 2,6× moins de pixels qu'aujourd'hui, la classe d'écran D94 et un étirement de 3 % vers le panneau.
   Il faut aussi forcer la régénération du cache MobileGestalt : la sauvegarde unique de `b2l_profile_cache_refresh`
   existe déjà, donc il faut une nouvelle génération.
3. **B1L-0006 (racine)** : Réduire la transparence (centre de contrôle, horloge).
4. Fond d'écran : à réévaluer après B1L-0005 (classe d'écran correcte).

## Étape 0 : journaux et base de travail (aucune écriture sur le téléphone)

Sans ces données, les étapes 3 et 4 ne seraient que des suppositions.

1. Depuis TWRP, exporter les journaux unifiés iOS (tracev3) et les rapports de plantage `.ips` :
   `B2L_BOARD=beyond1lte B2L_SERIAL=<SERIAL> scripts/pull-b2l-unified-logs.py --adb <adb> --include-crash-reports --output out/logs/0003.logarchive`.
   On cherchera : le plantage de Preferences (exception, backtrace), les erreurs CoreAnimation, CoreImage et CoreGraphics
   (`invalid image colorspace: NULL`, `invalid context`), et PosterBoard.
2. Relire USERDATA (32 Gio) vers le Mac, pour obtenir `ios26-userdata-live.img`.
   **Indispensable avant toute modification de la racine** : le cache natif a réécrit des pages traduites
   dans les fichiers iOS du téléphone. Réécrire l'image d'origine effacerait ces traductions
   et ramènerait un démarrage froid de 40 minutes et plus.
   Les changements de racine se font ensuite sur cette copie, puis `flash-chunked.sh` ne réécrit que les morceaux modifiés.

## Étape 1 : B1L-0004, résolution 2/3 en @2x (BOOT seulement)

Le plus gros gain de fluidité, et indépendant des autres problèmes.

- Aujourd'hui : framebuffer de 1440×3040 et `display-scale = 3`, soit 480×1013 points (`uefi/XnuLoader/DeviceTree.c`).
- Cible : **960×2026 pixels en @2x**. On garde **les mêmes 480×1013 points**, donc les mêmes mises en page,
  avec **2,25× moins de pixels** à dessiner pour le CPU.
- Le kernel agrandit l'image ×1,5 vers les 1440×3040 de l'écran. C'est la méthode du S7 (`sweetfb_scale`,
  « logical = 2/3 panel, x1.5 », patches 0222 et 0236 de `ios-herolte`).
- À modifier :
  - `B2LFramebuffer.cpp` : taille logique annoncée à iOS, plus une copie mise à l'échelle (CPU, puis GPU au palier 6.3) ;
  - `DeviceTree.c` : `display-scale = 2` et `artwork-scale-factor = 2` ;
  - régénérer le cache MobileGestalt, via le rafraîchissement déjà présent (`B2L PROFILE: MobileGestalt refresh`).
    Le S7 a dû effacer ce cache une fois. `[VERIFY]`
- Risques : des ressources @2x absentes pour le profil D94 (repli @3x mis à l'échelle), et un coût CPU pour la copie ×1,5.
- Critère : écran de verrouillage et clavier nets, sans décalage du tactile. Mesurer le temps jusqu'à l'écran de verrouillage
  et l'utilisation CPU au repos, avant et après.
- Retour arrière : BOOT 0003.

## Étape 2 : B1L-0005, réduire la transparence (racine, très petit changement)

`[HYPOTHÈSE]` : le centre de contrôle et l'horloge rouge viennent des matériaux d'iOS 26 (Liquid Glass, vibrancy :
flou, matrice de couleur, couches de fond) que le rendu CPU ne sait pas appliquer. C'est le même symptôme que le
« voile rouge » de vibrancy sur le S7.

- Test le moins cher : activer *Réduire la transparence* dans les préférences d'accessibilité de la racine.
  Les surfaces deviennent opaques et l'horloge devrait passer en blanc uni.
- Si le centre de contrôle s'affiche : la cause est confirmée, et la vraie correction (filtres, puis GPU) passe en phase 6.
- Si rien ne change : la cause est ailleurs, et les journaux de l'étape 0 tranchent.

## Étape 3 : B1L-0006, plantage de Réglages

Cause inconnue : ce sera le rapport `.ips` de l'étape 0.
- Sur le S10+, Réglages s'ouvre depuis 0099. On a les mêmes patches kernel (état 0100), donc le plantage peut venir
  d'une différence du S10, ou d'un panneau lié au matériel (batterie, écran, Wi-Fi).
- Pistes à vérifier dans l'ordre : exception dans Preferences.app ou un de ses bundles, service absent (XPC),
  ou propriété MobileGestalt incohérente.

## Étape 4 : fond d'écran

`[HYPOTHÈSE]` : PosterBoard n'a aucun fournisseur de poster. Ce sont des extensions installées comme des apps,
et l'installation d'apps est bloquée sur HFS (erreur 166, « 50 candidates, 0 installée » sur le S10+).
- Court terme : image fixe dessinée depuis SpringBoard, comme le S7 (`gpu/eagl/src/herosb.c`).
- Long terme : compatibilité de staging sur HFS (`docs/hfs-apfs-installation.md` de l'auteur), qui débloque aussi les apps.

## Ensuite (plan général)

- Performances : déplacer `BTAudioHALPlugin` (sur le S7, le CPU au repos passe de >300 % à <25 %), démons qui bouclent,
  traduction arm64e statique de la racine (voir la discussion du 2026-11-04).
- Phase 5 : Wi-Fi (Broadcom PCIe). Phase 6 : GPU Mali-G76 (`docs/reference-herolte-gpu.md`).
- Veille et limite thermique : toujours absentes, ne pas laisser l'appareil allumé sans surveillance.

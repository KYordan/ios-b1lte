# B1L-0249 — extensions d'apps : réglage ExtensionKit + base LaunchServices reconstruite (écrit USERDATA, accord du propriétaire)

## Cause (voir `extensions-userdata.md`)

USERDATA relue après 0248 : aucune des 296 extensions des apps (WorldClockWidget, CalendarWidgetExtension, WeatherWidget…)
n'est dans la base LaunchServices ; pas de `/private/var/preferences/FeatureFlags/Domain/`. fairplayd.H2 est relancé
15 fois par démarrage (log 0247). Piste : icônes Horloge / Calendrier / Centre de contrôle (non prouvé).

## Changement (noyau, `b2l_apps_layout.h` `b2l_ext_reset`, appelé après `b2l_cc_layout_reset` dans `b2l_userspace_seed.h`)

Une seule fois avant launchd (marqueur `/private/var/db/b2l-ext-reset-v1`, posé seulement si tout a réussi) :
- écrit `/private/var/preferences/FeatureFlags/Domain/ExtensionKit.plist` = `enforce_xpc_cache_code_signing` `Enabled` false ;
- `disabled.plist` de launchd renommé `.b2l-before-0249` puis réécrit : mêmes 3 clés + `com.apple.fairplayd.H2` true ;
- les 2 bases LaunchServices renommées `.b2l-before-0249` (lsd reconstruit).
Rien n'est effacé. Interrupteur : boot-arg `b2l_ext_reset=0`. Lignes de log `B2L EXT:`.
Source : ios-pipa (github.com/nishant713/ios-pipa, licence MIT), NOTES.md boot 50, `tools/pipa-root-tweaks.py`
`extensions()`, `tools/root-files/ls-rebuild.txt` ; code réécrit, pas copié. Inclut 0248 (GPU pour tous, set v118).

## Risques

- Le premier démarrage peut rester longtemps sur la pomme pendant la reconstruction.
- Retour : renommer les `.b2l-before-0249` à leur place (TWRP), ou retirer ExtensionKit.plist.

## Installé

- SYSTEM-OK, BOOT `ea11bae01ba66d9de352d6b00737adf007fa2bb7f5daead9596d087c390b2bf5`, BOOT-OK (noyau contient « B2L EXT »).

## Résultat

- **Pire** (propriétaire) : toutes les icônes d'apps deviennent des carrés transparents, et ouvrir une app gèle tout.
- Log `out/logs/0249-boot.txt` :
  - les 2 bases LaunchServices sont renommées, disabled.plist (+fairplayd.H2) est écrit ;
  - **ExtensionKit flag error=1 (EPERM)** : `/private/var/preferences/FeatureFlags` existe déjà, à mobile (501) en 0775,
    et `b2l_apps_directory` refuse un dossier qui n'appartient pas à root. Le réglage n'est donc pas écrit et le marqueur
    n'est pas posé (au prochain démarrage, les bases déjà renommées ne seront pas renommées une seconde fois) ;
  - GPU : backboardd, SpringBoard, iconservicesagent (relancé 3 fois), WidgetRenderer_Default et cameracaptured ont un
    appareil ; 0 job en échec. Aucune app ouverte n'a demandé d'appareil : le gel ne vient pas du GPU des apps ;
  - mémoire GPU au pic : 1355 Mo d'adresses, dont backboardd 673 Mo (150 Mo jetables).
- Hypothèse (non prouvée) : base reconstruite → cache d'icônes à refaire → toutes les icônes repassent par le rendu GPU
  d'iconservicesagent (le chemin qui rate déjà Horloge/Calendrier) ; et le lancement d'une app attend son icône.

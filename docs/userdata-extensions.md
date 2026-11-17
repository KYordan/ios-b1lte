# Extensions d'apps non enregistrées (2026-11-14, USERDATA relue après 0248)

Lecture seule : `out/live/ios26-userdata-gpu-248.img` (pull-delta, 974 blocs, 970 s).

## Constat

- `/Applications/*.app` contient 296 `.appex` (ex. `MobileTimer.app/PlugIns/WorldClockWidget.appex`,
  `MobileCal.app/PlugIns/CalendarWidgetExtension.appex`).
- La base LaunchServices (`…/InternalDaemon/*/Library/Caches/com.apple.LaunchServices-20971545-v2.csstore`, 14 nov 22:52)
  n'en nomme **aucune** : WorldClockWidget 0, CalendarWidgetExtension 0, WeatherWidget 0. Les apps elles-mêmes y sont
  (com.apple.mobiletimer 15, com.apple.mobilecal 12). Les extensions de `/System/Library/ExtensionKit` y sont.
- `/private/var/preferences/FeatureFlags/Domain/` n'existe pas.

## Référence : ios-pipa (github.com/nishant713/ios-pipa, licence MIT)

- Même symptôme (« LaunchServices registers apps but no app extensions »), réglé à leur boot 50 (NOTES.md) :
  `tools/pipa-root-tweaks.py` `extensions()` écrit `/private/var/preferences/FeatureFlags/Domain/ExtensionKit.plist`
  = `{enforce_xpc_cache_code_signing: {Enabled: false}}` ; plus la base LaunchServices effacée une fois
  (`tools/root-files/ls-rebuild.txt`) pour que lsd réenregistre tout ; et fairplayd.H2 / bluetoothd coupés
  (un démon qui plante en boucle bloquait le réenregistrement, boots 47/48).
- La chaîne `enforce_xpc_cache_code_signing` existe dans notre cache iOS 26 (`dyld_shared_cache_arm64e.01`).
- Non vérifié : que ce réglage suffise sur iOS 26, et que ce soit la cause des icônes Horloge/Calendrier/Centre de contrôle.

## Proposition (écrit USERDATA : accord du propriétaire nécessaire)

Le noyau, une seule fois avant launchd (comme `b2l_apps_layout.h`) : écrire ExtensionKit.plist, renommer les deux
bases LaunchServices en `*.b2l-before-0249` (rien n'est effacé), interrupteur boot-arg.

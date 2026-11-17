# Nouveau USERDATA propre (B1L-0250, préparé le 2026-11-15)

Demande du propriétaire : refaire un USERDATA plus propre, en suivant ios-pipa. Le nôtre date du 3 novembre et a
accumulé des réglages posés après coup, dont certains ont échoué (ExtensionKit en 0249).

## Ce qui change dans la préparation

`beyond2lte-ios/scripts/prepare-ios-root.py` pose maintenant, **avant le premier démarrage**, des réglages repris
d'ios-pipa. Source : github.com/nishant713/ios-pipa, `tools/pipa-root-tweaks.py`, commit `830ec1c`, licence MIT
(Copyright (c) 2026 Leo Malik, dipa-studio).

| Réglage | Pourquoi |
|---|---|
| ExtensionKit `enforce_xpc_cache_code_signing` désactivé (`/private/var/preferences/FeatureFlags/Domain/ExtensionKit.plist`) | sinon iOS refuse toutes les extensions d'apps (widgets, fonds d'écran) ; dossiers au nom de mobile, comme iOS les crée |
| `disabled.plist` de launchd : `com.apple.fairplayd.H2` et `com.apple.bluetoothd` désactivés | ils redémarrent sans cesse et bloquaient le scan des apps de LaunchServices chez ios-pipa ; **retirer bluetoothd quand le Bluetooth reprend** |
| `launchd.plist` : `_PanicOnCrash` renommé `_PanicOnCrasX` | un démon qui plante ne fait plus paniquer le téléphone (ici une panique ressemble à un gel total) |
| SpringBoard : `BKNoWatchdogs`, exception de chien de garde pour purplebuddy | une app lente n'est plus tuée |
| `ByHost/com.apple.migration.<UUID>.plist` (`LastSystemVersion`) | sinon la migration de données peut se relancer à chaque démarrage ; UUID fixe de `B2LPlatformExpert.cpp` (B1L-0161) |

Déjà présents avant : Setup sauté, migration (fichier simple), conteneurs, journaux, liens des cryptex.

**Laissés de côté volontairement :**
- les réglages CoreAnimation d'ios-pipa (deep framebuffer, compression) : c'est à la session Metal de décider ;
- le retrait de CommCenter et de l'enregistreur MediaToolbox : non observés comme problème ici ;
- le déplacement des apps vers `/Applications` : le noyau le fait déjà avant launchd (`b2l_apps_layout.h`, B1L-0024/0025).

Testé sur une fausse racine : `--dry-run`, application, puis `--verify`. **Pas encore testé sur une vraie image.**

`b1l-tools/build-ios-root.sh` ne refuse plus de tourner quand une image existe. Il la renomme
`ios26-userdata.hfs.img.before-<date>`, sans rien supprimer.

## Étapes

1. **Le propriétaire lance** (sudo, que l'assistant n'a pas) :
   ```bash
   sudo bash ~/Developer/s10-port/b1l-tools/build-ios-root.sh
   ```
   Durée : 30 à 40 minutes environ. Résultat : `beyond2lte-ios/out/ipsw/ios26-userdata.hfs.img`, 32 Gio. Il y a 206 Go
   libres.
2. L'assistant vérifie l'image (`--verify`, `fsck_hfs`, hash), puis demande l'accord du propriétaire pour écrire USERDATA
   (`flash-delta.sh`). La session Metal doit libérer le téléphone.
3. Premier démarrage : long (bases LaunchServices, icônes). Il faut retaper le mot de passe Wi-Fi.

## Ce qui reste

- **Gardé :** nos bibliothèques GPU (recopiées depuis SYSTEM par le noyau au démarrage) et le cache natif (sur SYSTEM).
- **Perdu :** le mot de passe Wi-Fi et les données des apps.

## Traduction PAC (ajoutée le 2026-11-15)

Notre ancienne racine avait reçu après coup la traduction PAC, avec `install-b1l-0011-root.sh` et `0012`. Sans elle,
chaque instruction PAC du système piège, et le téléphone serait extrêmement lent. `build-ios-root.sh` la refait
maintenant lui-même (étape 5b) :
- **Cache partagé.** `static-work/out-0250` a été régénéré le 2026-11-15 depuis `static-work/orig`, avec le même
  `nc-static` que 0012 (`apply` puis les deux `nop-call` MediaServices de 0011). Résultat : **41 502 070 mots**, comme
  `apply3.txt` de 0012. `compare` contre l'original sur les 84 fichiers donne 0 changement inattendu, sauf les deux NOP
  voulus dans `.15` (`static-work/compare-0250.txt`).

  Attention : `static-work/out` est une traduction plus ancienne (4 nov. 13:39, sans PACGA v2, sans îlots de stubs,
  sans les NOP). Ce n'est **pas** la version du téléphone, contrairement à ce que disait `menage-disque.md`.
- **Écriture dans l'image.** Elle se fait sur place : même taille, inode et propriétaire. Le script vérifie d'abord que
  chaque fichier de l'image est bien le cache d'origine.
- **Programmes hors cache.** `nc-static macho-apply` traduit les programmes hors cache, comme en 0011, et aussi les
  50 apps de `/private/var/staged_system_apps`, que le noyau déplace ensuite vers `/Applications` (comme en 0018).
  Un contrôle final vérifie qu'il ne reste aucun mot à convertir.

Revu et sans suite :
- `set-root-prefs.sh springboard-watchdogs` est déjà couvert par `prepare-ios-root.py` ;
- `reduce-transparency` n'a jamais été appliqué selon les docs ;
- les copies d'apps de 0013 ont été annulées par 0018, et le noyau fait le reste (0024/0025).

**Ce qui revient tout seul :** la dictée coupée (0164). Le noyau la patche avant launchd à chaque démarrage, et sa garde
accepte le cache d'origine comme le cache traduit.

## À coordonner

Le code noyau de 0249 (`b2l_apps_layout.h`, session Metal) réécrit une fois `disabled.plist` et renomme les bases
LaunchServices. Sur une racine neuve, il remplacerait notre `disabled.plist` (bluetoothd en moins) : à retirer ou à
désactiver avant le premier démarrage.

**Réglé (session Metal, B1L-0250, BOOT `39be1437…`) :** `b2l_ext_reset` est coupé par défaut et ne revient qu'avec le
boot-arg `b2l_ext_reset=1`. Le GPU reste ouvert à tous (0248, ensemble `b2l-metal-v118`). La session Metal ne veut pas les
réglages CoreAnimation d'ios-pipa pour l'instant. Elle n'écrit plus dans USERDATA, et le téléphone est libre, en TWRP.

## Écrit sur le téléphone (2026-11-15)

- Image : `beyond2lte-ios/out/ipsw/ios26-userdata.hfs.img`, sha256 `1354515aa4495693f773986dfd90aaa86e4663f070e48a85a009a580fbeace42`.
  `fsck_hfs` OK ; cache : 84 fichiers traduits ; hors cache : 0 erreur, il ne reste aucun mot à convertir.
- `flash-delta.sh` vers `userdata` (`/dev/block/sda34`) : 6 578 blocs sur 8 192 écrits en 241 passes, chacun relu sans
  erreur (journal `out/live/flash-userdata-250.log`). Redémarrage vers le système lancé ensuite.
- Pas de sauvegarde de l'ancien USERDATA (choix du propriétaire). La dernière copie reste
  `out/live/ios26-userdata-gpu-248.img` (14 nov.).
- Le premier démarrage n'a pas encore été observé.

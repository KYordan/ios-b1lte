# Build B1L-0027 : modules du Control Center rendus chargeables, fond bleu au premier plan

Date : 2026-11-05. Statut : **installé et testé : Control Center avec contrôles et fond bleu fonctionnent** (2026-11-05). BOOT seulement. Base : B1L-0026.

## Constat sur B1L-0026

Le noyau a tout exécuté :
- `B2L CC: layout reset renamed=2` ;
- `B2L SB: dylib publication error=0`, bibliothèque insérée dans SpringBoard (pid 28) ;
- `B2L AMFI: dyld policy for pid 28: 0x23`.

Ce que montre ensuite le téléphone :
- **La bibliothèque s'est chargée.** Elle a posé la vue bleue dans `_SBWallpaperSecureWindow` (`PBUIPosterWallpaperRemoteViewController`, 480×1013, niveau 1035). Mais elle l'a mise à l'index 0, sous la vue distante du poster, qui est opaque et noire : rien de visible.
- **Le Control Center a recréé sa disposition par défaut**, puis l'a vidée aussitôt (journal de SpringBoard) :
  - `Remove invalid module com.apple.mediaremote.controlcenter.nowplaying for module not loadable` ;
  - même chose pour ConnectivityModule, MuteModule, etc.
  - Tous les modules exigent `Required Device Capabilities: arm64e`, et MobileGestalt ne répond pas `arm64e` (cache : `CPUArchitecture = arm64`). Aucun module n'est donc « chargeable ».

## Changements (dylib v2, nouveau chemin car le noyau ne remplace jamais un fichier existant)

1. `userspace/b2l-springboard.m`, qui devient `/usr/local/lib/b2l-springboard-v2.dylib` :
   - `-[CCSModuleMetadata requiredDeviceCapabilities]` est remplacé par une version qui retire `arm64e`, installée au chargement et à chaque image ajoutée. Le CPU émulé exécute bien le code arm64e ;
   - dans les fenêtres « Wallpaper », la vue bleue est posée **par-dessus** et ramenée au premier plan à chaque tour ;
   - dans `SBHomeScreenWindow`, elle va tout au fond, comme libherosb (herolte).
2. `b2l_apps_layout.h` : seconde remise à zéro de la disposition du Control Center (marqueur `b2l-cc-reset-v2`, suffixe `.b2l-before-0027`).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-ccbg2-0027/boot.img` (26 011 680 octets) | `40be11f58266dbae9eb4399f9f37e2212932e943daae4469f5e54b64c39da6da` |
| Kernel | `2bfff8aaf57269e29898b4044bff23f54465a79e952a4ea881366816f392d501` |
| `b2l-springboard-v2.dylib` (88 768 octets) | `2d88d9517d63cfccc4327f40d831db537af150c15b06aa1c5873f89dae19256e` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-ccbg-0026/boot.img`. Kill switch : `b2l_sb_inject=0`.

À faire ensuite : faire répondre `arm64e` par MobileGestalt lui-même (la vraie cause), ce qui servira aussi aux widgets et aux apps.

# B1L-0273 — Audio : iOS reconnaît le modèle (acoustic-id 8017)

## Pourquoi

En 0272, iOS ne démarre jamais la lecture : VirtualAudio dit « Product with Acoustic ID '0' is not handled » et n'a pas de table de routage (voir `build-b1l-0272.md`, Résultat).

## Changement

`xnu/iokit/Kernel/arm/B2LPlatformExpert.cpp` :
- crée `/product` s'il est absent, puis `/product/audio` dans le plan IODeviceTree ;
- y met `acoustic-id = 8017`, la valeur de DeviceTree.d94ap ;
- journal : `B2L AUDIO: device tree /product/audio acoustic-id=8017 set (nodes made N)` ;
- coupure : `b2l_audio_adt=0`.

Le reste est inchangé : plug-in v2 avec trace d'IO, `B2LAudio.h`, et le set Metal v133 (`--no-libs`).

## Risques

- Avec un modèle reconnu, VirtualAudio charge les réglages D94 (`AID8017`) : chaînes DSP du haut-parleur, routage, micros. Il peut leur manquer autre chose (syscfg, protection des HP).
- MobileGestalt voit maintenant un `/product` avec seulement `audio`.

## Installé

- Contient aussi le correctif Bluetooth de la session Bluetooth : dylib v2 insérée dans bluetoothd, plateforme forcée à 0x16 (`b2l_bt_platform=0` pour couper).
- SYSTEM-OK, BOOT `36b91e4d96d87243a7e4f8396b602e1a4c000198cf06687d0e3e5962c34662ab`, BOOT-OK. Noyau non strippé : `out/mach-0273.unstripped`.
- Téléphone redémarré vers le système.

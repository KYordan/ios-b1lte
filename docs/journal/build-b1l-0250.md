# B1L-0250 — code 0249 coupé par défaut (noyau seul, rien d'écrit dans USERDATA)

## Pourquoi

- 0249 a rendu toutes les icônes transparentes (base LaunchServices reconstruite sans le réglage ExtensionKit, EPERM).
- Le propriétaire refait un USERDATA propre selon ios-pipa (`scripts/prepare-ios-root.py`, `docs/userdata-propre.md`,
  session « Kernel compilation setup pour S10 »). Sur cette racine neuve, `b2l_ext_reset` aurait réécrit `disabled.plist`
  en retirant bluetoothd.

## Changement (`b2l_apps_layout.h`)

`b2l_ext_reset` : `on = 0` par défaut ; le boot-arg `b2l_ext_reset=1` le remet. Le reste (GPU pour tous, set v118) est inchangé.

## Installé

- SYSTEM-OK, BOOT `39be14375eb6a55e408cd415a4a75e54934839a6587ef7033e9845eaf9d3fb61`, BOOT-OK. Téléphone laissé en TWRP pour le USERDATA propre.

## Résultat

- À voir sur le nouveau USERDATA : aucune ligne `B2L EXT:` de renommage attendue.

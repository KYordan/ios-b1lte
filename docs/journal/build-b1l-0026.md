# Build B1L-0026 : Control Center remis à zéro, fond bleu provisoire dans SpringBoard

Date : 2026-11-05. Statut : **installé dans BOOT (sda14), relecture `7a28ed16…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0025.

## Constat sur B1L-0025 (apps présentes)

- `B2L LAYOUT: apps moved=50 in_place=50/50`. Le marqueur avait échoué (`error=22`) : `O_NOFOLLOW` combiné à `O_NOFOLLOW_ANY` donne EINVAL dans `vn_open`.
- **Le Control Center s'ouvre, mais vide.**
  - Ses modules se chargent ; seuls 3 sont ignorés, dont 2 « unsupported » et 1 « disallowed ».
  - En revanche, `/private/var/mobile/Library/ControlCenter/ControlsIconState.plist` contient une liste de contrôles **vide** (`iconLists = [[]]`). Elle a été écrite avant que les apps et l'extension de contrôles existent, avec `deviceDependentModuleMigrated = 1`.
- **Le fond d'écran est noir.** PosterBoard ne peut pas le dessiner sur le rendu CPU (même constat sur le S7 dans le dépôt herolte).

## Changements

1. **Control Center** (`b2l_apps_layout.h`, `b2l_cc_layout_reset`) :
   - une seule fois, `ControlsIconState.plist` et `ControlsMigrationSettings.plist` sont renommés en `.b2l-before-0026`, pour que ControlCenterUI recrée sa disposition par défaut ;
   - marqueur `/private/var/db/b2l-cc-reset-v1` ;
   - kill switch `b2l_cc_reset=0`.
2. **Fond provisoire bleu**, comme libherosb dans wardevsa/ios-a5xelte (herolte) :
   - `userspace/b2l-springboard.m`, compilée en `b2l-springboard-v1.dylib` (arm64e, signée ad hoc) par `scripts/build-springboard-dylib.py`. Elle pose une vue bleue au fond de chaque fenêtre dont la classe contient « Wallpaper ».
   - Elle note les fenêtres trouvées dans `/private/var/mobile/Library/Logs/b2l-springboard.log`.
   - Le noyau publie la bibliothèque dans `/usr/local/lib/` (`b2l_userspace_seed.h`).
   - Il ajoute `DYLD_INSERT_LIBRARIES` au lancement de SpringBoard (`b2l_exec_graphics_diag.h`).
   - La politique « AMFI » répond à l'appel 90 de dyld pour ce seul PID (`b2l_amfi_constraint.h`).
   - Kill switch `b2l_sb_inject=0`.
3. Correction du marqueur des apps : `O_NOFOLLOW` retiré.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-ccbg-0026/boot.img` (26 011 680 octets) | `7a28ed1617890b24c7e5497881082faf3861f274513651ce34a13e91af927bf9` |
| Kernel | `c578bb5ff0728438efd5f9c84a17e9b572578ad03e0b4cb9a884c5a344433903` |
| `b2l-springboard-v1.dylib` (87 664 octets) | `33e32e59521c1a55bf82936e239cbb6e219691bf5b7e422ae682fbb03d5a5906` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-s7apps-0025/boot.img`.

Si SpringBoard ne démarre plus, le boot-arg `b2l_sb_inject=0` coupe l'injection.

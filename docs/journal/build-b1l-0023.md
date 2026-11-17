# Build B1L-0023 : tactile qui se relance, leurre APFS pour le helper d'installation, lanceur v4

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `cf9cf6be…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0022.

## Constat sur B1L-0022

- **Tactile.** `B2L INPUT: sec_ts disabled after 3 transfer failures`. Les 3 échecs ont eu lieu pendant le démarrage des cœurs secondaires : le transfert était fini (`done=1`), mais il a été vérifié après le délai de 20 ms, le fil ayant été préempté. Les boots 0004 à 0020 avaient aussi ces échecs isolés, jamais 3 de suite. Ce n'est pas lié au décodeur decmpfs.
- **Apps.** La tentative 3/3 du lanceur a démarré, puis a été coupée par le redémarrage. Le reçu est épuisé (`attempts=3`).
  Au boot 0021, l'erreur était encore MIInstallerErrorDomain 166 « got hfs », dans la chaîne IXErrorDomain 2 « Failed to create promise staging root directory ».
  La chaîne « Expected install path … APFS » n'existe que dans `MobileInstallation.framework/XPCServices/com.apple.MobileInstallationHelperService.xpc`. Ce service XPC est un process distinct, absent de la liste du leurre.

## Changements

1. `xnu-overlay/iokit/Kernel/arm/B2LInput.cpp` :
   - un transfert terminé est revérifié une dernière fois après le délai ;
   - après 3 échecs, le tactile est mis en pause et réinitialisé 1 s plus tard (jusqu'à 4 essais à chaque fois), au lieu d'être coupé pour tout le boot.
2. `xnu-overlay/bsd/vfs/b2l_fstype.h` :
   - nom lu sur 32 octets (`p_name`) ;
   - « apfs » est aussi annoncé à `com.apple.MobileInstallationHelperService` ;
   - installd et installcoordinationd sont maintenant comparés exactement.
3. Lanceur relancé sur des chemins neufs, pour 3 nouveaux essais :
   - exécutable et job en `-v4` ;
   - cache launchd `b2l-launchd-bootstrap-v5.plist` ;
   - reçu `b2l-system-app-bootstrap-v3.plist` ;
   - journal `/private/var/log/b2l-system-app-bootstrap-v4.log`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-touchapps-0023/boot.img` (25 913 008 octets) | `cf9cf6bef0bc07bff17e2002c775fa81db3c2aaa5356ffba4112d0da786751d8` |
| Kernel | `53225037c34711492a7ccd19ee0289cb8deb5fd8525a4a81aeb335891709b039` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-decmpfs-0022/boot.img`.

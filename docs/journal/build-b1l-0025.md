# Build B1L-0025 : apps déplacées comme sur le S7 (herolte), adresses MAC synthétiques

Date : 2026-11-05. Statut : **installé et testé : les apps sont présentes** (2026-11-05). Control Center sans rendu. BOOT seulement. Base : B1L-0024.

## Constat sur B1L-0024

- Noyau : `B2L LAYOUT: apps moved=0 in_place=0/50 placeholders_parked=0 failed=0`.
- `/private/var/staged_system_apps` est **vide** sur le téléphone : le migrateur d'iOS l'a vidé lors d'une tentative précédente.
- Les 50 placeholders ont été mis de côté au premier boot de 0024, et les deux bases LaunchServices renommées en `.b2l-before-0024`.
- Les 50 copies de B1L-0013 (déjà traduites) restent dans `/private/var/b2l-parked-0018/Applications`.

## Référence

wardevsa/ios-a5xelte, branche `herolte` (Galaxy S7, iOS 16.7.16) :

- `tools/a5xelte-stage-ios-root.sh` (`STAGE_FULL=1`) : `mv staged_system_apps/X.app /Applications/X.app` et `rm AppPlaceholders/X.app`.
- `HEROLTE_PROGRESS.md` et `tools/mkadt_herolte.py` : sans `mac-address-wifi0`, `mac-address-bluetooth0` et `mac-address-ethernet0` dans `/chosen`, MobileGestalt ne peut pas calculer l'UDID. lsd ne finit alors jamais de remplir la base des apps. Chez nous, « Could not get wifi address » apparaît 755 fois au boot 0023, et l'arbre matériel n'a aucune adresse MAC.

## Changements

1. `xnu-overlay/bsd/kern/b2l_apps_layout.h` :
   - si une app manque dans `staged_system_apps`, elle est prise dans `/private/var/b2l-parked-0018/Applications/X.app` ;
   - les bases LaunchServices sont renommées en `.b2l-before-0025` ;
   - le marqueur n'est toujours écrit qu'avec 50 apps sur 50.
2. `xnu-overlay/iokit/Kernel/arm/B2LPlatformExpert.cpp` :
   - ajoute dans `/chosen` des adresses `02:42:32:4c:10:01/02/03` (wifi0, bluetooth0, ethernet0), localement administrées, jamais celles du téléphone ;
   - kill switch `b2l_synth_mac=0`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-s7apps-0025/boot.img` (25 913 192 octets) | `23b381d518e6440e8a230a23de095e08e84f0db2f4b6a7770b8b4a700159ff94` |
| Kernel | `323474b7307a986d65dd6665f9303ba1e623058c1449a205cdc5f5aa4ff2bcee` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-applayout-0024/boot.img`.

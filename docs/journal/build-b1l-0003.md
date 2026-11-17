# Build B1L-0003 : tactile (firmware UEFI)

Date : 2026-11-04. Une seule modification par rapport à B1L-0002 : **le firmware UEFI**.
Kernel (`362e15c7…`), loader, SYSTEM et USERDATA sont inchangés.

## Cause du tactile absent en 0001/0002

Journal de 0002 : `B2L TOUCH EFI: version=1 locate=0x800000000000000e` (EFI_NOT_FOUND).
Le loader (`uefi/XnuLoader/TouchHandoff.c`) cherche `gB2LTouchDiagnosticProtocolGuid`
`{0x47c2c225,0x58af,0x47aa,…}`. Ce protocole et l'alimentation des rails `tsp_io` / `tsp_avdd`
n'existent que dans la branche tactile de l'UEFI de l'auteur, pas sur `master`. Sans le firmware
pour allumer la puce, le kernel n'obtient jamais de réponse (`cmd=0x22`, 4 timeouts), car il
n'initialise jamais lui-même l'alimentation du tactile.

Le commit cité par l'auteur (`48add5bb…`, `docs/build-0053/0054.md`) n'est plus dans le dépôt public.
Commit retenu : **`42412d0`** (« drain large sec_ts event backlogs ») de `codex/touch-from-8549bfa`.
- `B2LTouchDiagnostic.h` y est identique à l'ABI du loader (aucune différence hors commentaires depuis `72a0db8`).
- Par rapport à `master`, la carte mémoire et le reste de l'overlay ne changent pas ; seuls le tactile,
  les applications de test et la déclaration du GUID dans le `.dec` diffèrent.

## Construction

- Overlay : worktree `~/Developer/s10-port/uefi-touch-42412d0`, appliqué sur Mu-Silicium `a8ca6f0d`
  (arbre `mu-work-b1l-touch`), puis `make-board-platform.py` (corrigé : les références
  `beyond1ltePkg/beyond2lte.dec` deviennent `beyond1lte.dec`).
- `B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 scripts/build-boot-firmware.sh` active `B2LTouchDxe`
  (DXE.inc) et la liaison `HsI2cLib` (S5E9820Pkg.dsc.inc), comme `yap.sh --touch`.
- FD de 4 Mio, rempli à 24 % ; `B2LTouchDxe.efi` de 48 640 octets.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-touch-0003/boot.img` (25 893 504 octets) | `0fbcf3ba8f8426ba3ea8815356d481627b5923c6ca1b49d82632e64d2d6cf3d9` |
| Firmware | `52d8e75c3369b94c74cac36e9137ee65c252921c3a07fbe84c6f441d637599b6` |
| Kernel (inchangé) | `362e15c7b6594316cc57ac61dd621d70b8d7c62d2a8c7dad5ead26f5ae38e727` |

Vérifié : `package-boot.py --verify` OK, DTB du S10 ajouté, GUID du protocole identique côté loader et côté firmware.

## Attendu au démarrage

`B2L TOUCH EFI: … locate=0x0 init=0x0 … dvdd=0x0 avdd=0x0 …` (succès), puis
`B2L TOUCH INIT: stage=ready ready=1`, et `B2L INPUT PERF … enabled=1`.
Retour arrière : réécrire `out/b1l-ios-0002/boot.img` (`fbf6b520…`).

## Installation et résultat

- 2026-11-04 : gardes TWRP OK (root, SM-G973F, `sda14`, 57 671 680 octets) ; image poussée, hash contrôlé ;
  `dd conv=fsync` ; relecture de 25 893 504 octets = `0fbcf3ba…`, **identique**. Seul BOOT a été écrit.
- **Vérifié (propriétaire)** : le tactile fonctionne, ainsi que le clavier et l'écran de verrouillage.
  Réglages plante et le centre de contrôle ne s'affiche pas. Suite : `docs/plan-phase4.md`.

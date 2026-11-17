# B1L-0215 : menu de démarrage dans le chargeur UEFI (test)

**Le noyau est celui de 0213, à l'identique.** Seul le firmware UEFI change : son chargeur XnuLoader affiche maintenant
un menu avant XNU. Ce build sert à tester le menu.

## Contenu

| Élément | SHA-256 | Origine |
|---|---|---|
| boot.img | `c58655ab…14b2c` | `beyond2lte-ios/out/b1l-menu-0215/` |
| Noyau | `3b937e6c…89526` | extrait du boot.img de 0213, identique à son manifeste |
| Firmware | `e97441b4…02c8` | `B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 scripts/build-boot-firmware.sh` |
| Chargeur | `d952d0b0…e5dd` (43 008 o) | branche `refactor/arm64e-uefi` commit `911faf7` |

Construit dans une copie de `mu-work-b1l-touch`, que `b1l-build.sh` n'utilise donc pas.

Contrôle de reproductibilité, avec le code d'origine dans la même copie :
- le chargeur se reconstruit à l'identique (`bcf747b1…`) ;
- le volume firmware ne diffère que d'**un octet** : la date de build (« 11/10/2026 » devient « 11/14/2026 »).

Le firmware 0215 n'a donc que deux différences avec 0213 : le chargeur et la date.

Flash BOOT avec `flash-boot.sh` : relecture `c58655ab…` → **BOOT-OK**. SYSTEM et USERDATA n'ont pas été touchés.

## Le menu (voir `docs/boot-menu.md` de la branche)

- **Volume +/−** : choisir.
- **Power** : valider (au relâchement).
- **Bixby** : arrêter le compte à rebours.
- Sans touche, iOS démarre au bout de 5 s.

Entrées :
1. Démarrer iOS.
2. Ne pas démarrer.
3. Redémarrer en recovery. Écrit PMU INFORM3 = 0x12345674 et SYSIP_DAT0 = 0xFF. **Non vérifié.**
4. Redémarrer.

Touches maintenues au démarrage :
- **Volume −** : menu sans compte à rebours.
- **Volume +** : curseur placé sur recovery.

## Pour revenir à 0213

Reflasher `beyond2lte-ios/out/b1l-metal-0213/boot.img` (`3bb1295f…`) depuis TWRP avec `flash-boot.sh`.

## Résultat

À remplir après le test du propriétaire. Points à vérifier :
1. Le menu s'affiche-t-il ?
2. Le compte à rebours, puis le démarrage d'iOS.
3. Les touches de volume et Power.
4. « Ne pas démarrer ».
5. « Redémarrer ».
6. « Recovery » : arrive-t-on dans TWRP ?

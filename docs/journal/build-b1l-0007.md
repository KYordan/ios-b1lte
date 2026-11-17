# Build B1L-0007 : écran logique 880×1912 @2x + mesure de fréquence CPU

Date : 2026-11-04. Statut : **installé dans BOOT (sda14) le 2026-11-04, relecture SHA-256 identique ; pas encore testé.** BOOT seulement (USERDATA et SYSTEM inchangés).

Contenu : B1L-0005 (firmware `503dd43a…`, loader `7b9fb7e5…`, inchangés) + une sonde **de lecture seule** dans
`xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp`. La sonde ne change aucun comportement, d'où l'exception à « un changement par build ».

## Sonde `B2L CPUFREQ`

Pendant la vérification des timers (le thread est lié à chaque CPU tour à tour) : interruptions masquées, compteur de
cycles PMU (`PMCCNTR_EL0`) comparé au timer architectural (26 MHz) sur 2 ms, 3 essais, maximum gardé.
L'état PMU est sauvegardé puis restauré. Aucune horloge ni tension n'est modifiée.
Sortie : `B2L CPUFREQ: cpu=N A55|A75|M4 ~XXXX MHz` (journal persistant et IOLog).

Risque : si l'EL2/EL3 de Samsung piège l'accès PMU (MDCR), panique au démarrage → retour au BOOT B1L-0003.

But : savoir à quelle fréquence S-Boot laisse les cœurs. Si les M4 ne sont pas à ~2,7 GHz, piloter le DVFS (ACPM)
devient le plus gros gain possible pour le rendu CPU, sans toucher USERDATA.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-display-cpufreq-0007/boot.img` (25 893 504 octets) | `3f07092f2e2ba3910b21abccb644ca522b060888c05596ced2bcc3dff0479382` |
| Kernel | `cbeeb2cd7a8143c26265d4cf1ec822339c32a47186c075fcac460e0c7f602a0c` |

Vérifications : `package-boot.py --verify` OK, 0 instruction LDAPR dans le kernel.
Note : `build-kernel.sh` compile `xnu/`, pas `xnu-overlay/` ; recopier le fichier modifié avant de compiler.

## Premier démarrage attendu

MobileGestalt régénère son cache (profil v3) : premier démarrage plus long, puis taille 440×956 points.
Retour arrière : `out/b1l-touch-0003/boot.img` (`0fbcf3ba…`).

## Résultat observé (B1L-0007)

Panique au démarrage, CPU0 : `Interrupts held disabled timed out: 2000 us >= 461 us`. La fenêtre de mesure de 2 ms
avec interruptions masquées dépasse la limite de XNU. L'accès PMU lui-même n'a pas piégé.

# Build B1L-0008 : même contenu, fenêtre de mesure de 200 µs

`span = CNTFRQ / 5000` (200 µs, 5 200 ticks à 26 MHz), 5 essais séparés par `IOSleep(1)`, maximum gardé.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-display-cpufreq-0008/boot.img` (25 893 504 octets) | `113e189809ba47fdc08f61e008cd4ecbae599adbffcd52d50f7cadc60acba0c2` |
| Kernel | `5c493d5531f8980158eaa880333d7cbf3d307902cea5a141a2fb7190c4bef0ae` |

Installé dans BOOT (sda14) le 2026-11-04 : garde-fous OK, relecture SHA-256 `113e1898…` identique. Pas encore testé.

## Résultat observé (B1L-0008, 1er démarrage)

- Pas de panique. `B2L CPUFREQ` : **A55 ~1053 MHz, A75 ~1222 MHz, M4 ~1378 MHz** (max. usine : 1950 / 2314 / 2730 MHz).
  S-Boot laisse tous les clusters à environ **50 %** de leur fréquence maximale : piloter le DVFS (ACPM) pourrait presque doubler la vitesse CPU.
- Profil v3 : `MobileGestalt refresh error=0`. backboardd ouvre le framebuffer (l'écran console s'efface à ce moment-là).
- Le propriétaire est passé dans TWRP après quelques minutes ; le journal s'arrête vers 254 s, avant la première image.
  En B1L-0004/0006, la première image n'arrivait qu'après ce point : démarrage probablement pas bloqué, à refaire en attendant plus longtemps.

## Résultat observé (B1L-0008, 2e démarrage) : écran noir

Pas de logo Apple, écran noir pendant ~20 min. Le journal s'arrête à 249 s (264 s au 1er démarrage), juste après les
appels backboardd `sel 74`, avant toute image : le kernel se fige (ou panique, console cachée) en mode mis à l'échelle.
Cause exacte non trouvée : `b2l_blit_scaled` paraît borné et passe ses tests. **L'écran logique 880×1912 est mis de côté.**

# Build B1L-0009 : retour à l'écran 1:1 @3x, sonde gardée, profil v4

- Firmware et loader reconstruits **sans** `B2L_FB_LOGICAL`/`B2L_DISPLAY_SCALE` : identiques à B1L-0003
  (firmware `52d8e75c…`, loader `97a4a731…`).
- Kernel : sonde `B2L CPUFREQ` (200 µs) + `b2l_profile_cache.h` en **v4** (MobileGestalt régénère encore une fois,
  car son cache contient maintenant le profil 440×956 @2x).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-cpufreq-0009/boot.img` (25 893 504 octets) | `b80d5871c7b661f4aae72984ad94e4de7269747c634f8eab8b7962025706834a` |
| Kernel | `f8bac309049ba409bb7f390c00100a554dac742b6ae1e15ab8644f6f9ff135dc` |

Installé dans BOOT (sda14) le 2026-11-04 : garde-fous OK, relecture SHA-256 `b80d5871…` identique. Pas encore testé.

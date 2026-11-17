# Build B1L-0028 : T1, throttle thermique du CPU

Date : 2026-11-05. Statut : **flashé puis remplacé par B1L-0029 (qui le contient) avant tout boot ; à tester via B1L-0029.** BOOT seulement. Base : B1L-0027.
Flashé sans nouvelle confirmation, à la demande du propriétaire (« une fois ça fait, flashe-le sans me demander »).

## Changement

- **`xnu-overlay/iokit/Kernel/arm/B2LThermal.h`** (nouveau, C pur) :
  - carte des registres TMU et étalonnage eFuse (faits de Samsung, repris de HakiOS `hakios.exynos_tmu.h`, famille 9810/9820) ;
  - zones du DTB du S10 : bloc TOP 0x10090000, BIG = sondes 0x7c0, MID = 0x20, LITTLE = 0x10 ; bloc SUB 0x100A0000, G3D = 0x21e ;
  - règle du régulateur `b2l_thr_step`.
- **`B2LCPU.cpp`** :
  - `b2l_acpm_send_rate()` : la requête de fréquence ACPM, devenue réutilisable ;
  - fil `b2l_thermal_thread`, lancé après le DVFS de démarrage :
    - si le boot a laissé un bloc TMU arrêté, il ne met que `CORE_EN`, jamais `THERM_TRIP_EN`, ni seuils, ni interruptions ;
    - chaque seconde, il lit le maximum de chaque zone ;
    - par cluster (A55 ↔ LITTLE, A75 ↔ MID, M4 ↔ BIG) : au moins 65 °C, un niveau fvmap plus lent ; au plus 60 °C, un niveau plus rapide, jamais au-dessus de la cible du boot ; au moins 72 °C sur la zone la plus chaude, plancher immédiat (≈1 GHz) ;
    - si la table fvmap est illisible, il utilise une échelle de secours de 100 % à 45 % de la cible.
- **Journal :** `B2L THERMAL: big=… mid=… little=… g3d=… C; kHz …` toutes les 10 s, et une ligne à chaque changement de niveau.
- **Boot-args :** `b2l_thermal=0` (arrêt), `b2l_thr_hi`, `b2l_thr_lo`, `b2l_thr_crit` (°C), `b2l_tmu_enable=0`.
- **Test hôte** `tests/test_thermal.c` : 34 vérifications, toutes **ok** (registres, conversion un ou deux points, règle du régulateur).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-thermal-0028/boot.img` (26 028 376 octets) | `b59446f99911f9ff759015cd19859d7cd8ca457f5fe37df4e729a31b3e67fd96` |
| Kernel | `93f8e8aa314cf11f2246ed09cc6f1e14e4f7ff265fd6cf2993a927d0d1687eb7` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-ccbg2-0027/boot.img`.

À vérifier au prochain passage dans TWRP :
- que les températures lues sont plausibles (repos 35–50 °C) ;
- la ligne `table=` de chaque cluster ;
- les changements de niveau sous charge.

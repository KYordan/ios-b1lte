# Build B1L-0030 : capteurs du 9820 corrigés (T1), fréquence GPU par ACPM (G4)

Date : 2026-11-05. Statut : **installé et testé (2026-11-05).** BOOT seulement. Base : B1L-0029.

## Constat sur B1L-0029 (testé)

- **G3 réussi.**
  - `GPU_ID 0x72110000` (G76 r0p0), 12 cœurs `0x770077`, `MMU_FEATURES 0x2830` (48/40 bits), tables à 4 niveaux.
  - Reset et cœurs allumés.
  - **Job en mode identité puis job par nos tables de pages : `0xc0ffee01 -- the GPU ran it`.**
  - `HakiosMali started`, et un rapport par minute.
- **T1 faux.** `big=85 g3d=85` en permanence, mid et little illisibles. Le M4 a été bloqué à tort au plancher (936 MHz). Cause : j'avais utilisé la carte des registres TMU du 9810. Le 9820 a la sienne (Samsung `exynos_tmu.c`, `CONFIG_SOC_EXYNOS9820`) :
  - `TRIMINFO(p) = p×4`, `CONTROL` à 0x50 ;
  - codes courants à `0x84 + (s/2)×4`, décalage `(s%2)×16`.
  - Sur le 9820, le TMU appartient au micrologiciel thermique ACPM.

## Changements

1. **`B2LThermal.h`** : carte du 9820. Test hôte `tests/test_thermal.c` mis à jour, 34 vérifications **ok**.
2. **`B2LCPU.cpp`** :
   - le TMU passe en **lecture seule**, plus aucune écriture ;
   - au démarrage, journal des codes bruts de chaque sonde (`B2L THERMAL: tmuN sM code=… trim=… -> … mC`).
3. **G4 : fréquence GPU.**
   - Index ACPM 10, d'après le `cal_id 0x0b04000a` du G3D, sur le même modèle que les CPU (2/3/4).
   - Lecture et journal de la table fvmap[10] (`B2L GPU DVFS: level …`), puis requête du niveau juste sous 702 MHz (le max du DTB), comme décidé (« presque max, plus sûr »).
   - Le GPU devient le 4e canal du régulateur thermique : zone G3D, plancher 400 MHz.
   - Kill switch `b2l_gpu_dvfs=0`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-thermgpu-0030/boot.img` (26 053 744 octets) | `d954c349f132f16822400eacc9ec2b33f202140a48d1a23a383c58d5034e1387` |
| Kernel | `b8111da3df48afd0be03aa6d5096d4526ce63159da7149bc77eb8c8982789098` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-mali-0029/boot.img`.

À vérifier :
- températures plausibles (repos 35–50 °C) ;
- table fvmap[10] et requête GPU ;
- les jobs de test du Mali passent toujours après la montée en fréquence.

## Résultat sur le téléphone

- **G4 :**
  - `fvmap[10]` lue, 12 niveaux : 702, 676, 650, 598, 572, 433, 377, 325, 260, 200, 156 et 100 MHz (681 à 537 mV) ;
  - **requête à 676 MHz acceptée (`ret=0`)** ;
  - les jobs 7 et 8 du Mali passent toujours (`0xc0ffee01`).
- **T1 :**
  - températures plausibles, de 50 à 66 °C (big, mid, little, g3d) ;
  - le régulateur agit : 65 °C → un niveau plus lent, 60 °C → un niveau plus rapide ;
  - la puce plafonne vers 62–66 °C, au lieu des 76 °C vus avant ;
  - sous charge continue, les cœurs finissent au plancher (M4 936 MHz, A55 949 MHz), d'où une perte de performance.

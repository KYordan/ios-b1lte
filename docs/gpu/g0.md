# GPU G0 : décisions (2026-11-04)

## 1. CoreAnimation d'iOS 26 a-t-il encore un rendu GLES ? **Oui.**

Analyse de `QuartzCore` extrait du cache 23G90 (`dsc_extractor`, `llvm-nm`) :
- `CA::OGL::GLESContext` : ~45 méthodes réelles (`GLESContext(void*)`, `init_state`, `set_surface`, `bind_image_impl`,
  `create_surface_from_iosurface`, `make_buffer_current(CA::WindowServer::Surface*)`, `release_display_surface`,
  `warmup_shaders`, `copy_destination`, `bind_gaussian`…), plus `GLContext` (54) et `GLESImage`.
- Les deux autres moteurs : `MetalContext` (306 symboles) et `SWContext` (rendu logiciel actuel, 37 + 379 `SW`).
- `+[CARenderer rendererWithEAGLContext:options:]` et `CAEAGLLayer` complets ; `OpenGLES.framework` présent
  (`GLEngine.bundle`, `libGLImage`, `libGLProgrammability`, 44 symboles `EAGLContext`).

`make_buffer_current(WindowServer::Surface*)` montre que le contexte GLES sait dessiner les surfaces du serveur
d'affichage, pas seulement des calques isolés. **Décision : la voie EAGL (méthode S7, Mesa/Panfrost) est la voie
principale.** Metal → Vulkan reste une recherche secondaire.

À faire : trouver ce qui choisit Metal/SW/GLES pour le WindowServer (constructeur de `CA::WindowServer::Display`,
préférences `CA*`), par recherche de signatures, sans adresse codée en dur.

## 2. Mesa

À vérifier (version et niveau de fonctions de Panfrost sur Bifrost v7/G76, PanVK).

## 3. Faits matériels (DTB du S10)

| Élément | Valeur |
| --- | --- |
| GPU | `mali@18500000`, `compatible = "arm,mali"` |
| Domaine d'alimentation | `pd-embedded_g3d@15862100` (reg 0x20), `cal_id 0xb1380007`, **`need_smc = 0x18410204`** |
| État d'alimentation | `gpu_pmu_status_reg_offset 0x2104` (PMU 0x15860000), masque `0x1` |
| Horloge (CAL) | `g3d_cmu_cal_id 0x0b04000a` (DVFS GPU via ACPM, comme les CPU) |
| Fréquences | `gpu_max_clock 702000`, `gpu_min_clock 156000`, démarrage `260000` kHz ; table DVFS (9 × 8) |
| Tension par défaut | `gpu_default_voltage 800000` µV |
| Thermique | capteur TMU `G3D@100A0000` ; seuils 95 / 105 / 115 °C |

G1 (B1L-0014) : lecture seule de l'état d'alimentation ; `GPU_ID` lu seulement si le domaine est déjà sous tension.
L'allumage (SMC + CAL) vient après, avec la séquence Samsung exacte.

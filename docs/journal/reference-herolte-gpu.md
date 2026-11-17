# Référence : GPU du port Galaxy S7 (`wardevsa/ios-a5xelte`, branche `herolte`)

Clone local : `~/Developer/s10-port/ios-herolte` (commit `23669fa`).
Lu le 2026-11-03. `[VERIFY]` = à prouver sur iOS 26 / S10.

## Ce que fait le S7

iOS 16.7.16 (iPhone X), XNU 8796, Mali-T880 MP12 (Midgard), GPU à 650 MHz,
~35-38 fps. Pas de Metal.

Chaîne de rendu :

```
CoreAnimation (renderer OpenGL ES, CA::OGL::GLESContext)
  -> pont EAGL (gpu/hakios-gl, gpu/eagl) + hooks dans le dyld shared cache (ca_hooks.c)
  -> OSMesa + Mesa Panfrost 25.0.7 (gpu/mesa, cross-compilé iPhoneOS arm64)
  -> libdrm de remplacement (gpu/mesa/herodrm.c) : ioctls panfrost -> IOKit user client
  -> service kernel HeroGPU (patches 0225-0267) : power domain, clocks, MMU/AS, job slots
```

Patches kernel GPU, dans l'ordre où ils ont été trouvés :
- 0225 probe (PMU G3D, CMU, ID GPU) ; 0226-0234 premiers jobs, du clear au quad texturé ;
- 0235-0237 présentation via GPU et mise à l'échelle ;
- 0239 user client ; 0247-0249 barrières, BO de surface, récupération de faute VA ;
- 0256/0264-0267 fréquence et tension d'usine (le gain majeur) ;
- 0258/0266/0273 récupération des `JOB_READ_FAULT`.

Autres gains mesurés et réutilisables tels quels :
- Déplacer `BTAudioHALPlugin` : le CPU au repos passe de >300 % à <25 % (sur 800 %).
- Résolution logique réduite (750×1334 @2x mise à l'échelle par le GPU) : 72 % de pixels en moins.
- Apps copiées dans l'image racine, démons gourmands envoyés sur les petits cœurs.

## Ce qui se transpose au S10 (Mali-G76 MP12, Exynos 9820)

| Élément | Verdict |
| --- | --- |
| Service kernel de jobs (modèle Job Manager) | **Transposable.** Le G76 (Bifrost) est encore un GPU « JM », pas CSF : mêmes job slots, MMU/AS et IRQ GPU/JOB/MMU. Les adresses changent. |
| Mesa Panfrost | **Transposable.** Panfrost gère Bifrost v7 (G72/G76). Il faut le compilateur Bifrost au lieu de Midgard dans `build-mesa.sh`. |
| `herodrm.c` (libdrm → IOKit) | **Réutilisable** presque tel quel, avec un user client équivalent dans le kernel S10. |
| Quirks T880, shaders Midgard (`herolte-midgard-shader.py`) | Spécifiques à Midgard, à jeter. |
| Pont EAGL + `ca_hooks.c` | **Point bloquant** `[VERIFY]` : dépend du renderer OpenGL ES de CoreAnimation dans iOS 16. Il faut vérifier dans le shared cache d'iOS 26 si `CA::OGL::GLESContext` existe encore. Sinon, c'est la voie Metal → Vulkan (dada) seule. Les adresses du cache sont propres à 16.7.16. |
| Palier « mise à l'échelle par le GPU » (0235/0236) | **Le plus réaliste à court terme** : utile même sans OpenGL ES ni Metal côté iOS. |
| Fréquence/tension d'usine | La méthode s'applique ; les tables de l'Exynos 9820 sont à prendre dans le DTB et le kernel Samsung. |

## Adresses Exynos 9820 (DTB de base, `beyond2lte-uefi/dtb-analiz`)

- `mali@18500000`, taille 0x5000, IRQ SPI 0x120 / 0x11f / 0x121 (job / mmu / gpu `[VERIFY]` ordre).
- `g3d_genpd_name = "pd-embedded_g3d"`, `g3d_cmu_cal_id = 0xb04000a`.
- À comparer au S7 : PMU `0x105c0000`, CMU `0x14aa0000`, GPU `0x14ac0000` (Exynos 8890).

## Correspondance avec le plan (phase 6)

1. Alimenter le G76 et lire son ID : adapter 0225 (power domain `pd-embedded_g3d`).
2. Job de test kernel : adapter 0226-0234 avec des descripteurs Bifrost.
3. Mise à l'échelle par le GPU : adapter 0235/0236.
4. Mesa pour iOS : `build-mesa.sh` avec `-Dgallium-drivers=panfrost`, compilateur Bifrost.
5. Rendu iOS : EAGL si iOS 26 a encore le renderer GLES de CA, sinon Metal → Vulkan (PanVK).

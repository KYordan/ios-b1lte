# Build B1L-0016 : GPU G2a (reset + alimentation des cœurs) + flous moins chers (essai)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `ce5b897c…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0015 (DVFS actif, variables CA).

## GPU G2a (`xnu-overlay/iokit/Kernel/arm/B2LMali.h`, `B2LCPU.cpp`)

Uniquement si `pd-embedded_g3d` est déjà allumé et `GPU_ID[31:16] == 0x7211` (G76) ; coupe-circuit `b2l_gpu=0`.
1. Lecture : `CORE/TILER/MEM/MMU_FEATURES`, `THREAD_MAX`, `GPU_STATUS`, `GPU_FAULTSTATUS`.
2. IRQ GPU/JOB/MMU masquées (sondage ; la GIC ne route pas encore les SPI Mali), effacées.
3. `GPU_COMMAND = SOFT_RESET`, attente `RESET_COMPLETED` (bit 8) ≤ 100 ms.
4. `L2_PWRON` → `L2_READY`, `TILER_PWRON` → `TILER_READY`, `SHADER_PWRON` (12 cœurs) → `SHADER_READY`, ≤ 100 ms chacun.
5. État `AS0..7` (STATUS, FAULTSTATUS).
6. Extinction shaders → tiler → L2 (pas de chaleur tant qu'aucun job n'est soumis).
Aucune table MMU, aucun job. Registres selon la carte kbase/Panfrost.

## Flous (essai)

`CA_BACKDROP_SCALE=0.25` ajouté pour backboardd. Analyse de la lecture par QuartzCore non aboutie (référence indirecte) :
effet non vérifié. Coupe-circuit séparé : `b2l_ca_backdrop=0`.

## Journaux iOS

Décodeur Mandiant `macos-UnifiedLogs` (09e6e6e) compilé localement (accord du propriétaire) :
1 225 275 / 1 464 424 entrées lues, mais **1 223 214 chaînes du cache partagé non résolues** (`hcsd` v2, iOS 26)
→ messages LaunchServices / SpringBoard / PosterBoard illisibles. À adapter séparément.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-gpu-g2-0016/boot.img` (25 910 096 octets) | `ce5b897c74d209fe7e9b9e7b4c96af01a93f56ce30e3ca7c5e9d8da87ccb3585` |
| Kernel | `94583c13f1de5c92783bf3c27462d110bc3fd8b99e2686381fbadb732797d1d3` |

Retour arrière : `out/b1l-caenv-0015/boot.img`.

## Résultat observé (B1L-0016)

- Démarre ; « un peu plus fluide ». Clavier toujours très lent ; pas de Control Center ; aucune différence visible des flous
  (`CA_BACKDROP_SCALE` sans effet observable).
- **G2a validé** : `soft reset done`, `L2=ready(1) tiler=ready(1) shaders=ready(770077)`, AS0..7 sans faute, cœurs éteints ensuite.
  `MMU_FEATURES=0x2830` (VA 48 bits, PA 40 bits), `TILER_FEATURES=0x809`, `MEM_FEATURES=0x301`, `THREAD_MAX=0x300`.

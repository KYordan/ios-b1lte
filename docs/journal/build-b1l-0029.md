# Build B1L-0029 : GPU G3, service noyau du Mali (port de HakiosMali)

Date : 2026-11-05. Statut : **installé dans BOOT (sda14), relecture `adb07d98…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0028 (le throttle T1 est inclus). Plan : `docs/gpu/plan.md`, étape G3.

## Origine

Repris de Scion-Project/hackios-xnu-kernel (licence APSL 2.0, en-têtes conservés) :
- `drivers/gpu/hakios.mali.c` devient `xnu-overlay/bsd/dev/b2l_mali_core.c` ;
- `hakios.mali_va.h` est repris tel quel (`bsd/dev/`) ;
- `drivers/gpu/hakios.mali.cpp` devient `xnu-overlay/iokit/Kernel/arm/B2LMaliService.cpp`.

HakiOS l'a écrit pour le Mali-G72 du S9 (Bifrost JM, comme notre G76), à partir de leur pilote du Mali-G52 qui faisait tourner Mesa Panfrost sur le Hot 20. Il n'avait jamais tourné sur le S9.

## Adaptations au S10 (marquées « B2L » dans le code)

- **GPU :** registres à 0x18500000, `GPU_ID >> 16` attendu 0x7211 (G76) ; famille 0x7001 tolérée.
- **pd-embedded_g3d :** PMU 0x15860000 + 0x2104, bit 0. Le domaine doit être allumé par S-Boot (constaté en B1L-0016). Ce portage ne l'allume jamais, ne touche ni au PLL (CMU_G3D non mappé) ni au rail PMIC (`mali_pll=0`, `mali_acpm=0`). La fréquence viendra par ACPM en G4.
- **IOSurface :** `MiniIOSurface` est remplacé par `B2LSurface`, dont les pixels commencent à l'offset 0 de `memory` ; nouveau champ `maliSeq` dans `B2LSurface.h`. Pas de pool de scan-out ni de surfaces en mémoire client.
- **Nom du service :** `HakiosMali` et `HakiosMaliUserClient` sont gardés, pour que leur libdrm (`tools/mesa/hakios-drm`, qui cherche « HakiosMali ») serve sans modification en G5.
- **Démarrage :** depuis `B2LCPU.cpp`, juste après `b2l_gpu_probe()` (G1/G2). Fil propre, étapes 1 à 9 de HakiOS (lecture, contrôle d'alimentation, GPU_ID, reset, cœurs, job en mode identité, job par tables de pages, service), puis un rapport par minute.
- **Kill switches :** `b2l_mali=0` (rien), `mali=N` (s'arrête à l'étape N). Les options HakiOS (`mali_fastmap`, `mali_nap_us`, `mali_flush`…) sont conservées.
- **Build :** `patches/0104-b2l-mali-service-build.patch` (`bsd/conf/files`, `iokit/conf/files`).

## Critères de réussite (journal TWRP)

- `mali: 4: GPU_ID 0x72110000…`, puis `mali: 7` et `mali: 8` : les deux jobs écrits (mode identité, puis par les tables de pages).
- `mali: 9: HakiosMali started`, puis `HakiosMali: the GPU is there for programs to draw with`.
- Toutes les minutes, une ligne `mali: step 9 of 9 …` (aucun client avant G5/G6).

Une faute ou un job non terminé est journalisé avec son adresse (`mali: the last of N unfinished…`).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-mali-0029/boot.img` (26 053 408 octets) | `adb07d9894c92fef5f246877249a48017e74c1eb72f7f501ae95cae3d6fcaa07` |
| Kernel | `331b478bce1e31b3c5e2cec425e971080c0752bf4c2057e4ae1f2cc87131df9e` |

Firmware `52d8e75c…` et loader `97a4a731…` inchangés. Retour arrière : `out/b1l-thermal-0028/boot.img`.

# Build B1L-0069 : M5, la texture sur IOSurface demandée au bon service noyau

Date : 2026-11-07. Base : B1L-0068.

## Constat sur B1L-0068 (`out/logs/0068-boot1.txt`)

- `M5: device Mali-G76 (Panfrost) (Hakios)` : le device Metal est bien là.
- `M5: IOSurface …, ID 39, 256 bytes a row` : la surface est créée et partagée.
- `M5: newTextureWithDescriptor:iosurface:plane: -> nil` : la texture n'est pas créée.
- **Cause.** Le code de Scion (`hakiosdrawable.m`, `rwd_gpu_open`) cherche le service noyau `HakiosGPU`, celui de l'Adreno, pour décrire la surface (requête `0x7e`). Sur le S10, ce service est `HakiosMali`.
- Le reste de la chaîne existe déjà depuis l'essai OpenGL ES :
  - `HakiosMali` répond à `0x7e` avec exactement la même structure (`struct mali_surface` = `struct rwd_surface`) ;
  - le shim DRM : `drmPrimeFDToHandle` envoie l'ID de surface à `0x7e` ;
  - l'import de panfrost (`pan_kmod_bo_import`) prend la taille à `hakios_drm_surface` ;
  - `libhakiosair` exporte `rwa_texture_from_surface` (import linéaire, 64 octets par ligne au moins).
- La raison du refus n'apparaissait pas dans le journal : le test M5 tournait après la fin de la capture de la sortie du pilote.

## Changements

- `scion/hakiosdrawable.m` : `rwd_gpu_open` essaie `HakiosGPU`, puis `HakiosMali`.
- `b2l_metal_test.m` : `m5_surface_test()` tourne pendant que la sortie du pilote va encore dans le journal (`drv: …`).
- Ensemble `b2l-metal-v25` :

| Fichier | SHA-256 |
| --- | --- |
| `libb2l-metal-test` | `b3314d07…` |
| `libhakiosmetal` | `24c22f59…` |
| `libhakiosair` | `47aae23e…` |

  Le noyau pointe vers v25.

## Ce qu'il faut regarder

- Lignes `M5:` : rouge puis vert sur les 4096 pixels.
- Un écart dans un seul sens montrerait un problème de cache CPU/GPU, la mémoire des IOSurfaces étant en cache côté CPU, alors que le Mali ne regarde pas dans les caches.

## Installé

- SYSTEM : 5 blocs sur 1750 réécrits (20 Mio), en 95 s.
- BOOT : `out/b1l-metal-0069/boot.img`, relecture `f08cc54dd512d8cc0c77068e7398a4b5f556dffd3e7e3c5f7cedf8e91ad459ec` (BOOT-OK).

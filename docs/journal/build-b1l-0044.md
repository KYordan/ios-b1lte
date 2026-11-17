# Build B1L-0044 : Metal M1, un MTLDevice qui existe sur le Mali

Date : 2026-11-06. Statut : **installé : SYSTEM (3 blocs, copie `9aaab8d7…`, les 3 fichiers vérifiés) et BOOT (relecture `521e8c7b…`) ; pas encore testé.** Base : B1L-0043. Plan : `docs/metal-plan.md`, étape M1.

## Ce que Metal d'iOS 26 attend (Metal 23G90, désassemblé)

1. `-[MTLIOAccelServiceGlobalContext init]` cherche les services `IOServiceMatching("IOAcceleratorES")`.
2. Pour chacun, `getMetalPluginClassForService()` lit `MetalPluginName` et charge `/System/Library/Extensions/<nom>.bundle`.
3. Il prend la classe `MetalPluginClassName` (vérifiée par `isSubclassOfClass:`) et appelle `initWithAcceleratorPort:deviceClass:`.

C'est le même mécanisme que celui exploité par HakiOS sur iOS 16.

## Changements

- **Noyau :**
  - `xnu-overlay/iokit/Kernel/arm/B2LMetalAccel.cpp` : classe `IOAcceleratorES` et service `B2LMetal`, avec `MetalPluginName=B2LMetalV1` et `MetalPluginClassName=HakiosMetalDevice`. Il est publié après HakiosMali (`HakiosMaliStart`). Interrupteur : `b2l_metal=0`. Ajouté au build par `patches/0107-b2l-metal-accel-build.patch`.
  - `b2l_userspace_seed.h` (`b2l_metal_publish`) : copie depuis SYSTEM `/b2l-metal-v1` vers `/System/Library/Extensions/B2LMetalV1.bundle/{B2LMetalV1,Info.plist}` et `/usr/local/lib/b2l-metal-v1/libb2l-metal-test.dylib`.
  - `b2l_exec_graphics_diag.h` : backboardd reçoit la bibliothèque de test Metal (`b2l_metal_test=1` par défaut), sauf si `b2l_gl=1` (pont GLES).
- **Plug-in** `userspace/metal/B2LMetal.m` : copie de `hakios-metal/plugin/HakiosMetal.m` (MIT, licence et commit à côté), compilé **sans** cœur GPU. Le device existe, les objets Metal de base aussi, et tout ce qui n'est pas implémenté est signalé. Modifications B2L :
  - les traces vont aussi au journal noyau (`b2l-metal: …`) ;
  - le device n'est montré qu'à un processus qui a `B2L_METAL_TEST` ;
  - le nom indique le Mali-G76.

  Avec `hakios_metallib.{c,h}`.
- **Test** `userspace/metal/b2l_metal_test.m`, injecté dans backboardd :
  - QuartzCore est gardé hors de Metal (`bl CAMetalContextCreate` devient `mov x0,#0`, mot vérifié), donc le rendu CPU reste en place ;
  - après 20 s, il crée le device et note son nom, puis teste une file de commandes, un command buffer (commit/wait) et un buffer ;
  - le test ne tourne qu'une fois par boot, et un plantage pose un marqueur (pas de boucle) ;
  - les traces sont préfixées `b2l-mtest: …`.
- `scripts/build-metal.sh` ; `b1l-tools/system-add-gl.sh` copie maintenant tous les fichiers du dossier (plus seulement les `.dylib`).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-metal-0044/boot.img` | `521e8c7b93fae149ca901135078de0d256f932c0dcb34a17bbd046901f638e0d` |
| Kernel (`kernel.macho`) | `8e8ecd8595b99a3352539160c2bc5776b3a419ae3147ead56b2b889dbd5129e3` |
| `B2LMetalV1` | `002e3120…` |
| `libb2l-metal-test.dylib` | `27f120d5…` |

## Retour arrière

Reflasher `out/b1l-cpudefault-0043/boot.img` (`f478d903…`) ou `out/b1l-thermgpu-0030/boot.img`.

## B1L-0045 (suite) : la variable de test posée dès le chargement

- **Constat sur 0044 (`out/logs/0044-boot1.txt`) :**
  - `IOAcceleratorES published`, le bundle et le test publiés (`error=0`), le test injecté dans backboardd ;
  - Metal charge `B2LMetalV1` dans SpringBoard, cameracaptured, chronod, iconservicesagent…, qui sont filtrés comme prévu ;
  - dans backboardd, un autre composant interroge Metal avant le test (`backboardd is not shown the device`). La liste des devices est faite une fois par processus, donc `MTLCreateSystemDefaultDevice -> 0x0`.
- **Correctif :** `setenv("B2L_METAL_TEST")` dans le constructeur de la bibliothèque, avant tout appel à Metal. Test publié dans `b2l-metal-v2` (`libb2l-metal-test.dylib` `00b0e994…`). Le bundle reste `B2LMetalV1` (inchangé).
- **Installé :** SYSTEM (3 blocs, copie `2dca2386…`) et BOOT `out/b1l-metal-0045/boot.img` (relecture `58cba87a56102aafd992f91d8c405e7d8130ecc387dbe81827ddfbc3eb01159a`).

## B1L-0046 (suite) : QuartzCore réellement tenu hors de Metal

- **Constat sur 0045 (`out/logs/0045-boot1.txt`) :**
  - **M1 atteint :** `backboardd is shown the device`, `the class HakiosMetalDevice is registered`, `a device for the accelerator port 85515 (made)`. Les classes queue, buffer, function, library, fence et pipeline sont enregistrées.
  - **QuartzCore 23G90 contient son `default.metallib` :** `189 functions (37 vertex, 128 …)`. Contrairement à HakiOS sur iOS 16, rien à fabriquer.
  - **Puis abort de backboardd (slide `0x280000`) :** `AccelServer::renderer()+0x50` → `CA::OGL::new_metal_context+0xac4` → `-[_MTLBinaryArchive loadFromURL:error:]` → `deserializeBinaryArchiveHeader` → `_MTLReportFailure`.
  - `renderer()` n'appelle `CAMetalContextCreate` que si le contexte Metal partagé (`[x20,#0x138]`) est vide. Un autre composant de backboardd l'avait déjà créé avec notre device, donc le correctif sur le `bl` était contourné.
  - **À retenir pour M6 :** QuartzCore charge une archive de pipelines binaires faite pour les GPU Apple. Le plug-in devra la refuser proprement (erreur, pas d'abort).
- **Correctif (test `b2l-metal-v3`, `f5c6d7ea…`) :** en plus du `bl`, le `cbz x0` de `0x183f3bdf0` (`0xb4000a20`) devient `b 0x183f3bf34`. Le chemin sans Metal est toujours pris.
- **Référence ajoutée :** `danstronics/ios-s9plus-exynos/metal-wip` (S9+, Metal→Vulkan via metal2vulkan et panvk, inachevé), cloné dans `ref-scion/ios-s9plus-exynos`. Leçon pour M5 : sur Exynos, les IOSurface en mémoire cachée demandent de vrais `DC CIVAC` côté noyau, par plage (ligne par ligne : 163 ms pour 12 Mo).
- **Installé :** SYSTEM (3 blocs, copie `feeacb50…`) et BOOT `out/b1l-metal-0046/boot.img` (relecture `b9817f66964f8dce066b9f31fae5621bd8f7dad3919d0160b3ebc8e0c1de1175`).

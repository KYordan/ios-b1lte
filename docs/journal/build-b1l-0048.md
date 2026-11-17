# Build B1L-0047/0048 : `targetDeviceArchitecture`, puis Metal M2 (buffers et copies par le GPU)

Date : 2026-11-06. Statut : **0047 installé mais non démarré (remplacé par 0048) ; 0048 installé : SYSTEM (8 blocs, copie `ce0d4c44…`, les 4 fichiers vérifiés) et BOOT (relecture `d113f8d8…`) ; pas encore testé.** Base : B1L-0046.

## Constat sur B1L-0046 (`out/logs/0046-boot1.txt`)

- Le device est créé (`accelerator port 10931 (made)`). Puis backboardd fait un abort avant le test : `B2L FAULTMSG … +[MTLLoader sliceIDForDevice:legacyDriverVersion:airntDriverVersion:]`.
- Pile d'appels (slide `0x1ae40000`) : `_MTLReportFailure` dans `sliceIDForDevice:` (`+0x98`).
- Désassemblage (Metal 23G90) : `[device targetDeviceArchitecture]`, et si c'est nil, `…cold.1` signale une faute (abort). Le stub de `_MTLDevice` renvoie nil.

## B1L-0047 : `targetDeviceArchitecture`

- `userspace/metal/B2LMetal.m` : `-targetDeviceArchitecture` renvoie un vrai `MTLTargetDeviceArchitecture` avec `cpuType` `'B2L\0'` (`0x42324c00`) et `subType` 1. Il ne correspond à aucune tranche des archives d'Apple, qui sont donc refusées sans abort.
- Bundle renommé `B2LMetalV2` (le noyau ne remplace pas un fichier existant), dossier `b2l-metal-v4`.
- BOOT `out/b1l-metal-0047/boot.img` (`3a6d3754…`) et SYSTEM (copie `0aa091cc…`) ont été écrits, puis remplacés par 0048 sans démarrage, pour économiser un boot.

## B1L-0048 : M2

- **Cœur Metal sur Gallium** `userspace/metal/b2l_metal_core.c` (MIT). Il implémente la partie de `hakios_metal.h` (HakiOS) dont le plug-in a besoin :
  - **device :** l'écran de `osmesa_create_screen()`, soit panfrost via hakios-drm et HakiosMali, plus un `pipe_context` sous verrou ;
  - **buffers :** `PIPE_BUFFER` avec mapping persistant et cohérent ;
  - **files de commandes :** un thread par file, ordre de commit respecté, callbacks sur ce thread ;
  - **command buffers :** copie (`resource_copy_region`) et remplissage (`clear_buffer`) enregistrés, puis exécutés au commit ; ensuite `flush` et attente du fence (10 s au plus).

  `hakios_metal_buffer_gpu_address` renvoie 0 pour l'instant.
- **Intégration :** `scripts/build-mesa.sh` copie le cœur et `hakios_metal.h` dans `src/gallium/targets/osmesa` et l'ajoute à `libOSMesa`, qui exporte 24 symboles `hakios_metal_*`.
- **Plug-in compilé avec `HAKIOS_METAL_GPU`**, lié à `/usr/local/lib/b2l-metal-v5/libOSMesa.8.dylib`. Bundle `B2LMetalV3`.
- **Test M2** (`b2l_metal_test.m`) : deux buffers de 256 Ko. Le CPU remplit A selon un motif. Le GPU copie la première moitié de A dans B et remplit la seconde moitié de B avec `0x5a`. Le CPU vérifie octet par octet et mesure le temps. Ligne attendue : `M2: … copied 131072 of 131072 bytes right, filled 131072 of 131072 right: M2 done`.
- **Noyau :** `IOAcceleratorES` → `B2LMetalV3`. `b2l_metal_publish` publie aussi `libOSMesa.8.dylib` (dossier `b2l-metal-v5`).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-metal-0048/boot.img` | `d113f8d89526812f4aa245dc4bbb7eb089016af88ac041e961db607c1114a2e5` |
| `B2LMetalV3` | `14772a62…` |
| `libOSMesa.8.dylib` (v5) | `a06ab3aa…` |
| `libb2l-metal-test.dylib` | `432d2116…` |

## Retour arrière

Reflasher `out/b1l-cpudefault-0043/boot.img` (`f478d903…`).

## Constat sur B1L-0048 et B1L-0049

- **Le cœur Metal s'ouvre sur le Mali dans backboardd :** `b2l-mcore: open: Mali-G76 (Panfrost), Gallium context …`, `newCommandQueue: a queue of the GPU's`.
- **QuartzCore de backboardd prend le device par un autre chemin que `renderer()` :**
  - il charge `QuartzCore.framework/default.metallib` (tranche AIR64 universelle, 1 552 128 octets, 189 fonctions) ;
  - il crée des textures et demande `renderCommandEncoderWithDescriptor:`, `copyFromTexture:…` et `generateMipmapsForTexture:`, qui ne sont pas implémentés ;
  - il fait un abort (signal 6) avant notre test.
- La liste des devices Metal est faite une fois par processus, donc test et serveur d'affichage ne peuvent pas partager backboardd.
- **B1L-0049 :**
  - le test (`b2l-metal-v6/libb2l-metal-test.dylib`, `46ff19d1…`) est injecté dans **notre démon `b2l-system-app-bootstrap-v4`** (lancé par launchd à chaque boot, root, sans Metal par ailleurs) ;
  - il pose `B2L_METAL_TEST`, lance le test et attend sa fin dans un `atexit`, 90 s au plus ;
  - backboardd ne reçoit plus rien et ne voit plus le device (comme en 0044) ;
  - le bundle (`B2LMetalV3`) et `libOSMesa` restent ceux de `b2l-metal-v5`.

## Charge CPU (question du propriétaire)

- `B2L LIVE: traps=30917755` à 435 s (0 à 72 s), soit environ 86 000 pièges par seconde une fois l'interface lancée.
- `B2L EMU PERF: samples=30194 wrapper_ns=345509653` : environ 11 µs par piège mesuré.
- iOS 26 n'existe qu'en arm64e. Les cœurs de l'Exynos 9820 n'ont pas PAC (ARMv8.3) : chaque instruction PAC non transformable en NOP (`autda`, `retab`, `pacga`…) part en piège dans le noyau, qui l'émule. Cela représente environ un cœur entier en permanence.
- Les projets de référence tournent sur des versions d'iOS qui existent encore en arm64 simple (iOS 16 pour HakiOS), donc sans ce coût.
- **B1L-0049 installé :** SYSTEM (copie `809ed129…`) et BOOT `out/b1l-metal-0049/boot.img` (relecture `58b7f321c209fec21332f8921d658474e04ec70a0cd19e94208601e5ccc88014`).

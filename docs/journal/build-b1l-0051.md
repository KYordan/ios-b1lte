# Build B1L-0050/0051 : mesure des pièges PAC, test Metal dans `tzinit`, Metal M3 (driver Mali de Scion)

Date : 2026-11-06. Statut : **0050 flashé (BOOT `d1c5235c…`) mais non démarré ; 0051 installé : SYSTEM (6 blocs, copie `38ba0080…`) et BOOT (relecture `fd052b9a…`) ; pas encore testé.** Base : B1L-0049.

## Constat sur B1L-0049

- Plus de plantage de backboardd : le démarrage normal, avec le logo, est revenu.
- Le test M2 n'a jamais tourné : le démon `b2l-system-app-bootstrap-v4` n'est plus lancé (`B2L APPS: system-app layout active; installer launcher not published`, depuis B1L-0025).
- **Cache natif appris** (la traduction PAC persistante sur SYSTEM) presque inopérant :
  - au démarrage, `pre-init apply applied=49 already=2066 skipped=63523` ;
  - pendant le boot, `learned=0`, avec des centaines de milliers d'observations perdues ou ignorées (`dropped`, `skipped`) ;
  - `reclaim_disabled=1` (`undo archive unavailable (5)`).
- **Pièges :** `traps=24294618` à 345 s. Environ 11 µs par piège (`EMU PERF`), dont environ 4 µs dans `validate_pc`.

## B1L-0050 : mesurer avant d'optimiser le PAC

- **`b2l_arm64e_compat.c`** : compteurs par type de piège (PAC/AUT registre, hint, RETAA/AB, BRAA/BLRAA, LDRAA/AB, LDAPR, PACGA, autre) et part dans le shared cache. Ligne `B2L EMU CLASS` toutes les 30 s (`b2l_persistent_log.c`).
- **`b2l_native_cache.c`** : chaque abandon a maintenant sa raison (`B2L NCACHE WHY`).
  - Apprentissage : `unresolved`, `identity`, `disk-differs`, `integrity`, `measured`, `other-file`, `original`.
  - Application au démarrage : `inode-gone`, `mismatch`.
- **Hôte du test Metal :** `tzinit`, reconnu par la fin de son chemin. C'est le réglage du fuseau horaire : root, une fois par boot, sans lien avec l'affichage.

## B1L-0051 : M3 avec le driver Metal Mali de Scion

- **Découverte :** le dépôt de référence `Scion-Project/hackios-xnu-kernel`, déjà la source de nos patches Mesa (`REF` de `build-mesa.sh`, donc `air-metal.patch` déjà appliqué), contient le driver Metal prévu pour le Mali-G72 du S9. Il est sous licence MIT (`userland/src`, `tools/mesa`) :
  - `tools/mesa/air/air_metal.c` → `libhakiosair` : AIR → NIR → compilateur Bifrost de panfrost, Gallium sur HakiosMali ;
  - `userland/src/hakiosmetal_air.m` + `hakiosdrawable.m` → `libhakiosmetal` : la sous-classe de `_MTLDevice` et tous les objets Metal ;
  - `metal_drv_test.m` : crée le device et teste la compilation et l'exécution d'un kernel système (« status 4 » = terminé), puis un dessin comparé à softpipe.

  Leur `GPU.md` (hakios-starlte) prévoit exactement ce test (M1 de leur plan) ; il n'y avait pas encore de résultat publié sur le S9.
- **Chez nous :**
  - `build-mesa.sh` ajoute à `libhakiosair` la dépendance `idep_xmlconfig` manquante, puis le compile (9,9 Mo).
  - `scripts/build-metal-scion.sh` remplace `/redwood/lib/` par `/usr/local/lib/b2l-metal-v7/`. Il compile `libhakiosmetal`, puis `metal_drv_test` en bibliothèque (`main` renommé `b2l_drv_main`) avec les accesseurs arm64e de `__darwin_arm_thread_state64` (correctif B2L). Il compile enfin notre test.
  - `b2l_metal_test.m` : après M2, `run_drvtest()` lance `metal_drv_test PortraitFilters/default.metallib image_calcminmax0`, puis `MetalFaceMask_InitMinMax`. Sorties préfixées `drv: …` dans le journal noyau.
  - Le noyau publie les quatre bibliothèques dans `/usr/local/lib/b2l-metal-v7/` et injecte `libb2l-metal-test` v7 dans `tzinit`.

| Artefact | SHA-256 |
| --- | --- |
| `libhakiosair.dylib` | `aa2805c6…` |
| `libhakiosmetal.dylib` | `4c752feb…` |
| `libb2l-metal-drvtest.dylib` | `d16ffecb…` |
| `libb2l-metal-test.dylib` | `c62ed6c4…` |

## Retour arrière

Reflasher `out/b1l-cpudefault-0043/boot.img` (`f478d903…`).

## Constat sur B1L-0051 (`out/logs/0051-boot1.txt`)

- **M1 et M2 validés dans `tzinit` :**
  - `MTLCreateSystemDefaultDevice -> HakiosMetalDevice`, `open: Mali-G76 (Panfrost)`, command buffer `status 4` ;
  - **`M2: status 4 (no error) in 16300 us: copied 131072 of 131072 bytes right, filled 131072 of 131072 right: M2 done`** : copie et remplissage de 256 Ko faits par le Mali, exacts octet par octet.
- **M3 n'a pas vraiment tourné :** `metal_drv_test` a trouvé le device de notre plug-in (créé par M2) et n'en a pas ajouté de second (`Metal's own discovery already has a device, so this process adds NOTHING`). Son « compiled » vient du pipeline vide de notre plug-in, sans code pour le Mali.
- **Pièges PAC par type (525 s) :** `ret=17738111 br=6255605 ldra=1386221 pacaut=40823`, soit 70 % `retab`/`retaa`, 25 % `braa`/`blraa` et 5 % `ldraa` ; `in-shared-cache=25410812` (99,96 %).
- **Cache natif :**
  - `apply mismatch=63523` : tous les enregistrements sont rejetés au démarrage ;
  - pendant le boot, `measured=47336` : l'apprentissage écarte presque tout comme du « texte mesuré » (exclusion prévue pour corecrypto) ; `learned=409`.
- **Contradiction à lever :** la copie traduite sur le Mac (`static-work/out`) ne contient plus que 20 `retab` et 106 346 `braa`/`blraa`. L'image USERDATA la plus récente sur le Mac a `.03` identique à la copie traduite et `.01` modifié en plus. Le téléphone prend pourtant 17,7 millions de pièges `retab`.
- **Services en boucle :**
  - `SiriAUSP`, `MauiAUSP`, `MacinTalkAUSP` et `KonaSynthesizer` : 60 lancements chacun, tous terminés par un abort ;
  - `fairplayd.H2` (34), `bluetoothd` (33), `wifip2pd` (31) et `watchdogd` (16) sont relancés sans trace de plantage.

## B1L-0052

- **M3 seul** (`b2l-metal-v8/libb2l-metal-test.dylib`, `5079a067…`) : dans `tzinit`, sans `B2L_METAL_TEST`. Notre plug-in se cache, donc la découverte de Metal ne donne rien et `metal_drv_test` crée le device de `libhakiosmetal` (bibliothèques v7).
- **CPU :** `b2l_exec_block_loops()` refuse (`EPERM`, avant dyld) l'exec des quatre synthétiseurs vocaux. Interrupteur : `b2l_block_tts=0`.
- **Diagnostic PAC :** `B2L NCACHE EXAMPLE` donne, pour les premiers rejets « texte mesuré » et « mismatch », le fichier, l'offset, l'instruction et la plage mesurée.
- **B1L-0052 installé :** SYSTEM (4 blocs, copie `bbebc686…`) et BOOT `out/b1l-metal-0052/boot.img` (relecture `f844d65016a5eb19e0674d37b8319232749ccc95ae1fa15f4ae01f2b35ad336b`).

## B1L-0052 → panic ; B1L-0053

- **Constat (photo) :** `B2L INIT EXIT: pid=18 … status=4`, `tzinit[18] exited -- no exit reason available -- (signal 0, exit status 4)`, puis panic.
  - `tzinit` est une tâche de démarrage que launchd exige réussie. `metal_drv_test`, qui a cette fois réellement tenté M3, s'est terminé par `exit(4)` à l'intérieur de `tzinit`.
  - Le marqueur n'étant posé qu'en fin de test, chaque boot de 0052 refait ce panic.
- **Correctif (`b2l-metal-v9/libb2l-metal-test.dylib`, `f3c6ed0a…`) :**
  - le marqueur « déjà lancé ce boot » est posé avant tout ;
  - le test tourne dans un **processus fils** (`fork()` dans le constructeur, avant tout second thread) qui finit par `_exit()` ;
  - `tzinit` continue aussitôt, sans être affecté par le code de sortie du test.
- BOOT `out/b1l-metal-0053/boot.img`, flashé dès que le téléphone est en TWRP.
- **B1L-0053 installé :** SYSTEM (4 blocs, copie `cf4024d2…`) et BOOT `out/b1l-metal-0053/boot.img` (relecture `804741103f6f5eeba95fb7f64160895520b34f2b20dc0c731a6378a7a33f746e`). Les journaux des boots 0052 (époque 56, 68 Ko) s'arrêtent avant la sortie du test : la cause de l'`exit(4)` sera lue dans 0053.
- **Flash SYSTEM accéléré** (`b1l-tools/system-add-fast.sh`, `hash-device.sh`, `flash-delta.sh` avec `DEV_HASHES` et `SAVE_HOST_HASHES`) :
  - le téléphone calcule une empreinte par bloc de 4 Mo (8 workers) ; seules les empreintes passent par USB ;
  - si elles sont identiques à `out/live/b1l-system-live.blockhashes`, la lecture de 7 Go par USB est sautée ;
  - `flash-delta.sh` réutilise ces empreintes et sauve celles de la nouvelle image ;
  - en cas d'écart ou sans cache, retour à la lecture complète vérifiée (comme `system-add-gl.sh`).

  Cache initial calculé depuis la copie live après le flash 0053.

## Constat sur B1L-0053 (`out/logs/0053-boot1.txt`) et B1L-0054

- **Démarrage normal**, plus de panic.
- **Synthétiseurs bloqués :** `B2L EXEC: …/KonaSynthesizer.appex/KonaSynthesizer refused`, de même pour MauiAUSP, SiriAUSP et MacinTalkAUSP.
- **PAC :**
  - les rejets « texte mesuré » sont dans `dyld_shared_cache_arm64e.31`, plage `0x1516c00+0x874d0`, c'est-à-dire **corecrypto** (exemples : `blraa`, `retab`, `pacia` à `0x151b59c`, `0x151b5ac`, `0x151b520`). Le code qui piège le plus souvent est donc probablement corecrypto, dont le texte est exclu de la traduction parce qu'il vérifie sa propre intégrité ;
  - les rejets à l'application (« mismatch ») concernent le binaire `dyld` lui-même (`same_file=0` : fichier changé depuis l'enregistrement).
- **M3 :**
  - dans le processus fils, la découverte de Metal ne donne rien (notre plug-in se cache) ;
  - `libhakiosmetal` se charge : `step 4 isSubclassOfClass:_MTLDevice PASSES`, `the compiler library Mali-G76 (Panfrost)`, **`step 5 gave HakiosMetalDevice`** ;
  - puis le fils reçoit l'exception 2 (instruction illégale). Le noyau note `B2L COMPAT: rejected … pc=0x19e471e2c x16=0x952d0001121160a0` : l'émulation PAC refuse une branche dans ce processus issu d'un `fork()`.
- **B1L-0054 :** plus de `fork`. Le test (`b2l-metal-v10`, `06dbbea1…`) tourne dans un thread de **`weatherd`**, un démon ordinaire que launchd relance (pas une tâche de démarrage), une fois par boot (marqueur posé d'abord). Premier flash avec `system-add-fast.sh`.
- **B1L-0054 installé :** SYSTEM (copie `55d14515…`, 4 blocs) et BOOT `out/b1l-metal-0054/boot.img` (relecture `4ac8f858c32176de2363bf1b1beece16b4e843f224828b0dbbeac0b72fab6046`). Flash SYSTEM en 684 s :
  - empreintes du téléphone en 26 s ;
  - mais lecture complète quand même, car le téléphone réécrit lui-même des blocs de SYSTEM à chaque boot (journal persistant, cache natif).
- **`system-add-fast.sh` corrigé :** seuls les blocs changés sur le téléphone sont relus (au plus 256, chacun vérifié contre l'empreinte donnée par le téléphone) et recopiés dans la copie live. Lecture complète seulement au-delà, ou sans cache.

## Constat sur B1L-0054 et B1L-0055

- Dans `weatherd`, le test va jusqu'à `step 5 gave HakiosMetalDevice` (device de Scion créé, compilateur `Mali-G76 (Panfrost)`), puis `SIGNAL 4 at 0x19cb8bbf8`, appelé depuis `b2l_drv_main+0x5320`. Le `fork` n'était donc pas la cause.
- **Cause :** `metal_drv_test` calcule l'adresse de la fonction privée `MTLAddDevice` comme `MTLCreateSystemDefaultDevice - 0x1c3e0` (`MTL_*_UNSLID`, adresses d'iOS 16.5). Sur iOS 26, ce calcul tombe au milieu d'une autre fonction.
- **Correctif :** dans Metal 23G90, `MTLAddDevice` est un symbole exporté (`T _MTLAddDevice`, `0x18612ab04`). Le test le prend par `dlsym(RTLD_DEFAULT, "MTLAddDevice")`. Aucune autre adresse fixe dans `hakiosmetal_air.m`, `hakiosdrawable.m` ni le test.
- Ensemble Scion recompilé en `b2l-metal-v11` (`libhakiosair` `25becb34…`, `libhakiosmetal` `92a9ec96…`, `libb2l-metal-drvtest` `9041dd93…`, `libb2l-metal-test` `a97149f3…`) ; le noyau publie et injecte la v11.
- **B1L-0055 installé :** SYSTEM (7 blocs) et BOOT `out/b1l-metal-0055/boot.img` (relecture `523c0c12d908d894bad7e1e69f8e6b893b0cdd915feb8364e01265e55650ce20`). **Flash SYSTEM en 112 s au lieu de 684 s** : empreintes du téléphone en 26 s, 4 blocs relus, 7 blocs écrits et relus.

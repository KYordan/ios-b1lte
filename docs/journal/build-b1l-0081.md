# Build B1L-0081 : M6b, deuxième essai (`targetDeviceArchitecture` et stubs d'Apple)

Date : 2026-11-07. Base : B1L-0080 (backboardd plantait sur `targetDeviceArchitecture`).

## Changements

**`scion/hakiosmetal_air.m` :**
- **`-targetDeviceArchitecture`** répond un `MTLTargetDeviceArchitecture` de type CPU `'B2L'`, sous-type 1, comme notre ancien plug-in depuis B1L-0047. Aucune tranche des archives d'Apple ne correspond, donc elles sont refusées, et QuartzCore compile ses pipelines depuis l'AIR.
- **Réponses sûres** pour les méthodes que le balayage de `metal_drv_test` signalait encore comme stubs d'Apple, qui font un abort :
  - ensembles de résidence (`use/add/removeResidencySet(s)`) : rien à faire, tout est résident ;
  - `setResponsibleTaskIDs:count:` et `encodeConditionalAbortEvent:` : rien à faire ;
  - `insertSplit` et `barrierAfterQueueStages:beforeStages:` : rien à faire, une seule file qui exécute dans l'ordre ;
  - `setVertexBuffer:offset:attributeStride:atIndex:` : renvoyé vers `setVertexBuffer:offset:atIndex:` ;
  - `supportsPlacementSparse` répond NO, `sparseBufferTier` répond 0 ;
  - `newTextureWithDescriptor:iosurface:plane:slice:` : la tranche 0 est renvoyée vers `…plane:`, les autres tranches sont refusées ;
  - `gpuResourceID`, `uniqueIdentifier` et `resourceIndex` de l'état de profondeur.

  Chaque réponse est journalisée la première fois (`B2L: … (answered by B2L, B1L-0081)`).

**Un plug-in par ensemble :** le bundle s'appelle `B2LMetalv37`. Le noyau ne remplace jamais un fichier publié, et le plug-in est lié au pilote de son propre ensemble ; `B2LMetalV4` pointait vers le pilote v36.

**Noyau :** `MetalPluginName` vaut `B2LMetalv37`, la publication passe à v37, et le retour automatique est inchangé.

## Installé

- **SYSTEM :** 5 blocs réécrits, en 92 s.
- **BOOT :** `out/b1l-metal-0081/boot.img`, relecture `b9dc3b9340e2ee8401af2084769dbef14344162bd4ef41c1eabe1af0f6e4ed62` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Outil : suivi des builds

- `b1l-tools/b1l-build.sh NNNN SET [--mesa] [--no-libs]` enchaîne Mesa, les bibliothèques, le noyau, l'image BOOT, le flash SYSTEM et le flash BOOT. Il note chaque étape dans `~/Developer/s10-port/progress/NNNN.status`.
- `b1l-tools/watch-build.sh` affiche cet état en direct, toutes les 2 s : étapes cochées, durées, et ce que fait l'étape en cours (`[n/m]` de Mesa, fichier compilé, étape du flash SYSTEM, gardes et relecture du BOOT). Il suit tout seul le build suivant.
- Il tourne dans l'onglet de terminal « suivi build/flash ». À utiliser à partir de B1L-0082.

## Résultat (`out/logs/0081-boot1.txt`) : QuartzCore compile ses pipelines, mais certains sont refusés

- **backboardd va beaucoup plus loin :**
  - `targetDeviceArchitecture answers cpuType 'B2L'` ;
  - file de commandes créée ;
  - QuartzCore compile ses pipelines de rendu (`com.apple.coreanimation.P0071Xm_A2Xghfc`, `P0081Xm_A2Xghfc`, `P2C16Xm_TbriA2Xhf_Isrc`…) et y décrit ses formats de cibles (81 = BGRA8, 115 = RGBA16Float…).
- **Refus :** 4 pipelines sont refusés par le traducteur AIR, toujours pour la même raison : `interface: an inline sampler whose state is not a constant`.
- **Puis backboardd sort** avec la raison 13, sous-code 4 : CoreAnimation s'arrête de lui-même faute de pipeline. Ça arrive deux fois (pid 62, puis 126). Au 3ᵉ démarrage, le retour automatique au CPU s'enclenche, ce qui explique que le propriétaire a vu l'interface habituelle.
- **Cause :** les shaders de QuartzCore d'iOS 26 déclarent leurs samplers « inline » dans la forme à deux mots : l'état, puis le décalage de LOD. Le traducteur de Scion ne lisait que la forme à un mot, car aucun module de son corpus n'utilisait l'autre.

# B1L-0256 — tas Metal (MTLHeap) pour RenderBox, qui dessine les icônes

## Cause (journaux iOS du démarrage 0254)

- Le style d'icône arrive enfin (`[IconStyle] … populated`), mais iconservicesagent journalise 8 fois
  `[com.apple.renderbox:error] unable to create heap: 0` (niveau Fault). Notre `-newHeapWithDescriptor:` répondait
  toujours `nil`, et `heapTextureSizeAndAlignWithDescriptor:` répondait 0.
- Ce que RenderBox utilise (lu dans ses `__objc_selrefs` et à 0x196ac4618, cache 23G90) : `setSize:`,
  `setHazardTrackingMode:2`, type automatique laissé par défaut, puis `newTextureWithDescriptor:`, `size`, `usedSize`,
  `type`, `storageMode`, `cpuCacheMode`, `setPurgeableState:` et `makeAliasable`.

## Changement (`userspace/metal/scion/hakiosmetal_air.m`, sauvegarde `.before-0256`) ; set **v121**

- Nouvelle classe `HakiosMetalHeap` (tas automatique) : chaque ressource est une allocation normale du device. Le tas
  compte ce qu'il a promis par rapport à sa taille (`nil` s'il n'y a plus de place, comme le veut Metal) et récupère
  la part d'une ressource quand elle est libérée (objet associé). Les tas « placement » (avec offsets) sont refusés.
- `heapTextureSizeAndAlignWithDescriptor:` : estimation (octets par texel × mips × couches × échantillons), arrondie à
  4 Kio ; `heapBufferSizeAndAlignWithLength:options:` : longueur arrondie à 4 Kio.
- Raccourci assumé : `makeAliasable` ne libère rien (la mémoire ne revient qu'à la libération de la ressource).

## Installé

- SYSTEM-OK, BOOT `233447fcd9c966a6dfc592ac1127567ce8a50c275eba0b9d218e4ae5f87a2700`, BOOT-OK ; `HakiosMetalHeap` présent dans
  `build-metal/b2l-metal-v121/libhakiosmetal.dylib`. Téléphone redémarré vers iOS.

## Résultat

- Testé : icônes toujours vides, apps toujours bloquées. Plus aucune ligne `unable to create heap` (le tas est accepté).
  Cause suivante : le pipeline RenderBox « custom effect » de SpringBoard est refusé par le traducteur (build-b1l-0257.md).

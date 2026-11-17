# B1L-0253 — le bloc deallocator des tampons sans copie est gardé (set v120)

## Cause (plantage d'iconservicesagent à IconRendering+0x55208, encore là en 0252)

Désassemblage d'IconRendering (cache `static-work/orig`) :
- `0x1b38b036c` : RenderBox remplit une texture SDF par `sdfTextureWithBufferAllocator:` ; l'allocateur d'IconRendering
  garde le seul MTLBuffer alloué (x20), puis `0x1b38aeb44` crée l'objet qui tient une **référence faible** vers ce tampon
  (champ +96) et sa propre texture (+88) faite par `newTextureWithDescriptor:offset:bytesPerRow:` sur un tampon
  `newBufferWithBytesNoCopy:length:options:deallocator:` posé sur les octets de ce tampon. Le bloc `deallocator`
  (`_Block_copy` à 0x1b38aed04, relâché par IconRendering juste après la création, 0x1b38aed40) est ce qui garde le
  tampon d'origine en vie.
- Notre pilote jetait ce bloc (`(void)deallocator`, B1L-0186). Le tampon d'origine était donc libéré aussitôt, la
  référence faible devenait nil, et `0x1b38b2208` (`brk #1`, déballage forcé) arrêtait iconservicesagent : icônes grises.
- Metal (documentation de `newBufferWithBytesNoCopy:length:options:deallocator:`) : le bloc est appelé quand le tampon
  est libéré.

## Changement (`hakiosmetal_air.m`)

`HakiosMetalBuffer` garde une copie du bloc (`hakiosSetDeallocator:length:`) et l'appelle dans `dealloc` avec le
pointeur et la longueur de l'appelant. Le reste du « sans copie » est inchangé (copie tenue à jour, B1L-0236). Set v119 → v120.

## Installé

- SYSTEM-OK, BOOT `f5035bc67e80d1c3d293c63d650eeae80e83ab29b7594f9485f2bd73f1e35d5e`, BOOT-OK ; `hakiosSetDeallocator` présent dans libhakiosmetal v120.

## Résultat

- **Plantage corrigé** : log `out/logs/0253-boot.txt`, plus aucune « exception 6 » d'iconservicesagent (seul MessagesBlastDoor plante, sans lien).
- **Icônes toujours vides** (propriétaire : « aucun changement »). iconservicesagent ne fait que 6 passes de rendu et 1 seul
  retour d'image (89x89) ; les images intermédiaires de SpringBoard (glyphe du téléphone 120x120 et 122x122) sont bien dessinées par le GPU.
- Suite : lecture des journaux iOS (USERDATA relue en lecture seule) pour voir pourquoi les icônes ne sont pas demandées ou pas livrées.

## Journaux iOS (USERDATA relue en lecture seule : `out/live/ios26-userdata-icons-253.img`, démarrage 0252 du 15 nov. 02:02)

- SpringBoard, 13 fois : `[com.apple.SpringBoard:IconStyle] [_postersStyleStateDidChange] nil poster style state; fetching`
  puis `still nil; bailing`. Le style d'icône d'iOS 26 dépend de l'état du fond d'écran (poster), qui n'arrive jamais.
- PosterBoard, dans le même temps : `failed to correct purgability … errno=-1` (90 fois, sur
  `/var/mobile/Library/Application Support/PRBPosterExtensionDataStore/…`), `cannot create a descriptorStoreCoordinator`,
  échec de transaction SQLite (Code=4), et pour l'écran verrouillé `initialPosters is nil`.
- Hypothèse (non prouvée) : PosterBoard ne peut pas poser l'attribut « purgeable » (fonction d'APFS) sur notre racine
  HFS+, n'enregistre donc aucun poster, SpringBoard n'a pas d'état de style et n'applique aucune image d'icône :
  d'où les carrés de verre vides et les widgets « Waiting… ». Ce n'est plus le pilote GPU (le rendu d'iconservicesagent
  ne plante plus).

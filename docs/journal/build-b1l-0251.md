# B1L-0251 — bufferBytesPerRow / bufferOffset des textures sur tampon (set v119)

## Plantage étudié (USERDATA propre, log `out/logs/userdata-250-boot1.txt`, signalé par la session noyau)

- iconservicesagent meurt sur EXC_BREAKPOINT à 0x1baf6a208 ; glissement 0x76b8000 → 0x1b38b2208 =
  **IconRendering+0x55208**, instruction `brk #1` (arrêt Swift). Même décalage déjà vu en 0235, 0239, 0249.
- Désassemblage (cache `static-work/orig`) : la fonction lit `height` (champ +80) et `bufferBytesPerRow` (champ +88,
  la texture que IconRendering crée par `newTextureWithDescriptor:offset:bytesPerRow:` sur un tampon
  `newBufferWithBytesNoCopy:…`), puis charge une référence **faible** (champ +96, `swift_unknownObjectWeakLoadStrong`)
  et s'arrête si elle est nil (déballage forcé). La référence faible est posée à la création de l'objet depuis son
  1er argument (un objet d'IconRendering, chemin `sdfTextureWithBufferAllocator:` probable). Non prouvé : pourquoi cet
  objet est déjà libéré (piste : ordre entre fin de rendu RenderBox / `setCompletedHandler:` et nos handlers Metal).
- Écart trouvé dans notre pilote : `bufferBytesPerRow` et `bufferOffset` répondaient toujours 0, même pour une texture
  faite sur un tampon (Metal répond la valeur donnée à la création).

## Changement (`hakiosmetal_air.m`)

`bufferOffset` / `bufferBytesPerRow` rendent `_liveOffset` / `_liveBytesPerRow` quand la texture est faite sur un tampon.
Set v118 → v119.

## Installé

- SYSTEM-OK, BOOT `f0efd1da772f2e66eae4d46d6a9e14e20d4932cfbf17d53228a55cfd6a18e50c`, BOOT-OK.

## Résultat

- En attente. Ce changement peut ne pas suffire : il corrige un écart réel, mais le lien avec le déballage nil n'est pas prouvé.

# Build B1L-0233 : comment RenderBox fabrique la fonction de ses effets personnalisés (diagnostic)

Date : 2026-11-14. Base : B1L-0232. Ensemble `b2l-metal-v104` (noyau reconstruit), Mesa inchangé, pilote recompilé.
Diagnostic seul : le rendu est le même qu'en 0232. Choisi par le propriétaire (« Oui, diagnostic »).

## Changement (`userspace/metal/scion/hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0233`)

- Pour chaque pipeline dont le shader appelle une fonction externe (`air.visible_function_reference`), 12 au plus,
  des lignes `B2L LINKED:` donnent :
  - ce que le descripteur lie (`vertexLinkedFunctions` / `fragmentLinkedFunctions` : `functions`,
    `binaryFunctions`, `privateFunctions`, `groups`) ;
  - le nom, la classe et la taille du bitcode de chaque fonction liée.
  Chaque module lié part au Mac en `B2LMOD l-<nom>` (`scripts/extract-b2lmod.py`).
- `newLibraryWithStitchedDescriptor:error:` : ligne `B2L STITCH:` (nombre de fonctions et de graphes, la réponse de
  Metal.framework ou son erreur, le texte de chaque graphe).
- `newLibraryWithSource:options:error:` : ligne `B2L SOURCE:` (taille, réponse ou erreur, 900 premiers caractères).
- Dans ces deux cas, le travail reste celui de Metal.framework (`_MTLDevice`) : le pilote ne fait que noter.

## Installé

- **Vérifié :** les chaînes `B2L LINKED`, `B2L STITCH` et `B2L SOURCE` sont dans `build-metal/b2l-metal-v104/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `94d0677b9847c061f5169f0233df8efb4c522fa6ca9337a7301a0e8eb3e6e5ac` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0233-boot.txt`, modules dans `out/b2lmod-0233/`)

- Pour chaque pipeline d'effet (SpringBoard et iconservicesagent), `fragmentLinkedFunctions` est un
  `MTLLinkedFunctionsInternal` avec **un seul `privateFunctions` : `custom_fn`**, de classe `_MTLFunctionInternal`, de
  type 5 (fonction visible), avec du bitcode AIR ordinaire. Sa taille dépend du pipeline : 4576, 4512 ou 4480 octets.
- Aucune ligne `B2L STITCH` ni `B2L SOURCE` pendant ce démarrage : pas d'assemblage ni de compilation de source à
  gérer. Il suffit de **lier un module AIR à un autre**.
- Le module `l-custom_fn` (4576 octets) a été récupéré. Il prend une position `float2`, des paramètres, un pointeur
  `args`, une texture et un sampler, et rend un `half4`.
- Suite : B1L-0234.

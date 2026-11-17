# Build B1L-0092 : mémoire partagée de taille donnée au dispatch

Date : 2026-11-08. Base : B1L-0091.

## Pourquoi

Le journal de 0091 montre **233 dispatchs refusés** de `compute_average_luma` (QuartzCore), le noyau qui mesure la luminosité moyenne de l'arrière-plan. Le Liquid Glass s'en sert pour choisir sa teinte (claire ou foncée). Ce noyau prend un tampon `threadgroup` dont la taille n'est donnée qu'au dispatch (`-setThreadgroupMemoryLength:atIndex:`), que le pilote ignorait.

## Changements

1. **`hakiosmetal_air.m` :** l'encodeur compute retient la taille donnée à l'index 0, la copie dans le travail enregistré, et la passe à la bibliothèque avant le dispatch (`rwa_set_threadgroup_memory`, cherché par `dlsym`, facultatif).
2. **`apply.py` (B1L-0092), `air_metal.c` :** `rwa_set_threadgroup_memory` ; le dispatch n'est plus refusé quand une taille est donnée ; l'état compute est reconstruit quand la taille change, avec `shared_size` porté à cette taille. Un noyau qui a aussi des tableaux partagés fixes reste refusé (même adresse de base 0).

Vérifié sur le Mac (`air2nir`) : `compute_average_luma` n'a pas de tableau partagé fixe (`load_shared`/`store_shared` à base 0, barrières de groupe).

Ensemble `b2l-metal-v48`, plug-in `B2LMetalv48`. Mesa toujours avec ses assertions signalées.

## Installé

- 1er essai : échec de compilation (`-Wmissing-prototypes` sur `rwa_set_threadgroup_memory`), rien d'écrit ; prototype ajouté dans le patch.
- **Vérifié :** `_rwa_set_threadgroup_memory` exporté par `libhakiosair.dylib`.
- **SYSTEM :** 3 passes écrites et relues, en 82 s (SYSTEM-OK).
- **BOOT :** `out/b1l-metal-0092/boot.img`, relecture `2cdf0289fa9465dc6e52d0b6bfd775b184a556d09c1702932557064ae4f18a87` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0091/boot.img`.

## Résultat (boot 1, photos `b1l-0092-centre-controle.jpg`, `b1l-0092-fichiers-onglets.jpg`)

- Démarrage sur le GPU.
- **Centre de contrôle :** tuiles toujours gris foncé opaques, fond flouté correct. Pas de changement visible de teinte.
- **App Fichiers, barre d'onglets :** la **bulle de sélection Liquid Glass s'affiche avec sa réfraction** (bord irisé, loupe sur l'icône) ; la barre elle-même est grise translucide.
- Journal pas encore lu.

## Journal (`out/logs/0092-boot1.txt`)

- 0 assertion, 0 plantage, 1 démarrage de backboardd, 16 images.
- **`compute_average_luma` passe le contrôle de la mémoire partagée** mais échoue encore 48 fois : « reads texture 0 and 1 texture was bound ». Un objet est bien lié à l'index 0, mais il n'a pas de texture de la bibliothèque dessous (`mesaTexture` nul).
- Toujours **116 fautes 0x51** ; 7 fautes MMU (dont 2 sur un tampon libéré) ; 9 attentes de plus de 2 s.
- Le propriétaire note que la Calculatrice est plus rapide qu'en rendu CPU (touches Liquid Glass).

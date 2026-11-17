# Build B1L-0104 : gros tampons GPU, vues de texture, bascule sans boucle active

Date : 2026-11-08. Base : B1L-0103.

## Changements

1. **Noyau, `B2LMaliService.cpp` (`createBO`)** : les tampons GPU de 1 Mo et plus sont alloués **par pages** (`IOBufferMemoryDescriptor::inTaskWithPhysicalMask`, adresses sur 40 bits), et non plus dans le tas noyau des données partagées, où ceux de 9 à 36 Mo échouaient. Sinon, retour à l'ancien chemin.
2. **Vues de texture** :
   - `apply.py` B1L-0104 (`air_metal.c`) : `rwa_texture_view`, une seconde `rwa_texture` sur la même `pipe_resource` (une référence de plus), lue dans un autre format de même taille de bloc par sa vue d'échantillonnage (`b2l_view_format`) ;
   - `hakiosmetal_air.m` : `-newTextureViewWithPixelFormat:` (et la forme avec type, niveaux, tranches) rend une vraie texture quand c'est la texture entière, du même type. Sinon `nil`, dit une fois.
3. **Noyau, `B2LFramebuffer.cpp`** : l'attente de la prise en compte d'une bascule dort (`IOSleep(1)`, 50 ms au plus) au lieu de tourner (`IODelay`).

Ensemble `b2l-metal-v60`, Mesa recompilé.

## Installé

- **Vérifié :** `rwa_texture_view` exporté par `libhakiosair.dylib`.
- **SYSTEM :** SYSTEM-OK en 74 s. **BOOT :** relecture `2ba6b20c77e9593ee29221fec78f8ce68f27ca65bf589863a8fb8bc689d93f8c` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0103/boot.img`.

## Résultat (photos `b1l-0104-centre-controle.jpg`, `b1l-0104-accueil.jpg`, `b1l-0104-widgets.jpg` ; journal `out/logs/0104-boot.txt`)

- Propriétaire : des icônes ne s'affichent toujours pas correctement (Horloge, Calendrier) ; Liquid Glass toujours cassé ; dock et widgets noirs ; **backboardd plante à l'ouverture de la Calculatrice** (deux fois) ; toujours lent.
- Journal : **0 tampon GPU refusé, 0 échec de `compute_average_luma`, 0 faute GPU (ni 0x51 ni MMU)**. 4 démarrages de backboardd.
- **Plantage 1** (symbolisé avec `static-work/orig`, glissement `0xc8dc000`) : `objc_msgSend` sur un pointeur invalide, dans **`-[_MTLCommandBuffer commitAndWaitUntilSubmitted]`**, appelé par `CA::OGL::MetalContext::flush` ← `IOMFBDisplay::finish_update` ← `Server::render_for_time`. La méthode d'Apple lit un état que notre sous-classe ne pose jamais.
- **Plantage 2** : `abort` via `____forwarding___` → `-[NSObject doesNotRecognizeSelector:]` : un message qu'un objet ne comprend pas (lequel : la pile est coupée).
- Outils écrits (scratchpad de la session) : `dscsym.py` (glissement et image d'une adresse à partir d'une pile) et `dscname.py` (symbole local le plus proche, depuis `dyld_shared_cache_arm64e.symbols`).

# Build B1L-0241 : app « Test Metal » à l'écran

Date : 2026-11-14. Base : B1L-0240. Ensemble `b2l-metal-v112`. Demande du propriétaire : une app qui affiche à
l'écran des tests Metal, de tout ce qu'une app pourrait attendre, installée de la façon la plus rapide.

## Installation (la plus rapide)

Le noyau insère déjà `libb2l-metal-app.dylib` dans l'app **Horloge** (B1L-0079, `b2l_exec_graphics_diag.h`, coupe :
boot-arg `b2l_metal_app=0`). Cette bibliothèque est maintenant `userspace/metal/b2l_metal_suite.m` (au lieu de la
démo de triangles `b2l_metal_app.m`, gardée). Aucun paquet d'app, aucune signature, aucun changement du noyau hors
ensemble. Ouvrir Horloge → écran « Test Metal » par-dessus l'app ; « Fermer » le retire, « Relancer » refait les tests.
Chaque ligne va aussi au journal du noyau (`b2l-mtest:`).

## Tests (API Metal publique uniquement ; code écrit pour le port, rien de copié)

- **Appareil** : `MTLCreateSystemDefaultDevice` (celui qu'une app reçoit ; sinon la suite continue avec l'appareil du
  pilote fait à la main), `MTLCopyAllDevices`, propriétés, `supportsFamily` (Apple1–9, Common1–3, Metal3), MSAA 4x, file.
- **Shaders** : `newLibraryWithSource:` (compilateur d'Apple sur le téléphone ; le Mac n'a pas d'outil Metal), forme asynchrone, `newDefaultLibrary`.
- **Tampons** : `newBufferWithBytes`, 64 Mo, blit vers privé et retour, `fillBuffer`, `newBufferWithBytesNoCopy` écrit par le GPU.
- **Calcul** : 1 M additions, mémoire threadgroup + barrière + atomique.
- **Textures** : écrire/relire 8 formats, `generateMipmaps`, blit tampon → texture.
- **Dessin** : effacement, triangle (vertex descriptor), indexé + instances, mélange alpha, viewport + scissor,
  profondeur, échantillonnage nearest/linear, MSAA 4x + résolution, `setFragmentBytes` vers RGBA16F, compute qui écrit
  une texture, dessin dans une IOSurface lue par le CPU, dessin dans une texture posée sur la mémoire de l'app (cas des icônes RenderBox).
- **Synchro** : handlers scheduled/completed, `MTLSharedEvent`, 200 command buffers à la suite.
- **Écran** : `CAMetalLayer` animé (carré en haut à droite), images/s sur 180 images.

## Installé

- **Vérifié :** « Test Metal » dans `build-metal/b2l-metal-v112/libb2l-metal-app.dylib` (compilé sans avertissement sur le Mac).
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `c005535a2dfc69a139df631800f93fac513fd2b1d5c3762c0959d1110fea9f0b` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat

- Pas testé : Horloge met ~30 s à ouvrir sa fenêtre (et Calendrier aussi, d'après le propriétaire). Hôte changé en B1L-0242.

## Retour en arrière

- `scripts/build-metal-scion.sh.before-0241` (démo de triangles de 0079).

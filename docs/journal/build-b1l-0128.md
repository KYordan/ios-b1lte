# Build B1L-0128 : d'où vient l'adresse fautive, tailles d'attachements, photo en RGBA

Date : 2026-11-08. Base : B1L-0127 (vues partielles toujours actives, choix du propriétaire : décodage direct). Ensemble `b2l-metal-v77`, Mesa recompilé.

## Changements

1. **Noyau, recherche de l'adresse fautive** (`B2LMaliService.cpp` `hakios_mali_scan`, appelé depuis `b2l_mali_core.c`) : à chaque faute, les tampons du job fautif (`last_seq` = son numéro, impossibles à libérer tant qu'il n'a pas fini) sont parcourus. Chaque mot de 64 bits qui pointe au plus 1 Mo sous l'adresse fautive est dit (`B1L-0128: job … buffer … holds … at byte …; around: …`) : on saura quel descripteur porte la mauvaise adresse. Registre des tables de tampons des clients (8 au plus).
2. **Attachements de tailles différentes** (`apply.py` B1L-0128) : Metal les permet, la zone de rendu est la plus petite. 0127 en refusait 170 (l'attachement 1 faisait toujours le double de l'attachement 0) ; le framebuffer prend maintenant la plus petite taille.
3. **Trace des attachements** (`B2L ATT: …`, 16 premiers) : indice, niveau, tranche, taille, nombre de niveaux, type et classe de la texture, pour savoir si l'attachement 1 a un niveau non nul.
4. **Fond d'écran** (`b2l-springboard.m` **v5**) : la photo n'apparaissait pas en 0125 (noir). Laissée en JPEG, iOS donne au serveur de rendu une IOSurface YCbCr à plusieurs plans, que le pilote Mali ne sait pas importer. Elle est maintenant redessinée une fois dans un bitmap RGBA 8 bits à la taille de l'écran (remplissage, recadrage haut et bas), comme le fond généré. Nouveau nom v5 (la vérification du fichier v4 échouait en 0126 et 0127, erreur 5).

## Installé

- **Vérifié :** noyau (`B1L-0128`, `springboard-v5`), patch Mesa appliqué, `B2L ATT` dans `libhakiosmetal.dylib`.
- **BOOT :** relecture `63a068f87a21bd02c24b514097a82c128fdde0d0f08f8d7955f181e0d89b92b4` (BOOT-OK). Redémarré vers le système.
- **Retour en arrière stable :** `out/b1l-metal-0126/boot.img` avec SYSTEM v75 (vues coupées, mais fond v4 noir).

## Résultat (photos du propriétaire)

- **« Ce build marche très bien. »** Photo du propriétaire en fond d'écran ; **Liquid Glass rendu** (Centre de contrôle, dock, dossiers du tiroir d'apps).
- Restent : le haut lumineux du fond d'écran rendu tout blanc, des icônes absentes dans le Centre de contrôle, le dock qui clignote parfois, Horloge et Calendrier incomplets, widgets blancs.

## Gardé pour être reflashé

`bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128` (téléphone en TWRP) : SYSTEM (ensemble `b2l-metal-v77`) puis BOOT, avec les gardes habituelles, puis redémarrage. Copie figée dans `~/Developer/s10-port/saved-builds/0128/` (BOOT sha256 `63a068f87a21bd02c24b514097a82c128fdde0d0f08f8d7955f181e0d89b92b4`).

# B1L-0259 — un verrou pour le contexte Gallium (SpringBoard plantait en boucle, bloqué sur le logo)

## Cause (log noyau 0258)

- SpringBoard plante 3 fois (pids 28, 236, 291), puis « MACH Reboot » : le démarrage reste sur la pomme.
- pid 236 : `panfrost_draw_vbo+0xff0` (v7) lit `ctx->prog[PIPE_SHADER_VERTEX]` (ctx+0xe508) = NULL, appelé par `rwa_render_draw`.
- pid 291 : `jm_launch_draw_v7+0x3c` lit le même champ = NULL, appelé par `blitter_draw` ← `do_blits`.
- Les deux pointeurs de pile (0x101b7…, 0x112b8…) sont ceux de fils secondaires, pas du fil principal.
  En 0257, SpringBoard avait déjà planté 3 fois dans notre bibliothèque, à des endroits différents et aussi sur des fils secondaires.
- Lu dans le code : la bibliothèque Mesa n'a **qu'un** `pipe_context` par appareil, et un contexte Gallium n'est pas thread-safe.
  `hakiosmetal_air.m` l'appelait sans aucun verrou depuis tous les fils : `-commit` (dessins), création et libération de
  pipelines, textures, échantillonneurs, etc. `rwa_render_destroy` fait `bind_vs_state(NULL)` (et
  `panfrost_bind_shader_state` remet `prog[type] = NULL`) pendant qu'un autre fil dessine : c'est le NULL lu.
  Depuis 0257, l'effet personnalisé se compile, donc SpringBoard dessine et crée davantage de pipelines en parallèle.

## Changement (`userspace/metal/scion/hakiosmetal_air.m`, sauvegarde `.before-0259`)

- Un mutex récursif global `b2l_mesa_mu`. La macro `B2L_MESA_SCOPE()` le prend jusqu'à la fin de la méthode. Elle est placée en tête
  des 31 méthodes qui appellent la bibliothèque Mesa : créations, `dealloc`, `replaceRegion`, `getBytes`, `runDraw`, `runBlit`, etc.
- `-commit` le prend avant la boucle de travail et le rend à `done:`, avant le rendu du créneau et les gestionnaires.
- `-waitUntilCompleted` : seul `mesa.finish` est verrouillé. L'attente de la barrière est au niveau de l'écran et reste libre.
- Tas : la ressource refusée est libérée hors du `@synchronized` du tas, pour éviter un ordre de verrous inversé.
- Raccourci noté : un seul verrou pour tout, donc une compilation bloque les dessins. À séparer pour les 40 i/s.
- Set v122 → v123. Mesa est inchangé.

## Risques

- Interblocage si une méthode qui tient un `@synchronized` attend le verrou, pendant qu'un fil qui tient le verrou attend ce même `@synchronized`.
  Cas vérifiés : le tas, corrigé ; le tampon de commandes, où le fil de complétion ne l'attend pas sous le verrou.
- Fluidité : les fils sont désormais sérialisés.

## Installé

- SYSTEM-OK, BOOT `04b02d7ea3e14ee1da1c1f63897bde80fad8ed17690ef83568eee8b120681482`, BOOT-OK ; `libhakiosmetal.dylib` v123 importe `pthread_mutex_lock` (v122 non).

## Résultat

- Testé (photos du propriétaire) : **le téléphone démarre et les apps s'ouvrent**. La plupart des icônes sont là, mais certains pictogrammes sont déformés
  (Fichiers, Livres, Plans, Contacts) et d'autres restent vides ou gris (Météo, Calendrier, Photos, News, Santé, Réglages, Traduire, dossier Utilitaires).
  Le haut du fond d'écran est beige, avec un bord jaune ondulé. Les widgets affichent « Waiting… ». Calendrier et Horloge ont le même problème qu'avant.

## Analyse après test (log noyau `out/logs/0259-boot.txt`, journaux iOS de USERDATA `ios26-userdata-icons-259.img`)

- Aucun plantage, aucun refus de pipeline : les icônes fausses viennent d'un rendu faux, pas d'une erreur.
- Les images de diagnostic `B2LMOD l-img-*` sont perdues dans le log noyau (anneau écrasé), mais **complètes dans les journaux iOS**
  (le noyau y est copié) : 37 reconstruites (`grep -o "B2LMOD .*"` du `log show`, puis extract-b2lmod.py et b2limg-to-png.py,
  après avoir retiré le suffixe de taille ajouté en 0257).
- Ce qu'on voit : les calques d'icônes sont dessinés (combiné Téléphone, dossier Fichiers, disque Safari). Les masques R8 ont une
  trame de points régulière, et les cartes RGB10A2 (format 90) sont très bruitées. Rien ne prouve encore lequel des shaders est faux.
- ImageIO de SpringBoard : `decodeRGBXFromTwiddled … tempLinearBufferSize (0) < required sliceBytes (4377600) for blockDim 360x760` (5 fois),
  probablement l'instantané du fond d'écran.

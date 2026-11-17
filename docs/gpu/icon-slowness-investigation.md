# Enquête : icônes cassées et perte de vitesse (2026-11-15, après B1L-0268)

Étape 1 (investigation) et étape 2 (plan). **Rien n'a été modifié pour cette enquête.** Sources : sauvegardes `.before-NNNN`
du pilote et de Mesa (la série icônes n'est pas dans git : le dernier commit de `beyond2lte-ios` couvre 0001–0213), logs noyau
`out/logs/0242…0267-boot.txt`, documents `build-b1l-02xx.md`.

## 0. En bref

1. **Le chemin GPU lui-même n'a pas ralenti.** Le même test plein écran (`CAMetalLayer`, app Test Metal) donne 10,8 i/s en 0242,
   10,9 en 0247 et en 0264, puis 9,5 en 0267. Ce qui a changé, c'est la **quantité de travail** demandée au GPU et au processeur.
2. Avant la série icônes, les pipelines RenderBox des icônes étaient **refusés**. Rien n'était dessiné, donc c'était rapide, mais
   les icônes restaient vides. Depuis 0257, ils se compilent, et un seul pipeline prend **~10 s** dans iconservicesagent : 4 créations
   en 39,6 s, dont une de 18,3 s. Il n'existe **aucun cache de shaders compilés**, ni sur disque ni en mémoire.
3. Depuis 0248, **3 à 4 processus de plus** utilisent le GPU : cameracaptured, MercuryPosterExtension, SBRendererService et
   WidgetRenderer. cameracaptured à lui seul passe 31,9 s à compiler 34 pipelines. Le noyau exécute les travaux GPU un par un pour tous.
4. Des **diagnostics coûteux** sont restés allumés sur les chemins chauds :
   - les images envoyées au log (`B2LMOD l-img`, 14 520 lignes en 0267, avec des pauses de 2 ms sous le verrou), déclenchées
     justement aux 256e et 4096e dessins, donc pendant la régénération ;
   - une recherche de chaîne à chaque passe (`B2L PASS`).
5. **Le cache d'icônes sur disque est juste.** C'est prouvé en 0262 : 82 icônes relues, Horloge et Calendrier compris. La casse
   vient du **dessin en direct** par SpringBoard et RenderBox, c'est-à-dire de notre pilote ou du traducteur. Le projet n'a pas de
   « code icônes » à lui : tous les correctifs de la série sont dans le pilote Metal et dans Mesa.
6. Le pilote **émule par des copies** ce qu'un GPU Apple fait sans copie : textures posées sur un tampon ou sur une IOSurface.
   Hors des points de copie, ces copies peuvent être périmées. C'est la cause la plus probable du schéma « juste au repos, cassé au
   mouvement, juste après retour sur la page », mais elle reste **à prouver** (section 4).

## 1. La série « icônes » build par build

| Build | Changement | Vitesse | Icônes | Verdict |
|---|---|---|---|---|
| 0248 | GPU ouvert à tous les processus | **plus lent** : +4 processus sur le GPU | aucun effet | revenir à la liste (boot-arg `b2l_gpu_all=0`) |
| 0251/0253 | textures sur tampon ; plantage d'iconservicesagent corrigé | neutre | nécessaire | garder |
| 0256 | tas Metal (`MTLHeap`) | neutre | nécessaire (sinon RenderBox ne dessine rien) | garder |
| 0257 | pipelines « custom effect » acceptés | **plus lent** : pipelines de ~10 s | les pictogrammes apparaissent | garder, mais avec un cache de shaders |
| 0258 | mémoire purgeable reprise par le noyau | non mesuré (purge jamais vue déclenchée) | aucun | à surveiller |
| 0259 | verrou global du contexte Gallium | **plus lent** : les fils sont sérialisés | nécessaire (sinon SpringBoard plante en boucle) | garder, en le réduisant (0268) |
| 0260/0262/0265/0267 | diagnostics (`B2L DRAW`, images, `B2L LOCK`, `B2L HOLD`) | **plus lent** : images sous le verrou | — | couper sauf les compteurs |
| 0261 | reprise de primitive 0xFFFF | neutre | juste selon la spécification de Metal | garder |
| 0263 | tampons en mappage persistant | neutre | aucun effet visible | garder (contrat Gallium) |
| 0264 | app Test Metal injectée dans Dictaphone par défaut | seulement quand Dictaphone s'ouvre | — | couper par défaut |
| 0266 | position du fragment +0,5 | neutre | aucun effet visible, mais exigé par Metal et vérifié par test | garder |
| 0268 | traduction des shaders hors du verrou | à mesurer | — | garder si stable |

**Retirer le « patch icônes » en bloc ramènerait la vitesse, mais aussi les icônes vides.** Les parties inutiles et coûteuses sont
0248 (trop de processus), les diagnostics et l'injection de 0264. Les correctifs de fond (0257, 0259, 0261, 0263, 0266) sont exigés
par la spécification ou par la stabilité.

## 2. Comment Metal est utilisé ici

- **Appareil** : un plug-in (`b2l_metal_plugin.m`) fait répondre `MTLCreateSystemDefaultDevice()` par notre pilote
  `libhakiosmetal` (`hakiosmetal_air.m`, environ 8 000 lignes, d'après Scion, MIT). Un `rwa_device` Mesa et **un seul**
  `pipe_context` par processus, protégés par un verrou unique (0259).
- **Files et command buffers** : `HakiosMetalCommandQueue` / `HakiosMetalCommandBuffer`. Les encodeurs **enregistrent** le travail
  (`HakiosMetalWork`). `-commit` le rejoue dans Gallium sous le verrou, puis vide le contexte. Le noyau (`B2LMaliService`) exécute
  les chaînes de jobs **une à la fois** pour tout le système. La fin est attendue par une barrière (fence). Ensuite viennent les
  écritures retour, les événements et les gestionnaires de fin.
- **Tampons** : objets panfrost en mappage persistant (0263). « Sans copie » (`NoCopy`) veut dire, ici, une copie à l'entrée et à la sortie.
- **Textures** :
  - toutes en mode **Shared**, sauf MSAA ;
  - un tas (`MTLHeap`) n'est qu'un compteur : chaque ressource est une allocation à part, et `makeAliasable` ne fait rien ;
  - textures posées sur un tampon, ou sur une IOSurface dont les lignes ne font pas un multiple de 64 octets : **copie privée**
    rafraîchie à chaque `setFragmentTexture` (`hakiosRefreshLiveCopy`, copie par le processeur sous le verrou), et
    **réécrite dans le tampon** après le GPU (`hakiosWriteBack`, lecture du GPU puis copie).
- **Shaders** : les `.metallib` d'Apple (AIR, bitcode LLVM) sont traduits en NIR (`air_to_nir`), puis compilés par le compilateur
  Bifrost de panfrost, **à chaque création de pipeline et dans chaque processus**. Les fonctions liées (`custom_fn`) sont relues à
  chaque fois.
- **Cache** : aucun cache de pipelines. Sur iPhone, les binaires compilés sont mis en cache par le système. Ici, chaque démarrage
  et chaque processus retraduit et recompile.
- **`MTLFence`** : `newFence` répond nil. Ce n'est pas gênant tant que tout s'exécute dans l'ordre de soumission.

## 3. Où est partie la vitesse (mesuré)

| Coût | Mesure | Build |
|---|---|---|
| Création de pipeline, iconservicesagent | 4 créations, 39,6 s, **~9,9 s chacune** (une de 18,3 s) | 0267 |
| Création de pipeline, cameracaptured | 34 créations, 31,9 s (~0,94 s chacune) | 0267 |
| Création de pipeline, backboardd / SpringBoard | 120 en 8,1 s / 91 en 4,6 s | 0267 |
| `-commit` sous le verrou, backboardd | 10,3 s cumulées, 2,0 s d'un seul tenant | 0267 |
| Copies vivantes / écritures retour | iconservicesagent 559 ms ; SpringBoard 2,1 s | 0267 |
| Processus sur le GPU | 5 en 0247 → 8 en 0267 | logs |
| Images de diagnostic dans le log | 14 520 lignes (base64, jusqu'à 4 Mo par processus, pauses de 2 ms, sous le verrou) | 0267 |
| Mémoire GPU de backboardd | 711 Mo réservés en 750 objets, dont 343 Mo marqués purgeables | 0267 |

Les « grosses lenteurs ponctuelles au moment où les icônes se régénèrent » s'expliquent par trois mesures :
- la création d'un pipeline d'icône prend ~10 s ;
- jusqu'à 0268, le processus entier restait bloqué pendant ce temps ;
- les images de diagnostic, déclenchées pendant la même phase, s'y ajoutent.

## 4. Pourquoi les icônes sont redessinées alors qu'elles existent déjà justes

- **Ce n'est pas le cache d'icônes.** Le fichier `com.apple.IconsCache` est juste (0262).
- **C'est iOS 26 qui recompose les icônes en direct.** SpringBoard pose le pictogramme et l'effet de verre avec RenderBox
  (pipelines `custom_effect` / `uber`), et redessine les mêmes tailles des centaines de fois : les dumps des 256e et 4096e dessins
  existent parce que ces compteurs sont atteints. Ce comportement vient d'iOS, pas d'un bug de cache chez nous. On ne peut pas
  l'éviter sans toucher à SpringBoard ; on peut seulement le rendre juste et rapide.
- **Ce qui, chez nous, peut rendre ce redessin faux par moments.** Classé du plus probable au moins probable :
  1. **Copies vivantes périmées.** Sur un GPU Apple, une texture posée sur un tampon *est* le tampon. Ici, c'est une copie mise à jour
     à deux moments seulement :
     - à l'encodage du `setFragmentTexture`, alors que le dessin s'exécute plus tard, au `-commit` ;
     - après la fin du GPU.
     Une cible de rendu posée sur un tampon n'est **remplie qu'une fois, à sa création** (raccourci de 0235, noté dans le code). Si
     RenderBox réutilise ce tampon ou cette texture pour une autre icône, ou recharge son contenu (`loadAction` Load), l'ancienne image
     reste. Une vue reconstruite crée des textures neuves, donc des copies neuves : c'est cohérent avec « je quitte, je reviens, c'est juste ».
  2. **Lecture entre processus avant la fin de l'écriture.** Il n'est pas vérifié que backboardd attende la fin du dessin de
     SpringBoard dans une IOSurface avant de la lire. ios-pipa a dû ajouter des barrières par surface dans le noyau.
  3. **Shaders RenderBox encore faux dans un cas non couvert par les tests**, par exemple un dégradé de verre ou un masque de distance
     à une taille ou un format précis. Les tests unitaires passent (43 OK), mais ils ne rejouent pas les vrais pipelines de SpringBoard.
- Pistes de la liste demandée, avec leur état :

| Piste | État |
|---|---|
| Cache absent ou invalidé | cache d'icônes sur disque juste ; **cache de pipelines absent** (cause de lenteur, pas de casse) |
| Textures recréées à chaque image | non mesuré ; les copies vivantes sont rafraîchies à **chaque** liaison, même sans changement |
| BGRA/RGBA, alpha prémultiplié, sRGB | aucun indice : 8 formats et la passe R8/RG16F/BGRA8 passent les tests |
| Mipmaps | textures sur tampon refusées au-delà d'un niveau (règle de Metal), aucun refus vu |
| Pas de ligne / alignement | règle de 64 octets du Mali : c'est elle qui crée les copies vivantes (lignes de 480 ou 384 octets) |
| Synchronisation CPU/GPU | **piste principale** : copies prises à l'encodage, pas à l'exécution ; barrières entre processus non vérifiées |
| Textures purgées | 343 Mo marqués purgeables dans backboardd ; la reprise par le noyau (0258) n'a jamais été vue déclenchée |
| Mode de stockage | tout en Shared, ce qui est cohérent avec les copies ; pas d'indice de bug |

## 5. Centre de contrôle

D'après vos retours de 0262 et 0263, ses icônes sont **fausses aussi**. Elles sont dessinées dans SpringBoard (et chronod pour les
contrôles WidgetKit) par les mêmes chemins RenderBox et CoreAnimation, sans aucun code du projet entre les deux. Cela pointe vers
**le pilote et le traducteur**, pas vers un code d'icônes du projet (il n'y en a pas). Une photo du Centre de contrôle sur le build
actuel le confirmerait.

## 6. ANGLE, iOS Pipa, MoltenVK

- **ios-pipa** (port d'iOS par Leo Malik, licence MIT, clone local du 12 novembre) **n'utilise pas ANGLE**. Il tourne sur iOS 16, où
  CoreAnimation sait encore dessiner en OpenGL ES par `EAGLServer` ; il branche ce chemin sur le GPU Adreno (eagl.c, repris de
  HackiOS). Ses icônes sont dessinées par le processeur. **iOS 26 n'a plus EAGLServer** (B1L-0031) : CoreAnimation, RenderBox et
  le verre ne parlent plus que Metal.
- **ANGLE** traduit OpenGL ES vers Metal ou Vulkan. C'est l'inverse de notre besoin : iOS 26 *émet* du Metal, et le Mali *comprend*
  GLES ou Vulkan. Un pont « GL → Metal » n'apporterait rien, et un pont « GL comme ios-pipa » n'existe plus dans iOS 26.
  **Non recommandé.**
- **MoltenVK** traduit Vulkan vers Metal : même problème de sens. La variante utile serait **Metal → Vulkan sur panvk**, le pilote
  Vulkan de Mesa, présent pour Bifrost v6/v7 dans notre Mesa 25.0.7 (`src/panfrost/vulkan`, archs 6 et 7). Son niveau de maturité
  sur le G76 n'est pas vérifié.
  - Gains attendus : enregistrement de commandes par fil sans verrou global, barrières explicites, cache de pipelines sérialisable.
  - Coût : réécrire la couche `rwa_*` (air_metal.c, ~7 000 lignes) et une bonne part du pilote, soit plusieurs semaines.
  - **Pas maintenant.** À réévaluer si les étapes P1 à P4 ne suffisent pas.

## 7. Plan priorisé

Un build par étape, mesures avant et après : `B2L HOLD`/`B2L LOCK`, `CAMetalLayer` i/s, temps de création des pipelines et mémoire
(`mali mem`), plus votre retour à l'écran.

| # | Étape | Pourquoi (cause démontrée) | Gain attendu | Coût |
|---|---|---|---|---|
| P1 | Couper les diagnostics des chemins chauds (images `B2LMOD`, `B2L PASS`, `DRAW`, `TEX`, `LIVE`), garder les compteurs | 14 520 lignes et des pauses sous le verrou | fin des à-coups liés au log | 1 build |
| P2 | Revenir à 4 processus GPU (`b2l_gpu_all=0`) et couper l'injection Test Metal par défaut | +4 processus depuis 0248, 32 s de compilation pour cameracaptured | GPU et cœurs libérés pour SpringBoard et backboardd | 1 build (boot-arg) |
| P3 | Lire le résultat de 0268 (traduction ou compilation) puis **cache de shaders compilés** : mémoire par processus, puis disque partagé (clé = hachage du bitcode, des fonctions liées, des constantes, du format des cibles et de la version du pilote) | pipelines de ~10 s, recompilés à chaque démarrage et dans chaque processus | création en millisecondes après le 1er démarrage | 2 à 3 builds |
| P4 | Copies vivantes : ne copier que si la source a changé (`IOSurfaceGetSeed` pour les surfaces), copier **au moment de l'exécution** (au `-commit`), et rafraîchir les cibles de rendu chargées (`loadAction` Load) | copies à chaque liaison ; raccourci de 0235 | moins de copies ; corrige la piste n° 1 si elle est la bonne | 2 builds |
| P5 | Preuve de la casse : un diagnostic ciblé qui compare, pendant un mouvement, le contenu du tampon et celui de la copie au moment du dessin, et compte les cibles chargées sans rafraîchissement | aucune hypothèse de casse n'est encore prouvée | dit laquelle des trois pistes est la bonne | 1 build |
| P6 | Barrières d'écriture par surface dans le noyau (leçon ios-pipa), si P5 montre des lectures avant la fin | piste n° 2 | plus d'images à moitié dessinées | 2 builds |
| P7 | Optimisations ensuite : regroupement des dessins en passes, sous-allocation par command buffer (ios-pipa : de 200 µs à 13 µs par dessin), réserve de textures et de tampons, triple tampon du `CAMetalLayer`, DVFS | mesures de 0264/0267 | vers 40 i/s | plusieurs builds |

Ordre recommandé : **P1 et P2 d'abord** (retour rapide vers l'ancienne vitesse, sans perdre les icônes), puis P5 pour prouver la
cause de la casse **avant** de la corriger (P4 ou P6), et P3 en parallèle pour les lenteurs ponctuelles. Les processus retirés en P2
seront rouverts un par un, une fois P3 en place.

## 8. Ce que je ne peux pas vérifier sans l'appareil

- **Metal System Trace, GPU Frame Capture et signposts ne sont pas utilisables** : Instruments et Xcode ne peuvent pas se brancher
  sur ce port (pas de GPU Apple, pas de lien développeur). Les compteurs du pilote (`B2L HOLD`, `B2L LOCK`) et les temps de jobs
  du noyau les remplacent.
- Ce que voit l'écran : seules vos photos le disent.
- À fournir à chaque test :
  1. le build en TWRP après le test (je relève le log noyau moi-même) ;
  2. une photo de l'écran d'accueil **au repos** et une **pendant un défilement** ;
  3. une photo du **Centre de contrôle** ;
  4. l'heure approximative du moment où les icônes se cassent ;
  5. votre accord pour lire USERDATA en lecture seule si les journaux iOS sont utiles (lecture seule, rien d'écrit).

## 9. Comment fait hakios-arm64e-dada (revue du 2026-11-15)

Dépôt `superboyyy/hakios-arm64e-dada` (iOS 26 arm64e sur Xiaomi 15, Snapdragon 8 Elite, Adreno 830), lu sur GitHub sans rien
télécharger d'autre que deux correctifs texte (dossier scratchpad). Licences :
- noyau et userland : APSL 2.0 (HakiOS) ;
- outils : MIT ;
- correctifs GPU : licence de chaque projet, soit metal2vulkan en **GPL-2.0-or-later OR LGPL-3.0-or-later**, NativeMetal (UniXNU)
  en MIT et Mesa en MIT.

- **Leur chemin GPU** : Metal → **metal2vulkan** (steelbrain, en Rust : traduit l'AIR en SPIR-V et fournit l'API Metal sur Vulkan)
  → **Turnip** (le Vulkan de Mesa pour Adreno) → leur pilote noyau. Le plug-in Metal vient de UniXNU NativeMetal. Ils ont d'abord eu
  le même chemin que nous (HakiOS : air_metal.c + Gallium freedreno, `userland/tools/mesa/air`), puis sont passés à Vulkan (`gpu2/`).
- **Pourquoi on ne peut pas le reprendre tel quel** : metal2vulkan exige un appareil **Vulkan 1.2**. Dans notre Mesa 25.0.7, panvk
  (le Vulkan de Mesa pour le Mali) annonce **Vulkan 1.0** sur Bifrost v7 (`get_vk_version`), sans `descriptorIndexing` ni
  `shaderFloat16`, avec 4 jeux de descripteurs au plus. Il refuse même de s'ouvrir sur v6/v7 sans
  `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` (« not well-tested on v7 »). L'Adreno 830 a un Vulkan complet ; le Mali-G76 non.
  Une Mesa plus récente n'a pas été vérifiée : il faudrait la télécharger.
- **Ce qu'on peut reprendre** (idées seulement, sans copier de code) :
  1. **Texture posée directement sur le tampon** (leur `PaddedTargetsPolicy::Alias`), au lieu d'une copie privée (« shadow »).
     Chez nous, les 13 textures sur tampon du log 0267 ont toutes un pas de ligne et un décalage multiples de 64 octets
     (256, 384, 512, 832), donc compatibles avec le Mali. Pourtant `newTextureWithDescriptor:offset:bytesPerRow:` les copie toujours.
     Les poser sur le tampon supprimerait les copies vivantes, les écritures retour et les copies périmées (piste n° 1 de la casse).
     Leur précaution : vérifier que les écritures du rendu par tuiles ne débordent pas après la dernière ligne ou colonne
     (16×4 texels sur Turnip ; tuiles de 16×16 sur le Mali, à tester).
  2. Leurs correctifs de performance cités (UniXNU 0003, 0005, 0009–0015) : préchauffage du cache de pipelines, moins d'allocations
     par dessin, liaisons inchangées sautées. Ce sont nos étapes P3 et P7.
  3. La validation **pixel par pixel contre le GPU d'un Mac** (même shader AIR, deux GPU).

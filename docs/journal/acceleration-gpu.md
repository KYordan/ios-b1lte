# Accélération GPU d'iOS 26 sur le Galaxy S10 : vue d'ensemble

Mis à jour : 2026-11-15 (B1L-0268). Le suivi build par build reste dans `metal-progress.md` ; ce document donne la vue d'ensemble :
comment l'accélération marche, ce qui marche, ce qui manque par rapport à un iPhone, et dans quel ordre on avance.

## 1. La chaîne, de l'app au Mali

```
App / SpringBoard / backboardd (Metal, CoreAnimation, RenderBox)
   │  API Metal d'Apple (MTLDevice, command buffers, pipelines)
   ▼
libhakiosmetal.dylib  — pilote Metal (userspace/metal/scion/hakiosmetal_air.m, d'après Scion, licence MIT)
   │  traduit chaque objet Metal (tampon, texture, passe, dessin) en appels « rwa_* »
   ▼
libhakiosair.dylib    — Mesa 25.0.7 modifié (userspace/mesa/air-b2l/apply.py)
   │  air_to_nir : shaders Apple (AIR, LLVM) → NIR
   │  air_metal.c : état Metal → état Gallium (pipe_context)
   │  panfrost + compilateur Bifrost : NIR → code Mali, jobs du GPU
   ▼
Noyau XNU : HakiosMali (IOAcceleratorES, mémoire GPU, MMU, jobs)  →  Mali-G76 MP12 (~676 MHz)
```

- iOS charge notre plug-in comme un vrai pilote : `MTLCreateSystemDefaultDevice()` renvoie « Mali-G76 (Panfrost) (Hakios) ».
- Les shaders d'Apple ne sont pas recompilés à partir de leur source : on traduit l'AIR déjà compilé (les `.metallib` du système).
  `newLibraryWithSource` passe par le compilateur d'Apple sur le téléphone, puis par la même traduction.
- Un seul `pipe_context` par processus, protégé par un verrou global depuis B1L-0259.
- L'affichage final passe par backboardd (CoreAnimation sur Metal) puis par l'écran.

## 2. Ce qui marche (test Metal de Dictaphone, B1L-0267 : 43 OK, 1 ÉCHEC, 1 SAUTÉ)

| Domaine | État |
|---|---|
| Appareil, file de commandes, command buffers, handlers, `MTLSharedEvent` | OK |
| Tampons : partagés, privés, 64 Mo, sans copie (`NoCopy`), blit, remplissage | OK |
| Calcul (compute) : 1 M additions, threadgroup + atomiques, écriture de texture | OK |
| Textures : 8 formats, mipmaps, blit tampon → texture, échantillonnage nearest/linear | OK |
| Rendu : effacement, vertex descriptor, indexé + instances, mélange alpha, viewport/scissor, profondeur, MSAA 4x + résolution | OK |
| Dessin dans une IOSurface et dans la mémoire de l'app (chemin des icônes RenderBox) | OK |
| Dérivées `dfdx`/`dfdy`/`fwidth`, y compris dans un `if` | OK (B1L-0264) |
| Compilation depuis la source | OK, mais 5,2 s |
| Plusieurs cibles (R8, RG16F, 4 cibles), lecture de la cible (framebuffer fetch), centre du pixel | OK (B1L-0265/0266) |
| `CAMetalLayer` à l'écran | OK, **9,5 à 10,9 images/s** seulement |

## 3. Ce qui ne marche pas encore

- **Icônes qui se cassent dès qu'il y a du mouvement** (et reviennent parfois justes). Le demi-pixel (corrigé en 0266) et les cibles
  multiples passent les tests, mais pas l'écran d'accueil en mouvement. Causes probables, à vérifier une par une :
  1. iOS 26 redessine les icônes à chaque image pendant un mouvement (reflet du verre, RenderBox « en direct » dans SpringBoard),
     alors qu'au repos il réutilise l'image du cache. Ce chemin à chaque image est celui qui casse.
  2. Les « copies vivantes » des textures (`hakiosRefreshLiveCopy`, `hakiosWriteBack`) : copies par le processeur entre la mémoire de
     l'app et la texture du GPU, qui peuvent se croiser avec un dessin ou une lecture en cours.
  3. Ordre entre processus : le noyau exécute les travaux GPU un par un, dans l'ordre d'arrivée. Il n'est pas encore vérifié que
     backboardd attende la fin de l'écriture d'une surface par SpringBoard avant de la lire (ios-pipa a dû ajouter cette attente, section 8).
- **Centre de contrôle, Horloge, Calendrier** : ils utilisent les effets de RenderBox (flou, vibrancy, lecture de la cible) et
  les mêmes chemins « en direct » ; mêmes causes probables que ci-dessus, plus le flou (passes de réduction successives).
- Verre liquide (Liquid Glass) : blanc sur fond clair. Il dépend des mêmes passes RenderBox et des filtres (flou, distance).
- Widgets bloqués sur « Waiting… », haut de l'écran d'accueil beige à bord jaune.
- Annonces : `supportsFamily` ne répond que A1 et C1, et MSAA 4x n'est pas annoncé alors qu'il marche.

## 4. Écart avec un iPhone (fonctionnalités Metal)

Ce qu'un GPU Apple fournit et que les apps et le système supposent :

1. **Familles GPU** (`MTLGPUFamilyApple7+`, `Metal3`) : beaucoup d'apps et de frameworks choisissent leur chemin selon la famille.
   Il faut annoncer ce qui est vraiment tenu, pas plus.
2. **Lecture de la cible dans le shader** (programmable blending, `[[color(n)]]` en entrée) : natif sur les GPU Apple, très utilisé
   par RenderBox et CoreAnimation. Sur le Mali, panfrost le fait par lecture du tile buffer.
3. **Mémoire de tuile, imageblocks, mémoire sans stockage** (`MTLStorageModeMemoryless`) : à émuler.
4. **MSAA** annoncé (4x marche déjà), formats compressés (ASTC est natif sur le Mali), textures 3D/tableaux/cubemaps.
5. **Argument buffers, indirect command buffers, heaps, fences/events** : en partie faits (heaps, events), le reste à tester.
6. **Raytracing, mesh shaders** : absents du Mali-G76 ; on ne les annonce pas.
7. **Compilation rapide et cache de pipelines** : sur iPhone, les pipelines sont en cache ; ici chaque processus retraduit.

## 5. Optimisation (mémoire, rendu, images/s) : constats chiffrés

| Mesure | Valeur | Source |
|---|---|---|
| `CAMetalLayer` plein écran | 10,9 images/s (objectif 40) | B1L-0264 |
| 200 command buffers vides | 0,82 ms chacun | B1L-0264 |
| Mémoire GPU totale à 3 min | ~290 Mo (backboardd ~230 Mo) | B1L-0247 |
| Attente sur le verrou global | forte au démarrage (8 s attendues sur 5 s dans SpringBoard), faible ensuite | B1L-0262 |
| Verrou tenu par la création de pipelines | jusqu'à 18,3 s (iconservicesagent), 0,8 s (backboardd), 0,6 s (SpringBoard) | B1L-0267 |
| Verrou tenu par `-commit` dans backboardd | 10,3 s cumulées, 2,0 s d'un seul tenant | B1L-0267 |
| `newLibraryWithSource` | 5,2 s | B1L-0264 |

Pistes, dans l'ordre :

0. **Traduction des shaders hors du verrou** (B1L-0268), puis compilation panfrost hors du verrou si elle domine.
1. **Ne plus attendre le GPU sous le verrou** : `rwa_dispatch` (compute) termine chaque dispatch par une attente complète
   (`finish_or_say_why "dispatch"`) ; les relectures de texture et les écritures retour attendent aussi.
2. **Cache des traductions de shaders** sur disque, partagé entre processus (comme le cache de pipelines d'Apple).
3. **Copies évitées** : `newBufferWithBytesNoCopy` répond par une copie, et les textures sur tampon sont des « copies vivantes »
   recopiées à chaque usage.
4. **Fréquence et charge GPU** : DVFS proche du maximum, mesure du temps GPU réel par image.
5. **Mémoire** : libérer les BO jetables, éviter les doubles tampons de RenderBox.

## 6. Ordre de travail (décidé par le propriétaire, 2026-11-15)

1. Réparer les icônes : passes multi-cibles R8/RG16F, relecture de la cible, puis Horloge, Calendrier, Centre de contrôle, verre.
2. Fonctionnalités d'un GPU Metal d'iPhone (section 4), chacune vérifiée par l'app Test Metal de Dictaphone.
3. Optimisation : mémoire, rendu, images/s (section 5), mesurée à chaque build.

## 7. Outils de diagnostic

- **App Test Metal** : `userspace/metal/b2l_metal_suite.m`, injectée dans Dictaphone par le noyau (boot-arg `b2l_metal_app=0` pour couper).
  Résultats à l'écran et dans le log noyau (`b2l-mtest:`).
- Lignes du pilote dans le log noyau : `B2L PASS`, `B2L DRAW`, `B2L LOCK`, `B2L WRITEBACK`, `B2L BUFTEX`.
- Images dessinées par le GPU : `B2LMOD l-img-…` (log noyau ou journaux iOS de USERDATA), puis `scripts/extract-b2lmod.py`
  et `scripts/b2limg-to-png.py`.
- Traducteur sur le Mac : `air2nir [--ll|--list|--report] FICHIER [FONCTION]` (build-mesa/build-host).

## 8. Pourquoi l'accélération marche chez ios-pipa (revue du 2026-11-15)

ios-pipa (dépôt MIT, Leo Malik, clone local du 12 novembre) n'est pas une version plus avancée du même chemin :

- **Autre iOS, autre chemin** : son interface accélérée tourne sur iOS 16 (20H392). CoreAnimation y dessine en OpenGL ES par
  un pont `EAGLServer` (eagl.c, repris de HackiOS, MIT). **iOS 26 n'a plus EAGLServer** (B1L-0031, `gpu-plan.md` 4c) : CoreAnimation,
  RenderBox et le verre liquide n'ont plus que Metal. On ne peut pas reprendre ce pont.
- **Ses icônes sont dessinées par le processeur**, pas par le GPU : il n'a pas les shaders RenderBox d'iOS 26 à traduire.
- Leçons utiles pour nous, tirées de son `userland/metal/README.md` (pilote hakios-metal pour Adreno) :
  1. **Barrières d'écriture des surfaces suivies par le noyau** : l'affichage attend que le GPU ait fini d'écrire une surface avant de
     la lire. Sans cela, image à moitié dessinée ou ancienne (notre piste 3 de la section 3).
  2. **Dessins regroupés en une passe** : 200 dessins en 591 µs au lieu de 8 576 µs quand chaque dessin était une passe.
  3. **Sous-allocation par command buffer** des tables et des programmes : de ~200 µs à ~13 µs par dessin.
  4. Le Centre de contrôle (vibrancy) n'a marché qu'avec la **lecture de la cible** dans le shader : chez nous elle passe le test (0265).
  5. Constat chez lui aussi : « CoreAnimation dans backboardd est limité par le processeur » ; les i/s se gagnent d'abord en retirant
     le travail du processeur (copies, attentes) autour du GPU.
- Ordre retenu : 0268 (verrou), puis barrières d'écriture des surfaces entre processus, puis regroupement des dessins et
  sous-allocation, chacun mesuré par `B2L HOLD` et l'app Test Metal.

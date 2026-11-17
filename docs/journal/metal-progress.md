# Accélération GPU (Metal sur le Mali-G76) : où on en est

Mis à jour : 2026-11-14, B1L-0238 : plantage de SpringBoard dans le retour du dessin corrigé ; B1L-0237 : images dessinées par le GPU envoyées au Mac (diagnostic) ; B1L-0236 : le dessin revient dans la mémoire d'image de RenderBox (tampons sans copie) ; B1L-0235 : le dessin dans une texture sur tampon n'est plus écrasé ; B1L-0234 : fonctions liées d'un pipeline (`custom_fn` des effets de RenderBox) ; B1L-0233 : diagnostic des effets personnalisés de RenderBox ; B1L-0232 : format RGB10A2Unorm (icônes 122×122 de SpringBoard) ; B1L-0231 : texture posée sur un tampon (copie tenue à jour) ; B1L-0230 : nombre de samplers du shader corrigé (`fa16dhmha4` refusé par le GPU, 0x5A) ; B1L-0229 : taille nulle pour une texture non liée, plus de gel au démarrage mais pas d'icônes ; B1L-0228 : textures non liées acceptées, gel à l'écran verrouillé ; B1L-0227 : plus aucun shader refusé ; B1L-0226 : phi de pointeurs (tracés de RenderBox, testé) ; B1L-0225 : 3 pipelines refusés trouvés, cause du plantage 0188 prouvée sur le Mac ; B1L-0224 : modules des shaders refusés de RenderBox envoyés au Mac ; cache d'icônes correct, défauts dans le dessin en direct de SpringBoard ; 2026-11-12, B1L-0191 : retour à l'état de 0187 (dernier build sans problème signalé), correctifs 0188–0190 mis de côté ; B1L-0190 : `return` au milieu d'un shader abaissé, `select` de pointeurs rendu à QuartzCore (clignotement, en test) ; B1L-0189 : saut que Bifrost ne sait pas émettre refusé et nommé au lieu de planter (en test) ; B1L-0188 : le traducteur accepte les phi de structures et de pointeurs dans un même tampon (shaders de RenderBox, en test) ; B1L-0187 : journal du pilote ouvert à SpringBoard, chronod et iconservicesagent (diagnostic) ; B1L-0186 : `newBufferWithBytesNoCopy:` répond par une copie (RenderBox plantait SpringBoard, en test) ; B1L-0185 : gros patch, mémoire GPU recyclée remise à zéro et GPU ouvert à SpringBoard, chronod et iconservicesagent (en test) ; B1L-0136 : les copies d'IOSurface à lignes non alignées sont relues avant chaque dessin (instantanés d'icônes vivants) ; 0135 : inventaire des textures (téléversements et formats lus par chaque passe) ; 0134 : appels IOSurface de chronod ; 0133 : liste des images importées ; 0132 : images à lignes non alignées copiées ; 0131 : rechargement des cibles relues ; 0130 : inventaire ; 0129 : effacement par attachement (tracés vectoriels) ; 0128 : Liquid Glass et photo en fond d'écran (reflasher : `b1l-tools/flash-saved.sh 0128`) ; 0123 : vues partielles ; 0121 : textures du verre allouées ; 0120 : retour à 0115 ; 0115 : très fluide ; 0113 : où part le temps ; B1L-0112 : plantage à l'ouverture des apps réglé, encore lent (avant : B1L-0111 plantage inchangé ; 0110 + command buffers gardés 2 s + imagent/dasd bloqués ; avant : diagnostic de la vie des command buffers ; avant : retenue de 30 s ; avant : command buffers asynchrones, correctifs de plantage ; avant : gros tampons GPU, vues de texture, bascule sans boucle active ; avant : lignes de 64 octets pour SpringBoard ; contient 0102 : LDAPR de Mesa réécrits, chronométrage des appels noyau ; avant : profil des interruptions PAC ; contient 0100 vérification rapide et 0099 viewport, scissor, lignes de 64 octets ; avant : zéro copie avec les images de backboardd, texture RG8 pour la luminosité du verre ; avant : backboardd ne dessine plus dans la mémoire affichée ; nettoyage de cache une fois ; recherche du canal d'écran).

## Socle (fait avant Metal)

- [x] Pilote noyau du GPU (HakiosMali) : le Mali s'allume, exécute des jobs, a sa propre table de pages
- [x] Fréquence GPU réglée (environ 676 MHz) et protection thermique
- [x] Mesa (panfrost, compilateur Bifrost) compilé pour iOS 26 et relié au pilote noyau
- [x] Essai OpenGL ES : iOS a été affiché par le GPU (B1L-0041/42), mais plus lent et plus cassé que le CPU, donc abandonné au profit de Metal (gardé en secours, `b2l_gl=1`)
- [x] Mémoire : la RAM des banques basses est traitée comme de la DRAM, le framebuffer est accessible au GPU (B1L-0040/42)

## M1 : iOS voit un GPU Metal

- [x] Service noyau `IOAcceleratorES` qui annonce un plug-in Metal (B1L-0044)
- [x] Plug-in installé dans `/System/Library/Extensions` et chargé par Metal
- [x] `MTLCreateSystemDefaultDevice()` renvoie notre GPU (B1L-0045)
- [x] Plantages de Metal corrigés (`targetDeviceArchitecture`, B1L-0047)
- [x] Le test tourne hors de backboardd, donc l'interface n'est jamais touchée (B1L-0049, puis `weatherd`)

## M2 : mémoire et copies faites par le GPU

- [x] Buffers en mémoire GPU, lisibles et écrivables par le CPU
- [x] File de commandes, command buffers, commit et attente
- [x] Copie et remplissage de 256 Ko faits par le Mali, exacts à l'octet près (B1L-0051, 16 ms)

## M3 : exécuter des shaders d'Apple (calcul)

- [x] Pilote Metal Mali de Scion intégré (licence MIT) : traducteur AIR → NIR → compilateur Bifrost
- [x] GPU Metal complet : 0 méthode manquante sur 129 (MTLDevice) et sur 414 (MTLDeviceSPI)
- [x] Adresses propres à iOS 16 corrigées pour iOS 26 (`MTLAddDevice`, B1L-0055)
- [x] `image_calcminmax0` et `MetalFaceMask_InitMinMax` (filtres photo d'iOS) compilés pour le Mali et exécutés avec des résultats justes (B1L-0055)

## M4 : dessiner avec des shaders d'Apple

- [x] Bibliothèque de shaders du moteur graphique d'Apple chargée (274 fonctions), pipeline de rendu construit, dessin terminé (`status 4`)
- [x] Correctif B1L-0058 (liste des entrées et sorties des shaders donnée à panfrost) : utile mais **ne suffit pas**, images toujours vides
- [x] Comparaison avec le moteur logiciel (B1L-0059) : **tous les dessins sont justes en logiciel**, donc la traduction Metal est saine et le bug est uniquement dans le chemin du pilote Mali (panfrost)
- [x] Trace d'un dessin sur le Mali (B1L-0060) : géométrie, viewport et images de destination corrects ; piste : la couleur est peut-être écrite dans la mauvaise image (cible 0 au lieu de 2)
- [x] Trace du dessin avec le vertex shader de QuartzCore (B1L-0061) : la matrice de position et les couleurs viennent toutes deux d'un buffer de constantes, qui se lit comme des zéros sur le Mali ; ce qui n'en dépend pas (la coordonnée de texture) arrive juste
- [x] Constantes recopiées par le pilote (B1L-0062) : les bonnes valeurs partent bien vers le GPU, mais le shader lit toujours zéro → la lecture de buffer de constantes par le shader (`LOAD.ubo`) ne marche pas sur ce Mali ; elle n'avait jamais servi en OpenGL ES
- [x] Constantes poussées directement dans les registres du shader (B1L-0063) : aucun changement, la cause est ailleurs ou incomplète
- [x] Trace avec constantes poussées (B1L-0064) : le shader reçoit maintenant ses constantes, mais l'image n°2 reste vide → le problème est dans l'écriture de la couleur (quelle image est visée, et avec quel format)
- [x] Lecture des trois images (B1L-0065) : rien n'est écrit nulle part → ce n'est pas l'écriture, c'est la **couverture** (les triangles ne couvrent presque aucun pixel)
- [x] **Cause trouvée :** sur le Mali, la transformation viewport est ajoutée par le compilateur dans le vertex shader, mais la passe qui le fait ne reconnaissait pas la forme des shaders préparés par la bibliothèque de Scion → positions jamais transformées, tout tombait sur le pixel (0,0)
- [x] **Correctif B1L-0066 validé : le dessin avec le vertex shader de QuartzCore est juste sur le Mali** (grille complète, 16 pixels sur 16, identique au moteur logiciel)
- [x] Trace du dessin à 3 images (B1L-0067) : réglages d'écriture valides ; le cas « seule la 3e image est écrite » reste vide, mis de côté (le rendu à une image, celui de QuartzCore, marche)
- [ ] Décalage d'index buffer et triangles très fins : petits écarts avec le moteur logiciel
- [ ] Pixel du shader d'Apple identique à la référence logicielle (`2458616957 …`)
- [ ] Dessins indexés justes (bonne moitié de l'image)
- [x] Dessin avec le vertex shader de QuartzCore : grille complète de 16 pixels (B1L-0066)
- [ ] Textures, mélange (blend) et profondeur vérifiés

## M5 : dessiner dans les surfaces d'écran (IOSurface) : terminé

- [x] Créer une texture Metal sur une IOSurface (la mémoire d'une fenêtre) : **le Mali l'efface en rouge puis en vert, 4096 pixels sur 4096, relus justes par le CPU (B1L-0070)**
  - [x] B1L-0068 : device et IOSurface OK, mais texture refusée (le pilote de Scion cherchait le service noyau de l'Adreno, `HakiosGPU`)
  - [x] B1L-0069 : service du Mali (`HakiosMali`) utilisé : **la texture sur IOSurface est créée** ; mais l'effacement n'écrit rien
  - [x] B1L-0070 : le pilote de Scion n'effaçait que pendant un dessin ; corrigé, **réussi**
- [x] Bug trouvé au passage : chaque dessin d'une passe réeffaçait les dessins précédents (corrigé dans B1L-0070, indispensable pour QuartzCore)
- [x] Cache CPU/GPU sur ces surfaces : les deux sens sont justes dans le test (motif CPU écrasé par le GPU, couleur GPU vue par le CPU)
- [ ] `newTextureWithDescriptor:iosurface:plane:slice:` encore un stub d'Apple (abort si appelé)
- [x] Un triangle dessiné par Metal dans une IOSurface, relu correctement par le CPU
  - [x] B1L-0071 : **identique au Mac, 0 ligne sur 64 différente** : deux triangles (`std_vert0_lpf` + `attachment_clear_frag_lpf` de QuartzCore) dans une passe, comparés ligne par ligne à l'image du GPU du Mac (2005 pixels)
- [x] Vitesse du pilote : une seule attente GPU par command buffer, un lot par passe, plus aucun piège PAC par dessin (de 8–15 ms à ~0,03–0,05 ms par dessin)
  - [x] B1L-0074 : plus d'attente par dessin, un seul lot GPU par passe : 2 triangles en 7,8 ms (au lieu de 16 à 30), 200 dessins justes ; le GPU n'attend plus que 2 à 11 ms, mais le CPU coûte 1,2 à 3,4 ms par dessin
  - [x] B1L-0075 : Mesa et le pilote sans retours signés : **0,65 à 0,91 ms par dessin** (environ 3 fois mieux), images toujours justes
  - [x] B1L-0076 : sans appels signés : **0,55 à 0,58 ms par dessin**, images justes
  - [x] B1L-0077 : profil : **48 pièges par dessin, tous dans les relais de l'éditeur de liens** (`__auth_stubs`, `__objc_stubs`), aucun chez Apple ; le gros cœur n'aide pas
  - [x] B1L-0078/0079 : relais réécrits sans saut signé : **0 piège par dessin, 26 à 56 µs par dessin** (objectif de 0,2 ms dépassé)
- [x] `CAMetalLayer` et ses drawables (ce que QuartzCore utilise pour présenter une image)
  - [x] B1L-0072 : **`nextDrawable` marche** (CoreAnimation crée la surface, notre pilote fournit la texture), `presentDrawable` en statut 4 ; mais le dessin est refusé (la texture n'était pas marquée cible de rendu)
  - [x] B1L-0073 : **dessin QuartzCore dans le drawable identique au Mac (0 ligne sur 64 différente), `presentDrawable` en statut 4**

## M6 : l'interface d'iOS rendue par Metal

Plan détaillé et décisions : [plan-vitesse-et-m6.md](plan-vitesse-et-m6.md). Critères de réussite : 40 images/s en défilement sur l'accueil, Liquid Glass correct, plus rapide que le rendu CPU.

- [x] M6a : app de test avec un `CAMetalLayer` plein écran, affichée par le serveur actuel (CPU) : présentation réelle, images/s
  - [x] B1L-0079 : **affiché à l'écran** (fond animé, triangles qui tournent) ; 20 à 23 images/s ; par image : encodage 0,17 ms, GPU ~6 ms, attente d'un drawable ~32 ms (le compositeur CPU de backboardd est le goulot)
- [ ] M6b : backboardd sur le Mali, derrière l'interrupteur noyau `b2l_metal_ui` (activé dans le BOOT de test, retour automatique au CPU si backboardd plante 2 fois)
  - [x] B1L-0080 : backboardd prend bien le Mali, mais plante (`targetDeviceArchitecture` manquant) ; le retour automatique au CPU a marché
  - [x] B1L-0081 : backboardd compile les pipelines de QuartzCore, mais 4 sont refusés (samplers inline en forme à deux mots) ; CoreAnimation s'arrête, retour au CPU
  - [x] B1L-0082 : samplers inline réglés ; deux nouveaux refus (`air.get_read_sampler`, pointeur à décalage calculé passé à une fonction)
  - [x] B1L-0083 : ces deux cas passent ; nouveaux refus (insertelement à position calculée, pointeur à la place d'un nombre)
  - [x] Outil `air2nir` sur le Mac : les 189 shaders de QuartzCore vérifiés en quelques secondes, sans boot
  - [x] B1L-0084 : **backboardd dessine sur le Mali pour la première fois** (une image de 1440×3040 envoyée à l'écran) ; mais le Mali arrête le job sur une instruction invalide (0x51, image en escalier), puis un pipeline est refusé (profondeur)
  - [x] B1L-0085 : les *function constants* arrivent bien (modules déjà spécialisés par QuartzCore, `A2Xghfc` = 75 592 instructions) ; le refus de profondeur a disparu ; l'instruction invalide 0x51 reste
  - [x] B1L-0086 : `/b2l-dump` tombe sur USERDATA (illisible depuis le Mac) ; la faute suit chaque fois `A2Xhf` (7 instructions) puis `TimgA2Xhf_Isrc`
  - [x] B1L-0087 : backboardd ne plante plus (4 images) ; SpringBoard plantait à cause du plug-in (`[self release]`) ; 19 modules de backboardd récupérés
  - [x] **Cause de l'instruction invalide trouvée sur le Mac** (`b2l-bicomp`, Mesa avec assertions) : `pan_lower_noperspective` multiplie une varying 16 bits par un `w` 32 bits
  - [x] B1L-0088 : **l'interface d'iOS s'affiche rendue par le GPU** (accueil, Réglages, Centre de contrôle, Contacts, clavier) ; défauts : teinte cyan, Liquid Glass sans flou, bord gauche coupé, lenteur
  - [x] B1L-0089 : les assertions nomment les causes : varyings 16 bits interdites en écriture sur v7, intrinsèques laissées par les valeurs système (dont `LEA_BUFFER`, instruction de Valhall)
  - [x] B1L-0090 : varyings élargies à 32 bits, instructions remplacées retirées ; plus aucune faute 0x51 « did not finish », mais une assertion sur `demote` (shader principal `A2Xghfc`) arrête backboardd 2 fois, d'où le repli sur le rendu CPU
  - [x] B1L-0091 : `demote`/`demote_if` émis comme `discard` par Bifrost ; **interface sur le GPU avec le flou** (Centre de contrôle) ; défauts : fond bleu, Liquid Glass gris opaque, icônes Horloge/Calendrier transparentes, Centre de contrôle lent ; journal : 112 instructions invalides (0x51), 30 fautes MMU dont 14 sur un tampon déjà libéré
  - [x] B1L-0092 : `compute_average_luma` (luminosité pour la teinte du Liquid Glass) n'est plus refusé : taille de mémoire partagée donnée au dispatch ; testé : la bulle Liquid Glass des onglets de Fichiers marche, tuiles du Centre de contrôle toujours grises ; le dispatch échoue encore (texture 0 sans texture de la bibliothèque dessous)
  - [x] B1L-0093 : diagnostic ; **chaque lot échoué de backboardd contient `A2Xghfc`** ; sur le Mac, l'instruction invalide vient de `bi_lower_swizzle` (Mesa)
  - [ ] B1L-0094 : `bi_lower_swizzle` corrigé (3 cas) ; les 41 modules compilent sur le Mac avec assertions ; flashé, testé avec 0095
  - [x] B1L-0095 : cache de shaders + mesures ; **0 faute 0x51** ; nettoyage de cache jusqu'à 2,9 s / 10 s, copie écran 16 ms par image ; affichage progressif (l'écran n'attendait pas le GPU)
  - [x] B1L-0096 : zéro copie jamais activé (blocs IDMA supposés à 0) ; cause du dessin progressif trouvée (surface 1 = mémoire affichée) ; (registres DECON/IDMA de l'Exynos 9820), attente du GPU avant d'afficher, surfaces d'écran contiguës sans nettoyage de cache
  - [x] B1L-0097 : plus de dessin progressif ; envoi au GPU 2–22 ms / 10 s ; adresse du panneau trouvée à `0x19071040` ; surface par défaut en mémoire ordinaire, nettoyage de cache une fois, recherche de l'adresse du panneau dans les registres d'affichage
  - [x] B1L-0098 : **zéro copie actif (0 copie CPU)** ; bascule écran avec largeur de ligne (`IDMA_SRC_SIZE`), surfaces de backboardd contiguës et en écriture combinée, import `RG8Unorm` ; diaporama dû à l'émulation PAC (20 000–80 000 interruptions/s, 11 µs chacune) ; viewport ignoré 1126 fois
  - [ ] B1L-0099 : viewport (rectangle) et scissor servis ; lignes de backboardd arrondies à 64 octets
  - [x] B1L-0101 : profil PAC ; émulation 1,9 µs par interruption ; dasd et d'autres démons dans corecrypto ; backboardd : LDAPR de Mesa ; mais backboardd reste à 82 s noyau / 90 pour une autre raison
  - [x] B1L-0100 : vérification rapide (11 µs → 1,9 µs par interruption PAC)
  - [x] B1L-0099 : viewport et scissor (visuellement mieux)
  - [x] B1L-0102 : LDAPR → LDAR dans nos bibliothèques ; chronométrage des appels HakiosMali/IOSurface/IOMFB
  - [x] B1L-0103 : backboardd ne brûle plus le CPU (LDAPR) ; 2–7 images/s, GPU 2–7 % : goulot d'attente ; gros tampons GPU refusés par le noyau ; vues de texture `nil` ; lignes arrondies à 64 octets aussi pour SpringBoard (source `LA08` du calcul de luminosité)
  - [x] B1L-0104 : plus de tampon refusé ni d'échec de luminosité ni de faute GPU ; plantages de backboardd (Calculatrice) ; tampons GPU ≥ 1 Mo alloués par pages, vues de texture, bascule d'écran qui dort
  - [x] B1L-0105 : attente asynchrone OK ; 10 images/s fixes ; plantage à l'ouverture d'app (command buffer libéré) ; une barrière par command buffer, rappels « terminé » hors du fil de rendu ; `commitAndWaitUntilSubmitted` ; parent des vues de texture
  - [x] B1L-0106 : plantage toujours là (command buffer sans rappel libéré) ; jusqu'à 24 bascules/s par moments ; command buffer retenu jusqu'à la livraison de ses rappels
  - [x] B1L-0107 : plantage restant (retenue relâchée après 1 s, trop tôt) ; command buffer retenu dès sa création (relâché après ses rappels, ou 1 s sans envoi)
  - [x] B1L-0108 : plantage inchangé ; retenue de 30 s pour un buffer non envoyé, emplacement de file rendu
  - [x] B1L-0109 : buffers vivants à l'envoi ; plantage dans objc_msgSend sur un isa aléatoire (objet détruit) ; journal de la vie des command buffers (commitAndWait, dealloc)
  - [ ] B1L-0110 : GPU 650 MHz fixes, diagnostics coupés (b2l_diag=2, b2l_perf, b2l_pacprof), bascule d'écran non bloquante (compilé, pas flashé seul)
  - [x] B1L-0111 : GPU 650 MHz et démons bloqués confirmés ; plantage inchangé ; 0110 + command buffers gardés 2 s après leurs rappels + imagent et dasd bloqués
  - [x] B1L-0112 : **plantage à l'ouverture des apps réglé** (ressources libérées envoyées par `-commit` : le travail enregistré les retient désormais) ; un peu plus rapide, encore très lent ; GPU occupé ≈ 8 %
  - [x] B1L-0113 : échantillonneur : le fil d'affichage de CoreAnimation fait tout le travail (73 % en utilisation, 35 % au repos) ; 3–8 images/s en utilisation, 10,0 fixes au repos ; deux copies CPU chaudes : `-replaceRegion:` (`util_copy_box`, fautes de page) et les constantes recopiées en entier à chaque dessin (`panfrost_emit_const_buf`)
  - [ ] B1L-0114 : échantillonneur étendu (pc noyau, type de faute, fautes de page, piles des fils actifs) ; flashé, testé avec 0115
  - [x] B1L-0115 : **« extrêmement plus fluide »** : constantes copiées seulement jusqu'où le shader lit (2 à 3,6 Go évités / 5 s) ; 10–14 images/s
  - [x] B1L-0116 : fond d'écran dessiné (dégradé, vagues, points) à la place du bleu uni (affiché correctement)
  - [x] B1L-0117 : boot-arg `b2l_diag` lu une fois (n'était relu à chaque appel système de backboardd/SpringBoard)
  - [x] B1L-0118 : états de shader mis en cache par contenu → **régression** (avec 0119 : 544 fautes GPU à l'adresse 0, lent, image cassée) ; retiré en 0120
  - [x] B1L-0119 : état mémoire au refus d'un tampon GPU (aucun refus pendant ce test)
  - [x] B1L-0120 : 0118 retiré : **retour à l'état de 0115 + fond d'écran** ; 0 faute GPU ; verre gris car ses textures (24–36 Mo) sont refusées avec 1,2 Go de RAM libre (plage `kmem_data_shared` de 1,93 Go pleine)
  - [x] B1L-0121 : plage `kmem_data_shared` à 2 999 Mo : 0 refus, 0 faute GPU ; verre toujours gris/noir (pas d'encodeur de copie)
  - [x] B1L-0122 : encodeur de copie (blit) ; QuartzCore ne s'en sert que pour de petits morceaux : verre inchangé
  - [x] B1L-0123 : vues de niveaux 1–3 d'une pyramide 768×1600 bien demandées ; mais 11 fautes GPU (lectures après un pool de 64/16 Ko), clignotements, Centre de contrôle plus cassé
  - [x] B1L-0124 : pire : 46 fautes GPU (dont tampons libérés), clignotements de toute l'interface ; la borne des constantes n'était pas en cause
  - [x] B1L-0125 : plus de clignotements, photo du propriétaire en fond d'écran ; verre toujours gris/noir ; 6 fautes GPU dans les passes du verre
  - [x] B1L-0126 : diagnostic des fautes GPU ; aucune faute pendant le test
  - [x] B1L-0127 : **vrai Liquid Glass sur le bouton Focus du Centre de contrôle** avec les vues de mipmap ; 14 fautes GPU à des adresses jamais données ; 170 dessins refusés (attachements de tailles différentes) ; fond noir
  - [x] B1L-0128 : **Liquid Glass rendu, photo en fond d'écran, « marche très bien »** ; reflashable : `b1l-tools/flash-saved.sh 0128`
  - [x] B1L-0129 : chaque attachement a sa propre action de chargement : correct, mais **aucun changement à l'écran** (glyphes, widgets toujours absents). Le haut clair du fond d'écran est la photo elle-même
  - [x] B1L-0130 : inventaire : aucun `path_*` ; glyphes vibrants = `fixed_frag` qui relit les attachements 0 et 1 (BGRA8 + RGBA16Float)
  - [x] B1L-0131 : panfrost recharge les cibles relues par le shader : glyphes Wi-Fi, AirDrop, Bluetooth et AirPlay du Centre de contrôle rendus
  - [x] B1L-0132 : images à lignes non alignées copiées au lieu d'être refusées : marche, mais aucun changement visible
  - [x] B1L-0133 : backboardd n'importe que 3 IOSurface ; icônes du menu d'extinction réparées (grâce à 0132) ; chronod reçoit « unsupported » pour 4 sélecteurs IOSurface
  - [x] B1L-0134 : le noyau accepte ces 4 appels de chronod : aucun changement visible. Journal 0134 relu : 7 imports IOSurface, tous grands (fond, dock, luminosité, InCallService, Preferences), aucune surface de glyphe ou de widget
  - [x] B1L-0135 : les textures de glyphes et d'icônes arrivent bien aux shaders (23 formes RGBA8 toutes échantillonnées) ; 3 imports seulement ; les instantanés d'icônes `240x240 LA08` ne sont importés qu'une fois, **avant** que l'application les dessine
  - [x] B1L-0136 : la copie d'IOSurface à lignes non alignées est relue avant chaque dessin (instantanés d'icônes vivants) ; diagnostic `B2L LIVE`. **Constat : la piste « copie périmée » n'est pas la cause — le chemin n'est jamais exécuté.** Aucune surface `240x240` n'est importée, aucun `B2L LIVE: surface`, aucun `copied into a texture`. `MobileTimer id 16`, `MobileCal id 26/30`, `Spotlight id 14` créent bien des surfaces `240x240 LA08` mais elles sont créées puis abandonnées (jamais de lookup, jamais d'import). `docs/journal/build-b1l-0136.md` complété.
  - [ ] B1L-0185 : gros patch (2 changements regroupés à la demande du propriétaire) : tampons de ressource repris du cache panfrost mis à zéro (coins bleus, teinte au premier affichage) ; GPU Metal donné à SpringBoard, chronod et iconservicesagent en plus de backboardd (glyphes, Horloge, Calendrier, widgets). Installé (BOOT `06ffd047…`), en test. Note : les noyaux 0137–0184 renvoyaient tous au pilote v85. **Résultat : SpringBoard plante en boucle dans RenderBox (`tables_buffer`), piste du GPU hors backboardd confirmée.**
  - [x] B1L-0186 : `newBufferWithBytesNoCopy:` répond par une copie au lieu de `nil`. **iOS démarre avec 4 processus sur le GPU ; teinte violette disparue (0185).** Icônes Horloge/Calendrier, glyphes et widgets toujours absents.
  - [x] B1L-0187 : diagnostic, le pilote journalise aussi SpringBoard, chronod et iconservicesagent. **Cause trouvée : le traducteur AIR → NIR refuse des shaders de RenderBox (SwiftUI) : phi de structure, phi de pointeurs sur une arête de boucle (`uber_vertex`).** Reproductible sur le Mac : `air2nir --report out/rb-check/default.metallib`.
  - [x] B1L-0188 : phi de structures, phi de pointeurs dans un même tampon et `select` de pointeurs traduits (`apply.py`, `air_to_nir.c`). `uber_vertex` se traduit sur le Mac. **Résultat : les shaders passent le traducteur, mais SpringBoard plante en boucle dans Bifrost (`bi_emit_jump`, cible de saut nulle).**
  - [x] B1L-0189 : garde, un saut que Bifrost ne sait pas émettre est refusé avec le nom du shader ; cible nulle retirée dans `bi_emit_jump`. **Résultat : démarre ; seul `fa16dhmha4` reste refusé (`return` au milieu) ; mais l'interface clignote en mouvement.**
  - [x] B1L-0190 : `nir_lower_returns` ; `select` de pointeurs comme avant 0188 pour QuartzCore. **Résultat : aucune amélioration, apps lentes à ouvrir.**
  - [x] B1L-0191 : retour à l'état de 0187 à la demande du propriétaire (BOOT `dbb192a5…`). Correctifs 0188–0190 gardés dans `userspace/mesa/air-b2l/renderbox-0188-0190.py.txt`. **Appris : les shaders de RenderBox compilent tous avec ces correctifs, mais rien n'apparaît de plus ; le blocage est après le dessin.**
  - [x] 2026-11-14 (Mac, sans flash) : cache d'icônes d'iOS lu sur USERDATA : **icônes correctes** (Horloge avec ses traits) → les défauts viennent du dessin en direct de RenderBox dans SpringBoard ; `b2l-bicomp` remis en état : avec 0188, `uber_vertex` passe Bifrost mais fait **75 Ko de code** (suspect de la lenteur de 0190)
  - [x] B1L-0224 : diagnostic (pipelines refusés de RenderBox nommés, modules envoyés au Mac) ; **n'a pas tourné** : le noyau ne remplace jamais les bibliothèques d'un ensemble déjà publié (`v95` gardé par erreur)
  - [x] B1L-0225 : diagnostic : **3 pipelines refusés seulement** (`fa16dhmha4`+`uber_vertex` phi de structure ; deux pipelines de tracé phi de pointeurs) ; modules dans `out/b2lmod-0225`. Sur le Mac : **cause du plantage 0188 = `return` incorporé sans être abaissé** (et 0190 l'abaissait trop tard) ; avec le bon ordre, les 6 modules compilent ; `fa16dhmha4` = 93 Ko de code
  - [x] B1L-0226 : phi de pointeurs (pipelines de tracé) : **les tracés de RenderBox passent et dessinent**, plus de refus pour eux ; écran inchangé (Horloge sans traits, Calendrier vide, glyphes du Centre de contrôle manquants)
  - [x] B1L-0227 : phi de structures + `return` abaissé avant l'incorporation : **plus aucun shader refusé** ; écran inchangé ; mais 34 dessins de SpringBoard refusés (`fa16dhmha4` lit une texture non liée)
  - [ ] B1L-0228 : textures/samplers déclarés mais non liés acceptés (texture nulle, sampler par défaut) ; flashé (BOOT `85a6ddb5…`) : gel à l'écran verrouillé, SpringBoard plante sur la taille d'une texture non liée
  - [ ] B1L-0229 : taille 0 pour une texture non liée (`panfrost_upload_txs_sysval`) ; flashé (BOOT `7b8e7cbe…`) : démarre, mais pas d'icônes, gels ; 70 jobs GPU en 0x5A sur `fa16dhmha4`
  - [ ] B1L-0230 : `sampler_count` = samplers utilisés (jusqu'à 14), pas le nombre de textures ; flashé (BOOT `3c308e95…`) : Horloge et Calendrier dessinés (cadran strié), disparaissent au changement de page ; centre de contrôle sans icônes ; journal propre (aucun job refusé)
  - [ ] B1L-0231 : `newTextureWithDescriptor:offset:bytesPerRow:` d'un tampon = copie tenue à jour ; flashé (BOOT `5587ec82…`) : pas de changement visible ; le format 90 (RGB10A2) est refusé
  - [ ] B1L-0232 : `MTLPixelFormatRGB10A2Unorm` → `R10G10B10A2_UNORM` (ordre des bits pris dans MoltenVK) ; flashé (BOOT `3d6ee122…`) : aucune différence visible ; 38 pipelines « effet personnalisé » de RenderBox refusés (fonction `custom_fn` assemblée à part, non gérée)
  - [ ] B1L-0233 (diagnostic) : fonctions liées, assemblages et source compilé notés dans le journal ; flashé (BOOT `94d0677b…`) : `custom_fn` arrive en module AIR dans `fragmentLinkedFunctions.privateFunctions` (pas d'assemblage)
  - [ ] B1L-0234 : modules liés lus avec le shader, `<f>.MTL_VISIBLE_FN_REF` → `<f>` ; passe sur le Mac ; flashé (BOOT `6f52d428…`) : journal propre (aucun refus), pas mieux à l'écran
  - [ ] B1L-0235 : texture sur tampon où le GPU écrit = remplie une seule fois ; flashé (BOOT `f85666a4…`) : pas mieux
  - [ ] B1L-0236 : retour du dessin (texture sur tampon → tampon → octets de l'appelant) à la fin du command buffer ; flashé (BOOT `c5dae724…`) : le retour tourne, pas mieux à l'écran
  - [ ] B1L-0237 (diagnostic) : images des cibles sur tampon envoyées au Mac ; flashé (BOOT `e967e7d7…`) : formes justes mais criblées de bruit, icône 204×204 presque vide ; SpringBoard plante dans `hakiosSyncOut`
  - [ ] B1L-0238 : `hakiosSyncOut` protégé (appelant présent, zone projetée) ; flashé (BOOT `512f97a0…`), en test ; piste suivante : surfaces YCbCr à deux plans de chronod
  - [ ] À faire ensuite : texture posée sur un tampon, chronod.
- [ ] B1L-0137 : instrumentation des accès IOSurface (lookup, create port, metadata set/copy) avec l'ID de surface et le PID du demandeur, pour identifier qui consomme la surface `240x240 LA08` et pourquoi backboardd ne l'importe jamais. Ensemble `b2l-metal-v86`, noyau uniquement. Constat : aucune surface `240x240` n'est jamais look up ; le chemin n'existe pas.
- [ ] B1L-0138 : accepte le sélecteur IOSurface 17 pour `iconservicesagent` (comme chronod en 0134). Constat : `accepted 17`, mais toujours aucun lookup sur les surfaces `240x240` ; le sélecteur 17 n'était pas le blocage.
- [ ] B1L-0139 : instrumentation userspace (`hakiosmetal_air.m`, ensemble `b2l-metal-v88`) pour tracer : (1) création de texture `240x240` via `newTextureWithDescriptor:`, (2) liaison d'une texture `240x240` à un sampler de fragment (`setFragmentTexture:atIndex:`), (3) upload vers une texture `240x240` (`replaceRegion:`). Objectif : confirmer si le chemin Metal consomme ou non les instantanés d'icône `240x240 LA08`.
  - [ ] Les 16 noyaux à mémoire de tuile : réservés aux GPU Apple 4+, a priori pas demandés à notre GPU déclaré Apple 1 (à confirmer dans le journal)

- [x] QuartzCore (dans backboardd) accepte notre GPU au lieu du rendu CPU (B1L-0088)
- [ ] Ses 189 shaders (`QuartzCore.framework/default.metallib`) compilés pour le Mali : 173 traduits par le traducteur AIR (B1L-0084), les 16 noyaux à imageblocks restent
- [ ] Archive de pipelines précompilés d'Apple refusée proprement (sans plantage)
- [x] Écran de verrouillage affiché par le GPU (B1L-0088, couleurs fausses)
- [ ] Liquid Glass (flous, verre) rendu
- [ ] Cache de shaders sur disque, pour éviter les saccades de compilation
- [ ] Fluidité mesurée : 40 images/s en défilement sur l'accueil, et plus rapide que le rendu CPU

## À côté : le CPU

- [x] Pièges PAC mesurés par type : 70 % retours signés, 25 % appels signés, 5 % lectures signées, presque tous dans le shared cache
- [x] Synthétiseurs vocaux qui plantaient en boucle bloqués (B1L-0052)
- [x] Piste principale trouvée : corecrypto, exclu de la traduction PAC
- [ ] Réduire les pièges PAC du code d'Apple : patch au chargement par le noyau (`retab`→`ret`, `blraaz`→`blr`, `aut*`→NOP), après M6, corecrypto exclu
- [ ] App « Informations » (About) qui ne s'ouvre pas
- [x] B1L-0164 : clavier figé → dictée coupée dans UIKit (dock du clavier noyé par les changements de disponibilité de la dictée) ; testé : clavier fluide
- [x] B1L-0165 : gestes du bas (accueil, multitâche) → marques de bord (FromEdgeTip + direction) dans les événements tactiles ; testé : gestes OK
- [ ] B1L-0166 : démarrage ~3 min → purge des 63 523 enregistrements périmés du cache natif (60 s par démarrage) ; flashé, à tester
- **0239** (diagnostic) : réceptions de surfaces par port notées, images par processus ; testé : aucune surface chronod reçue, recopie des icônes seulement si quelqu'un attend → 0240.
- **0240** : recopie des dessins RenderBox même sans attente de l'appelant ; testé : aucun effet (le cas n'arrive jamais).
- **0241** : app « Test Metal » dans Horloge (tests à l'écran) ; pas testé : Horloge met ~30 s à s'ouvrir.
- **0242** : app « Test Metal » déplacée dans Dictaphone (s'ouvre en 2 s) ; testé : 24 OK, 9 ÉCHEC (appareil système nil, familles, MSAA, NoCopy+blit, blit→texture, compute→texture, MTLSharedEvent), CAMetalLayer 11–18 i/s ; compilation depuis le source OK. Piste : Horloge/Calendrier lents = attente de leur icône vivante chez iconservicesagent.
- **0243** : copies tampon ↔ texture codées, tampon sans copie rendu à l'app après copie ; flashé (BOOT `b86e33bc…`), **testé OK** (échecs 4 et 5 corrigés).
- **0244** : `write_texture` (compute) stocke avec le type de la valeur ; flashé (BOOT `791ecd3f…`), **testé OK** (échec 6 corrigé).
- **0245** : résolution MSAA (blit Gallium) ; bip de démarrage coupé (audio niveau 5) ; flashé (BOOT `f062d363…`), **testé OK** (résolution rouge ; bip non joué).
- **0246** : MTLEvent / MTLSharedEvent (signal à la fin du command buffer, listeners) ; flashé (BOOT `18fa0c62…`), **testé OK** ; SpringBoard ne demande aucun événement.
- **0247** (diagnostic) : mémoire GPU par processus toutes les 60 s, part marquée jetable par Mesa ; flashé (BOOT `b5ee8db6…`), testé : ~290 Mo réels à 3 min (backboardd ~230 Mo), dont 27 Mo jetables. Test Metal : **29 OK, 4 ÉCHEC** (appareil système ×2, familles, MSAA annoncé), CAMetalLayer 10,9 i/s. But : tout le système sur le GPU sans trop de mémoire.
- **0248** : GPU ouvert à tous les processus (liste B1L-0185 levée, boot-arg `b2l_gpu_all=0` pour la remettre) ; set v118 ; 1er flash sans librairies (set mal nommé `v118`) → pas de GPU ; reflashé (BOOT `4921dded…`), à tester.
- **0249** : extensions d'apps (aucune enregistrée) — réglage ExtensionKit, base LaunchServices reconstruite, fairplayd.H2 coupé (écrit USERDATA, accord du propriétaire) ; flashé (BOOT `ea11bae0…`) : **pire**, toutes les icônes transparentes et les apps gèlent ; réglage ExtensionKit non écrit (EPERM).
- **0250** : code 0249 coupé par défaut (boot-arg `b2l_ext_reset=1` pour le remettre), avant le USERDATA propre ; flashé (BOOT `39be1437…`), téléphone laissé en TWRP.
- **0251** (testé : plantage toujours là, icônes grises) : plantage iconservicesagent localisé (IconRendering+0x55208, référence faible nil) ; `bufferBytesPerRow`/`bufferOffset` des textures sur tampon corrigés (répondaient 0) ; set v119 ; flashé (BOOT `f0efd1da…`), à tester.
- **0252** : app Test Metal retirée de Dictaphone par défaut (boot-arg `b2l_metal_app=1` pour la remettre) ; flashé (BOOT `46587916…`), à tester.
- **0253** : cause du plantage d'iconservicesagent trouvée — le bloc `deallocator` des tampons sans copie était jeté, le tampon d'origine d'IconRendering libéré trop tôt ; bloc gardé et appelé à la libération ; set v120 ; flashé (BOOT `f5035bc6…`) : **plantage disparu**, mais icônes toujours vides.
- **0254** (noyau) : HFS+ accepte la demande APFS « purgeable » 0xC0084A44 de PosterBoard (sans poster, SpringBoard n'applique aucun style d’icône) ; flashé (BOOT `cc011e66…`) : **PosterBoard réparé** (journaux iOS : plus d'erreur purgeable, style d'icône reçu), icônes toujours vides.
- **0255** (noyau) : les 4 autres requêtes « purgeable » de PosterFoundation acceptées aussi sur HFS+ (filet) ; flashé (BOOT `93138266…`).
- **0256** : tas Metal (`newHeapWithDescriptor:`) : RenderBox, qui dessine les icônes, journalisait `unable to create heap` ; set v121 ; flashé (BOOT `233447fc…`) : tas accepté, icônes toujours vides.
- **0257** (Mesa) : le traducteur garde un pointeur rangé dans une variable locale (pipeline RenderBox « custom effect » de SpringBoard, qui pose le pictogramme dans l'icône, refusé 19 fois) ; cache d'icônes 0248 perdu au USERDATA propre (370 → 3) ; set v122 ; flashé (BOOT `65ec61fa…`) : **icônes en partie revenues** ; puis mémoire épuisée → SpringBoard plante → panic.
- **0258** (noyau) : la mémoire GPU que Mesa marque purgeable (~600 Mo) est vraiment reprise quand la mémoire manque (comme le shrinker panfrost de Linux) ; flashé (BOOT `9ba922e4…`). Testé : bloqué sur la pomme, SpringBoard plante en boucle (ctx->prog[VS] NULL sur des fils secondaires) ; purge pas déclenchée.
- **0259** (set v123) : un verrou global autour de tous les appels au contexte Gallium dans hakiosmetal_air.m (SpringBoard dessinait depuis plusieurs fils sans verrou) ; flashé (BOOT `04b02d7e…`) : **démarre, apps ouvrables**, icônes en majorité présentes mais pictogrammes déformés ou vides ; Calendrier/Horloge inchangés ; haut du fond d'écran beige ; widgets « Waiting… ».
- **0260** (diagnostic, set v124) : cache d'icônes 0259 relu → pictogrammes coupés en diagonale (un triangle sur deux faux, pipelines custom effect) ; ligne `B2L DRAW` par forme de dessin ; flashé (BOOT `892145ce…`) : RenderBox dessine par lots `1 0 2 3 65535 5 4 6…`, et la reprise de primitive était désactivée.
- **0261** (Mesa, set v125) : reprise de primitive à 0xFFFF/0xFFFFFFFF pour les dessins indexés, comme Metal ; flashé (BOOT `829d9e95…`) : toujours cassé à l'écran (cache d'icônes de 0259 réutilisé ?), très lent ; cache d'icônes effacé sur USERDATA (accord, sauvegarde gardée) → **nouveau cache juste (140 icônes, Horloge et Calendrier compris)**, mais SpringBoard recasse plus tard ; lent.
- **0262** (diagnostic, set v126) : images des dessins tardifs de SpringBoard et attente sur le verrou ; testé : icônes recassées après un moment ; verrou contendu seulement au démarrage ; cache d'icônes 0262 entièrement juste (casse au dessin en direct).
- **0263** (Mesa, set v127) : tampons Metal créés en mappage persistant ; panfrost remplaçait le BO sous le pointeur `contents` lors d'une copie/remplissage (données périmées → traits/triangles) ; testé : pas d'amélioration visible (rayures horizontales, lenteur) ; masques R8 faux une ligne sur deux par bloc 2×2 → piste dérivées.
- **0264** (test, set v128) : app Test Metal remise dans Dictaphone + tests des dérivées et de la passe R8/RG16F/BGRA8 (parité 2×2) ; testé : 35 OK, 3 ÉCHEC ; dérivées justes ; **passe R8+RG16F+BGRA8 : seule BGRA8 écrite** (RenderBox accumule sa couverture en RG16F).
- **0265** (test, set v129) : cibles séparées (R8, RG16F, float/float2/float4), 3 cibles BGRA8, relecture de la cible ; testé : cibles écrites mais **position décalée d'un demi-pixel** (0264 s'était trompé) ; relecture OK.
- **0266** (Mesa, set v130) : position du fragment +0,5 (panfrost donne le coin du pixel sur Bifrost) ; testé : même problème (icônes cassées dès le mouvement), très lent ; verrou attendu 4,1 s sur 5 s dans SpringBoard.
- **0267** (diagnostic, set v131) : temps de verrou tenu par endroit (`B2L HOLD`). Testé : 43 OK, 1 ÉCHEC (MSAA pas annoncé), `CAMetalLayer`
  9,5 i/s. La création de pipelines tient le verrou jusqu'à 18,3 s (iconservicesagent), 0,8 s (backboardd), 0,6 s (SpringBoard).
- **0268** (optimisation, set v132, Mesa) : la traduction AIR → NIR d'un pipeline se fait sans le verrou, seule la compilation panfrost le garde. Flashé (BOOT `a9bff31f…`), mais **jamais chargé** (plug-in `B2LMetalv131` resté, voir build-b1l-0268.md).
- **0269** (set v133) : GPU réservé à backboardd comme avant 0185 (boot-args `b2l_gpu_icons=1` / `b2l_gpu_all=1` pour rouvrir) ; les icônes repassent par le dessin sans GPU d'Apple. 1er flash (BOOT `74a1cf8e…`) sans effet : le plug-in v131 de 0267 restait chargé (nom `B2LMetalvNNN` pas changé, 0268 non plus) ; nom corrigé, image SYSTEM désencombrée (anciens sets retirés), reflashé (BOOT `276d422e…`) : seul backboardd a le GPU (vérifié dans le log) ; compilation panfrost = 59,5 s dont une de 49,2 s dans backboardd ; retour visuel en attente.

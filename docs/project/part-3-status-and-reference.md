# Documentation du projet — Port iOS 26 sur Galaxy S10 (beyond1lte)

## Partie 3 — Journaux de build, état actuel, plan GPU et référence

---

### 11. Journaux de build (`docs/journal/build-b1l-XXXX.md`)

114 journaux existent (0001 à 0139, avec des trous). Chaque journal contient : Date, Base, Objectif, Changements, Installé (hash BOOT relu), Résultat, Rollback.

**Progression majeure :**

| Build | Ce qui a changé / résultat |
|-------|---------------------------|
| **0001** | Premier démarrage XNU sur le S10, sans iOS. 8/8 CPU vérifiés. Panic attendu (pas de racine HFS). |
| **0004** | `BKNoWatchdogs = YES` dans les préférences de SpringBoard (empêche le watchdog de tuer Réglages). |
| **0005** | Écran logique 880×1912 @2x (440×956 points, taille iPhone 16 Pro Max) au lieu de 960×2026. Réduction de 2,6× du nombre de pixels. |
| **0014** | DVFS CPU (A55 1846, A75 2210, M4 2080 MHz) + sonde GPU G1 (lecture seule). GPU_ID `0x72110000` (G76), 12 cœurs. |
| **0015** | Variables CoreAnimation injectées par le kernel (`CA_DISABLE_ASYNCHRONOUS`). Clavier fonctionne (lent). |
| **0016** | GPU G2a : soft reset, L2/tiler/shaders allumés et éteints. |
| **0017** | IOSurface multi-plan + premier job GPU (WRITE_VALUE `0xb21cafe5` exécuté par le Mali). |
| **0028** | Throttle thermique T1 (capteurs TMU 9820 corrigés). |
| **0029** | GPU G3 : service noyau HakiosMali démarré, jobs en mode identité puis par tables de pages. |
| **0030** | T1 corrigé + G4 : fréquence GPU à 676 MHz par ACPM. Puce plafonne à 62-66 °C au lieu de 76 °C. |
| **0040** | RAM des banques basses traitée comme DRAM (corrige un panic d'alignement Mesa). |
| **0041/0042** | iOS rendu par le GPU (EAGL → Mesa Panfrost → Mali). Plus lent et plus cassé que le CPU. |
| **0043** | Retour au rendu CPU par défaut (GLES en secours, `b2l_gl=1`). Décision de passer à Metal. |
| **0044** | Service noyau `IOAcceleratorES` annonçant un plug-in Metal. |
| **0045** | `MTLCreateSystemDefaultDevice()` renvoie le GPU. |
| **0047-0049** | Plantages Metal corrigés, test hors backboardd. |
| **0051** | Copie et remplissage de 256 Ko faits par le Mali, exacts à l'octet près. |
| **0055** | Plug-in Metal Scion intégré : 0 méthode manquante sur 129 (MTLDevice) et 414 (MTLDeviceSPI). `image_calcminmax0` exécuté avec résultats justes. |
| **0058-0066** | Correction des bugs de rendu Metal (constantes shader, viewport, couverture). Dessin QuartzCore juste sur le Mali (B1L-0066 : grille 16×16 identique au logiciel). |
| **0070-0073** | Texture Metal sur IOSurface : Mali efface en rouge puis vert, relu juste par le CPU. `CAMetalLayer` fonctionne, `presentDrawable` statut 4. Dessin QuartzCore dans le drawable identique au Mac (0 ligne sur 64 différente). |
| **0074-0079** | Vitesse du pilote : de 8-15 ms à ~0.03-0.05 ms par dessin, puis 0 piège PAC par dessin, 26-56 µs par dessin. |
| **0080-0083** | backboardd sur le Mali. Plantages corrigés (targetDeviceArchitecture, samplers inline, insertelement). Outil `air2nir` : 189 shaders QuartzCore vérifiés sur le Mac. |
| **0084** | backboardd dessine sur le Mali pour la première fois (une image 1440×3040 envoyée à l'écran). |
| **0088** | **L'interface d'iOS s'affiche rendue par le GPU** (accueil, Réglages, Centre de contrôle, Contacts, clavier). Défauts : teinte cyan, Liquid Glass sans flou, bord gauche coupé, clavier sans fond des touches. |
| **0090-0095** | Varyings élargis à 32 bits, `demote` → `discard`, `bi_lower_swizzle` corrigé. Cache de shaders sur disque. 0 faute 0x51. Zéro copie activé (0 copie CPU). |
| **0096-0098** | Plus de dessin progressif. Zéro copie actif (IDMA, surfaces contiguës). Viewport ignoré 1126 fois. |
| **0099-0103** | Viewport et scissor servis. Lignes de 64 octets pour SpringBoard. Plus de gros tampons refusés (allocation par pages). Vues de texture. backboardd ne brûle plus le CPU (LDAPR). |
| **0104-0112** | Tampons GPU ≥ 1 Mo alloués par pages. Attente asynchrone des command buffers. Plantage à l'ouverture des apps réglé (0112). 2-7 images/s, GPU 2-7%. Goulot d'attente. |
| **0113-0115** | Constantes copiées seulement jusqu'où le shader lit (2-3.6 Go évités / 5s). 10-14 images/s. **« extrêmement plus fluide »** (propriétaire). |
| **0116-0120** | Fond d'écran dessiné (dégradé, vagues, points) à la place du bleu uni. Retour à 0115 (régression 0118 retirée). |
| **0121-0125** | Plage `kmem_data_shared` à 2999 Mo. Encodeur de copie (blit). Photo du propriétaire en fond d'écran. Verre toujours gris/noir. |
| **0126-0128** | **Vrai Liquid Glass sur le bouton Focus du Centre de contrôle** (0127). **Liquid Glass rendu, photo en fond d'écran, « marche très bien »** (0128). Reflashable : `flash-saved.sh 0128`. |
| **0129-0131** | Chaque attachement a sa propre action de chargement. Inventaire des shaders : aucun `path_*`, glyphes vibrants = `fixed_frag` framebuffer fetch. Panfrost recharge les cibles relues : glyphes Wi-Fi, AirDrop, cellulaire, Bluetooth, AirPlay du Centre de contrôle rendus. |
| **0132** | Images à lignes non alignées copiées au lieu d'être refusées. Icônes du menu d'extinction réparées. Aucun changement visible pour les glyphes manquants. |
| **0133** | backboardd n'importe que 3 IOSurface (fond, luminosité, InCallService). `chronod` reçoit « unsupported » pour 4 sélecteurs IOSurface. Aucune surface de glyphe ou de widget. |
| **0134** | Noyau accepte les 4 appels de chronod. Aucun changement visible. 7 imports IOSurface, tous grands. |
| **0135** | Les textures de glyphes et d'icônes arrivent bien aux shaders (23 formes RGBA8 toutes échantillonnées). 3 imports seulement. Les instantanés d'icônes `240x240 LA08` ne sont importés qu'une fois, **avant** que l'application les dessine. |
| **0136** | Copie d'IOSurface à lignes non alignées relue avant chaque dessin (instantanés d'icônes vivants). **Constat : la piste « copie périmée » n'est pas la cause — le chemin n'est jamais exécuté.** Aucune surface `240x240` n'est importée. Aucune `B2L LIVE: surface`. `MobileTimer id 16`, `MobileCal id 26/30`, `Spotlight id 14` créent bien des surfaces `240x240 LA08` mais elles sont créées puis abandonnées (jamais de lookup, jamais d'import). |
| **0137** | Instrumentation des accès IOSurface (lookup, create port, metadata) avec ID de surface et PID. **Constat : aucune surface `240x240` n'est jamais look up ; le chemin n'existe pas.** |
| **0138** | Accepte le sélecteur IOSurface 17 pour `iconservicesagent`. `accepted 17`, mais toujours aucun lookup sur les surfaces `240x240`. Le sélecteur 17 n'était pas le blocage. |
| **0139** (en cours) | Instrumentation userspace (`hakiosmetal_air.m`, ensemble `b2l-metal-v88`) pour tracer : création de texture `240x240`, liaison à un sampler de fragment, upload via `replaceRegion:`. Objectif : confirmer si le chemin Metal consomme ou non les instantanés d'icône `240x240 LA08`. Résultat en attente. |

**Build stable de référence pour rollback :** 0128 (ensemble `b2l-metal-v77`), reflashable avec `flash-saved.sh 0128`.

---

### 12. Plan GPU (Mali-G76 MP12) — `docs/gpu/plan.md`

Décision du propriétaire : **ne plus investir dans le rendu CPU pour Liquid Glass et les effets ; tout passe par le GPU.**

Chaîne visée :
```
CoreAnimation (backboardd, CA::OGL::GLESContext)
  └─ pont EAGL dans backboardd (libb2l-gl.dylib)
       └─ libOSMesa + Mesa 25.0.7 Panfrost, compilateur Bifrost
            └─ libdrm de remplacement → IOKit
                 └─ service noyau B2LMali : interface DRM panfrost 1.2
                      └─ Mali-G76 : MMU AS0, job slots 0/1, IRQ ou scrutation
```

| Étape | Contenu | Statut |
|-------|---------|--------|
| G0 | Choix de la voie (GLES présent dans iOS 26) | Fait (`docs/gpu/g0.md`) |
| G1 | Lecture de l'ID GPU | Fait (B1L-0014/0015) |
| G2a | Reset + alimentation des cœurs | Fait (B1L-0016) |
| G2b | Premier job GPU | Fait (B1L-0017) |
| G3 | Service noyau B2LMali | Fait (B1L-0029) |
| G3b | Interruptions (GIC au lieu de scrutation) | À faire |
| G4 | Fréquence GPU (DVFS par ACPM) | Fait (B1L-0030, 676 MHz) |
| G5 | Mesa pour iOS | À faire |
| G6 | CoreAnimation sur le GPU (pont EAGL) | À faire |
| G7 | Affichage sans copie (zéro copie) | Fait partiel (B1L-0096 à 0098) |
| G8 | Liquid Glass | En cours (rendu sur bouton Focus, 0127-0128) |

**Étape T1 (prioritaire) :** throttle thermique CPU. Fait (B1L-0030) : puce plafonne à 62-66 °C au lieu de 76 °C.

---

### 13. Plan Metal — `docs/metal-plan.md`

Décidé après B1L-0042 : quitter GLES pour Metal car le serveur d'affichage et Liquid Glass d'iOS 26 sont écrits pour Metal.

| Étape | Contenu | Statut |
|-------|---------|--------|
| M1 | Noyau publie `MetalPluginName=B2LMetal` / `MetalPluginClassName=B2LMetalDevice` | Fait (B1L-0044) |
| M2 | Mémoire et copies faites par le GPU | Fait (B1L-0051) |
| M3 | Exécuter des shaders d'Apple (calcul) | Fait (B1L-0055) |
| M4 | Dessiner avec des shaders d'Apple | Fait (B1L-0066, dessin QuartzCore juste) |
| M5 | Dessiner dans les surfaces d'écran (IOSurface) | Fait (B1L-0070 à 0073) |
| M6 | L'interface d'iOS rendue par Metal | En cours (B1L-0088 à 0139) |

**Critères de réussite M6 :** 40 images/s en défilement sur l'accueil, Liquid Glass correct, plus rapide que le rendu CPU.

---

### 14. Référence GPU du port S7 — `docs/gpu/herolte-reference.md`

Le port Galaxy S7 (`wardevsa/ios-a5xelte`, iOS 16.7.16, Mali-T880 Midgard) sert de référence :
- Chaîne : CoreAnimation (GLES) → pont EAGL → OSMesa + Mesa Panfrost → libdrm de remplacement (`herodrm.c`) → service kernel HeroGPU.
- ~35-38 fps à 650 MHz.
- Éléments transposables au S10 : service kernel de jobs (Job Manager), Mesa Panfrost, `herodrm.c`.
- Point bloquant : pont EAGL + `ca_hooks.c` dépend du renderer GLES de CoreAnimation dans iOS 16, à vérifier dans iOS 26.

---

### 15. Faits mesurés sur le S10 — `docs/device-facts.md`

| Élément | Valeur |
|---------|--------|
| `ro.boot.em.model` | `SM-G973F` |
| `ro.product.device` (TWRP) | `beyond1lte` |
| Bootloader | `G973FXXUHHXJ1` |
| Recovery | TWRP 3.7.1_12 Artisan v2, kernel ExtremeKernel 4.14.356 |
| DTB live `model` | `Samsung BEYOND1LTE EUR OPEN 26 board based on EXYNOS9820` |
| RAM | ~7,1 Go visibles (MemTotal 7 421 288 kB), 8 cœurs |
| Partition boot | `sda14`, 57 671 680 octets |
| Partition system | `sda25`, 7 340 032 000 octets |
| Partition userdata | `sda34`, 499 641 200 640 octets (~465 Gio) |

**Ce qui doit changer pour beyond1lte par rapport à beyond2lte :**
1. UEFI : partir de l'overlay `beyond2ltePkg`, renommé `beyond1ltePkg`, avec le DTB du S10.
2. DTB du boot.img : équivalent de `Resources/DTBs/beyond2lte.dtb` (base + overlay rev 26 + `model`).
3. Identité et gardes : `package-boot.py` (board beyond1lte, model SM-G973F), `pull-b2l-log.py` (exiger `ro.boot.em.model == SM-G973F`), `DeviceTree.c`.
4. Kernel XNU : aucune adresse matérielle à changer d'après les mesures.

---

### 16. Récapitulatif de l'état global (2026-11-10)

Le projet a dépassé la phase de preuve de concept :
- **Boot complet** : S-Boot → BootShim → UEFI → XnuLoader → XNU → HFS root → launchd → SpringBoard, vérifié sur le S10.
- **Rendu GPU Metal actif** : l'interface iOS est rendue par le Mali-G76, avec Liquid Glass sur le bouton Focus du centre de contrôle.
- **Performance** : 10-14 images/s, « extrêmement plus fluide » que le rendu CPU (propriétaire, B1L-0115).
- **Reste à faire :** glyphes manquants (boutons ronds, Concentration, volume), icônes Horloge/Calendrier, widgets blancs, chemin des instantanés d'icône `240x240` (diagnostic en cours, builds 0136-0139), Wi-Fi, Bluetooth, audio, cellulaire, caméra, veille.

La documentation précédente (Parties 1 et 2) couvre l'arborescence, les scripts, l'environnement de build, le processus de build et la politique de sécurité.
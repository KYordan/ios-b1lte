# Plan GPU (Mali-G76 MP12) : document de référence

Créé le 2026-11-05, après B1L-0027 (apps, Control Center et fond provisoire fonctionnent sur le rendu CPU).
Ce fichier est la feuille de route GPU. Chaque build GPU y renvoie, et ses cases sont mises à jour au fil des essais.
Décision du propriétaire : **ne plus investir dans le rendu CPU pour Liquid Glass et les effets** ; tout passe par le GPU.

## 1. Où on en est (déjà fait)

| Étape | Build | Résultat |
| --- | --- | --- |
| G0 : choix de la voie | doc `gpu-g0.md` | CoreAnimation d'iOS 26 a encore `CA::OGL::GLESContext` (moteur OpenGL ES) → voie EAGL + Mesa Panfrost |
| G1 : lecture de l'ID | B1L-0014 | `GPU_ID 0x72110000` (G76), `SHADER_PRESENT 0x770077` (12 cœurs), AS 0xff, JS 7 |
| G2a : reset et alimentation | B1L-0016 | soft reset, L2 / tiler / shaders allumés et éteints, domaine `pd-embedded_g3d` déjà allumé au boot |
| G2b : premier job | B1L-0017 | job WRITE_VALUE par table de pages AArch64 4K sur AS0 : `result=b21cafe5` |

Code : `xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp` (`b2l_gpu_probe`, `b2l_mali_first_job`) et `B2LMali.h`.
Kill switches : `b2l_gpu`, `b2l_gpu_job`.

## 2. La chaîne visée

```
CoreAnimation (backboardd, CA::OGL::GLESContext)            ← rendu actuel : CA::OGL::SWContext (CPU)
  └─ pont EAGL dans backboardd (libb2l-gl.dylib)            ← HakiOS userland/src/eagl.c (MIT)
       └─ libOSMesa + Mesa 25.0.7 Panfrost, compilateur Bifrost   ← HakiOS tools/mesa (patches darwin)
            └─ libdrm de remplacement → IOKit               ← HakiOS tools/mesa/hakios-drm
                 └─ service noyau B2LMali : interface DRM panfrost 1.2   ← HakiOS drivers/gpu/hakios.mali.{c,cpp}
                      └─ Mali-G76 : MMU AS0, job slots 0/1, IRQ ou scrutation
```

Références locales (`~/Developer/s10-port/ref-scion`) :
- **HakiOS** (`hackios-xnu-kernel` et `hakios-starlte/GPU.md`). Écrit pour le Mali-G72 du S9, de la même famille Bifrost JM que notre G76. Jamais testé sur leur téléphone, mais l'interface a été vérifiée contre Mesa.
- **herolte** (`ios-a5xelte`, S7, Mali-T880 Midgard, iOS 16.7). Cette chaîne y **tourne réellement**, à 35-38 images/s. C'est la meilleure preuve que la méthode marche.

## 3. Liquid Glass : ce qu'on sait

- Les filtres `CA::OGL::GlassBackgroundFilter` / `GlassForegroundFilter` et les nœuds `SDFNode` / `SDFGaussianNode` sont dans la couche **commune** `CA::OGL`, partagée par les moteurs Metal, GLES et logiciel.
- `GlassBackgroundFilter::can_render` (désassemblé) ne vérifie **pas** le type de moteur. Il ne teste qu'un champ de l'état graphique (`[gstate+0x10] == 0`).
- **Hypothèse de travail :** le moteur GLES peut dessiner Liquid Glass. À confirmer à l'étape G6. Le S7 (iOS 16) avait le Control Center sans flou, mais Liquid Glass n'existait pas encore.
- **Plan B si G6 montre que le verre ne se dessine pas en GLES :** Metal par Gallium (HakiOS `hakiosmetal_air.m` + `tools/mesa/air`, shaders AIR → NIR → compilateur Bifrost). C'est un chantier long, pas avant que GLES marche.

## 4. Étapes

Une étape = un build et un flash (BOOT), avec ses kill switches. Les étapes userspace demandent en plus des fichiers dans la racine (voir §5).

| # | Étape | Contenu | Critère de réussite (journal TWRP) | Statut |
| --- | --- | --- | --- | --- |
| G3 | Service noyau B2LMali | Port de `hakios.mali.c/.cpp` : allocateur d'adresses GPU 32 Mio–4 Gio (`hakios.mali_va.h`), BO créer / mmap / fermer, import IOSurface (0x7e), SUBMIT, WAIT_BO, GET_PARAM depuis nos registres G76. Fautes MMU lues et journalisées. Kill switch `b2l_mali=0` | `B2LMali started`, service visible dans l'IORegistry, l'auto-test noyau passe par le chemin BO | **fait (B1L-0029) : 2 jobs `0xc0ffee01`, HakiosMali démarré** |
| G3b | Interruptions | Router les SPI Mali par le GIC (job, MMU, GPU), à la place de la scrutation | les jobs finissent par IRQ, plus de délai de scrutation | à faire |
| G4 | Fréquence GPU | DVFS du G3D par ACPM, comme pour les CPU (index et table fvmap de la G3D). Défaut proposé : un cran sous le max (voir question 3) | fréquence mesurée stable, pas de faute | **fait (B1L-0030) : 676 MHz par ACPM, jobs OK** |
| G5 | Mesa pour iOS | Mesa 25.0.7, `-Dgallium-drivers=panfrost`, patches darwin de HakiOS, hakios-drm, arm64e signé ad hoc : `libOSMesa` + `libb2l-drm` | un programme de test (`mali_test` : buffer, job, `0xc0ffee01` écrit par le GPU ; puis un triangle) | à faire |
| G6 | CoreAnimation sur le GPU | Pont EAGL injecté dans backboardd par le noyau (même mécanisme que la bibliothèque de SpringBoard : `DYLD_INSERT_LIBRARIES` + réponse AMFI 90). Table `eagl_slots.txt` à **redériver pour l'OpenGLES d'iOS 26** | écran d'accueil dessiné par le Mali ; « hakios-gl: » dans les journaux ; images/s mesurées ; Liquid Glass visible ou non | à faire |
| G7 | Affichage sans copie | Les surfaces du serveur d'affichage dessinées directement en mémoire scannée (zéro copie), synchronisation des images | fluidité, CPU au repos bas | à faire |
| G8 | Liquid Glass | Si absent en G6 : diagnostiquer les fonctions de shader refusées ; sinon plan B Metal (§3) | effets de verre rendus | à faire |

## 4b. Vérifié pour G6 (2026-11-05)

- **La table de dispatch d'OpenGLES n'a pas changé entre iOS 16.5 et iOS 26** :
  - outil `tools/derive-eagl-slots.py` (dans beyond2lte-ios), qui lit les routines `gl*` d'`OpenGLES.framework` 23G90 : `TPIDRRO_EL0+0xf0` → contexte, entrée à `[ctx+off]`, premier argument `[ctx+0x20]` ;
  - **les 457 positions sont identiques** à `eagl_slots.txt` de HakiOS (iOS 16.5), y compris les 73 qu'utilise CoreAnimation ;
  - relevé conservé dans `tools/eagl-slots-ios26-23G90.txt`.
- Le pont `eagl.c` de HakiOS s'applique donc sans redériver la table.

## 4c. Constat B1L-0031 (2026-11-05) : pas de repli GLES dans le serveur d'affichage d'iOS 26

- Côté noyau, tout a fonctionné : copie depuis SYSTEM, injection dans backboardd (pid 62), AMFI 90 accordé. Mais `HakiosMali` a **0 client** : CoreAnimation n'a jamais appelé EAGL.
- `CA::WindowServer::AccelServer::renderer()` (iOS 26, à la place de `EAGLServer`) : `CAMetalContextCreate()` → `new_metal_context`, et **retour NULL sans Metal**. Plus aucune branche `[EAGLContext initWithAPI:]`.
- `CA_ENABLE_OGL` est lu dans le constructeur d'`AccelServer`, mais sa valeur est jetée (vestige).
- `GLESContext::GLESContext(void*)` et `gles_new_context` existent encore, mais ne servent qu'aux `CARenderer` des apps (`context_changed`).
- **Deux voies :**
  1. remplacer `AccelServer::renderer()` dans backboardd pour qu'il construise un `GLESContext` sur le pont (adresses 23G90, comme HakiOS le faisait en 16.5) ;
  2. un vrai périphérique Metal au-dessus de Mesa (Gallium + AIR, HakiOS), long.
- Choix du propriétaire : à reporter ici.

## 5. Contraintes connues

- **Taille des bibliothèques.** Mesa Panfrost pèse plusieurs dizaines de Mo. Le noyau ne publie que des fichiers de 4 Mio au plus (`b2l_apps_sync_payload`), et BOOT n'a qu'environ 31 Mio de libre. Où les mettre est la question 1.
- **Le noyau ne remplace jamais un fichier existant qui diffère** : chaque nouvelle version de bibliothèque prend un nouveau nom (`-vN`).
- **arm64e** : toutes les bibliothèques userspace sont compilées en arm64e, comme le lanceur et `b2l-springboard`. Le noyau traduit et émule le PAC.
- **Gels possibles** : faute MMU, `JOB_READ_FAULT` (vu sur le S7). Il faut toujours un kill switch, un retour au rendu CPU sans reflasher, et la journalisation des fautes.
- **Licences** : les pilotes noyau de HakiOS sont dérivés de XNU (APSL) ; le pont EAGL est sous licence MIT. On garde leurs en-têtes et on cite la source dans chaque fichier repris.

## 6. Décisions du propriétaire (2026-11-05)

1. **Bibliothèques (Mesa, pont EAGL) : partition SYSTEM** (sda25, qui contient déjà le cache HFS natif et le journal). Le noyau les rend visibles à iOS depuis SYSTEM, sans flash USERDATA.
2. **Liquid Glass : GLES d'abord**, Metal seulement si le verre ne s'affiche pas en G6.
3. **Fréquence GPU : presque max, plus sûr** (un cran sous 702 MHz, ~650 MHz).
4. **Code HakiOS et herolte repris directement**, avec leurs licences et en-têtes.
5. **Ajout demandé : limite de température CPU avec throttle.** Le CPU monte à 76 °C ; 55–60 °C est acceptable, 76 °C non. C'est l'étape **T1**, à faire avant G3 : elle protège aussi tous les essais GPU.

## 7. Étape T1 : throttle thermique CPU (prioritaire)

- Lire les capteurs TMU de l'Exynos 9820 (adresses et étalonnage dans le DTB et le kernel Samsung : zones des clusters BIG/MID/LITTLE, et G3D `TMU@100A0000`).
- Régulateur dans le noyau, sur la même voie ACPM que le DVFS actuel (B1L-0015) :
  - au-dessus de 65 °C, la fréquence du cluster concerné baisse d'un cran par seconde ;
  - sous 60 °C, elle remonte d'un cran ;
  - à 72 °C, elle tombe au plancher immédiatement.
- Les mêmes capteurs serviront pour le GPU en G4.
- Journal : `B2L THERMAL: zone=… temp=… cluster=… khz=…` toutes les 10 s et à chaque changement.
- Kill switch : `b2l_thermal=0`. Seuils réglables par boot-args.
- Critère de réussite : sous charge, la température plafonne autour de 65 °C au lieu de 76 °C.

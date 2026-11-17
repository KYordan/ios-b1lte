# Metal sur le Mali-G76 : le plan

Date : 2026-11-06. Décidé par le propriétaire après B1L-0042, voir la mémoire `metal-plan-decisions`.

## Pourquoi quitter GLES

- B1L-0041 et B1L-0042 rendent bien iOS sur le Mali, par Mesa panfrost derrière le pont EAGL de HakiOS. Le résultat est plus lent et plus cassé que le rendu CPU : couleurs fausses, pas de Liquid Glass, saccades.
- Dans QuartzCore 26, le chemin GLES n'est plus exécuté par Apple. Il a fallu corriger un oubli d'Apple (`push_surface`) et d'autres suivront.
- Le serveur d'affichage et Liquid Glass d'iOS 26 sont écrits pour Metal.
- **GLES reste en secours :** désactivé par défaut depuis B1L-0043, `b2l_gl=1` le réactive.

## Ce que fait HakiOS (référence : `~/Developer/s10-port/ref-scion/hakios-xnu-kernel-oos`)

Sur un Adreno (Snapdragon 7c+), le serveur d'affichage tourne en Metal à environ 100 images/s, selon `docs/metal-plan.md` de leur dépôt. Leur pilote a trois parties :

| Partie | Chez HakiOS (Adreno) | Licence | Pour nous |
| --- | --- | --- | --- |
| A. Compilateur | AIR (bitcode des `.metallib`) → NIR : `tools/mesa/air/*.c` + `air-frontend.patch`, pour **Mesa 25.0.7**, puis ir3 | MIT | Le front-end AIR→NIR se reprend tel quel (même Mesa que nous). Le NIR va au **compilateur Bifrost** au lieu d'ir3. |
| B. Le périphérique | `userland/src/HakiosMetalCA.m` : sous-classe de `_MTLDevice` et tous les objets Metal, au-dessus de l'API C `hakios_metal.h` (environ 130 fonctions utilisées) | MIT | Repris presque tel quel. |
| C. Le cœur GPU | `hakios-metal/` (C++) : paquets PM4, registres A6XX, mémoire msm | MIT | **À remplacer :** même API C `hakios_metal.h`, mais au-dessus de **Gallium panfrost** (Mesa), qui construit déjà les jobs Mali, compile en Bifrost et gère la mémoire via HakiosMali. |
| D. Noyau | `IOAcceleratorES` avec `MetalPluginName` / `MetalPluginClassName` (boot-arg `metal=1`), requêtes GPU | APSL | Ajouter ces propriétés à un service de notre noyau. HakiosMali sert déjà Mesa (interface panfrost DRM). |

**Choix pour la partie C : Metal par-dessus Gallium plutôt que des jobs Mali écrits à la main.**
- Le NIR issu de l'AIR passe à `pipe_context->create_fs_state/create_vs_state/create_compute_state` (`PIPE_SHADER_IR_NIR`).
- Les commandes Metal deviennent des appels Gallium : `draw_vbo`, `launch_grid`, `blit`, `clear`, `set_framebuffer_state`, sampler views, etc.
- Gallium panfrost tourne déjà sur le téléphone (B1L-0041/0042 : 9184 job chains sans échec).

## Étapes (test d'abord, serveur d'affichage ensuite)

| # | Étape | Vérifié par |
| --- | --- | --- |
| M1 | Le noyau publie `MetalPluginName=B2LMetal` et `MetalPluginClassName=B2LMetalDevice` sur un service `IOAcceleratorES` (désactivé par défaut, `b2l_metal=1`). Le bundle `B2LMetal.bundle` (plug-in HakiOS) est sur SYSTEM. | `metal_test` sur le téléphone : `MTLCreateSystemDefaultDevice()` renvoie notre classe. |
| M2 | Buffers : `hakios_metal_buffer_*` au-dessus de `pipe_resource` (panfrost) | Écriture puis relecture d'un buffer |
| M3 | Compilateur : front-end AIR de HakiOS dans notre build Mesa ; compute par `launch_grid` | Un kernel de calcul donne les valeurs attendues |
| M4 | Rendu : pipeline et encodeur de rendu, un triangle dans une texture | Lecture des pixels |
| M5 | IOSurface : texture sur la mémoire d'une surface (requête `0x7e` de HakiosMali, framebuffer compris depuis B1L-0042) | Triangle visible dans une IOSurface |
| M6 | Serveur d'affichage : QuartzCore choisit Metal, avec les shaders de son `default.metallib` (à vérifier dans le 23G90, voir ci-dessous) | Écran de verrouillage, puis fluidité mesurée |

## Questions ouvertes

- **QuartzCore 23G90 a-t-il son `default.metallib` ?** HakiOS (iOS 16.5) devait le fabriquer (`tools/ca-metallib.py`). À vérifier dans `ipsw-23G90/OS.dmg`.
- **Métal et `pacibsp`/PAC :** le plug-in est compilé en arm64e comme nos dylibs actuelles. Même traitement que `libb2l-gl`.
- **Vitesse :** compilation des shaders Bifrost à la première utilisation. Un cache de shaders sur disque (cache disque de Mesa) sera probablement nécessaire.

## Flashs

Pour ce chantier, le propriétaire a autorisé les flashs BOOT et SYSTEM sans demande préalable, avec tous les garde-fous habituels.

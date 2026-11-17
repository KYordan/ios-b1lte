# Build B1L-0105 : command buffers asynchrones et correctifs de plantage

Date : 2026-11-08. Base : B1L-0104.

## Vitesse : une barrière par command buffer

Avant : `-commit` avec un « completed handler » (QuartzCore en met sur presque tous) attendait que le GPU ait fini **tout** son travail (`rwa_finish`), sur le fil de rendu. CoreAnimation ne pouvait pas encoder l'image suivante pendant que le GPU dessinait : 2 à 7 images par seconde, avec un GPU occupé 2 à 7 %.

- **`apply.py` B1L-0105 (`air_metal.c`)** :
  - `rwa_flush_fence` rend la barrière de ce qui vient d'être soumis ;
  - `rwa_fence_wait` l'attend par l'écran (`fence_finish` sans contexte), ce qui est sûr depuis un autre fil ;
  - `rwa_fence_release` la libère.
- **`hakiosmetal_air.m`** :
  - `-commit` garde la barrière de son propre travail ;
  - les rappels « terminé » sont livrés depuis une file série du pilote (`b2l.metal.completion`), après l'attente de **cette** barrière, dans l'ordre des commits ;
  - `-commit` rend la main tout de suite ;
  - `-waitUntilCompleted` attend la barrière du buffer, depuis n'importe quel fil (`@synchronized`).

## Plantages

- `-commitAndWaitUntilSubmitted` est servi (notre `-commit` a soumis le travail quand il rend la main), au lieu de la méthode d'Apple qui lisait un état jamais posé.
- Vue de texture : `-parentTexture` rend la texture d'origine, gardée par objet associé.
- `doesNotRecognizeSelector:` de nos 10 classes écrit d'abord dans le journal le message non compris : `B2L: <classe> does not answer -<sélecteur>`.

Ensemble `b2l-metal-v61`, Mesa recompilé.

## Installé

- **Vérifié :** `rwa_flush_fence` et `rwa_fence_wait` exportés par `libhakiosair.dylib` ; `libhakiosmetal.dylib` contient la file `b2l.metal.completion` et le message « does not answer ».
- **SYSTEM :** SYSTEM-OK en 71 s. **BOOT :** relecture `e03e013e13b94100e758da9af2669c2724ad2ffe7d3af2c7f121fa0f3cb9600b` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0104/boot.img`.

## Résultat (journal `out/logs/0105-boot.txt`)

- Propriétaire : pas d'amélioration visuelle ni de fluidité ; **backboardd plante dès qu'une app s'ouvre** et repasse sur le CPU.
- **L'attente asynchrone fonctionne** : plus d'attente de job GPU (`kHakiosMaliWait`) sur le fil de rendu. Mais l'écran reçoit **exactement 10 images par seconde** (100 bascules / 10 s, très régulier) avec un GPU presque inactif : une cadence imposée.
- **Plantage** (2 démarrages ratés) : `objc_msgSend` sur un command buffer **déjà libéré**, dans le bloc `CA::OGL::MetalContext::flush … block_invoke_2` (exécuté sous `BlockMonitoring`). Le bloc fait `BOOL ok = [commandBuffer commitAndWaitUntilSubmitted]` (sélecteur lu dans le stub Objective-C). Metal garde un command buffer en vie jusqu'à la fin de son travail, et QuartzCore compte dessus. → B1L-0106.
- **Cadence** : CoreAnimation **n'active jamais les notifications vsync** du pilote d'écran (aucune ligne « notifications type »), et 12 sélecteurs IOMFB répondent « non pris en charge » (12, 15, 22, 25, 27, 29, 33, 50, 57, 68, 79, 84). Piste suivante.

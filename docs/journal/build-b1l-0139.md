# Build B1L-0139 : tracer si le chemin Metal consomme les instantanés d'icône 240×240

Date : 2026-11-09. Base : B1L-0138. Ensemble `b2l-metal-v88` (utilisateur uniquement ; noyau inchangé depuis 0138, seul le tag de l'ensemble change). Un changement de diagnostic, avec son instrumentation.

## Diagnostic (journaux 0136, 0137, 0138)

- Les surfaces `240x240 LA08` (instantanés d'icône vivant : chiffre du Calendrier, aiguilles de l'Horloge) sont créées par Spotlight, Preferences, MobileTimer et MobileCal, mais **jamais importées, jamais look up, jamais échantillonnées** par backboardd ni par aucun autre processus.
- Le build 0137 a instrumenté le noyau (lookup, create port, metadata) : **aucun `lookup` sur aucune surface** dans tout le journal.
- Le build 0138 a accepté le sélecteur IOSurface 17 pour `iconservicesagent` (`accepted 17`), mais **toujours aucun `lookup`** sur les surfaces `240x240`. Le sélecteur 17 n'était pas le blocage.
- Conclusion : le problème n'est pas dans le chemin IOSurface/noyau. Il faut vérifier si le chemin **Metal/userspace** consomme jamais ces surfaces : une texture `240x240` est-elle jamais créée, liée à un sampler, ou uploadée via `replaceRegion:` ?

## Changement (`hakiosmetal_air.m`, instrumentation `B2L ICON`)

1. **`newTextureWithDescriptor:`** : logue `B2L ICON: created 240x240 texture format %u type %u` quand une texture ordinaire de taille 240×240 est créée.
2. **`setFragmentTexture:atIndex:`** : logue `B2L ICON: bound 240x240 texture at fragment sampler %u` quand une texture 240×240 est liée à un sampler de fragment (une fois par texture distincte).
3. **`replaceRegion:`** : tagge `B2L ICON: upload to 240x240 texture ...` quand on upload des données vers une texture de destination 240×240.

## Installé

- **En cours :** build 0139, ensemble `b2l-metal-v88`.

## Résultat

- En attente du test et du journal 0139.
- À lire dans le journal : les lignes `B2L ICON`. Si aucune n'apparaît, le chemin Metal ne consomme absolument pas les surfaces `240x240` et le problème est ailleurs (probablement dans la façon dont SpringBoard récupère les icônes d'accueil, en dehors de IOSurface/Metal).

# Clavier inutilisable : diagnostic (2026-11-11)

Symptôme (propriétaire) : le clavier se fige, une touche met parfois une minute à s'afficher, le système bloque parfois.

## Méthode

- USERDATA relue en lecture seule (`pull-delta.sh`, image `out/live/ios26-userdata-kbd-164.img`, 638 blocs changés, 628 s).
- Instantanés système d'iOS (`CrashReporter/*.ips`, bug_type 509, pris par le watchdog de FrontBoard) symbolisés sur le cache d'origine
  (frames `[image, offset]` : image du cache `cc670b30…` → VA non glissée = 0x180000000 + offset, puis `dscname.py`).

## Constat

- Le CPU n'est pas saturé : sur 1 100 s de fonctionnement, les threads `idle` du noyau totalisent ~6 000 s (8 cœurs).
- Mais le **thread principal de Spotlight** est en marche permanente : **685 s de CPU sur 1 100 s**. Dans 5 instantanés sur 6
  (plusieurs démarrages), sa pile est la même :

```
AFDictationConnection _tellSpeechDelegateAvailabilityChanged (bloc)
 → UIDictationConnection dictationConnnectionDidChangeAvailability:
 → UIDictationController postNotificationName:
 → UISystemKeyboardDockController updateDockItemsVisibility…
 → configuredGlyphWithName: → UIImage imageNamed: → _UIImageCollectImagePathsForPath (recherche de fichiers)
```

  La disponibilité de la dictée change sans arrêt ; à chaque changement, le **dock du clavier** (bouton micro) recharge ses
  images sur le thread principal. Toute app qui affiche le clavier reçoit le même flot : son thread principal est occupé,
  les touches attendent derrière → délais d'une minute.
- Indices du déclencheur (non prouvé) : `-[AFDictationConnection networkAvailability:isAvailable:]` (accessibilité réseau),
  `kAFPreferencesDidChangeDarwinNotification` très fréquent dans os_log, plantages en boucle de SiriAUSP / MauiAUSP /
  MacinTalkAUSP / KonaSynthesizer (synthèse vocale).
- Préférences : aucune clé `Dictation Enabled` dans `com.apple.assistant.support.plist` → dictée active par défaut.

## Correctif proposé

1. Test sans build : Réglages → Général → Clavier → désactiver **Dictée** (et Siri si possible). Le réglage persiste.
2. Si confirmé et qu'on veut le rendre automatique : écrire `"Dictation Enabled" = false` dans
   `com.apple.assistant.support.plist` (USERDATA : accord du propriétaire requis), ou trouver la source des changements.

## Suite (même jour)

- L'option « Dictée » n'apparaît pas dans Réglages → Général → Clavier : le test sans build est impossible.
- `-[UIDictationController dictationEnabled]` (UIKit, 0x1897a7e48) appelle d'abord libMobileGestalt avec la clé `dictation`
  et renvoie NON si la réponse est fausse. Couper la capacité MobileGestalt `dictation` désactive donc la dictée dans
  tout UIKit, dock du clavier compris. Clé obfusquée du cache : `1uZbhSbBhsNCsVSsopZ4qg`
  (base64(md5("MGCopyAnswer" + "dictation"))[:22]).
- Le cache `com.apple.MobileGestalt.plist` n'est pas lisible sans droits root depuis l'image montée : son contenu n'est pas vérifié.

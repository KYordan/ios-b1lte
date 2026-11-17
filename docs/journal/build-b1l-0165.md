# B1L-0165 : gestes « retour à l'accueil » et multitâche

Ensemble : `b2l-metal-v88` (noyau seul, `--no-libs`). Part de 0164.

## Pourquoi

Le propriétaire signale que glisser depuis le bas de l'écran (retour à l'accueil, multitâche) ne fait rien.

- Le tactile descend bien jusqu'en bas (y jusqu'à 4082 sur 4095 dans les journaux 0150–0162) ; le port annonce un iPhone17,2
  (sans bouton d'accueil), donc iOS est bien en mode gestes.
- UIKit tire le type de bord d'un toucher du **masque d'événement** du doigt : `_UIEventHIDTouchEdgeTypeForHIDEvent`
  (0x1896e38d4) lit le champ 0xB0007 (`kIOHIDEventFieldDigitizerEventMask`) : 0x800 `FromEdgeTip` → 1, 0x1000 `FromCorner` → 2,
  0x2000 `SwipePending` → 3, 1<<18 `SwipeLocked` → 4 ; `_UITouchSetBasicTouchPropertiesFromEvent` en tire aussi `_edgeAim`
  (bits 24–27, direction du glissement). Le micrologiciel tactile d'Apple remplit ces bits ; notre `B2LHIDTouch` ne le faisait pas.

## Changement

`xnu-overlay/iokit/Kernel/arm/hid/B2LHID.cpp` : à l'appui, si le doigt commence dans une bande de 3 % (123/4095, ~90 px) d'un
bord, le pilote fixe pour toute la durée du toucher `FromEdgeTip` + la direction vers l'intérieur (bas → `SwipeUp`,
haut → `SwipeDown`, gauche → `SwipeRight`, droite → `SwipeLeft`) dans le masque du doigt et de la collection.
Le journal `B2L HID: touch … edge=0x…` montre ces bits.

## Résultat

Journal 0165 : les touchers partant du bas portent bien `edge=0x1000800` (FromEdgeTip + SwipeUp), et ceux du haut `0x2000800`. Testé par le propriétaire : **le glissement depuis le bas marche** (retour à l'accueil, multitâche).

# B1L-0203 — événements Wi-Fi d'iOS 26 (fin de join), traceur retiré

Base : `b2l-metal-v95`, `--no-libs`. Comme 0202 (SHA-1), avec ces changements :
- traceur d'ioctl retiré (`xnu/bsd/kern/sys_generic.c` restauré depuis `../sys_generic.c.bak`). Il bloquait 50 ms à
  chaque ioctl, et Safari et WebKit en ont fait environ 2 400 ;
- client d'événements Wi-Fi d'iOS 26 ajouté (détail ci-dessous).

## Cause de « échec du join » trouvée par désassemblage (cache 23G90, `/usr/sbin/wifid` de OS.dmg)
- wifid rejoint par `Apple80211AssociateAsync` et attend `APPLE80211_M_ASSOC_DONE` (9). Il ne demande jamais
  `ASSOCIATE_RESULT` (21) : `__waitForJoinResult` ne sert qu'au join synchrone.
- `Apple80211EventMonitoringInit2` appelle `Apple80211EventMonitoringHalt` puis `Apple80211EventMonitoringInitIOUC`, et
  rien d'autre : plus de socket d'événements.
- `InitIOUC` passe par la connexion de type 1 au contrôleur (le journal montrait `wifid open IO80211Controller type 1`,
  refusée). Elle appelle, dans l'ordre :
  - la struct 5 (1 octet en sortie, qui doit être ≠ 0) ;
  - la struct 3 ;
  - `IOConnectMapMemory` type 1 (lecture seule, la file) et type 2 (deux u32 : lecture, écriture) ;
  - l'appel asynchrone 2 (port de réveil ; le rappel est `Apple80211EventMonitoringHelper`).
- Une entrée de la file : en-tête de 48 octets (u32 87 ; u16 longueur à +24 ; u16 octets sautés à +26 ; u32 événement à
  +28 ; nom d'interface sur 16 octets à +32), puis les données. Un u32 80, ou moins de 48 octets avant la fin de la file,
  veut dire « avancer de 4 octets ».
- `__writeEventBuffer` en refait le `kern_event_msg` du socket : les données restent celles d'avant.

## Correctif
- `iokit/Kernel/arm/B2LHakiosWLAN.cpp` :
  - `IO80211Controller::newUserClient` type 1 → `B2L80211EventClient` (file de 64 Ko, page de contrôle) ;
  - `b2l_80211_event_post()` écrit l'événement pour chaque client puis réveille wifid (`sendAsyncResult64`) ;
  - les autres types restent refusés, donc les requêtes restent des ioctl.
- `bsd/dev/b2l_hakios_apple80211.c` `post()` : chaque événement va aussi à `b2l_80211_event_post()`.
- Journal : `B2L 80211 EVENTS: ...` (ouverture, sélecteurs, 40 premiers événements, pertes).

## À tester
- Join depuis Réglages : coche et « connecté » attendus.
- Internet hors Safari (App Store, Météo, recherche web de Spotlight).
- Safari Wi-Fi coupé, pour savoir si le gel de Safari vient du Wi-Fi ou de WebKit (JIT rejeté par l'émulation arm64e).

## Résultat
Le client IOKit n’a servi à rien. Le type 1 sur `AppleWLANDriver` est ouvert par wifid lui-même (sélecteur 0), pas par IO80211, et toutes les lignes disent `no reader`. Le framework, lui, ouvre `HakiosWLAN` type 0 ; c’est refusé, et il retombe alors sur le socket d’événements (vendeur 1, classe 6, sous-classe 1 : ce que nous envoyons). La feuille de mot de passe reste ouverte. Gel environ 2 s après l’ouverture de l’App Store, 17 s après le join. Retiré en 0204.

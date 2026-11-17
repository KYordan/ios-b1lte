# Build 161 : UUID de plateforme synthétique (débloque configd)

## Diagnostic

Build 160 : le `start()` d'IPConfiguration dans configd attend indéfiniment dans `gethostuuid()`. XNU (arm) ne publie la ressource `IOPlatformUUID` que si `/chosen` contient `unique-chip-id` + `chip-id`, et notre arbre n'a ni l'un ni l'autre. Les `prime` de configd ne tournent donc jamais, `State:/Network/Interface` reste vide, et wifid ne voit pas en0.

## Changement

**Hors fichiers Wi-Fi (accord du propriétaire du 11/11/2026)** :
- `xnu-overlay/iokit/Kernel/arm/B2LPlatformExpert.cpp` (copié dans `xnu/`) : après `IODTPlatformExpert::start`, si le fournisseur n'a pas d'`IOPlatformUUID`, il pose un UUID synthétique fixe (`02423240-4c10-5000-8000-0000000000b2`, pas celui du téléphone) et publie la ressource. Journal : « B2L: synthetic IOPlatformUUID published ». `unique-chip-id` n'est volontairement pas ajouté, pour ne pas changer l'identité vue par MobileGestalt.
- `xnu-overlay/osfmk/arm64/b2l_prof.c` : le profileur vise de nouveau `backboardd`.

Les traces 157-159 restent pour vérifier que configd lit IFLIST et interroge en0.

## Attendu

configd passe ses `start()`, puis le KernelEventMonitor journalise « Prime ». configd liste les interfaces (`B2L RTSOCK` IFLIST) et interroge en0 (`net: en0 is asked ... by configd`). wifid ne journalise plus « en0 does not exist in dynamic store ».

## Installé avec hash BOOT

- Build : **161**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `11646b587ea59eb2643026e225cf757c0c315025c6fe8641c741b32b9b101b15` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 54 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Ça marche : le Wi-Fi s'allume dans l'interface (constaté par le propriétaire). Dans le journal :
- `B2L: synthetic IOPlatformUUID published: yes` ;
- configd lit IFLIST (`B2L RTSOCK`) et interroge en0 ;
- wifid ouvre HakiosWLAN et envoie ses requêtes Apple80211.

Pas de liste de réseaux : `SCAN_REQ` est refusé (erreur 22), car la requête iOS 26 fait 5528 octets. Corrigé au 162.

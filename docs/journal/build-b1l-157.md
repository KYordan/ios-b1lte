# Build 157 : WiFi, trace des ioctls de configd sur en0 (diagnostic)

## Diagnostic

Build 156 : `en0` est nommée Wi-Fi par l'InterfaceNamer, mais `wifid` ne la trouve pas dans `State:/Network/Interface` (liste tenue par le KernelEventMonitor de configd : `link_add` à l'arrivée de `KEV_DL_IF_ATTACHED`, puis `SIOCGIFMEDIA` sur l'interface). Le pilote `en0` ne trace pas les ioctls de configd (exclu depuis le 152).

## Changement

**Diagnostic seul** : `xnu-overlay/bsd/dev/b2l_hakios_net.c` (copié dans `xnu/`) trace aussi les ioctls de `configd` sur `en0`, 40 au plus (compteur séparé ; les 80 des autres processus sont inchangés). Si configd n'envoie aucun `SIOCGIFMEDIA` (0xc02c6938 / 0xc0286938) à `en0`, il n'a jamais traité son arrivée.

Lecture : journal noyau seul (pas de relecture de USERDATA).

## Installé avec hash BOOT

- Build : **157**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `7acdeb11d6b92a457cad06d73375190d35e58db26d842e5ace061d43ef9f7117` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 75 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Journal `out/logs/0157-boot.txt`, boot sans régression.

- **Aucun ioctl de `configd` sur `en0`** ; `wifid` en envoie une vingtaine (dont `SIOCGIFMEDIA` 0xc02c6938).
- `en0` est attachée très tôt (ligne 956, avant les tâches de démarrage de launchd) : `configd` démarre donc avec `en0` déjà présente, et sa liste initiale (getifaddrs) devrait la contenir. Son KernelEventMonitor ne la traite pourtant jamais (`link_update_status` ferait un `SIOCGIFMEDIA`).
- Les Info.plist de KernelEventMonitor, InterfaceNamer et IPMonitor sont normaux (`Builtin`, rien de désactivé). Le noyau émet bien `KEV_DL_IF_ATTACHED` (`dlil.c:5345`).
- Suite proposée (hors fichiers Wi-Fi, accord demandé) : tracer, pour `configd` seulement, l'ouverture de sa socket d'événements noyau et son filtre (`kern_event.c`), et la lecture de la liste des interfaces (`sysctl_rtsock`, `NET_RT_IFLIST2`) avec son résultat.

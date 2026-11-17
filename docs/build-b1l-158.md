# Build 158 : WiFi, trace de configd (socket d'événements, liste des interfaces) (diagnostic)

## Diagnostic

Build 157 : `configd` n'envoie aucun ioctl à `en0`, alors que `en0` existe avant son démarrage. Son KernelEventMonitor ne la met jamais dans `State:/Network/Interface`, où `wifid` la cherche.

## Changement

**Diagnostic seul, hors fichiers Wi-Fi (accord du propriétaire du 11/11/2026)**, sous `#if defined(B2L)`, ne journalise que le processus `configd` (20 lignes au plus par point) :

- `xnu-overlay/bsd/kern/kern_event.c` (nouveau dans l'overlay, copié dans `xnu/`) : `kev_attach` (`B2L KEV: configd opens an event socket`) et `SIOCSKEVFILT` (`B2L KEV: configd filter … vendor class subclass`).
- `xnu-overlay/bsd/net/rtsock.c` (nouveau dans l'overlay) : fin de `sysctl_rtsock` pour `NET_RT_IFLIST`/`NET_RT_IFLIST2` (getifaddrs) : erreur et taille (`B2L RTSOCK: configd op … -> error …, … bytes`).

La trace des ioctls de configd sur `en0` (157) reste.

## Installé avec hash BOOT

- Build : **158**, SET `b2l-metal-v88` (`--no-libs`). Premier essai : erreur de compilation (`-Wbounds-safety-strict-terminated-by-cast` dans `rtsock.c`), corrigée avec `__unsafe_null_terminated_from_indexable(who)`.
- BOOT relu : `eae46d4d97cbee7307387f0c483aa94e2eb1481e1c8f46d4c5079c5c5047e407` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 57 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Journal `out/logs/0158-boot.txt`, boot sans régression, Wi-Fi grisé (attendu).

```
B2L KEV: configd opens an event socket 0xfffffff5ec2bfc40
B2L KEV: configd filter 0xfffffff5ec2bfc40 vendor 1 class 1 subclass 0
HakiosWLAN: IONetworkStack opened; registry busy state 0
```

- configd ouvre sa socket d'événements (Apple / réseau / toutes sous-classes) : le `load` de KernelEventMonitor a lieu.
- **Aucun `NET_RT_IFLIST`/`IFLIST2`** de configd sur tout le démarrage, et toujours aucun ioctl sur `en0` : le `prime` de KernelEventMonitor (getifaddrs puis `link_add` de chaque interface) n'est jamais atteint. configd charge, démarre puis « prime » ses modules dans l'ordre : un module bloque probablement avant.
- Référence ios-pipa (iOS 26, Wi-Fi complet, `NOTES.md` boot 55) : même méthode, trace des appels IOKit de configd.

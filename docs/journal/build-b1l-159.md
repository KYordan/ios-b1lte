# Build 159 : WiFi, trace IOKit de configd (diagnostic)

## Diagnostic

Build 158 : configd ouvre sa socket d'événements mais ne lit jamais la liste des interfaces ; son KernelEventMonitor ne passe jamais au `prime`. Hypothèse : un module de configd attend un service IOKit qui n'existe pas sur le portage.

## Changement

**Diagnostic seul**, dans la trace IOKit du 153 (`xnu-overlay/iokit/Kernel/IOUserClient.cpp`, copié dans `xnu/`, accord du propriétaire pour cette trace) :

- le processus `configd` est ajouté aux processus tracés (recherches de services, ouvertures de user clients) ;
- les enregistrements de notifications IOKit (`internal_io_service_add_notification`) sont tracés aussi, avec le type de notification et si un service correspondant existe déjà (`found=`).

Les traces `kern_event.c`, `rtsock.c` (158) et ioctls `en0` (157) restent.

## Installé avec hash BOOT

- Build : **159**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `b9e093328cb8176f4c3bf50795b73e5f5c726e99ddaa31c00a21c23775bacbf5` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 57 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Journal `out/logs/0159-boot.txt`, boot sans régression.

```
B2L IOKIT configd IOServiceFirstMatch found=1 … IONetworkStack
B2L IOKIT configd open IONetworkStack type 0
B2L IOKIT configd IOServiceFirstMatch found=1 … IONetworkInterface
```

- configd ne fait que ces appels IOKit, tous satisfaits : **il n'attend aucun service IOKit**.
- Code de `/usr/libexec/configd` (otool) : KernelEventMonitor est intégré au binaire. `prime_KernelEventMonitor` (0x1000261ec) fait seulement `dispatch_async` sur la file du module (0x1000857b8) ; le bloc (0x100026254) appelle `prime` (0x1000262a8 : log « Prime », puis la fonction 0x100026e54 qui fait `getifaddrs` et `link_add`). Jamais exécuté au 158 : soit la phase `prime` de configd n'est pas atteinte (un module bloque dans `load`/`start`), soit la file de KernelEventMonitor est occupée.
- Suite proposée : le profileur `osfmk/arm64/b2l_prof.c` (113), qui relève les piles des fils en attente, pointé sur `configd` au lieu de `backboardd` (accord demandé).

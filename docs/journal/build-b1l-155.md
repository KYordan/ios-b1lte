# Build 155 : WiFi, capture des messages de `wifid` (diagnostic)

## Diagnostic

Build 154 : `wifid` trouve `AppleWLANDriver` mais ne demande toujours ni l'alimentation ni un scan. La raison est dans ses messages `os_log`, que le portage ne capture pas ; l'analyse statique de `/usr/sbin/wifid` (23G90) est trop coûteuse (fonctions éclatées).

## Changement

**Diagnostic en lecture seule, hors fichiers Wi-Fi (accord du propriétaire du 11/11/2026)** :

- `xnu-overlay/bsd/kern/b2l_exec_graphics_diag.h` : à l'exec de `/usr/sbin/wifid`, ajout de `OS_ACTIVITY_DT_MODE=YES` à son environnement (même table que les réglages de `backboardd`) : ses messages `os_log` sont recopiés sur sa sortie d'erreur.
- `xnu-overlay/bsd/kern/sys_generic.c` (copié dans `xnu/`), sous `#if defined(B2L)` : dans `write_nocancel` et `writev_nocancel`, ce que le processus `wifid` écrit sur le descripteur 2 est recopié ligne par ligne dans le journal persistant (`B2L WIFID| …`), 480 octets par appel, 1 500 lignes au plus par démarrage. Coupure : `b2l_wifid_log=0`. L'écriture elle-même n'est pas modifiée.

Le reste est inchangé (trace IOKit du 153, `AppleWLANDriver` du 154). Compilation OK.

## Installé avec hash BOOT

- Build : **155**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `b2c89b30b3adcb91a575cd46d0e255d4859627507e42e7c0ea17c832da9ecdd7` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 66 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Boot iOS sans régression** (2 996 lignes `hakiosmetal[backboardd]`, aucun `B2L CRASH`, journal `out/logs/0155-boot.txt`). Wi-Fi toujours inactif.

```
B2L WIFID| CoreData: debug: Persistent History is disabled - 0
```

- La capture de la sortie d'erreur de `wifid` fonctionne (une ligne, écrite par CoreData lui-même).
- Aucun message `os_log` recopié : soit `OS_ACTIVITY_DT_MODE` n'a pas été injecté, soit libtrace ne le recopie pas pour ce démon. Non vérifiable : `b2l_early_trace` (trace de l'injection) écrit sur la console, pas dans le journal persistant.
- Suite sans nouveau build : lecture seule de USERDATA (racine iOS, tranches modifiées seulement, `pull-chunked.sh`) pour relire les journaux persistants de logd (`/private/var/db/diagnostics`).

### Lecture des journaux de logd (USERDATA en lecture seule)

- Image `out/live/ios26-userdata-diag-155.img` (64 tranches vérifiées), montée avec un fichier `-shadow` : l'image n'est pas modifiée.
- `log show` du Mac refuse l'archive (pas de `timesync`). Les tranches `tracev3` sont décompressées à la main (LZ4 `bv41` via libcompression) ; démarrage 155 = boot UUID `fbbe157a…` (`Persist/…94`, `Special/…b4`).
- `wifid` : `__WiFiManagerSetEnableState: state TRUE` (il veut allumer), santé matérielle OK (`success 1 with status 1`), puis **`en0 does not exist in dynamic store`** : aucun appareil Wi-Fi n'est créé.
- Code de `wifid` (otool) : il lit `State:/Network/Interface` (`SCDynamicStoreKeyCreateNetworkInterface`) et cherche `en0` dans la liste `Interfaces`.
- `/var/preferences/SystemConfiguration/NetworkInterfaces.plist` : `Interfaces` **vide**. L'`InterfaceNamer` de configd n'a jamais déclaré le registre calme : il attend d'ouvrir `IONetworkStack` (HakiOS : `MiniIONetworkStack.cpp`), que le portage n'a pas.

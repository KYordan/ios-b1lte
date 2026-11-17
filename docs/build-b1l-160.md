# Build 160 : WiFi, profileur pointé sur configd (diagnostic)

## Diagnostic

Build 159 : configd n'attend aucun service IOKit, mais le `prime` de son KernelEventMonitor (`dispatch_async` sur la file du module) ne s'exécute jamais. Il faut savoir où ses fils attendent.

## Changement

**Diagnostic seul, hors fichiers Wi-Fi (accord du propriétaire du 11/11/2026)** : `xnu-overlay/osfmk/arm64/b2l_prof.c` (profileur du 113, copié dans `xnu/`) vise `configd` au lieu de `backboardd` (`B2L_PROF_TARGET`), et écrit à chaque rapport l'adresse de l'image principale (`task->mach_header_vm_address`) pour symboliser les adresses de `configd`. Comportement de backboardd inchangé ; sa mesure est suspendue pour ce build. Les traces 157-159 restent.

Lecture : `B2L PROF WAIT` (pc, lr, 8 adresses de retour), symbolisées avec `b1l-tools/dsc/dscsym.py` (cache) et le désassemblage de `configd`.

## Installé avec hash BOOT

- Build : **160**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `1568c24f8712ef0c8a298497613a2426d5c34168a3b907cca7843d95fcf57ae0` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 56 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Cause trouvée. Le fil principal de configd (`tid 0x151`) attend 100 % du temps :
- configd est chargé à 0x101310000 ; le décalage 0x20cb0 suit l'appel `start()` d'un bundle (« calling start() for %@ ») ;
- les adresses 0x1031xxxxx sont dans `IPConfiguration.bundle` (base 0x103100000) : `0x30f70 bl 0x3ad70` → `0x3adb8 bl 0x431d8` → `0x4321c bl _gethostuuid`.

Le `start()` d'IPConfiguration ne revient donc jamais. Les `prime` de tous les modules (dont KernelEventMonitor) attendent la fin de tous les `start()`. Résultat : `State:/Network/Interface` n'est jamais rempli, et wifid ne voit pas en0.

Dans XNU, `gethostuuid` appelle `IOBSDGetPlatformUUID`, qui attend la ressource `IOPlatformUUID` sans délai. Sur arm, cet UUID est calculé à partir de `/chosen unique-chip-id` + `chip-id`, et notre arbre n'a ni l'un ni l'autre : la ressource n'est jamais publiée.

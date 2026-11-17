# Build B1L-0246 : MTLEvent / MTLSharedEvent

Date : 2026-11-14. Base : B1L-0245. Ensemble `b2l-metal-v117`.

## Cause (échec 7 de 0242 : `newSharedEvent` nil)

Le pilote ne répondait pas à `-newSharedEvent` / `-newEvent` (nil), et `-encodeSignalEvent:value:` ne faisait rien.

## Changement (`hakiosmetal_air.m`)

- Classe `HakiosMetalSharedEvent` (`<MTLSharedEvent>`) : `signaledValue` (lecture/écriture), `notifyListener:atValue:block:`
  (bloc appelé sur la file du listener quand la valeur est atteinte), `waitUntilSignaledValue:timeoutMS:`.
- Le command buffer note ses `encodeSignalEvent` et pose les valeurs quand il est fini (après les recopies 0236/0243),
  même si personne ne l'attend (attente de sa barrière sur la file du pilote, comme en 0240).
- Ligne `B2L EVENT: … asked for` une fois par processus : dira si SpringBoard / RenderBox (icônes) utilise des événements.
- Limites notées : pas de poignée inter-processus (`newSharedEventHandle` nil) ; `encodeWaitForEvent` n'attend pas (file unique, dans l'ordre).

## Installé

- Premier essai de compilation refusé sur le Mac (deux `\n` devenus de vrais retours à la ligne dans `RW_ONCE`), corrigé avant le build.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `18fa0c6270204081853753ff98aa0698e625de26bd40bbe4e72cb22d5f6a4080` (BOOT-OK). Pas redémarré : 0247 suit.

## Résultat

- OK : « MTLSharedEvent » valeur 7 (attendu 7). Une seule ligne `B2L EVENT`, venue de Dictaphone : SpringBoard et iconservicesagent ne demandent pas d'événement, ce n'est donc pas la cause des icônes manquantes.

## Retour en arrière

- `hakiosmetal_air.m.before-0246`.

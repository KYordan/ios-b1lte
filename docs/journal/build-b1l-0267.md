# B1L-0267 — diagnostic : qui garde le verrou global, et combien de temps

## Pourquoi

- 0266 : interface très lente ; icônes justes à l'arrêt, fausses dès qu'il y a du mouvement. Pendant l'usage, les fils de SpringBoard
  attendent le verrou global jusqu'à 4,1 s sur 5 s, ceux de backboardd 2,5 s. Il faut savoir quel endroit le garde aussi longtemps
  avant de le découper.

## Changement (`hakiosmetal_air.m`, sauvegarde `.before-0267`), sans effet sur le rendu

- Chaque prise extérieure du verrou note son endroit (`__func__` de la méthode, ou « commit work loop », « waitUntilCompleted finish »)
  et la durée tenue. Avec chaque ligne `B2L LOCK` (toutes les 5 s) : 4 lignes `B2L HOLD: <endroit> X ms in N, longest Y ms`.
- Lignes `B2L LOCK` : 60 par processus au lieu de 20, pour couvrir l'usage après le démarrage.
- Set v130 → v131. Mesa inchangé (0263 et 0266 restent).

## Installé

- SYSTEM-OK, BOOT `9f02da4e2320bee11e8418e8587ce352305be1780fb98f33dd0d238b3176ad01`, BOOT-OK ; « B2L HOLD » présent dans `libhakiosmetal.dylib` v131.

## Résultat (log noyau `out/logs/0267-boot.txt`)

- Test Metal de Dictaphone : 43 OK, 1 ÉCHEC (MSAA 4x pas annoncé), 1 SAUTÉ ; `CAMetalLayer` 9,5 images/s.
- Temps de verrou tenu, cumulé sur le log, par processus et endroit :

| Processus | Endroit | Total | Plus longue prise |
|---|---|---|---|
| iconservicesagent | `newRenderPipelineStateWithDescriptor:` | 39,6 s | **18,3 s** |
| cameracaptured | `newRenderPipelineStateWithDescriptor:` | 31,9 s | 8,3 s |
| backboardd | `-commit` (boucle de travail) | 10,3 s | 2,0 s |
| backboardd | `newRenderPipelineStateWithDescriptor:` | 8,1 s | 0,8 s |
| SpringBoard | `newRenderPipelineStateWithDescriptor:` | 4,6 s | 0,6 s |
| backboardd | `replaceRegion:` | 3,9 s | 0,1 s |
| SpringBoard | `-commit` | 3,8 s | 0,2 s |
| backboardd | `newTextureWithDescriptor:` | 2,8 s | 0,4 s |
| SpringBoard | `hakiosWriteBack` | 2,1 s | 0,4 s |

- Conclusion : la création des pipelines (traduction AIR → NIR puis compilation panfrost) tient le verrou unique du processus pendant
  des secondes ; pendant ce temps, aucun dessin du même processus ne part. Première optimisation : B1L-0268.

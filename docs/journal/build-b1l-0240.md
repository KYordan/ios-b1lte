# Build B1L-0240 : recopie du dessin même quand personne n'attend

Date : 2026-11-14. Base : B1L-0239 (diagnostics gardés). Ensemble `b2l-metal-v111`.

## Pourquoi

Voir le résultat de 0239 : la recopie des dessins de RenderBox (icônes) vers la mémoire de l'appelant ne se faisait
que si un `waitUntilCompleted` ou un completed handler était posé sur le command buffer.

## Changement (`hakiosmetal_air.m`, `hakiosDeliverHandlers`)

- Un command buffer qui dessine dans une texture posée sur un tampon attend désormais sa barrière sur la file
  `b2l.metal.completion` (comme ceux qui ont des handlers), puis recopie, même sans handler.
- Compteur : `B2L WRITEBACK: N command buffers with nobody waiting, written back anyway` (puissances de 2).

## Installé

- **Vérifié :** « nobody waiting » dans `build-metal/b2l-metal-v111/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `258b80304f4521449adca1f3c67f2367645d29c2f962c5f93e2bf6c8ca7ab364` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0240-boot.txt`)

- **Aucune amélioration** (propriétaire). **Aucune ligne « nobody waiting »** : chaque command buffer qui dessine sur un tampon était déjà attendu. Hypothèse de 0239 fausse ; le correctif est sans effet mais sans danger (gardé).
- Toujours 3 sortes de cible recopiées dans SpringBoard (120×120 R8, 120×120 BGRA, 122×122 RGB10A2).

## Retour en arrière

- `hakiosmetal_air.m.before-0240` (état 0239).

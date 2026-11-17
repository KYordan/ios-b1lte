# Build B1L-0075 : Mesa et le pilote Metal sans retours signés

Date : 2026-11-07. Base : B1L-0074.

## Changement

- **`-fno-ptrauth-returns`** pour Mesa (`cross.ini` de `build-mesa.sh`, en C, C++ et Objective-C) et pour le pilote et les tests Metal (`build-metal-scion.sh`).
  - La signature de l'adresse de retour (`pacibsp`/`retab`) est interne à chaque fonction, donc la retirer ne change aucune interface avec iOS.
  - Essai sur le Mac : `pacibsp` et `retab` deviennent un simple `ret`.
- **Ce qui reste signé :** les appels par pointeur de fonction (`blraaz`). Les pointeurs donnés au système (`pthread_create`, `qsort`…) doivent rester signés, sinon iOS les refuse.
- **`build-mesa.sh` :** si `cross.ini` change, le dossier de build est supprimé, ce qui force une recompilation complète avec les nouvelles options.
- **Ensemble `b2l-metal-v31`**, et le noyau pointe vers v31.

## Attendu

- **`M5 bench` :** le temps par dessin devrait baisser nettement.
- **Ce qui ne baissera pas :** les appels par pointeur signés restent, ainsi que les retours dans le code d'Apple (Metal, Foundation, libobjc). Le chiffre restant mesurera ce coût.

## Installé

- **Vérification :** `libhakiosair` et `libhakiosmetal` ne contiennent plus aucun `retab` (contre 4328 et 114 avant).
- **SHA-256 :** `libhakiosair` `90709fad…`, `libhakiosmetal` `a4b18d9f…`, `libb2l-metal-test` `23162ee4…`.
- **SYSTEM :** 5 blocs réécrits, en 58 s.
- **BOOT :** `out/b1l-metal-0075/boot.img`, relecture `38f45956a032a146a7ed926202332f7d455e8f75c2054b42a1e40b48415f99ee` (BOOT-OK).

## Note : le noyau accepte déjà les pointeurs non signés

- `b2l_arm64e_compat.c` émule PAC en mode « stripped-pointer ABI » :
  - `pac*` laisse le pointeur tel quel ;
  - `aut*`, `retab` et `blraa*` retirent les bits de signature et acceptent le pointeur, en vérifiant seulement que la cible est exécutable ;
  - seul `pacga` calcule une vraie valeur (SipHash).
- **Conséquence 1 :** compiler aussi sans appels signés (`-fno-ptrauth-calls`) ne casse aucune interface. C'est l'étape suivante (B1L-0076), pour retirer les 3067 `blraaz`.
- **Conséquence 2 :** un patch statique du code d'Apple (`retab` → `ret`, `blraaz` → `blr`, `aut*` → NOP) serait sûr sémantiquement, sauf pour corecrypto, qui vérifie l'intégrité de son code.

## Résultat (`out/logs/0075-boot1.txt`) : environ 3 fois plus rapide

- **200 dessins dans une passe :** 130 à 182 ms, soit **0,65 à 0,91 ms par dessin**, contre 1,2 à 3,4 ms dans 0074. Le GPU reste à environ 1 ms d'attente.
- **Deux triangles :** 3,6 à 4,2 ms (contre 7,8 à 10,8).
- **`nextDrawable` :** 9,0 ms (contre 20,6).
- **Compilation de la paire de shaders :** 0,22 à 0,37 s (contre 0,43 à 0,80).
- **Toujours juste partout :** `0 rows differ` à chaque mesure, `presentDrawable` en statut 4.

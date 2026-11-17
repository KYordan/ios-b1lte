# Build B1L-0077 : profil des pièges PAC d'un dessin, et mesure sur gros cœur

Date : 2026-11-07. Base : B1L-0076 (0,55 à 0,58 ms par dessin). Build de mesure : le pilote est inchangé.

## Changements

**Noyau :**
- `b2l_arm64e_compat.c` : un « fil surveillé ». Pour ce fil seulement, le noyau compte les pièges par type (les 9 classes de B1L-0050) et par adresse, pour les 64 premières adresses distinctes.
- `b2l_mali_core.c` : `sysctl kern.hakios.pac_watch`. Écrire 1 surveille le fil appelant, en repartant de zéro ; écrire 0 arrête. Une lecture rend 138 mots : 9 types, 64 adresses, 64 compteurs, et les pièges sans case libre.

**Test :** la mesure de 200 dessins tourne deux fois :
1. là où le planificateur met le fil ;
2. puis épinglée sur un gros cœur (`kern.hakios.bind_cpu=7`, un M4), car le planificateur ne distingue pas les A55 des M4.

La dernière passe de chaque série est profilée, avec deux lignes :
- `M5 PAC (…)` : pièges par dessin et par type ;
- les 16 adresses les plus fréquentes, traduites en bibliothèque et fonction par `dladdr`.

**Ensemble `b2l-metal-v33`**, et le noyau pointe vers v33.

## Ce qu'il faut regarder

- Combien de pièges coûte un dessin, et chez qui : Metal d'Apple, libobjc, libsystem par les relais signés, ou encore nos bibliothèques.
- L'écart entre « any core » et « big core ».

## Installé

- **Noyau :** il contient bien le sysctl `pac_watch`.
- **SYSTEM :** 5 blocs réécrits, en 94 s.
- **BOOT :** `out/b1l-metal-0077/boot.img`, relecture `afefb37dbeb62ea61734fac6eb35f56f0bfa56ee18705252404abc3663500716` (BOOT-OK).

## Résultat (`out/logs/0077-boot1.txt`) : le coût restant vient des relais de l'éditeur de liens

- **48 pièges PAC par dessin, tous des sauts signés (`braa`), aucun dans le code d'Apple** (`in the shared cache 0`).
- **D'où ils viennent :**
  - les relais `objc_msgSend$…` de `libhakiosmetal` (section `__objc_stubs` : `mesaBuffer` 12 par dessin, puis `mesaTexture`, `vertexStreamCount`, `recordDraw:`, `writesDepth`, `recordWork:`, `addObject:`…) ;
  - les relais `__auth_stubs` de `libhakiosair` vers libsystem (les adresses `?+0`).
- **Le compte explique tout le temps restant :** 48 × ~11 µs ≈ 0,53 ms, pour 0,55 à 0,7 ms mesurés par dessin.
- **Le gros cœur n'aide pas :** 1,55 à 2,07 ms épinglé sur le CPU 7, contre 0,65 à 1,75 ms sans épinglage, et le même nombre de pièges. Le coût est dans le piège, pas dans la vitesse du cœur.

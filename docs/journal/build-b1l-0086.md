# Build B1L-0086 : récupérer les modules spécialisés de backboardd

Date : 2026-11-07. Base : B1L-0085.

## Changements

- **Noyau** (`b2l_userspace_seed.h`) : crée au démarrage `/b2l-dump`, avec les droits 01777, sur SYSTEM (`b2l_open_directory`).
- **Bibliothèque** (`apply.py`, B1L-0086), dans backboardd seulement : chaque pipeline de rendu écrit son module de fragment (`f-<nom>.air`) et de vertex (`v-<nom>.air`), une fois par nom et 160 fichiers au plus.
- **Récupération :** au flash suivant, `system-add-fast.sh` relit les blocs modifiés de SYSTEM dans `out/live/b1l-system-live.img`. Il suffit alors d'ouvrir cette image sur le Mac pour lire `/b2l-dump`, puis de traduire et compiler les vrais shaders de backboardd hors du téléphone.
- Ensemble `b2l-metal-v42`, plug-in `B2LMetalv42`.

## Installé

- **SYSTEM :** 6 blocs réécrits, en 75 s.
- **BOOT :** `out/b1l-metal-0086/boot.img`, relecture `f82d31487a7d529c118ea34635d4eb3a8f3caaba21185a0d72d8e4f8179539f5` (BOOT-OK).

## Résultat (`out/logs/0086-boot1.txt`)

- **Écran :** le même vert en escalier ([b1l-0086-ecran.jpg](b1l-0086-ecran.jpg)).
- **`/b2l-dump` est créé (`error=0`), mais sur USERDATA :** la racine du téléphone est `rd=b2l-userdata`, et SYSTEM n'y est pas monté (le noyau en lit les fichiers directement). Le Mac ne peut donc pas relire les modules par ce chemin ; `b1l-tools/system-pull-changed.sh` (relecture des blocs modifiés de SYSTEM, sans écriture) l'a confirmé.
- **Même schéma de faute qu'en 0085 :**
  - la 1ʳᵉ faute 0x51 suit la création du pipeline **`A2Xhf` (7 instructions)** ;
  - la 2ᵉ suit celle de **`TimgA2Xhf_Isrc`** ;
  - chaque fois, l'image est envoyée à l'écran juste après.

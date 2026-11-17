# B1L-0257 — le traducteur AIR→NIR garde un pointeur rangé dans une variable locale (RenderBox custom effect)

## Causes (log noyau et journaux iOS du démarrage 0256, USERDATA `ios26-userdata-icons-256.img` lu sans écrire)

1. **Pipeline refusé** : `B2L REFUSED SpringBoard: f-ffgea5cmba5dhmhba3 … v-custom_effect_vertex …: a store of a pointer into
   memory` (19 fois ; 14 en 0254 ; 0 avant, quand SpringBoard abandonnait faute de style d'icône). C'est l'effet RenderBox
   qui pose le pictogramme dans l'icône : le pictogramme lui-même est bien dessiné (image `l-img-SpringBoard-120x120-f10`,
   le combiné du Téléphone). Le fragment appelle `custom_fn` (fonction liée) ; une variante de `custom_fn` range sa
   texture dans le champ 0 de sa variable locale `RB::Layer` puis la relit. Notre traducteur refusait toute écriture
   d'un pointeur en mémoire : le commentaire de l'`alloca` annonçait un « AIR_VAL_SLOT » qui n'a jamais été écrit.
2. **Cache d'icônes perdu** : l'ancien USERDATA (0248) avait 370 icônes déjà dessinées dans
   `systemgroup.com.apple.lsd.iconscache` (71 Mo, datées 1970, donc d'avant 0207) ; le USERDATA propre de 0250 n'en a plus
   que 3. Les icônes s'affichaient avant 0249 parce qu'elles étaient lues en cache, pas dessinées.

## Changement (Mesa, `userspace/mesa/air-b2l/apply.py`, section B1L-0257 ; set **v122**)

- `air_to_nir.c` / `air_private.h` : un pointeur écrit dans une variable locale à adresse constante est gardé tel quel
  au moment de la traduction (`b2l_ptr_slots`) et rendu par une lecture de la même adresse. Deux pointeurs différents au
  même endroit → la relecture reste refusée (on ne devine pas lequel). Rien d'autre ne change.
- `air_module.c` : l'enregistrement de module 20 (GUIDLIST, LLVM récent) est ignoré (il ne sert qu'à ThinLTO).
- `air2nir.c` (outil Mac seulement) : `B2L_LINK=fichier.air` relie une fonction comme le fait le téléphone.
- `hakiosmetal_air.m` : chaque variante d'une fonction liée est copiée dans le log (nom + taille), pas seulement la première.

## Vérifié sur le Mac (air2nir reconstruit)

- Fragment + `custom_fn` d'origine : traduit. Fragment + `custom_fn` modifié pour ranger la texture dans `RB::Layer` et
  la relire : refusé avant, **traduit** maintenant (un `tex` sur la texture 0).
- La section 0257 d'`apply.py`, rejouée sur une copie d'avant 0257, redonne exactement l'arbre testé.

## Non prouvé

- Que la variante réelle refusée sur le téléphone soit exactement de cette forme (elle n'était pas dans le log : 0257 la
  capturera si elle est encore refusée).
- La cause du blocage à l'ouverture des apps : aucun journal ne contient une ouverture faite à la main.

## Installé

- SYSTEM-OK, BOOT `65ec61fa8eeb846826bc0061b7d3b14df2e0a06ed0c81ce61a6d49d916f3ae9e`, BOOT-OK ; texte du correctif présent dans
  `build-metal/b2l-metal-v122/libhakiosair.dylib`. Téléphone redémarré vers iOS.
- Ouverture des apps (Réglages) : le processus `Preferences` n'a jamais été lancé (absent du log noyau, qui note chaque
  programme) et SpringBoard n'a pas planté : il bloque avant de lancer l'app. Journal iOS de ce moment perdu.

## Résultat

- **Des icônes s'affichent** (premières depuis 0249), certaines manquantes ou déformées. Puis SpringBoard plante 3 fois
  (EXC_BAD_ACCESS près de 0 : 0xc4, 0x227) et le noyau panique « Halt/Restart Timed Out @IOPlatformExpert.cpp:884 ».
- Juste avant : `HakiosMali: no memory for a buffer of 9043968 bytes (free 159 MB …)`, puis une faute MMU du Mali sur le
  travail de SpringBoard (`plane__vertex,plane_color_fragment,…`). Mémoire GPU : backboardd 452 Mo (228 purgeables),
  iconservicesagent 372 Mo (213), SpringBoard 349 Mo (176). Suite : build-b1l-0258.md.

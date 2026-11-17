# Plan : vitesse du pilote Metal, puis M6 (interface sur le GPU)

Date : 2026-11-07. Décidé avec le propriétaire, après B1L-0075.

## Décisions

| Question | Choix |
| --- | --- |
| Priorité après B1L-0075 | Finir la vitesse du pilote avant de toucher l'interface |
| Coût PAC du code d'Apple | Patch au chargement par le noyau, **après** M6 (corecrypto exclu) |
| Entrée dans M6 | Une app de test avec un `CAMetalLayer` d'abord, backboardd ensuite |
| Critères de réussite | 40 images/s en défilement sur l'accueil, Liquid Glass correct, plus rapide que le rendu CPU |

Le cache de shaders sur disque n'est pas un critère de réussite. Il reste prévu dans M6 (étape 6), parce qu'il évite les saccades.

## Étape 1 : vitesse du pilote (objectif : ~0,2 ms par dessin)

Mesure de référence : la ligne `M5 bench` (200 dessins dans une passe, image vérifiée contre le Mac).

| Build | Changement | Mesure attendue |
| --- | --- | --- |
| B1L-0075 (flashé) | Mesa et pilote sans retours signés (`-fno-ptrauth-returns`, 0 `retab`) | Le temps par dessin (1,2 à 3,4 ms avant) |
| B1L-0076 | Sans appels signés (`-fno-ptrauth-calls`) : retire les 3067 `blraaz`. Sûr, car le noyau accepte déjà les pointeurs non signés | Ce que coûtaient les appels signés |
| B1L-0077 | Relais vers les fonctions du système (`__auth_stubs`) : voir si l'éditeur de liens peut produire des relais non signés | Ce qui reste des appels vers libSystem |
| B1L-0078 | Pas de reliaison inutile : un shader, des constantes ou des tampons inchangés depuis le dessin précédent ne sont pas reliés à nouveau | Le coût propre à Mesa et panfrost |
| (si besoin) | Profil d'un dessin : compteurs de pièges PAC du processus pendant la mesure, temps par fonction | Le prochain point lent |

**Critère pour passer à l'étape 2 :** environ 0,2 à 0,3 ms par dessin, ou une explication chiffrée de ce qui reste, si c'est dans le code d'Apple.

## Étape 2 : M6a, une app de test à l'écran

- Une petite app avec un `CAMetalLayer` plein écran, affichée par le serveur d'affichage actuel (rendu CPU) :
  - elle dessine une image animée simple avec les shaders de QuartzCore ;
  - elle compte ses images par seconde.
- **Ce qu'elle valide :**
  - la présentation réelle à l'écran (`presentDrawable` → file d'images de CoreAnimation → compositeur → écran) ;
  - l'attente par le noyau de la fin du GPU avant l'affichage (`HakiosMaliDrawn`, `maliSeq`) ;
  - les images par seconde réelles d'une app Metal.
- **Risque faible :** l'interface reste en rendu CPU.

## Étape 3 : M6b, backboardd sur le GPU, derrière un interrupteur

- Un réglage au boot, désactivé par défaut, laisse QuartzCore de backboardd voir le Mali. Le boot normal reste en rendu CPU.
- **À traiter, d'après ce qu'on a déjà vu :**
  - les 189 shaders de QuartzCore, dont `fixed_frag_lph_cpf` et sa soixantaine de *function constants* ;
  - l'archive de pipelines précompilés d'Apple, à refuser proprement ;
  - `newTextureWithDescriptor:iosurface:plane:slice:` (encore un stub d'Apple, qui fait un abort) ;
  - les formats que le compositeur demande, par exemple YCbCr 420 en deux plans pour la vidéo.
- Ensuite : écran de verrouillage, puis écran d'accueil.

## Étape 4 : Liquid Glass

- Flous et verre (passes `downsample`, `upsample` et `variable_blur` de QuartzCore).
- Contrôle visuel : Centre de contrôle, dock et barres.

## Étape 5 : mesure contre les critères

- Images par seconde en défilement sur l'écran d'accueil (objectif : 40).
- Comparaison sur les mêmes écrans entre GPU et CPU (la bascule se fait par l'interrupteur de l'étape 3).

## Étape 6 : patch PAC du code d'Apple au chargement (après M6)

- Le noyau remplace dans les pages de code, au moment où elles sont chargées :
  - `retab` → `ret` ;
  - `blraaz` → `blr` ;
  - `aut*` → NOP.

  Ça prolonge le cache natif existant, dont il faut corriger les rejets (« mismatch »).
- Rien n'est modifié sur disque, et corecrypto est exclu (il vérifie l'intégrité de son code).
- **Effet attendu :** une baisse forte des ~51 000 pièges par seconde du système, pour tout iOS et pas seulement le GPU.
- Cache de shaders sur disque, contre les saccades de compilation (0,5 à 1 s par nouveau shader).

## Règles inchangées

- Un changement par build, avec ses notes `docs/journal/build-b1l-NNNN.md`.
- `metal-progress.md` mis à jour après chaque build et chaque test.
- Flash sans demander, pour le travail Metal, avec toutes les gardes (TWRP root, modèle, taille, hash, relecture).

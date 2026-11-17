# Build B1L-0088 : varyings 16 bits sans perspective (Mesa), plug-in qui ne libère plus

Date : 2026-11-07. Base : B1L-0087.

## Changements

1. **Mesa, `pan_lower_noperspective.c`** (`apply.py`, B1L-0088) : `w` (`pos_w` côté vertex, `fragcoord.w` côté fragment) est converti à la taille de la varying (`nir_f2fN`) avant la multiplication. C'est la cause trouvée par `b2l-bicomp` de l'instruction invalide 0x51 (images en escalier depuis B1L-0084). Avec le correctif, `A2Xhf` et `TimgA2Xhf_Isrc` se compilent sur le Mac (192 et 272 octets de code Bifrost).
2. **Plug-in `b2l_metal_plugin.m`** : pour un processus qui n'est pas backboardd, `initWithAcceleratorPort:` renvoie `nil` **sans** `[self release]`. Ça corrige les plantages de SpringBoard (`-[_MTLDevice dealloc]` sur un objet jamais initialisé).
3. **Le reste est inchangé :** envoi des modules par le journal, test M5 en `_lpf` et `_lph`.

Ensemble `b2l-metal-v44`, plug-in `B2LMetalv44`, Mesa recompilé.

## Installé

- **Vérifié :** le correctif est présent dans `pan_lower_noperspective.c` de l'arbre Mesa compilé pour le téléphone, et le plug-in `B2LMetalv44` est reconstruit.
- **SYSTEM :** 6 blocs réécrits, en 79 s.
- **BOOT :** `out/b1l-metal-0088/boot.img`, relecture `ac07beec6ccda836a719ac804ce89556a8edd7596083a659dc367760c062b0c7` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img`.

## Résultat : l'interface d'iOS rendue par le Mali

- **Démarrage :** les lignes de la console se figent (la première image de l'interface a été envoyée), puis l'écran reste noir jusqu'à un appui sur le bouton power. L'écran de verrouillage apparaît alors, en cyan, avec la barre du bas mais sans le cadenas.
- **Ce qui marche, sur les photos du propriétaire :**
  - **écran d'accueil** : les icônes des apps ont leurs vraies couleurs ([b1l-0088-accueil.jpg](b1l-0088-accueil.jpg)) ;
  - **Réglages** : liste, icônes, texte, barre de recherche en verre ([b1l-0088-reglages-1.jpg](b1l-0088-reglages-1.jpg), [b1l-0088-reglages-2.jpg](b1l-0088-reglages-2.jpg)) ;
  - **Centre de contrôle** : les éléments Liquid Glass ont leurs formes et leurs bords lumineux ([b1l-0088-centre-controle.jpg](b1l-0088-centre-controle.jpg)) ;
  - **Contacts** : « New Contact », avec le clavier ([b1l-0088-contact.jpg](b1l-0088-contact.jpg)).
- **Défauts visibles :**
  1. **Teinte cyan** : sous les noms des apps (pavés cyan à la place du texte), écran de verrouillage, tout l'écran Contacts. Le rouge semble perdu dans certains shaders, sans doute ceux de vibrance et de flou, ou le mélange.
  2. **Liquid Glass incomplet** : formes et bords présents, mais pas de flou ni de contenu dans les pastilles du Centre de contrôle.
  3. **Bord gauche coupé** dans Réglages (« ettings »), avec une bande grise translucide.
  4. **Clavier** : les lettres sont dessinées sans le fond des touches, par-dessus le contenu.
  5. **Lent et instable** au toucher.
- **Fond d'écran noir :** PosterBoard n'a pas de rendu, comme en rendu CPU.

## Journal (`out/logs/0088-boot1.txt`)

- **Stabilité :** backboardd ne démarre qu'une fois (aucun plantage), SpringBoard ne plante plus, aucun pipeline refusé. 193 pipelines de rendu sont créés et 16 images envoyées à l'écran pendant le boot.
- **Le test `_lph` de weatherd est juste :** `0 of 64 rows differ`, comme `_lpf`. Le 16 bits en soi marche ; c'était bien le cas « sans perspective ».
- **Il reste 99 fautes `0x51`** sur le slot 0 (fragments) : d'autres shaders produisent encore du code invalide, sans doute la cause d'une partie des défauts visibles.
- **39 modules reconstruits** dans `out/qc-check/mods-0088`.
  - `b2l-bicomp` ne trouve plus de faute de taille.
  - Ses autres arrêts sont des étapes que fait le pilote Gallium et que l'outil ne reproduit pas : `load_rt_conversion_pan` (conversions des cibles de rendu), valeurs système des vertex, `deref` restants.
  - Reproduire tout panfrost sur le Mac demanderait sa bibliothèque `lib`, qui n'est compilée qu'avec le pilote.

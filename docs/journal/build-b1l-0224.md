# Build B1L-0224 : shaders refusés de RenderBox nommés et envoyés au Mac (diagnostic)

Date : 2026-11-14. Base : B1L-0223 (noyau 0223 inchangé, `--no-kernel`). Ensemble `b2l-metal-v95`, Mesa recompilé.
Un seul changement, de diagnostic : le rendu est le même qu'en 0223.

## Constats avant ce build (sur le Mac, sans flash)

- **Refus du journal 0223** : les mêmes qu'en 0187 (phi de structure, phi de pointeurs sur une arête de boucle, une
  texture posée sur un tampon). Le « plus de 256 fois » vient du compteur du journal, qui double à chaque fois.
- **`air2nir` recompilé** (il faut `SDKROOT=$(xcrun --sdk macosx --show-sdk-path)` pour lier) : il refuse `uber_vertex` comme le téléphone.
- **Cache d'icônes d'iOS sur USERDATA, lu sans écrire** (`com.apple.IconsCache`, 366 fichiers `.isdata` = 48 octets
  d'en-tête + RGBA16F) : **les icônes sont correctes**, Horloge avec ses traits et Calendrier compris. Planche :
  `beyond2lte-ios/out/icons-cache-0223/icons-204.png` ; scripts en lecture seule à côté. Donc les défauts viennent du
  dessin en direct de RenderBox dans SpringBoard, pas d'iconservicesagent ni d'un cache périmé.
- **`b2l-bicomp` recompilé** (`scripts/build-bicomp.sh`, nouvelle option `--fn NOM` pour prendre une fonction d'un
  `.metallib`, `B2L_NIR=1` pour afficher le NIR donné à Bifrost). L'outil remplaçait l'adresse des tampons par la
  constante 0, d'où une constante 64 bits que Bifrost refuse : faux problème de l'outil. Elle est maintenant lue dans un
  tampon de constantes, comme panfrost le fait sur le téléphone.
- **Avec le correctif 0188, `uber_vertex` passe Bifrost sur le Mac, mais fait 75 520 octets de code et 64 registres.**
  C'est un suspect sérieux pour la lenteur de 0190.
- Les shaders spécialisés que SpringBoard demande (`fa16dhmha4`, `foaca5pa7dhmha4`…) ne sont pas dans
  `default.metallib`. `archive.metallib` n'en contient que des descripteurs de 732 octets. Il faut donc les prendre sur
  le téléphone : c'est l'objet de ce build.
- La conclusion de 0191 (« le blocage est après le dessin ») n'est pas prouvée : en 0189, SpringBoard n'a envoyé que
  49 command buffers, `fa16dhmha4` y était encore refusé, et la garde de 0189 supprimait un saut dans Bifrost (shader faux).

## Changement (`apply.py`, bloc B1L-0224)

- Pour chaque pipeline de rendu **refusé** dans SpringBoard, iconservicesagent ou chronod : une ligne
  `B2L REFUSED <processus>: f-<entrée>-<fnv32> (N bytes) v-<entrée>-<fnv32> (N bytes): <raison>` (200 au plus).
- Les deux modules AIR passent dans le journal du noyau (`B2LMOD`, comme en 0086), 40 modules et 1 Mo par processus au
  plus. Récupération : `python3 scripts/extract-b2lmod.py out/logs/0224-boot.txt out/b2lmod-0224`.
- Les envois de modules de backboardd (0086, coupés en 0110) restent coupés.

## Installé

- **Vérifié :** `air-b2l: refused RenderBox pipelines named… (B1L-0224)` dans `out/build-mesa-0224.log` ; `B2L REFUSED` dans `build-metal/b2l-metal-v95/libhakiosair.dylib`.
- **SYSTEM :** 6 blocs sur 1750, SYSTEM-OK. **BOOT :** relecture `2ab14dda35b9bf0b0ddb36105c5b37aaab4a0676e28cf085a6d261ff23310d7e` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour en arrière :** recompiler avec `userspace/mesa/air-b2l/apply.py.before-0224` (état 0223).

## Résultat (journal `out/logs/0224-boot.txt`)

- iOS démarre ; 15 refus comme en 0223 ; **aucune ligne `B2L REFUSED`, aucun module** : le diagnostic n'a pas tourné.
- **Cause (erreur de méthode) :** le noyau publie les bibliothèques de SYSTEM vers `/usr/local/lib/b2l-metal-vNN/` sur
  USERDATA (`b2l_gl_publish_one`, `b2l_userspace_seed.h`) et **ne remplace jamais un fichier déjà présent**. Garder
  l'ensemble `v95` avec `--no-kernel` laisse donc tourner les anciennes bibliothèques. Tout changement de Mesa ou du
  pilote demande un nouvel ensemble, donc un noyau reconstruit.
- Suite : B1L-0225, même diagnostic dans l'ensemble `b2l-metal-v96`.

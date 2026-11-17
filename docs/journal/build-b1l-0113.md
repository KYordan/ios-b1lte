# Build B1L-0113 : échantillonneur de backboardd (où part le temps)

Date : 2026-11-08. Base : B1L-0112. Noyau seulement (diagnostic) ; bibliothèques inchangées, ensemble `b2l-metal-v68` gardé.

## Pourquoi

Depuis 0112, plus de plantage, mais l'interface reste lente. Le GPU n'est occupé que 4 à 19 % du temps. Les journaux de 0105 (mesures encore actives) montraient, à 10 images/s, environ 10 ms de noyau par image : le reste du temps (≈ 90 ms) se passe dans backboardd, en calcul ou en attente, sans qu'on sache où.

## Changement

Nouveau `osfmk/arm64/b2l_prof.c` (ajouté à `osfmk/conf/files.arm64`), un fil noyau `b2l.prof` lancé par `AppleCLCD::start` :

- **sur le CPU** : chaque interruption qui tombe pendant qu'un fil de backboardd tourne note son pc et son lr (ou, si elle tombe dans le noyau, le pc/lr de l'appel système en cours). Accroche dans `sleh_irq` (`xnu/osfmk/arm64/sleh.c`, directement dans `xnu/`), anneaux par CPU, sans verrou ;
- **en attente** : toutes les 2 ms, l'état de chaque fil de backboardd (prêt/en cours ou en attente) et, pour un fil en attente, où il attend : pc, lr et 4 retours de sa chaîne de `fp` (`copyinmap`) ;
- toutes les 10 s : `B2L PROF:` (bascules d'écran par seconde, échantillons), `B2L PROF T:` (fils les plus actifs, % prêt/en cours, % en attente), `B2L PROF CPU:` (24 adresses les plus vues sur le CPU), `B2L PROF WAIT:` (24 attentes les plus fréquentes).
- `B2LFramebuffer.cpp` : compteur des SwapEnd (images envoyées par CoreAnimation) lu par l'échantillonneur.

## Installé

- **Vérifié :** les messages `B2L PROF` sont dans le noyau (7 chaînes).
- **SYSTEM :** SYSTEM-OK en 57 s. **BOOT :** relecture `26d1844c2573c4842bd07c5b50b3b9f8581301057b2028b3239e264161c51b8f` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0112/boot.img`.

## Résultat (journal `out/logs/0113-boot.txt`)

- Aucun plantage (un seul démarrage de backboardd).
- **Images/s mesurées** (SwapEnd) : 3 à 8 pendant l'utilisation ; **exactement 10,0/s** pendant les animations au repos (plafond de cadence).
- **Le fil `com.apple.coreanimation.display` (tid 0x255) fait tout le travail** : occupé 73 % du temps pendant l'utilisation, 35 % au repos. Les autres fils de backboardd sont presque toujours en attente.
- Deux points chauds, tous deux dans un `memcpy` (`libsystem_platform+0x990`, glissement du cache `0x1b0000`, `libhakiosair` chargée à `0x119400000`) :
  1. **Pendant l'utilisation : `util_copy_box`** (appel de `u_default_texture_subdata`, c'est-à-dire `-replaceRegion:` → `rwa_texture_write`). 18 294 échantillons sur 10 s, presque tous **dans le noyau, sur l'instruction de lecture** : la copie fait une faute de page à chaque page. CoreAnimation envoie des images au GPU par le CPU, et la mémoire source fait des fautes.
  2. **Au repos (10 images/s) : `panfrost_emit_const_buf+0x984`**. C'est la copie, à chaque dessin, de chaque tampon de constantes « user buffer ». Sur panfrost, tous les tampons de constantes Metal passent par là (`ubo_base` 1), et sans `air.buffer_size` la taille copiée est tout le reste du MTLBuffer.
- Les attentes des autres fils sont normales (boucles d'événements `mach_msg`, files de dispatch, fil de compilation de Mesa).

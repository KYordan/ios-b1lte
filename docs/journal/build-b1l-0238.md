# Build B1L-0238 : plus de plantage de SpringBoard dans le retour du dessin

Date : 2026-11-14. Base : B1L-0237 (diagnostic des images gardé). Ensemble `b2l-metal-v109` (noyau reconstruit),
pilote recompilé.

## Changement (`hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0238`)

- `-[HakiosMetalBuffer hakiosSyncOut]` n'écrit dans les octets de l'appelant que si :
  - l'appelant tient encore le tampon (`retainCount` > 1 ; la texture posée dessus tient l'autre référence) ;
  - toute la zone est encore projetée en écriture dans une même région (`mach_vm_region`).
  Sinon : ligne `B2L WRITEBACK: … no longer mapped writable` et rien n'est écrit.

## Installé

- Premier essai refusé à la compilation (`mach_vm.h` n'existe pas dans le SDK iOS) : `vm_region_64` à la place.
- **Vérifié :** « no longer mapped writable » dans `build-metal/b2l-metal-v109/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `512f97a0a615e10afbb4a6f2eaaa1f67c25d486d0488f5a509b01253df07513a` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0238-boot.txt`)

- **SpringBoard ne plante plus** (même processus, pid 28, tout le démarrage). Icônes inchangées d'après le propriétaire.
- Aucune ligne « no longer mapped » : la garde n'a pas eu à refuser.
- chronod ne crée **aucun port** pour ses surfaces YCbCr (`createport` absent pour les id 5 à 12) : elles ne semblent pas partagées avec backboardd. La piste « surfaces chronod » faiblit ; 0239 le vérifie.

## Remarque du propriétaire et piste pour Horloge/Calendrier

- Le propriétaire n'a aucun souci avec l'icône Téléphone : ce sont **Horloge, Calendrier et le centre de contrôle** qui
  manquent. Les images de 0237 ne les contenaient pas (seulement les 8 premières sortes de cible sur tampon).
- Piste : **chronod** (le processus des icônes vivantes Horloge/Calendrier) crée des surfaces **à deux plans YCbCr**
  (`xf44`, `444f` en 88×88 ; `xf20`, `420f` en 128×96). **Aucun processus ne les importe par Metal** dans nos
  journaux (aucune ligne `IMPORT` à deux plans), et le pilote n'importe que des surfaces à un plan. Cela
  collerait avec « l'icône apparaît (image du cache), puis disparaît (remplacée par l'instantané vivant que personne ne sait lire) ».

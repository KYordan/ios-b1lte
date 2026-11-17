# B1L-0248 — le GPU pour toutes les apps (set v118)

## Pourquoi

Demande du propriétaire : « que le système entier tourne sur le GPU sans consommer trop de mémoire ».
0247 a mesuré ~290 Mo réels pour les 4 processus autorisés, dont ~230 Mo pour backboardd. Une app de plus coûte
~16 Mo (le tas du tiler) plus ses tampons. Test Metal (0247) : une app ordinaire reçoit nil de `MTLCreateSystemDefaultDevice`.

## Changement (`userspace/metal/b2l_metal_plugin.m`)

- La liste de B1L-0185 (backboardd, SpringBoard, chronod, iconservicesagent) n'est plus appliquée : tout processus
  qui demande un appareil Metal reçoit le Mali.
- Interrupteur : boot-arg `b2l_gpu_all=0` remet la liste (lu par `sysctlbyname("kern.bootargs")`).
  Limite : un processus que le bac à sable empêche de lire `kern.bootargs` est quand même admis.
- Set de publication v117 → v118.

## Risques à surveiller

- Shaders que le traducteur AIR→NIR refuse (phi de struct, B1L-0187) : éléments vides dans des apps qui dessinaient bien au CPU.
- Mémoire : nombre de clients (le relevé `mali mem:` ne voit que 32 clients).
- Aucune famille GPU annoncée : certaines apps peuvent refuser le Metal reçu.

## Installé

- 1er essai : build lancé avec le set `v118` au lieu de `b2l-metal-v118`. Les librairies sont parties dans
  `build-metal/v118`, le téléphone n'a pas eu `b2l-metal-v118/` (`B2L GL: … libhakiosair.dylib … error=2`,
  `B2L METAL: publication error=2`) : aucun GPU, rendu CPU partout (« le GPU a crashé direct »). Ce n'était pas le changement 0248.
- 2e essai (même code, bon nom de set) : SYSTEM-OK, BOOT `4921ddedcb5f182487ea9f8586075b3d7b154495666c36434c4e24922fcad6a9`, BOOT-OK.

## Résultat

- En attente (Test Metal dans Dictaphone : les deux tests « appareil système » doivent passer ; relevé `mali mem:`).

## Retour en arrière

- Boot-arg `b2l_gpu_all=0`, ou `b2l_metal_plugin.m.before-0248` et set v119.

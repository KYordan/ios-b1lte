# Build B1L-0080 : M6b, premier essai de backboardd sur le Mali

Date : 2026-11-07. Base : B1L-0079 (M6a réussi : ~22 images/s dans Horloge, limité par le compositeur CPU).

## Principe

- Metal trouve son pilote par le service noyau `IOAcceleratorES`, en lisant `MetalPluginName` et `MetalPluginClassName`.
- Jusqu'ici, le plug-in était `B2LMetalV3`, l'ancien pilote M1/M2 qui ne montre son device à aucun processus. backboardd restait donc en rendu CPU.
- **Avec `b2l_metal_ui` activé,** le service annonce **`B2LMetalV4` / `B2LMetalUIDevice`** :
  - c'est un petit bundle (`userspace/metal/b2l_metal_plugin.m`) lié à `libhakiosmetal`, le pilote de Scion, par son chemin d'installation, donc la même image du pilote dans tous les processus ;
  - `B2LMetalUIDevice` est une sous-classe de `HakiosMetalDevice` qui **ne se montre qu'à backboardd** ;
  - les autres processus (apps, WebKit, Photos) n'ont pas de device, comme avant ;
  - QuartzCore, dans backboardd, prend alors le Mali au lieu du rendu CPU.

## Sécurité

- **L'interrupteur `b2l_metal_ui`** est activé par défaut dans ce BOOT de test ; `b2l_metal_ui=0` reste le coupe-circuit. Les arguments de boot sont figés dans le chargeur UEFI (`XnuLoader.c`), d'où ce choix à la compilation.
- **Retour automatique :** au 3ᵉ démarrage de backboardd depuis le boot (donc après 2 plantages), le noyau remet `B2LMetalV3` (`b2l_metal_ui_fallback`, appelé depuis `b2l_exec_graphics_diag.h`). Le backboardd suivant repart en rendu CPU.
- **Retour manuel :** reflasher `out/b1l-metal-0079/boot.img` si l'écran reste figé sans que backboardd plante.

## Journal

- `b2l-plugin:` : backboardd demande le device, puis le device est créé.
- `rw_print` du pilote de Scion : dans backboardd, ses 3000 premières lignes vont dans le journal noyau (préfixe `hakiosmetal[backboardd …]`). On y verra ce que QuartzCore demande et ce que le pilote refuse.
- `B2L METAL: backboardd start N`, et le message du retour automatique.

## Ensemble `b2l-metal-v36`

- **Contenu :** le bundle `B2LMetalV4` et son `Info.plist`, plus les bibliothèques de 0079 (relais non signés, M6a dans Horloge, test M5 dans weatherd).
- **Publication :** le noyau publie `/System/Library/Extensions/B2LMetalV4.bundle`. Il ne remplace jamais un fichier existant, donc un changement futur du plug-in demandera un nouveau nom de bundle (V5).

## Attendu

- **Le premier essai a peu de chances de tout afficher juste.** QuartzCore utilise beaucoup plus de fonctions que nos tests : 189 shaders, des *function constants*, des copies de textures, des mipmaps, des formats variés. Ce boot doit surtout montrer **où ça s'arrête**.
- Si l'interface s'affiche, M6a dans Horloge dira combien le compositeur sur GPU fait gagner (~22 images/s avant).

## Installé

- **Noyau :** il contient `B2LMetalV4` et le compteur de démarrages de backboardd.
- **SYSTEM :** 6 blocs réécrits, en 69 s.
- **BOOT :** `out/b1l-metal-0080/boot.img`, relecture `7c5eb1187bb76df86d2c5a5efa7acaeffb7d0ab0c5099ad5d0f832a1496c06c3` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0079/boot.img` (`46968a0e…`).

## Résultat (`out/logs/0080-boot1.txt`) : backboardd prend le Mali, puis plante

- **Publication et choix du plug-in :** `B2LMetalV4` est publié et `IOAcceleratorES` annonce `MetalPluginName B2LMetalV4 (b2l_metal_ui=1)`.
- **Démarrage 1 (pid 62) :**
  - `b2l-plugin: backboardd (pid 62) asks for a device: the Mali's`, puis le device est créé (`Mali-G76 (Panfrost)`, profil 0) ;
  - **abort (signal 6)** juste après.
- **Démarrage 2 (pid 128) :** même chose. `B2L FAULTMSG: pid=128 frame_arg=+[MTLLoader sliceIDForDevice:legacyDriverVersion:airntDriverVersion:]`, même PC que pour le pid 62.
- **Démarrage 3 :** `backboardd started three times: back to B2LMetalV3`. L'interface revient en rendu CPU : c'est ce que le propriétaire a vu (logo, puis rendu et vitesse habituels). **Le retour automatique a marché.**
- **Cause, la même qu'en B1L-0046 :** Metal demande `-targetDeviceArchitecture` au device et fait un abort sur `nil`. Notre ancien plug-in le gérait depuis B1L-0047, mais le pilote de Scion ne l'implémente pas.

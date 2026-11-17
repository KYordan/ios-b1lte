# Build B1L-0042 : le GPU dessine dans le framebuffer, plus de surface libérée dans la liste, flous en pleine résolution

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs, copie `6fbfa84a…`) et BOOT (relecture `f46d2caf…`) ; pas encore testé.** Base : B1L-0041.

## Constat sur B1L-0041 (premier vrai rendu GPU)

- **iOS est rendu par le Mali.** Le propriétaire voit l'écran de verrouillage, avec des défauts : texte lumineux, fond rouge et bleu, horloge blanche étrange à la place de Liquid Glass, saccades.
- Le journal (`out/logs/0041-boot1.txt`) montre des centaines d'images et 9184 job chains GPU, sans aucun échec. Le GPU est occupé à 2 %.
- **Plantage au bout d'environ 250 s :** `KERN_INVALID_ADDRESS` à `0xe0000001d`, `pc` dans `Context::push_surface` (`0x183f131d0`, parcours de la liste suivie). Le correctif 0041 (`track_surface`) en est la cause : QuartzCore crée régulièrement une nouvelle surface d'écran et libère l'ancienne sans la retirer de la liste.
- **Lenteur :**
  - Le partage de la surface 1 échoue (`surface 1 … copied, not shared`). La requête noyau `0x7e` renvoie `kIOReturnNoMemory`, car `mali: physical address 0xca000000 is not memory the kernel manages`. La surface 1 est le framebuffer physique (réserve DisplayReserved), que la MMU du GPU refusait.
  - Une image sur deux est donc recopiée par le CPU : 17 Mo, 150 à 350 ms (`copying out`).
  - Les premières images complexes prennent de 1 à 10 s, surtout pour compiler les shaders dans Mesa.
- **Liquid Glass :** le noyau imposait encore `CA_BACKDROP_SCALE=0.25` à backboardd (essai B1L-0016 pour le rendu CPU), soit des fonds floutés au quart de la résolution.

## Changements

- **`userspace/b2l_accel_gles.c` (bibliothèques `b2l-gl-v9`)** : le correctif `track_surface` est retiré. Le branchement « surface introuvable » de `push_surface` (`cbnz x8` à `0x183f131e4`, `0xb50001e8`) devient `b 0x183f13220` : une surface non suivie est acceptée, et aucune liste ne garde de pointeur. Nouvelle trace `b2l-gl: push_surface takes untracked surfaces: 0`.
- **`xnu-overlay/bsd/dev/b2l_mali_core.c`** : `hakios_scanout_carved()` reconnaît la réserve du framebuffer (`0xCA000000`, 20 Mo). Le GPU peut alors y dessiner directement, sans copie CPU.
- **`xnu-overlay/bsd/kern/b2l_exec_graphics_diag.h`** :
  - `CA_BACKDROP_SCALE` n'est plus injecté par défaut (`b2l_ca_backdrop=1` le remet) ;
  - `CA_DISABLE_ASYNC*` restent : ils évitent l'autre assertion de liste (`ogl-context.cpp:1186`).
- Chemins `b2l-gl-v9`. `libOSMesa.8.dylib` `e796b815…`, `libb2l-gl.dylib` `fad84ba2…`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-scanout-0042/boot.img` | `f46d2caf7ad8647206429342c07b1ca329e5968675f827d79ea0bc2b9518bbd9` |
| Kernel (`kernel.macho`) | `e4abdc586af900f01df40a6d9b96a7aa91a90e72a7432eb235bbedb4cea5d037` |

Installation : `system-add-gl.sh b2l-gl-v9`, puis `flash-boot.sh out/b1l-scanout-0042/boot.img f46d2caf…`.

## Ce qui reste à voir

- **Couleurs :** beaucoup de petites surfaces (icônes, glyphes) sont au format `''`, souvent en « alpha first » (ARGB). Le pont les copie en les convertissant. C'est la piste pour le texte lumineux et le rouge et bleu inversés.
- **Liquid Glass complet :** QuartzCore d'iOS 26 a probablement des effets de verre réservés à Metal. Le chemin GLES pourra au mieux rendre les flous.

## Retour arrière

Reflasher `out/b1l-thermgpu-0030/boot.img` (`d954c349…`).

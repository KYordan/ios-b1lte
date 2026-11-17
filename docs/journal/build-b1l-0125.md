# Build B1L-0125 : retour à l'état stable de 0122, et la photo du propriétaire en fond d'écran

Date : 2026-11-08. Base : B1L-0124. Ensemble `b2l-metal-v75`, Mesa recompilé.

## Pourquoi

- 0123 et 0124 : les vues de niveaux de mipmap laissent tourner des passes du flou et du verre qui font fauter le GPU (jusqu'à 46 fautes, dont sur des tampons libérés) et font clignoter toute l'interface.
- Demande du propriétaire : sa photo (`userspace/wallpaper/fond-proprietaire.jpg`, 1440×3115) en fond d'écran, à la place du fond généré.

## Changements

1. **Vues partielles désactivées** (`hakiosmetal_air.m`, `#define B2L_PARTIAL_VIEWS 0`) : une vue de quelques niveaux ou tranches redevient `nil`, et l'attachement `level`/`slice` est ignoré, comme en 0122. Le code de 0123 reste derrière l'interrupteur ; côté Mesa, les plages de niveaux restent à 0 sans ces vues, donc sans effet.
2. **0124 annulé** (`apply.py`) : retour aux bornes de constantes de 0115 (vitesse).
3. **Fond d'écran = photo du propriétaire** :
   - `scripts/build-springboard-dylib.py` recadre la photo au centre en 1440×3040 (le surplus en haut et en bas) avec `sips`, la réencode en JPEG sRGB 8 bits (qualité 88, 795 615 octets) et la lie dans la bibliothèque (section `__DATA,__b2l_wall`) ; taille maximale de la bibliothèque portée à 2 Mo ;
   - `userspace/b2l-springboard.m` (**v4**) lit cette section (`dladdr` + `getsectiondata`) et l'affiche en remplissage sans déformation ; si la section manque ou ne se décode pas, le fond généré de 0116 reste en secours ; une ligne dans `b2l-springboard.log` dit ce qui a été pris ;
   - renommage en `b2l-springboard-v4.dylib` (le noyau ne remplace jamais un fichier publié) dans les en-têtes du noyau ; charge utile 879 440 octets, sha256 `bb169c8a…`.

## Installé

- **Vérifié :** le noyau nomme `b2l-springboard-v4` ; `libhakiosmetal.dylib` (v75) contient le message `B2L_PARTIAL_VIEWS 0`.
- **BOOT :** relecture `b8a9f01dbbf22cfd0014d40629c2c6ce83571640b416a601ecf1dc23980882da` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0122/boot.img` avec SYSTEM v72.

## Résultat (journal `out/logs/0125-boot.txt`)

- **Propriétaire : plus de clignotements** ; photo en fond d'écran ; Liquid Glass toujours gris ou noir (dock, tiroir d'apps, Centre de contrôle). Le verre marche sur le rideau de l'écran verrouillé.
- 6 fautes GPU restent sans les vues partielles, toutes dans des passes du verre (`glass_background_sdf`, `glass_foreground_sdf`, `sdf_filter`), en **lecture** (`…2c1`) à des adresses qui ne sont dans aucun des 4 096 derniers tampons. La ligne « past the end » du noyau est fausse (différence négative).
- `compute_average_luma` échoue de nouveau (« reads texture 0 and 1 texture was bound ») : sa source est une vue partielle (de nouveau `nil`). Sans luminosité, la teinte du verre n'est pas calculée.

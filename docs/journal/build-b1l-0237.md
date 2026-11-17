# Build B1L-0237 : voir ce que le GPU dessine pour les icônes (diagnostic)

Date : 2026-11-14. Base : B1L-0236. Ensemble `b2l-metal-v108` (noyau reconstruit), Mesa inchangé, pilote recompilé.
Diagnostic seul : le rendu est le même qu'en 0236.

## Changement (`hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0237`)

- À chaque retour du dessin (`-hakiosWriteBack`), la 1re et la 4e image de chaque taille et format (8 sortes au plus)
  partent dans le journal en `B2LMOD l-img-<L>x<H>-f<format>-r<octets par ligne>-n<rang>` (même encodage que les
  modules, plafond commun de 800 Ko). Sur le Mac : `scripts/extract-b2lmod.py`, puis conversion en PNG.

## Installé

- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `e967e7d7c9c671045d2e9ef697efeb6435a030e94d6f901c1c131b75bef3f8a5` (BOOT-OK). Redémarré par `adb reboot`.
- Convertisseur : `scripts/b2limg-to-png.py DIR` (PNG sur damier gris, ×3).

## Résultat (journal `out/logs/0237-boot.txt`, images dans `docs/b1l-0237-images/`)

- Propriétaire : pareil à l'écran, plus un plantage (« backboardd »). En fait c'est **SpringBoard** qui a planté
  (pid 28 → 325) : `EXC_BAD_ACCESS` à `0x155b20000` (début de page), pc dans `memcpy`, appelé depuis la file de fin
  de command buffer du pilote. **C'est le retour du dessin de 0236** (`hakiosSyncOut`), qui écrit dans les octets de
  l'appelant quand l'appelant les a déjà rendus. Corrigé en B1L-0238.
- **Les 7 images reçues** (PNG sur damier, ×3) :
  - `122x122 f90` (RGB10A2) : une forme de **combiné de téléphone** en dégradé vert/bleu/violet. C'est vraisemblablement une
    carte de données (normales ou hauteur d'un glyphe « verre »), pas une image en couleur ;
  - `206x206 f90` : une forme arrondie, mêmes couleurs ;
  - `120x120 f10` (R8) : le même combiné, **flou et criblé de points clairs** ;
  - `120x120 f80` (BGRA) : 32 % de pixels dessinés ;
  - `204x204 f80` (BGRA, iconservicesagent) : **presque vide** (8 %), une tache pâle en haut.
- **Constat : les formes sont justes, mais toutes les images sont criblées d'un bruit fort**, et l'icône finale
  d'iconservicesagent est presque transparente. Suspects : la texture de bruit 32×32 (t3, `noise_tex`) appliquée trop
  fort ou lue avec le mauvais sampler (les 14 samplers de `fa16dhmha4`, 0230), ou un calcul faux dans le shader
  traduit (phi d'agrégats, 0227).

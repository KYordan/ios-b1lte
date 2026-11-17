# Build B1L-0239 : qui reçoit quelles surfaces, et images par processus (diagnostic)

Date : 2026-11-14. Base : B1L-0238. Ensemble `b2l-metal-v110` (nouvel ensemble obligatoire, voir 0224).
Un seul changement, de diagnostic : le rendu est le même qu'en 0238.

## Pourquoi

- Le propriétaire n'a aucun souci avec l'icône Téléphone ; il manque **Horloge, Calendrier et le centre de contrôle**.
- Les images de 0237 ne prenaient que les 8 premières sortes de cible, tous processus confondus sous un même nom : ni
  Horloge, ni Calendrier, ni le centre de contrôle n'y étaient.
- chronod crée des surfaces YCbCr à deux plans, mais en 0238 aucun port n'est créé pour elles et aucun import Metal à deux
  plans n'est vu. Le passage par port (sélecteur 34, contenu des calques envoyé à backboardd) n'était noté qu'une fois.

## Changement

- **Noyau (`B2LSurface.cpp`, sélecteur 34)** : chaque réception d'une surface par port est notée
  `B2L SURF: <processus> portlookup id=N WxH fmt 0x.. planes P -> 0x..` (300 au plus).
- **`hakiosmetal_air.m` (écriture en retour, 0237)** : images nommées par processus
  (`l-img-<processus>-WxH-f..-r..-nK`), 24 sortes au lieu de 8, dessins 1, 4 et 16 ; plafond d'envoi 64 modules et
  2,5 Mo par processus.
- `scripts/b2limg-to-png.py` lit les deux formes de nom.

## Installé

- Premier lancement raté (script appelé sans `bash`) ; relancé avec `bash b1l-build.sh 0239 b2l-metal-v110`.
- **Vérifié :** `img-%.24s` dans `build-metal/b2l-metal-v110/libhakiosmetal.dylib` ; `portlookup` dans `out/b1l-metal-0239/boot.img`.
- **SYSTEM :** 7 blocs sur 1750, SYSTEM-OK. **BOOT :** relecture `adab2dd7311edc20a74208be3fb38e555568744a9eea2bb232d8121fcc74abf6` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (journal `out/logs/0239-boot.txt`)

- Le propriétaire a vu **l'icône Horloge parfaite, et restée**, une fois, un moment après le démarrage (pas au démarrage).
- **Réceptions par port (backboardd)** : seulement des surfaces BGRA/LA08/RGhA à un plan venant de SpringBoard et
  Spotlight (dont six 102×102 RGBA que SpringBoard dessine par Metal). **Aucune surface YCbCr de chronod** : piste chronod abandonnée.
- **Images** : dans SpringBoard, seulement 3 sortes de cible ont jamais été recopiées (120×120 R8, 120×120 BGRA,
  122×122 RGB10A2) ; 16 lignes WRITEBACK en tout. Le journal a perdu une partie des envois (anneau plein).
- **Cause probable** : la recopie vers le tampon de l'appelant (0236) n'a lieu que si quelqu'un attend le command
  buffer (`waitUntilCompleted` ou un completed handler). Un appelant qui ne fait ni l'un ni l'autre ne récupère jamais
  son dessin ; l'Horloge vue une fois serait un dessin dont un autre appel a forcé l'attente. Suite : B1L-0240.

## Retour en arrière

- `B2LSurface.cpp.before-0239`, `hakiosmetal_air.m.before-0239` (état 0238).

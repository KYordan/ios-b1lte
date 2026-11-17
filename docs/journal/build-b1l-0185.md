# Build B1L-0185 : gros patch GPU — mémoire recyclée remise à zéro, GPU ouvert à SpringBoard, chronod et iconservicesagent

Date : 2026-11-12. Base : B1L-0184 (pilote Metal réellement installé : v85, celui du 0136). Ensemble `b2l-metal-v89`, Mesa recompilé.
**Deux changements regroupés, à la demande du propriétaire (« fais un gros patch pour le GPU »).**

## Diagnostic

Photos du propriétaire (0184) et journal `out/logs/0184-boot.txt` :
- **Coins bleus** sur les sections arrondies de Réglages, et **teinte violette** au premier affichage de certains éléments, qui disparaît ensuite.
- Centre de contrôle : certains glyphes manquent, d'autres sont des **carrés pleins** (commandes de lecture de « Not Playing »). L'heure de l'écran verrouillé a un **rectangle** derrière ses chiffres.
- Horloge sans chiffres, Calendrier vide, widgets blancs : rien de changé.
- **Seul backboardd reçoit un GPU Metal.** `b2l_metal_plugin.m` renvoie `nil` à tout autre processus (`b2l-plugin: backboardd … asks for a device` est la seule demande du journal).
  - SpringBoard, chronod (widgets, contrôles WidgetKit du Centre de contrôle) et iconservicesagent (icônes) n'ont donc aucun GPU.
  - Or ce qui manque est justement ce que ces processus dessinent hors écran. Le texte CoreText et les calques ordinaires, rendus par backboardd, s'affichent bien.
- **La mémoire GPU recyclée n'est jamais effacée.**
  - Le noyau rend chaque tampon neuf à zéro.
  - Mais le cache de tampons de panfrost (`panfrost_bo_cache_fetch`) redonne un tampon libéré **avec les pixels de son ancien usage**.
  - Sur les GPU d'Apple, une texture neuve part de zéro et QuartzCore dessine comme si c'était le cas. Ici, une texture neuve peut montrer un vieux contenu, par exemple le bleu du fond verrouillé : teinte au premier affichage, coins d'une couleur périmée.
- Les noyaux 0137 à 0184 renvoient tous au pilote `b2l-metal-v85`, si bien que l'instrumentation 0139 (`B2L ICON`) n'a jamais tourné.

## Changement

1. **`apply.py` (B1L-0185), `pan_bo.c` :** un tampon de ressource (texture ou tampon Metal, `PAN_BO_DELAY_MMAP`) repris dans le cache est mis à zéro. Un tampon neuf l'est déjà par le noyau.
2. **`b2l_metal_plugin.m` :** le GPU est donné à `backboardd`, `SpringBoard`, `chronod` et `iconservicesagent`. Le journal nomme chaque processus qui demande (`b2l-plugin: <nom> (pid …) asks for a device`).

## Installé

- **Vérifié :**
  - `air-b2l: recycled resource buffers zeroed` dans `out/build-mesa-0185.log` (`b2l_reused` dans `pan_bo.c`) ;
  - `chronod` et `iconservicesagent` dans `build-metal/b2l-metal-v89/B2LMetalv89` ;
  - le noyau nomme `b2l-metal-v89`.
- **SYSTEM :** SYSTEM-OK en 62 s (6 blocs sur 1750).
- **BOOT :** relecture `06ffd04747e956ef41864455d322437e4eda84654620f73cfee5b8b3769a910e` (BOOT-OK).
- Redémarré vers le système par `adb reboot`.

## Résultat (journal `out/logs/0185-boot.txt`)

- **Bloqué sur la pomme : SpringBoard plante en boucle** (pid 28, 340, 350 ; `EXC_BAD_ACCESS` à l'adresse 0x10).
- **La piste est confirmée :** `chronod` et `SpringBoard` demandent bien le GPU (`b2l-plugin: … asks for a device`, `device made`). Avant, ils n'en recevaient pas.
- Pile : `RB::RenderQueue::encode` → `RB::Device::tables_buffer()` dans **RenderBox** (le moteur de dessin de SwiftUI).
  - RenderBox enveloppe ses 16 Ko de tables par `newBufferWithBytesNoCopy:length:options:deallocator:`.
  - Notre pilote répondait `nil`, et RenderBox utilise la réponse sans la vérifier.
- chronod n'a pas planté. La remise à zéro des tampons n'a pas pu être jugée à l'écran.
- Suite : B1L-0186.

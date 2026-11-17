# B1L-0164 : clavier — couper la dictée dans UIKit

Ensemble : `b2l-metal-v88` (noyau seul, `--no-libs`). Part de 0163 (Wi-Fi), même arbre.

## Pourquoi

Voir [clavier-diagnostic.md](clavier-diagnostic.md) : la disponibilité de la dictée change sans arrêt ; à chaque fois, le dock du
clavier recharge ses images sur le thread principal de l'app, et les touches attendent derrière (jusqu'à une minute).
L'option « Dictée » n'existe pas dans Réglages sur ce port, donc on ne peut pas la couper à la main.

## Changement

Nouveau `xnu-overlay/bsd/kern/b2l_kbd_dictation.h`, appelé dans `b2l_userspace_before_init` (`b2l_userspace_seed.h`) avant
launchd. Il écrit dans `dyld_shared_cache_arm64e.03` (USERDATA, accord du propriétaire donné) :

| Méthode UIKit | VA | Décalage .03 | Nouveau code |
|---|---|---|---|
| `-[UIDictationController dictationConnnectionDidChangeAvailability:]` | 0x18966bb2c | 0x2a5fb2c | `ret` |
| `-[UIDictationController dictationEnabled]` | 0x1897a7e48 | 0x2b9be48 | `mov w0,#0 ; ret` |

Gardes : chemin sur la racine, en-tête `dyld_v1` et UUID du sous-cache `c88713fe…ec72`, premier mot = `pacibsp` d'origine ou NOP
traduit, second mot d'origine ; sinon rien n'est écrit. Relecture après écriture, fsync. Idempotent (déjà patché → compté
`already`). Désactivable par `b2l_kbd_dictation=0`.

Journal attendu : `B2L KBD: dictation patch patched=2 already=0 error=0` au premier démarrage, puis `patched=0 already=2`.

## Résultat

Testé par le propriétaire : **le clavier est fluide**.

# B1L-0200 — détecteur de blocage à l'écran (accord du propriétaire)

Base : `b2l-metal-v95`, `--no-libs`. Wi-Fi identique au 0199 (IPv6 refusé sur en0, PM off, déconnexions sérialisées,
test de l'événement au démarrage, journal 1 s).

- `osfmk/arm64/b2l_persistent_log.c` : `b2l_log_beat` = l'heure du dernier passage du thread du journal (chaque seconde).
- `osfmk/arm64/b2l_prof.c` : `b2l_hang_irq`, appelé à chaque IRQ (`xnu/osfmk/arm64/sleh.c`, `sleh_irq`, modifié
  directement dans `xnu/` comme le crochet B1L-0113) : note par CPU le pc/lr/fp interrompu ; si le journal n'a pas
  tourné depuis 6 s, dessine une fois à l'écran (via `b2l_early_panic`) chaque CPU : âge de la dernière IRQ, U/K,
  pc, lr, thread, et pour un CPU en noyau sa chaîne de frames (adresses noyau sans slide → `out/mach-0200.unstripped`).
  Un CPU qui tourne interruptions masquées apparaît avec un âge élevé.

## Plan pour écarter les causes
1. Photo de l'écran au gel → fonction exacte où chaque CPU est bloqué.
2. Si un CPU est dans le pilote Wi-Fi ou un accès PCIe → corriger le pilote.
3. Si c'est le réseau du noyau (dlil, nd6, in6...) → corriger notre interface en0.
4. Si c'est ailleurs (GPU, émulation arm64e, autre) → hors Wi-Fi : le dire, avec la preuve.

## Résultat
Gel au clic sur « Rejoindre » : écran noir, aucun texte dessiné. Le gel bloque donc aussi les interruptions de tous les CPU.
La cause probable a été trouvée ensuite (voir 0202) : SHA-1 du noyau via un pointeur NULL, appelé par IPv6 CGA.

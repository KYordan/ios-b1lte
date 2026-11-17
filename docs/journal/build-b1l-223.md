# B1L-0223 : trace des exceptions du code JIT de WebKit

Identique à 0222, plus deux changements :
- toute exception EL0 venant de WebKit est écrite d'avance (`trap` : esr, pc, far) si le pc est hors de la région
  partagée 0x180000000–0x300000000, c'est-à-dire du code JIT ou le binaire de l'app. Les appels système et les fautes de
  données sont exclus. La trace est placée après la réactivation des interruptions dans `sleh_synchronous`.
- `emu` ne trace plus que les pc hors de la région partagée. Les traps PAC de corecrypto ne prennent donc plus le
  budget.

BOOT `4917ceae…ad3c` : BOOT-OK. SYSTEM-OK.

## Résultat

Propriétaire : Safari plante toujours (gel, retour à TWRP). Log `out/logs/0223-safari.txt` :
- 21 fautes d'instruction normales (chargement des pages du binaire WebContent), puis `ic` et `done` sur
  0xb9c600000 (début de la zone JIT) ;
- ensuite plus rien : aucune exception venant du code JIT, aucun appel système tracé. Le système gèle moins d'une
  seconde après.

Le gel suit donc l'arrivée du JIT de WebKit, pas une exception. Patch écrit, **non appliqué ni compilé** (demande du
propriétaire) : `patches/0109-b2l-webkit-no-jit.patch`. Il refuse `MAP_JIT` aux processus `com.apple.WebKit*` par le
même retour EPERM qu'un processus sans droit JIT, pour que JavaScriptCore tourne sans JIT. Que JavaScriptCore se
replie proprement sur l'interpréteur n'est **pas vérifié**. La session Metal a le téléphone pour l'instant.

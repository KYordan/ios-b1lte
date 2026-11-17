# Build B1L-0117 : l'interrupteur b2l_diag lu une seule fois

Date : 2026-11-08. Base : B1L-0116. Noyau seulement, ensemble `b2l-metal-v69`.

## Pourquoi

L'échantillonneur (0115) trouve environ 2 000 échantillons noyau de backboardd en 10 s dans `PE_parse_boot_argn_internal`. `progress_role()` (`bsd/kern/b2l_diag.c`) est appelée à chaque appel système de SpringBoard et de backboardd (deux fois depuis `b2l_diag_syscall`) et, depuis B1L-0110, relit toute la ligne de démarrage pour savoir si les traces (`b2l_diag=2`) sont actives.

## Changement (`xnu-overlay/bsd/kern/b2l_diag.c`, copié dans `xnu/`)

- `b2l_diag_level()` lit le boot-arg une fois et le garde ; les quatre lectures passent par elle.
- `progress_role()` rend la main tout de suite si les traces sont coupées, avant de comparer les noms de processus.

## Installé

- **BOOT :** BOOT-OK. Pas redémarré (0118 suit).

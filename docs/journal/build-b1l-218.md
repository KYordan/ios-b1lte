# B1L-0218 : émulation arm64e complétée, cache d'instructions pour le JIT, noyau nettoyé

Construit avec `b1l-build.sh 0218 b2l-metal-v95 --no-libs` depuis `b1l`, après fusion de `refactor/arm64e-uefi`
(commit `b4d889b`).
- BOOT `c812c195…7584` : BOOT-OK.
- SYSTEM : 7 blocs sur 1750 récrits, SYSTEM-OK.
- USERDATA n'a pas été touché.

Le firmware et le chargeur de production (`mu-work-b1l-touch`) ont été reconstruits depuis `b1l`. Ils sont identiques
à 0217 (chargeur `cd8308b6…`, firmware `d9506d37…`) : le menu de démarrage reste.

## Ce qui change dans le noyau

1. **WebKit : vidage du cache d'instructions.**
   - La seule fonction d'iOS qui vide ce cache, `sys_icache_invalidate` (libsystem_platform), fait `IC IVAU` sans
     `DC CVAU` avant. Cela suffit sur les puces Apple (CTR_EL0.IDC=1), mais pas sur l'Exynos 9820.
   - Conséquence : le code JIT fraîchement écrit s'exécutait périmé. On compte 262 morts de WebKit sur des
     instructions pourtant valides, entre 0202 et 0213.
   - Correction : `SCTLR_EL1.UCI` est maintenant coupé sur B2L. L'`IC IVAU` du programme est donc piégé, et
     `sleh.c b2l_user_cache_op` nettoie puis invalide la ligne par l'alias physique.
   - Le correctif est enregistré dans `patches/0108-b2l-el0-cache-maintenance.patch`.
2. **Instructions ajoutées à l'émulation** (les cœurs du S10 n'ont ni lrcpc2, ni sb, ni dit, ni ssbs) :
   - LDAPUR et STLUR, avec toutes leurs variantes (signe, décalage, SP), et faute si l'accès est mal aligné ;
   - SB, et MSR DIT/SSBS (immédiat ou registre), traités comme sans effet ;
   - MRS DIT/SSBS, qui lisent 0 ;
   - FJCVTZS, déjà ajoutée sur la branche, est maintenant compilée dans le noyau.
3. **Nettoyage :**
   - le PACGA QARMA5/SipHash mort est supprimé (fichiers, tests, `conf/files`) ;
   - les mesures (comptes, PACPROF, fil surveillé, minutages, vérification complète du PC) ne sont compilées
     qu'avec `B2L_ARM64E_DIAG=1`. Par défaut, le chemin des pièges ne fait plus aucune mesure ;
   - un rejet d'émulation est journalisé au plus 64 fois par démarrage, avec l'instruction.

## Recensement du vrai code du cache 23G90 (`tools/scan-isa.py`, sections de code seulement)

| Famille | Nombre | Traitement |
|---|---|---|
| LDAPR | 134 175 | émulée, et traduite en LDAR dans le cache |
| FJCVTZS | 535 (JavaScriptCore, WebCore) | émulée |
| LDAPUR/STLUR | 14 | émulées (0218) |
| MSR DIT | 96 (corecrypto) | sans effet (0218) |
| SB | 6 | sans effet (0218) |
| FCMLA/FCADD | ~25 000 (libBLAS, calcul complexe seulement) | **non émulées** |
| FlagM, FRINT32/64, SSBS | 0 | — |

## Résultat

À remplir après le test du propriétaire :
1. Safari et les vues web : plus de morts de WebKit ?
2. Les journaux : lignes `B2L COMPAT: rejected` ou `unhandled-or-fault` restantes.

**Test du propriétaire : ouvrir Safari gèle tout le téléphone.**
- Le journal (`beyond2lte-ios/out/logs/0218-safari.txt`) s'arrête net, environ 2 s après le lancement de
  `com.apple.WebKit.WebContent`.
- Cause probable : `b2l_user_cache_op` passait par `phystokv()`, qui panique (« illegal PA ») sur la RAM hors de sa
  table. Or les paniques ne sont pas journalisées sur cette carte, d'où un gel sans message.
- Corrigé dans B1L-0219 : la maintenance passe par une fenêtre de copie du processeur, valable pour toute page.

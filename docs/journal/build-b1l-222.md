# B1L-0222 : trap de cache EL0 sans fenêtre de copie

`b2l_user_cache_op` (`xnu/osfmk/arm64/sleh.c`) n'utilise plus `pmap_find_pa` ni la fenêtre de copie. Pour chaque
ligne, elle fait :
1. couper les interruptions ;
2. `AT S1E0R` sur l'adresse utilisateur ;
3. si EL0 peut lire la ligne : `DC CVAU` puis `IC IVAU` sur cette même adresse (TTBR0 est encore celui du fil ;
   PAN ne s'applique pas aux opérations de cache par adresse).

La trace WebKit de 0221 reste en place. Elle ajoute une ligne `done` écrite d'avance après chaque opération :
- une ligne `done` présente : l'opération se termine, et le gel, s'il reste, vient d'ailleurs ;
- une ligne `ic` sans `done` : le gel est dans l'opération elle-même.

Limite : si un autre fil démappe la ligne entre la vérification et l'opération, le noyau prend une faute.

BOOT `9d8327ba…d953` : BOOT-OK. SYSTEM-OK.

## Résultat

Propriétaire : Safari s'ouvre en quelques secondes, puis le système gèle (retour à TWRP).
Log : `out/logs/0222-safari.txt`.
- `B2L WK 0: ic pid 367 0x5 0xbe05b0000` puis `B2L WK 1: done … par 0xff0000098e4c8b00`. **L'opération de cache se
  termine** : AT réussit, PA 0x98e4c8000. Le gel de 0218/0219 dans l'opération elle-même est donc corrigé.
- Le système gèle moins d'une seconde après, sans autre ligne. WebKit reprend alors la main, très probablement dans son
  code JIT. Les 100 lignes `emu` avaient déjà été prises par corecrypto. Voir 0223.

# B1L-0202 — SHA-1 du noyau (cause probable du gel Wi-Fi)

Base : `b2l-metal-v95`, `--no-libs`. Comme 0201 (IPv6 permis, traceur d'ioctl).

## Cause trouvée par lecture du noyau sur le Mac
- Le port n'a pas de kext corecrypto : `g_crypto_funcs` reste NULL.
- Ces fonctions avaient déjà un secours : MD5, SHA-2, AES, rand (`corecrypto_sha2.c` utilise `ccsha256_ltc_di`).
- **SHA-1 n'en avait pas** : `SHA1Init/Update/Final` lisent `g_crypto_funcs->ccsha1_di`, donc un pointeur NULL.
- Appelants de SHA-1 :
  - `bsd/netinet6/in6_cga.c` : adresse IPv6 link-local CGA, configurée par configd dès que en0 a un lien. Le gel
    survient à ce moment précis (0198/0199 : gel au lien, sans gel quand IPv6 est refusé) ;
  - MPTCP ;
  - `vfs_cprotect.c` ;
  - signature de code (`ubc_subr.c`, SHA-1 ancien format).

## Correctif
- `osfmk/corecrypto/b2l_ccsha1_ltc.c` (nouveau, ajouté à `xnu/osfmk/conf/files`, sauvegarde `../files.0201bak`) :
  `ccsha1_ltc_di` en C portable. Testé sur le Mac contre `shasum` : « abc » et un message de 2 blocs sont identiques.
- `libkern/crypto/corecrypto_sha1.c` : si `g_crypto_funcs == NULL`, utilise `ccsha1_ltc_di` (même schéma que
  `corecrypto_sha2.c`). Hors périmètre Wi-Fi : une seule fonction de hachage ajoutée, nécessaire à IPv6.

## Résultat
Le gel au join a disparu. Le réseau marche : 680 trames envoyées, 552 reçues (354 Ko), DHCP, DNS et HTTPS de Safari et trustd. Mais :
- Réglages dit que le join a échoué, alors que la barre d’état montre le Wi‑Fi. Cause (0203) : iOS 26 ne lit plus les événements Wi‑Fi sur le socket d’événements du noyau ;
- gel en chargeant une page dans Safari, pendant que WebContent plante en boucle (`B2L COMPAT: rejected` sur du code JIT, puis « Too many corpses »). Safari n’avait jamais été ouvert avant ce build.

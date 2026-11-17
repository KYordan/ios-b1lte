# Build B1L-0011 : PACGA en une instruction + correctif « Informations » + traduction hors cache

Date : 2026-11-04. Statut : **construit, pas installé.** BOOT + USERDATA (écriture longue).
Base : B1L-0010 (DVFS ACPM, écran 1:1).

## 1. PACGA, ABI B2L v2 (BOOT + cache + binaires)

- Avant : chaque PACGA piégeait (~11,6 µs par piège, dont 6,2 µs de vérification VM ; le SipHash ne coûte que 0,4 µs).
  ≥ 24 M paires en 14 min (B1L-0006).
- Maintenant : `PACGA Xd, Xn, Xm` ≡ **`EOR Xd, Xn, Xm, ROR #32`**, exécutée nativement.
  - `common/cache/b2l_cache_core.c` (`b2l_nc_native`) : règle partagée par le kernel (cache natif) et `nc-static`.
    Rm=31 (SP pour PACGA, XZR pour EOR) n'est pas réécrit.
  - `xnu-overlay/osfmk/arm64/b2l_compat.c` : les PACGA encore piégées calculent exactement la même valeur
    (`a ^ ror(b, 32)`, sans masque), pour que sites réécrits et émulés concordent.
  - La clé SipHash était aléatoire à chaque démarrage : aucune signature PACGA ne survit au redémarrage, donc pas de rupture persistante.
- Relevé du cache d'origine : 728 sites PACGA, résultat surtout comparé (`cmp`/`b.ne`) ou stocké ; les décalages vus
  (`lsr`, `orr … lsr`) n'utilisent que les bits hauts.
- Tests : `tests/arm64e/core/test_compat.c` mis à jour (valeur EOR, aucun rappel) : fixtures + 200 000 cas différentiels OK.
  Encodages vérifiés au désassembleur (`pacga x8,x8,x10` → `eor x8,x8,x10,ror #32` ; Rm=SP refusé).

## 2. Settings › Général › Informations

Cause : **bug Apple** dans MediaServices, atteint seulement quand `SecTaskCopyValueForEntitlement` renvoie une erreur
(jamais sur un vrai iPhone). `+[MSVEntitlementUtilities _checkBooleanEntitlement:task:]` et `_checkArrayEntitlement:group:task:`
rangent l'erreur avec `setError:` (retain + stockage, vérifié), puis la libèrent **deux fois** (`objc_release_x20`/`_x21` puis `CFRelease`).
L'ivar pointe vers une erreur libérée ; `.cxx_destruct` la relâche → SIGSEGV (`objc_release`, isa = mot de liste libre de malloc).

Correctif : `nc-static nop-call` remplace les deux `bl objc_release_xN` par `nop`, après vérification exacte de la cible :
`0x1ae8593f8 bl 0x1b2571fe0` et `0x1ae85d7f0 bl 0x1b2571ff0` (sous-cache `.15`). À faire plus tard : comprendre pourquoi
`SecTaskLoadEntitlements` échoue pour Réglages (DER des droits ? codes 33/7).

## 3. Traduction des binaires hors cache

`nc-static macho-apply` : sections d'instructions des tranches arm64e, sur place (même inode/propriétaire), idempotent.
Relevé avant : 2 473 fichiers Mach-O, 3 353 141 mots convertibles (PAC/AUT 1,05 M, hints 0,87 M, RETA 0,41 M, BRAA 1,02 M).

## Validation du cache (`static-work/out2`, depuis le cache **d'origine**)

`apply` : 4 317 images, 12 152 sections, **41 393 906 mots** (dont **687 PACGA**), 5 187 mots corecrypto exclus.
`compare` sur les 84 fichiers : 0 changement inattendu, sauf les 2 NOP voulus dans `.15`.

## Artefacts

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-pacga-0011/boot.img` (25 910 112 octets) | `82124880adeb18dad8507b96bd97d6b24a5300ffd9ee1da3d202cf8eef1d2e44` |
| Kernel | `a5392b257c874d7b43f8062fdd18c1e4678b92d767436820a2feee512f501cd6` |

Racine : `sudo bash b1l-tools/install-b1l-0011-root.sh` sur `out/live/ios26-userdata-live.img`.
Retour arrière : `out/live/ios26-userdata-live-0006.img` (clone) + BOOT `out/b1l-dvfs-0010/boot.img`.

## Ordre d'installation

BOOT d'abord, puis USERDATA, et ne redémarrer qu'après les deux. Un kernel 0011 avec l'ancienne racine reste cohérent
(tout PACGA y est émulé avec la nouvelle règle) ; l'inverse (nouvelle racine + kernel 0010 en SipHash) ne l'est pas.

## Note : état du téléphone

L'image hôte date d'avant le démarrage de B1L-0006. Ce que le téléphone a écrit depuis (caches MobileGestalt v3/v4,
pages apprises par le cache natif, journaux) sera remplacé ; tout se régénère au démarrage.

## Installation (2026-11-04)

- Racine (`install-b1l-0011-root.sh`, lancé par le propriétaire) : 13 fichiers de cache réécrits ; ~3 300 Mach-O traduits sur place,
  0 erreur ; contre-vérification : **0 mot convertible restant** ; `fsck_hfs` : « The volume B2LIOSROOT appears to be OK ».
- BOOT (sda14) : garde-fous OK, relecture SHA-256 `82124880…` identique.
- USERDATA (sda34) : **64/64 morceaux vérifiés** (39 écrits, 25 déjà identiques), rc=0. Pas encore testé.

## Résultat observé (B1L-0011)

- Démarrage **très rapide** ; Réglages s'ouvre. Plus aucun piège PACGA (`paired=0 hash_ns=0`).
- Informations : plus de SIGSEGV (correctif MediaServices efficace), mais arrêt TCC : « … must contain an
  NSAppleMusicUsageDescription key ». Cause : droits de Réglages illisibles (voir B1L-0012).
- Pièges : 196 M à 795 s, surtout `braa x16, x17` des îlots de stubs du cache (hors sections, non traduits ;
  le cache natif ne les réapprend pas : 381 805 apprentissages sautés à cause d'anciens enregistrements).
- DVFS : `parse=-2 … len=1` → **aucune fréquence changée** depuis B1L-0010.

# Build B1L-0101 : d'où viennent les interruptions PAC

Date : 2026-11-08. Base : B1L-0100 (qui contient 0099). Noyau seulement, diagnostic.

## Pourquoi

Le cache partagé traduit sur le Mac (`static-work/out`) n'a plus d'instruction PAC, sauf les 5 187 mots du texte mesuré de corecrypto, gardés d'origine volontairement (auto-test d'intégrité). Le cache natif du noyau n'apprend plus que des instructions de corecrypto (`measured` ≈ 43 000 par boot, `learned` ≈ 7) : les 20 000 à 80 000 interruptions par seconde viennent donc très probablement de corecrypto. Reste à savoir **qui l'appelle autant**.

## Changement (`b2l_arm64e_compat.c`, `b2l_persistent_log.c`)

Chaque interruption est comptée par (PC, LR, processus) dans une table de 1024 places (avec perte). Toutes les 30 s, les 24 plus fréquentes sont écrites puis la table repart à zéro :

`B2L PACPROF: <nombre> <processus>[pid] pc=0x… lr=0x… insn=0x…`

PC et LR sont des adresses d'exécution (avec glissement) ; le glissement se retrouve sur le Mac en faisant correspondre les instructions avec le cache.

Ensemble `b2l-metal-v57`.

## Installé

- **Vérifié :** le noyau contient `B2L PACPROF`.
- **SYSTEM :** SYSTEM-OK en 55 s. **BOOT :** relecture `1d9927c51d9201f2d108f0d38a230fd1ded0bbbb03b6b86a452f824306cbc157` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0098/boot.img`.

## Résultat (journal `out/logs/0101-boot.txt`)

- Propriétaire : **visuellement mieux** (viewport et scissor de 0099), **toujours lent et saccadé**.
- **Émulation PAC (0100) : 1,9 µs par interruption au lieu de 11 µs** (`wrapper_ns / samples` = 68 ms / 36 743 ; `vm_ns = 0`).
- **Profil `B2L PACPROF`** (cumul du boot) :
  - **`dasd`** : ≈ 7,2 millions, toutes à `pc 0x1eabc1348`/`0x1eabc1050` (`RETAB`) dans le cache partagé, la même adresse que `configd`, `appprotectiond`, `securityd`, `tccd`, `Spotlight` : une fonction de corecrypto très appelée, par des démons d'arrière-plan.
  - **`backboardd`** : ≈ 720 000, dont ≈ 680 000 sont des **`LDAPR`** (`f8bfc113`, `f8bfc2c8`, `b8bfc288`) à `0x117…`, hors du cache partagé : **les lectures atomiques de Mesa dans `libhakiosair`** (56 `LDAPR` dans le binaire). L'Exynos M4 n'a pas RCpc (ARMv8.3).
- **Mais backboardd passe toujours 82,6 s sur 90 dans le noyau** : à 1,9 µs, ses interruptions PAC ne font qu'≈ 1,4 s. Le temps noyau vient d'ailleurs. L'attente des jobs du GPU dort correctement (`hakios_mali_wait`). Le journal ne fait que ≈ 57 000 lignes sur le boot.
- GPU occupé 1 à 4 %. 0 faute 0x51, 10 fautes MMU. Zéro copie toujours actif.
- La surface de 352 octets par ligne est créée par **SpringBoard** (168×124, `LA08`), pas par backboardd : l'arrondi de 0099 ne la touchait pas.

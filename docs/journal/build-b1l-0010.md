# Build B1L-0010 : fréquence CPU via ACPM (presque au max)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14) le 2026-11-04, relecture SHA-256 `5972b6c1…` identique ; pas encore testé.** BOOT seulement. B1L-0009 avait démarré normalement (première image, journal jusqu’à 969 s).
Base : B1L-0009 (écran 1:1 @3x, firmware `52d8e75c…`, sonde `B2L CPUFREQ`, profil v4).

## Pourquoi

Mesure B1L-0008 : A55 ~1053, A75 ~1222, M4 ~1378 MHz, soit environ 50 % du maximum. S-Boot laisse les fréquences de démarrage
et rien ne les change ensuite.

## Comment (d'après le kernel Samsung, `LineageOS/android_kernel_samsung_exynos9820` `a412cfaf`)

- `drivers/soc/samsung/cal-if/acpm_dvfs.c` : `exynos_acpm_set_rate(id, rate)` envoie `{id, rate kHz, FREQ_REQ=0, 0}`.
  `FREQ_GET=1` relit la fréquence courante.
- `drivers/soc/samsung/acpm/acpm_ipc.c` : file en SRAM APM, numéro de séquence (6 bits) dans `cmd[0]<<16`,
  sonnette `INTGR0 = (1<<id)<<16`, réponse lue dans la file retour, puis `INTCR1`.
- C'est le **firmware APM** (chargé par S-Boot) qui règle PLL, PMIC et tensions, d'après sa propre table fréquence/tension (fvmap).
  Le kernel ne fixe aucune tension.

Valeurs du DTB du S10 : `acpm_ipc` reg `0x15900000` (boîte aux lettres) + `0x02039000`/`0x30000` (SRAM),
`initdata-base 0x7000`, `acpm` `fvmap_offset 0xa400`, `acpm_dvfs` canal 5, `cpufreq/domain@0/1/2` cal-id
`0x0b040002/3/4`, `max-freq` 1 950 000 / 2 400 000 / 2 080 000 kHz.

## Règle « presque au max » (choix du propriétaire)

Pour chaque cluster : le **plus haut palier de la table du firmware strictement inférieur au `max-freq` du DTB**.
Rien n'est envoyé si : l'analyse de la SRAM échoue, la table n'est pas strictement décroissante, une tension sort de
400–1300 mV, `FREQ_GET` ne répond pas, ou le palier choisi n'est pas plus haut que la fréquence actuelle.
Coupe-circuit : boot-arg `b2l_dvfs=0` (lecture seule).

## Fichiers

| Fichier | Changement |
| --- | --- |
| `xnu-overlay/iokit/Kernel/arm/B2LACPM.h` (nouveau) | analyse bornée de la SRAM (canal, plugin DVFS résident, fvmap), choix du palier |
| `xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp` | envoi/réception IPC (délais 15 ms / 50 ms), journal `B2L DVFS:` (table complète), nouvelle mesure `B2L CPUFREQ after-dvfs` |
| `tests/test_acpm.c` (nouveau) | SRAM synthétique : décalages, choix du palier, refus des tables et géométries invalides (ASan/UBSan) |

## Écritures matérielles de ce build

Écritures **uniquement** dans la SRAM APM (file d'envoi du canal 5, pointeur de lecture de la file retour) et les registres
`INTGR0`/`INTCR1` de la boîte aux lettres, exactement comme le pilote Samsung. Aucune écriture de stockage.

## Vérifications

- `tests/test_acpm.c` : toutes les vérifications passent. Kernel : 0 erreur, 0 LDAPR.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-dvfs-0010/boot.img` (25 910 112 octets) | `5972b6c16b88a8b556a602b09329e6d72ebc829e086f1a2b667772c7d4c7c2a7` |
| Kernel | `f83567d60ecb820784fd72c58052851fedd5886617eacfa0272eefd96bfcc14a` |

## Risques

- Pas de limite thermique : ne pas laisser le téléphone tourner sans surveillance. S'il chauffe fort, l'éteindre.
- Firmware qui ne répond pas : délai dépassé, message journalisé, rien n'est changé.
- Blocage au démarrage : remettre `out/b1l-cpufreq-0009/boot.img` (`b80d5871…`).

# Build B1L-0102 : `LDAPR` de Mesa réécrits + chronométrage des appels au noyau

Date : 2026-11-08. Base : B1L-0101.

## Changements

1. **`scripts/unsign-branches.py`** : `LDAPR`/`LDAPRB`/`LDAPRH` → `LDAR`/`LDARB`/`LDARH` dans nos images (les 56 de `libhakiosair`). `LDAR` ordonne au moins aussi fort : sans risque. Supprime ≈ 680 000 interruptions par boot dans backboardd.
2. **Noyau, `B2LPerf.h`** (diagnostic) : `B2LCallTimer` chronomètre chaque appel des clients HakiosMali (GPU), IOSurface et IOMobileFramebuffer, par sélecteur. Toutes les 10 s, après `B2L PERF` : `B2L PERF CALLS: mali N: X x T ms | surf N: … | fb N: …` (les 8 plus coûteux). Les attentes (sélecteurs de wait) comptent le temps dormi, pas du CPU.

Ensemble `b2l-metal-v58`.

## Installé

- **Vérifié :** `unsign-branches` : `libhakiosair.dylib: __TEXT,__text 56` ; plus aucun `LDAPR` dans le binaire.
- **SYSTEM :** SYSTEM-OK en 83 s. **BOOT :** relecture `131191eb32b85f4f5dae251ff971cb3498f0a761ed6dfef807d7cffcbfebae2f` (BOOT-OK).
- **Pas démarré seul :** B1L-0103 est construit juste après et le contient.

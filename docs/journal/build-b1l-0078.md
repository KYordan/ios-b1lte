# Build B1L-0078 : relais de l'éditeur de liens sans saut signé

Date : 2026-11-07. Base : B1L-0077. **Flashé mais jamais démarré : remplacé par B1L-0079, qui le contient.**

## Changement

- **Nouvel outil `scripts/unsign-branches.py`.** Dans toutes les sections d'instructions de nos bibliothèques, il remplace :
  - `BRAA/BRAB/BRAAZ/BRABZ Xn` → `BR Xn` ;
  - `BLRAA…` → `BLR Xn` ;
  - `RETAA/RETAB` → `RET`.

  `build-metal-scion.sh` l'appelle avant la signature ad hoc.
- **Pourquoi c'est sûr :** le noyau émule PAC en mode « pointeurs non signés », donc les adresses de `__auth_got` ne sont jamais vraiment signées, et un `br x16` va au même endroit que `braa x16, x17`.
- **Relais modifiés :**

| Bibliothèque | `__auth_stubs` | `__objc_stubs` |
| --- | --- | --- |
| `libhakiosair` | 162 | — |
| `libhakiosmetal` | 67 | 163 |
| `libb2l-metal-test` | 50 | 58 |
| `libb2l-metal-drvtest` | 78 | 116 |

- Ensemble `b2l-metal-v34`. BOOT : `out/b1l-metal-0078/boot.img`, relecture `d4c48c5f…`.

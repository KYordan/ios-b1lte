# Build B1L-0110 : GPU à 650 MHz, diagnostics coupés, bascule d'écran avec une image d'avance

Date : 2026-11-08. Base : B1L-0109. Demande du propriétaire.

## GPU à 650 MHz fixes (`B2LCPU.cpp`)

Le GPU était déjà à fréquence fixe, réglée par ACPM au démarrage : 676 MHz (le niveau le plus haut sous 702 MHz). La cible passe à **650 MHz** (niveau 2 de la table du firmware, 662,5 mV ; `b2l_acpm_pick` prend le niveau strictement en dessous, d'où `650001`). Le fil thermique peut toujours le baisser si le G3D chauffe.

## Diagnostics coupés

- **Mesa** : les vérifications (`assert`) étaient déjà coupées (`b_ndebug=true`). Coupés en plus (`apply.py` B1L-0110) : les modules de shaders envoyés dans le journal (`B2LMOD`) et la ligne par pipeline.
- **Pilote Metal** : messages répétés muets (`B2L_QUIET`) : réglages de blend, descripteurs de sommets, dumps `B2L fc`. Les erreurs restent, ainsi que le suivi des command buffers de B1L-0109 (le plantage n'est pas encore compris).
- **Noyau** :
  - les traces `b2l_diag` (IPC, attentes, progression, appels système, TCC, environnement de SpringBoard) ne s'activent plus qu'avec `b2l_diag=2`. Le chargeur UEFI passe `b2l_diag=1`, donc elles sont coupées sans reconstruire l'UEFI ;
  - `B2L PERF` / `B2L PERF CALLS` seulement avec `b2l_perf=1`, le profil PAC seulement avec `b2l_pacprof=1` (le comptage par interruption est sauté aussi).
- Restent dans la ligne de commande UEFI, à revoir avec une reconstruction de l'UEFI : `serial=3`, `-enable_kprintf_spam`, `debug=0x14e`.

## Bascule d'écran avec une image d'avance (`B2LFramebuffer.cpp`)

Comme les chaînes d'images d'Android et de Metal : une bascule écrit l'adresse et demande la mise à jour, puis **rend la main tout de suite**. Elle n'attend que la bascule **précédente**, si le panneau ne l'a pas encore prise, avant d'écrire ses registres : une seule bascule en vol. Les 8 premières restent attendues et vérifiées. Si une bascule n'est pas prise en 50 ms, retour à la copie comme avant.

Ensemble `b2l-metal-v66`, Mesa recompilé.

## Installé

- Compilé : Mesa (patch B1L-0110 appliqué), bibliothèques, noyau, `out/b1l-metal-0110/boot.img`.
- **Pas encore flashé** : le téléphone était démarré sur 0109, pas en TWRP (l'étape SYSTEM s'est arrêtée sans rien écrire). À flasher dès qu'il est en TWRP (`b1l-build.sh 0110 b2l-metal-v66 --no-libs --no-kernel`), puis redémarrer.

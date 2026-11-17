# B1L-0271 — Audio : iOS joue le son (lots 3 et 4)

## Pourquoi (demande du propriétaire, « lier l'audio à iOS »)

- Le bip de 0213 a prouvé tout le chemin matériel : RDMA0 → SIFS0 → UAIF1 → CS35L40 bot@40.
- Ce build garde ce chemin ouvert pour iOS et retire le bip de démarrage.

## Changements

- `xnu/iokit/Kernel/arm/B2LAudio.h` (lot 3, A6, `b2l_audio=7` par défaut) :
  - vérification silencieuse, puis chemin gardé ;
  - fil noyau toutes les 10 ms ;
  - ampli allumé seulement quand du son arrive, éteint après 3 s de silence ;
  - plafond −6 dB (`b2l_audio_max=48`), boost coupé ;
  - au bout de 3 erreurs d'ampli, haut-parleur coupé jusqu'au reboot.
- `xnu/bsd/kern/b2l_audio_seed.h` (nouveau) et 2 lignes dans `b2l_userspace_seed.h` :
  - `/dev/hakios-audio` ;
  - publication du plug-in HAL `HakiosAudio.driver` (HakiOS, MIT) depuis SYSTEM:/b2l-audio-v1.
- Contient aussi le Bluetooth étape 3 de 0270, inchangé. Set Metal v133 inchangé (`--no-libs`).
- Détails : `docs/audio/review-and-plan.md` §11.

## Coupe-circuits

- `b2l_audio=6` : bip de 0213, rien gardé.
- `b2l_audio=5` : pas de chemin.
- `b2l_audio_ios=0` : ni /dev ni plug-in.

## Installé

- SYSTEM : `/b2l-audio-v1` ajouté (4 blocs, SYSTEM-OK).

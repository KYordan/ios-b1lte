# Build B1L-0131 : panfrost recharge les cibles que le shader relit

Date : 2026-11-09. Base : B1L-0130. Ensemble `b2l-metal-v80`, Mesa recompilé.

## Diagnostic

- B1L-0130 :
  - les glyphes vibrants (Centre de contrôle, widgets) sont dessinés par `fixed_frag`, qui relit les attachements 0 et 1 (framebuffer fetch) ;
  - l'attachement 1 est un RGBA16Float de la taille de la passe.
- Panfrost (Gallium) ne recharge une cible couleur dans la mémoire de tuile (`fb->rts[i].preload`, `pan_job.c`) que si un dessin de la passe l'**écrit** (`panfrost_set_batch_masks_blend`, info de mélange activée) ou si `batch->read` la nomme.
- Or `batch->read` n'est rempli que pour la profondeur et le stencil (`pan_helpers.c`). Une cible seulement **relue** par le shader n'était donc jamais rechargée : la relecture rendait ce que la tuile contenait (vide ou résidu).

## Changement

- `apply.py` (B1L-0131, sur `gallium/drivers/panfrost/pan_cmdstream.c`, patch qui ne s'applique qu'une fois) : dans `panfrost_update_state_3d`, `batch->read |= outputs_read du shader fragment × PIPE_CLEAR_COLOR0`. Chaque cible lue par le shader est rechargée, qu'elle soit écrite ou non.
- Retrait de l'inventaire `B2L FN` de 0130 : il a donné sa réponse et coûtait une recherche à chaque dessin.

## Installé

- **Vérifié :** `air-b2l: colour targets read by framebuffer fetch preloaded` dans le journal de compilation Mesa, patch présent une seule fois dans `pan_cmdstream.c`.
- **SYSTEM :** SYSTEM-OK en 70 s (7 blocs sur 1750). **BOOT :** relecture `a0c79f2ecb6bf0a66b03ae5be30b46b9166e7257b6409340d56f7a20ff828627` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (photos du propriétaire, journal `out/logs/0131-boot.txt`)

- **Mieux en partie :** dans le Centre de contrôle, les glyphes Wi-Fi, AirDrop, cellulaire, Bluetooth et AirPlay s'affichent.
- **Toujours absents :** chiffres de l'Horloge, icône Calendrier vide, glyphes des boutons ronds du bas, contenu des widgets. Le glyphe en haut à gauche sort en hachures, comme une image lue avec une mauvaise largeur de ligne.
- **Journal :** aucun plantage. Les seuls imports refusés sont des surfaces `LA08` dont les lignes ne sont pas alignées sur 64 octets : 298×286 à 608 octets par ligne (InCallService), 240×240 à 480 (Spotlight). Suite : B1L-0132.

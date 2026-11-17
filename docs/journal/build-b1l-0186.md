# Build B1L-0186 : `newBufferWithBytesNoCopy:` répond par une copie (SpringBoard ne plante plus dans RenderBox)

Date : 2026-11-12. Base : B1L-0185. Ensemble `b2l-metal-v90` (Mesa inchangé).

## Diagnostic (journal `out/logs/0185-boot.txt`)

- Avec le GPU, SpringBoard plantait en boucle dans `RB::Device::tables_buffer()` (RenderBox), à l'adresse 0x10.
- RenderBox demande un tampon de 16 384 octets par `newBufferWithBytesNoCopy:length:options:deallocator:`. Le pilote répondait toujours `nil` (« not yet »), et RenderBox déréférence la réponse.

## Changement (`hakiosmetal_air.m`)

- `newBufferWithBytesNoCopy:…` répond par une **copie** (`newBufferWithBytes:length:options:`), avec une trace `newBufferWithBytesNoCopy of N bytes answered with a copy`.
- Raccourci noté dans le code : les écritures ultérieures de l'appelant ne sont pas vues, et le `deallocator` n'est jamais appelé.

## Installé

- **Vérifié :** `answered with a copy` dans `build-metal/b2l-metal-v90/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK en 60 s. **BOOT :** relecture `7d736f77a93b24331e2810a73b66dfb96f3f4ac52d3696017492d9b26c97c7e6` (BOOT-OK). Redémarré par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0186-boot.txt`, aucun plantage)

- **iOS démarre.** Propriétaire : la teinte violette a disparu (remise à zéro de 0185).
- Toujours absents : icônes Horloge et Calendrier (plaque grise ; le Calendrier est apparu une fois puis a disparu), une partie des glyphes du Centre de contrôle, contenu des widgets.
- `backboardd`, `SpringBoard`, `chronod` et `iconservicesagent` reçoivent tous un GPU (5 clients, 619 Mo d'adresses GPU).
- **Nouveau :** SpringBoard et iconservicesagent créent 82 surfaces `RGhA` (RGBA16Float : 120×120, 204×204, 184×184, 256×256, 448×256), contre 0 en 0184. Ils dessinent donc avec le GPU.
- Le pilote ne journalise que backboardd : on ne voit pas ce qu'il refuse aux autres processus.
- `newBufferWithBytesNoCopy:` est aussi appelé par backboardd (16 Ko, 32 Ko, 1,5 Mo), qui recevait `nil` jusque-là.
- SpringBoard : 6 créations d'une surface 1440×3040 BGRA refusées par le noyau (`0xe00002bd`), non traité.
- Suite : B1L-0187.

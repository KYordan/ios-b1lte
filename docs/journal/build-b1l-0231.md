# Build B1L-0231 : une texture posée sur un tampon (RenderBox), en copie tenue à jour

Date : 2026-11-14. Base : B1L-0230. Ensemble `b2l-metal-v102` (noyau reconstruit), Mesa inchangé, pilote recompilé.

## Changement (`userspace/metal/scion/hakiosmetal_air.m`, sauvegarde `hakiosmetal_air.m.before-0231`)

- `-[HakiosMetalBuffer newTextureWithDescriptor:offset:bytesPerRow:]` (et donc `newLinearTexture…`) ne répond plus
  nil. Le pilote crée une texture 2D à lui, remplie avec les octets du tampon, et la remet à jour chaque fois qu'elle
  est liée pour un dessin. C'est le mécanisme de copie tenue à jour de 0136, étendu à un tampon (`hakiosSetLiveBuffer:`).
- `-buffer` de la texture rend le tampon.
- Chaque demande est notée `B2L BUFTEX:` (taille, format, usage, décalage, octets par ligne), y compris les refus
  (pas 2D, plusieurs niveaux, hors du tampon).
- **Limite connue :** sens unique. Ce que le GPU écrit dans la texture ne revient pas dans le tampon. Une écriture
  dans le tampon par une passe antérieure du même command buffer n'est pas vue.

## Installé

- **Vérifié :** `B2L BUFTEX` dans `build-metal/b2l-metal-v102/libhakiosmetal.dylib`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `5587ec82d56c78164b7141edb32b87a2daa52b5a6db660fc30f31f1e5ddbd63a` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire, photo `b1l-0231-centre-controle.webp`)

- Pas de changement visible. Horloge et Calendrier apparaissent puis disparaissent au changement de page.
- Centre de contrôle : Wi-Fi, AirDrop, Bluetooth, soleil de la luminosité, « Focus » et « Not Playing » sont là. Le
  bouton données cellulaires a les **mêmes stries** que le cadran de l'Horloge. Recopie d'écran = un rectangle blanc.
  Les boutons du bas (lampe, minuteur, calculatrice, appareil photo…) sont vides.
- Journal `out/logs/0231-boot.txt` : les textures sur tampon sont maintenant faites (`B2L BUFTEX`) : R8 120×120 et 204×204,
  BGRA8 en 87, 128, 180, 204 et 216 px.
- **Mais SpringBoard demande aussi, à chaque fois, une texture 122×122 au format 90 (RGB10A2Unorm), et le pilote
  refuse ce format partout** (« packed into a 32-bit word… will not guess »). iconservicesagent en demande aussi.
- 12 jobs GPU en statut 0xff (déjà 2 en 0227), dont 3 dans des chaînes de `fa16dhmha4`. À suivre.
- Suite : B1L-0232.

# Build B1L-0132 : images à lignes non alignées copiées au lieu d'être refusées

Date : 2026-11-09. Base : B1L-0131. Ensemble `b2l-metal-v81` (Mesa inchangé).

## Diagnostic

- Après 0131, il manque encore des glyphes et des images (Horloge, Calendrier, boutons du Centre de contrôle, widgets), et un glyphe est hachuré.
- Le pilote refuse d'importer une IOSurface dont la ligne n'est pas un multiple de 64 octets (`rwd_surface_fits_adreno`). Le noyau n'arrondit la ligne que pour backboardd et SpringBoard (B1L-0099, 0103).
- Les autres processus créent des surfaces `LA08` (niveaux de gris avec alpha, comme les masques de glyphes et d'icônes) à 480 ou 608 octets par ligne. Elles étaient refusées au moins 128 fois par démarrage (`no drawable`).

## Changement (`hakiosmetal_air.m`, `newTextureWithDescriptor:iosurface:plane:`)

- Si l'import échoue pour une surface à un seul plan, en 2D, dont la ligne n'est pas alignée sur 64 :
  - une texture ordinaire est créée avec le même descripteur ;
  - le contenu de la surface y est copié (`IOSurfaceLock` en lecture, puis `replaceRegion` avec la ligne d'origine) ;
  - cette texture est rendue à QuartzCore.
- Trace `B2L: surface with N-byte rows copied into a texture`.
- Limite : la copie est faite une seule fois, à l'import. Une écriture ultérieure du CPU dans la surface n'est pas vue. Ça convient aux images fixes (icônes, glyphes).

## Installé

- **Vérifié :** trace `copied into a texture` dans `libhakiosmetal.dylib` (v81).
- **SYSTEM :** SYSTEM-OK en 58 s. **BOOT :** relecture `38177d211c391770e6f88a288e78ff52b673319394824b48fcbccf510762fecd` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat (journal `out/logs/0132-boot.txt`)

- **Propriétaire : aucune amélioration.**
- La copie marche : 1 surface (298×286, RG8, 608 octets par ligne) a été copiée, et plus aucun import n'est refusé. Mais ces images n'étaient pas celles qui manquent.
- Les surfaces à lignes élargies par le noyau pour SpringBoard sont de petites `LA08` (132×64, 156×134, 168×124), qui servent à mesurer la luminosité pour le verre, pas aux glyphes. Suite : diagnostic B1L-0133.

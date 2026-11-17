# B1L-0265 — test : qu'est-ce qui perd les écritures R8 / RG16F ?

## Pourquoi

- 0264 : dans une passe R8 + RG16F + BGRA8, seule BGRA8 est écrite. Il faut séparer format, nombre de composantes, plusieurs cibles
  et relecture de la cible (RenderBox relit sa couverture RG16F).

## Changement (`userspace/metal/b2l_metal_suite.m`, sauvegarde `.before-0265`)

- `testTargets`, après `testIconShaders` : R8 seule (sortie float, puis float4) ; RG16F seule (float2, puis float4) ; 3 cibles BGRA8 en
  float4 ; relecture de la cible (RGBA16F + RG16F, deux dessins qui ajoutent à l'ancienne valeur, comme l'accumulation de RenderBox).
- Set v128 → v129. Mesa et pilote inchangés.

## Installé

- SYSTEM-OK, BOOT `1fc83df80fefda98bf32d0882c206d6d8923f1da92800e3a0d670d5a6281baa6`, BOOT-OK.

## Résultat (log noyau `out/logs/0265-boot.txt`)

- 36 OK, 8 ÉCHEC, 1 SAUTÉ. R8 seule (float et float4), RG16F seule (float2 et float4) et **même 3 cibles BGRA8** : faux ; relecture de
  la cible (RGBA16F + RG16F) : **OK**.
- Les cibles sont donc **bien écrites** : la conclusion « seule BGRA8 écrite » de 0264 était fausse. Partout, le pixel (0,0) vaut 0 au lieu
  de 0,5/64 : chaque dégradé est en retard d'un demi-pixel. Le test BGRA8 de 0264 passait seulement grâce à sa tolérance plus large (2/255).
- Cause (code) : panfrost sur Bifrost annonce `fs_coord_pixel_center_integer` (pan_screen.c, `arch >= 6`) : sa position de fragment est le
  coin du pixel, et l'ajout de 0,5 revient à l'appelant. Le traducteur ne le faisait pas → correction en B1L-0266.


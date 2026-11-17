# Build B1L-0243 : copies tampon ↔ texture, et tampon « sans copie » rendu à l'app

Date : 2026-11-14. Base : B1L-0242. Ensemble `b2l-metal-v114`. Premier des correctifs demandés par le propriétaire
après l'app « Test Metal » (0242 : 24 OK, 9 ÉCHEC). Un thème par build : ici les **copies** (échecs 4 et 5 de 0242).

## Causes (lues dans `hakiosmetal_air.m`)

- `copyFromBuffer:…toTexture:` et `copyFromTexture:…toBuffer:` n'étaient **pas codées** depuis B1L-0122 : la copie était
  jetée (`B2L BLIT: … not implemented; dropped`). Test 0242 « blit tampon → texture » : texel 0.
- Un tampon `newBufferWithBytesNoCopy` est une copie ici (B1L-0186) ; la recopie vers la mémoire de l'appelant (0236)
  n'avait lieu que pour un dessin dans une texture posée dessus. Une copie (blit) ou un remplissage n'arrivait jamais chez l'app.

## Changement (`hakiosmetal_air.m`)

- Copies tampon → texture (blit 5) et texture → tampon (blit 6) enregistrées et exécutées dans l'ordre : attente du
  GPU (`rwa_finish`), puis `rwa_texture_write` / `rwa_texture_read` avec le pas de ligne donné par l'appelant.
- Un tampon sans copie qui reçoit une copie, un remplissage ou une copie texture → tampon est renvoyé à la mémoire de
  l'appelant une fois le GPU fini (même chemin que 0236/0238, avec sa garde).
- Limite notée dans le code : un tampon sans copie écrit par un calcul (compute) n'est pas renvoyé.

## Installé

- Compilé sans erreur.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `b86e33bc8f6af77c490fd4597ab2eac06338da432f2957de7b7ab9d55ca66249` (BOOT-OK). Pas redémarré : 0244 suit tout de suite (l'app de test donne un résultat par test, donc les changements restent séparables).

## Résultat

- OK (log `out/logs/0247-boot.txt`, Test Metal dans Dictaphone) : « blit tampon → texture » texel `0xffffffff` ; « newBufferWithBytesNoCopy » : le GPU écrit dans la mémoire de l'app. Échecs 4 et 5 de 0242 corrigés.

## Retour en arrière

- `hakiosmetal_air.m.before-0243`.

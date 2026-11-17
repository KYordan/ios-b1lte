# Build B1L-0127 : test, vues partielles réactivées pour le diagnostic des fautes

Date : 2026-11-08. Base : B1L-0126. Bibliothèques (`B2L_PARTIAL_VIEWS 1`) et noyau (nom de l'ensemble), ensemble `b2l-metal-v76`, Mesa inchangé.

## Pourquoi

0126 n'a vu aucune faute. Les vues de niveaux de mipmap en provoquaient 11 à 46 par boot (0123, 0124) : on les réactive le temps d'un test, pour que le diagnostic de 0126 dise si les adresses fautives viennent de tampons libérés trop tôt ou d'adresses périmées. **L'interface clignotera pendant ce test** ; retour à 0125 (ou correctif) ensuite.

## Installé

- **BOOT :** relecture `b831fdeda2d4784287cb60126dd3fb453ef947d7d24d9dc001f7fc4bb22053ad` (BOOT-OK). Redémarré vers le système.
- **Retour en arrière :** `out/b1l-metal-0126/boot.img` avec SYSTEM v75.

## Résultat (journal `out/logs/0127-boot.txt`, photo du propriétaire)

- **Propriétaire : le Liquid Glass se rend vraiment sur une partie du Centre de contrôle** (bouton Focus : réfraction et couleurs du fond, photo), affiché seulement par moments ; l'interface clignote ; le fond d'écran est noir.
- 14 fautes GPU, toutes des **lectures à des adresses jamais données au GPU** (aucune dans les 16 384 tampons libérés) : ce n'est pas un tampon libéré trop tôt. Forme : base alignée sur 64 Ko + petit décalage (`…0240`, `…0600`, `…2980`) → un descripteur du job porte une mauvaise adresse.
- Passes fautives : surtout `path_blit` + `attachment_clear` + **blend shader** (5), lecture de texture `TimgA2Xhf_IsrcCcl` (2), `variable_blur` + `glass_background`.
- Refus de notre côté : « colour attachment 1 is 768x1408 and attachment 0 is 384x704 » : une passe à deux attachements de tailles différentes (permis par Metal : la zone de rendu est la plus petite), refusée par notre contrôle.
- Le cache de framebuffer (B1L-0074) compare bien la taille : hors de cause.
- Suite possible : décoder le job fautif (pandecode) pour trouver le descripteur qui porte l'adresse.

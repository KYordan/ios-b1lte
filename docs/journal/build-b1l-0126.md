# Build B1L-0126 : d'où viennent les fautes GPU du verre (historique des libérations)

Date : 2026-11-08. Base : B1L-0125. Noyau seulement (diagnostic), ensemble `b2l-metal-v75`.

## Pourquoi

Le verre du Centre de contrôle a besoin des vues de niveaux de mipmap (0123), qui provoquent des fautes GPU et des clignotements. Même sans elles (0125), 6 fautes restent dans les passes du verre : des lectures à des adresses que la table des 4 096 derniers tampons ne connaît pas. Deux causes possibles, deux correctifs différents :
- un tampon libéré **avant** la soumission du job : adresse périmée dans un descripteur ;
- un tampon libéré **après** : le job s'en servait sans l'avoir déclaré (`bo_handles`), et le noyau ne l'a donc pas gardé.

## Changement (`xnu-overlay/bsd/dev/b2l_mali_core.c`, copié dans `xnu/`)

- Historique des 16 384 dernières libérations : adresse, taille, type (tampon panfrost ou IOSurface, dimensions, format), moment, nombre de jobs déjà en file.
- Le numéro du job en cours d'exécution est retenu.
- À chaque faute, une ligne `B1L-0126` : le tampon libéré qui contenait l'adresse, depuis combien de temps, et s'il a été libéré avant ou après la mise en file du job fautif ; ou bien que l'adresse n'est dans aucun tampon libéré connu.

## Installé

- **Vérifié :** messages `B1L-0126` dans le noyau.
- **BOOT :** relecture `e250ad21c0f05cacf6b761f1fabcc46e2ed382f62bcfee5cb5a7cf6980a79d6c` (BOOT-OK). Redémarré vers le système.

## Résultat (journal `out/logs/0126-boot.txt`)

- 0 faute GPU pendant ce test (elles sont intermittentes sans les vues partielles) : le diagnostic n'a rien eu à dire. → B1L-0127 réactive les vues pour provoquer les fautes.

# Build B1L-0118 : chaque état de shader fait une seule fois

Date : 2026-11-08. Base : B1L-0117. Ensemble `b2l-metal-v70`, Mesa recompilé.

## Pourquoi

L'échantillonneur (0115) : le fil d'affichage de CoreAnimation ouvre et ferme sans cesse la base du cache disque de Mesa (`mesa_db_unlock` → `fclose` → `close_nocancel`, ≈ 6 000 échantillons noyau en 10 s). Les piles montrent `panfrost_create_shader_state` et `panfrost_create_compute_state` : chaque nouveau pipeline, et chaque pipeline de calcul dont la taille de groupe change (`compute_state_for` n'en gardait qu'un), refait son état Gallium (passes NIR, recherche dans le cache disque, compilation si absent) pour un shader déjà compilé.

## Changement (`apply.py`, patch B1L-0118, `air_metal.c`)

- Table des états de shader pour tout le processus. La clé est le SHA-1 du NIR sérialisé, avec le device, l'étage et, pour le calcul, la taille de groupe et la mémoire partagée.
- `make_stage_cso` et `compute_state_for` cherchent d'abord dans la table et y rangent ce qu'ils font.
- Un état de la table n'est jamais détruit (`delete_compute_state`, `rwa_render_destroy`). Leur nombre est borné par le nombre de shaders distincts.
- `B2L CSO: N shader states made, M reused` dans le journal (backboardd, aux puissances de deux).

## Installé

- **Vérifié :** `B2L CSO` dans `libhakiosair.dylib` (v70) ; patch appliqué (« shader states made once a shader »).
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `520154c36f48ead33478b99dfdddfe1d5c4a6550bdc65a5c3adf0608d529b9c7` (BOOT-OK). Pas redémarré (0119 suit).

## Résultat

(à tester, avec 0116, 0117 et 0119)

# Build B1L-0115 : ne copier que ce que le shader lit d'un tampon de constantes

Date : 2026-11-08. Base : B1L-0114 (qu'il contient : échantillonneur étendu). Ensemble `b2l-metal-v69`, Mesa recompilé.

## Pourquoi

L'échantillonneur de 0113 montre qu'au repos (10 images/s), le fil d'affichage de CoreAnimation passe 35 % de son temps dans un `memcpy` appelé par `panfrost_emit_const_buf`. Sur panfrost, chaque tampon de constantes Metal est un « user buffer » (`ubo_base` 1 : panfrost pousse des mots d'UBO depuis le CPU), et Mesa en copie **toute la taille liée à chaque dessin**. Quand le shader ne donne pas `air.buffer_size` (argument `constant T *`), cette taille était tout le reste du MTLBuffer après le décalage, alors que QuartzCore range ses constantes dans de gros tampons.

## Changement (`userspace/mesa/air-b2l/apply.py`, patch B1L-0115)

- `nir_air.h` : `struct air_resource` gagne `b2l_read_end`.
- `air_metal.c` :
  - `b2l_ubo_read_ends` : à la création d'un pipeline (dessin : les deux étages ; calcul : le noyau), parcourt les `load_ubo` du NIR. Pour chaque tampon de constantes, il calcule jusqu'où les lectures peuvent aller : décalage constant, ou borne de `nir_unsigned_upper_bound`. La borne est inconnue (rien ne change) pour un index non constant, un `get_ubo_size` ou une borne ≥ 1 Mo ;
  - à la liaison, la taille copiée est ramenée à cette borne ;
  - `B2L UBO:` toutes les 5 s (backboardd) : liaisons, Ko copiés, Ko évités, liaisons sans borne.

## Installé

- Premier essai : Mesa refusé à la compilation (fonctions utilisées avant leur définition, `ALIGN` déjà défini par le SDK) ; rien n'a été écrit. Corrigé (prototypes, arrondi à la main).
- **Vérifié :** `B2L UBO` dans `libhakiosair.dylib` (v69).
- **SYSTEM :** SYSTEM-OK en 55 s. **BOOT :** relecture `1e9e55eaea20d5fc6053b85634fc82a681a335839607bb06f91d4a4b358456f0` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0112/boot.img` (SYSTEM : ensemble v68).

## Résultat (journal `out/logs/0115-boot.txt`, photos du propriétaire)

- **Propriétaire : « extrêmement plus fluide ».** Aucun plantage, un seul démarrage de backboardd.
- **`B2L UBO`** : en utilisation, **2 à 3,6 Go évités toutes les 5 s**, 200 à 700 Mo encore copiés (environ la moitié des liaisons restent sans borne : `unbounded`).
- **Images/s** : 10 à 14 en utilisation (3 à 8 en 0113).
- Diagnostic de 0114 :
  - les fautes de page ne dominent plus. Le temps noyau du fil d'affichage est surtout dans des **appels système** (`ec 0x15`), en majorité **`close_nocancel` (n° 399, fd 9)** appelé par `fclose`. Cause : le cache disque de Mesa (`mesa_db_unlock`), interrogé à chaque création d'état de shader (`panfrost_create_shader_state`, `panfrost_create_compute_state`) → B1L-0118 ;
  - environ 2 000 échantillons noyau dans **`PE_parse_boot_argn_internal`** : `progress_role()` (b2l_diag.c) relit la ligne de démarrage à chaque appel système de SpringBoard et de backboardd → B1L-0117.
- Défauts visuels restants (photos) : fond bleu uni (la vue de B1L-0027), dock et fonds de dossiers noirs, tuiles du Centre de contrôle grises et opaques, icônes Horloge et Calendrier vides.

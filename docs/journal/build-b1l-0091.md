# Build B1L-0091 : `demote` géré par le compilateur Bifrost

Date : 2026-11-08. Base : B1L-0090.

## Changement (`apply.py`, B1L-0091)

**`bifrost_compile.c`, `bi_emit_intrinsic` :** `demote_if` est émis comme `terminate_if` (`bi_discard_b32`), `demote` comme `terminate`. Le `DISCARD` de Bifrost garde déjà les lanes auxiliaires en vie tant qu'une dérivée en a besoin (`bi_helper_invocations.c`), ce qui est le contrat de `demote`.

Mesa reste compilé avec ses assertions signalées (`B2L_MESA_ASSERTS=1`). Ensemble `b2l-metal-v47`, plug-in `B2LMetalv47`.

## Vérifié sur le Mac (`b2l-bicomp`)

- `A2Xghfc` passe maintenant `demote`. Sur les 41 modules récupérés, 22 compilent jusqu'au binaire. Les autres s'arrêtent sur des étapes que seule la pile Gallium du téléphone fait : `load_rt_conversion_pan`, `load_viewport_scale`, `load_base_instance`, `load_ssbo_address` (valeurs système).
- L'outil fait maintenant `nir_lower_system_values`, comme le state tracker du téléphone.

## Installé

- **Vérifié :** le patch B1L-0091 est présent 2 fois dans `bifrost_compile.c` de l'arbre compilé.
- **SYSTEM :** 3 passes réécrites et relues, en 66 s (SYSTEM-OK).
- **BOOT :** `out/b1l-metal-0091/boot.img`, relecture `7cfb08c56efc18c956609e1ff87dcda616b0ccaf5a12b6b0c918548fe8cf0aef` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0088/boot.img`.

## Résultat (boot 1, photos `b1l-0091-ecran1..6.jpg`)

- **L'interface est rendue par le GPU**, plus de repli sur le CPU : écran de verrouillage, accueil, Centre de contrôle (et ses sous-pages), lecteur.
- **Le flou fonctionne** : le fond du Centre de contrôle est flouté (il ne l'était pas en 0088).
- **Défauts :** fond d'écran bleu uni (ce n'est **pas** une couleur fausse : c'est la vue bleue posée par B1L-0027 à la place du fond d'écran, PosterBoard n'ayant pas de rendu) ; l'heure de l'écran de verrouillage est cyan sur un rectangle plus foncé ; Liquid Glass gris opaque (tuiles grises, sans transparence), mais **la réfraction du Liquid Glass fonctionne** quand on tire l'écran de verrouillage vers le bas (photo 6 : le bord courbé déforme les icônes App Store et Books dessous) ; le gris viendrait donc de la couche de matière des tuiles (leur copie de l'arrière-plan), pas du shader de réfraction ; icône Horloge et Calendrier transparentes ; Centre de contrôle lent à apparaître.

## Journal (`out/logs/0091-boot1.txt`)

- 0 assertion de Mesa, 0 plantage, **1 seul démarrage de backboardd** ; 212 pipelines compilés ; 16 images envoyées à l'écran.
- **112 jobs arrêtés par le Mali sur une instruction invalide** (`status 0x51`, sans faute MMU) : les dessins concernés manquent à l'image. Probable cause du Liquid Glass gris et des icônes transparentes. Le pipeline fautif n'est pas identifiable dans le journal (le dernier pipeline compilé n'est pas forcément celui qui tourne).
- **30 jobs arrêtés par une faute MMU** (`status 0x8`), dont 14 **juste après la fin d'un tampon de 64 Kio déjà libéré** : un tampon rendu avant que le GPU ait fini de s'en servir.
- **10 attentes de plus de 2 s** (« THE GPU DID NOT FINISH ») : lenteur du Centre de contrôle.
- 6 surfaces refusées comme drawable : format 30 (`RG8Unorm`).
- Sur le Mac, les 22 shaders récupérés qui compilent jusqu'au bout ne montrent aucune instruction invalide au désassembleur : le fautif est parmi ceux qui ne sont pas récupérés, ou vient de la partie Gallium (blend shader, etc.).

## « caught burning CPU » (relu le 2026-11-08 à la demande du propriétaire)

- Aucun processus de numéro ≥ 3000. Le « 3000 et quelque » vu est sans doute le **fil 3571 d'`imagent`**.
- 0091 : `backboardd[62]` fil 495, **89 % sur ~101 s, dont 82,6 s en système (noyau) et 7,5 s en utilisateur** ; `imagent[71]` 61 % ; `Spotlight[1477]` 97 % ; `spindump[1634]` 54 %.
- Même profil en 0088 (backboardd 90 %, 82,9 s système), et déjà en 0084–0086. Absent quand le rendu est sur le CPU (0090).
- Lecture : backboardd ne brûle pas son temps dans le pilote Metal (utilisateur), mais **dans le noyau**. L'attente des jobs du Mali dort par tranches (`wait_job`, `lck_mtx_sleep_deadline`), donc le suspect principal est la **copie CPU de chaque image vers l'écran** (`B2LFramebuffer`, 1440×3040×4 = 17,5 Mo par image), le futur chantier « GPU → écran direct ». À mesurer.

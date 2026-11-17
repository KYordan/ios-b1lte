# Build B1L-0103 : lignes de 64 octets aussi pour SpringBoard

Date : 2026-11-08. Base : B1L-0102 (qu'il contient). Noyau seulement.

## Changement (`B2LSurface.cpp`)

L'arrondi des lignes à 64 octets (B1L-0099) s'applique aussi aux surfaces à un plan de **SpringBoard** : c'est lui qui crée la surface 168×124 `LA08` (352 octets par ligne) que lit `compute_average_luma` dans backboardd. Elle passe à 384 octets par ligne, que le Mali accepte.

Ensemble `b2l-metal-v59`.

## Installé

- **Vérifié :** le noyau contient `B2L PERF CALLS` (chronométrage de 0102).
- **SYSTEM :** SYSTEM-OK en 85 s. **BOOT :** relecture `884c1e42c20edeb3af03f3b868510eee21bb46262f390ee60c3c49511f8cc778` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0101/boot.img`.

## Résultat (journal `out/logs/0103-boot.txt`)

- Propriétaire : **pas de réelle amélioration**.
- **backboardd n'est plus signalé « caught burning CPU »** (il l'était à chaque boot GPU depuis 0084) : les `LDAPR` de Mesa étaient la cause de son temps noyau.
- Écran : **2 à 7 images par seconde** ; GPU occupé 2 à 7 % ; aucun processus saturé. C'est un goulot d'attente, pas de calcul.
- `B2L PERF CALLS` (≈ 170 s) : `kHakiosMaliWait` (0x7d) 273 appels, ≈ 19 ms chacun ; `SwapEnd` (fb 5) ≈ 8,6 ms (attente du panneau, en boucle active `IODelay`) ; `CREATE_BO` 7 220 × 0,5 ms ; `GEM_CLOSE` 6 003 ; `MADVISE` 208 000 (bon marché). Total ≈ 18 s : peu.
- **Le noyau échoue à allouer les gros tampons GPU** : `HakiosMali: no memory for a buffer` ×15 (9 à 36 Mo), donc le pilote refuse des textures (« the GPU driver would not allocate a 1088x2304x1 … RGBA16Float »). Ce sont les textures de fond du Liquid Glass du Centre de contrôle, et sans doute les éléments qui disparaissent puis reviennent.
- **Les vues de texture renvoient `nil`** (« a texture view is not implemented »). C'est la vraie raison de la texture 0 vide de `compute_average_luma` (158 échecs), pas le format.

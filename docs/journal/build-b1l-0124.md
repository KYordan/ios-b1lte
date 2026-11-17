# Build B1L-0124 : constantes bornées seulement quand tous les décalages sont constants

Date : 2026-11-08. Base : B1L-0123 (vues partielles gardées). Ensemble `b2l-metal-v74`, Mesa recompilé.

## Pourquoi

0123 : 11 fautes GPU, toutes des lectures juste après la fin d'un tampon de 64 Ko ou 16 Ko (pools de panfrost), dans les passes de flou et de verre que les vues de niveau laissent enfin tourner ; clignotements de l'interface. La borne de B1L-0115 sur la copie des constantes vient de `nir_unsigned_upper_bound` quand le décalage est calculé ; pour ces shaders, elle est trop courte.

## Changement (`apply.py`, `b2l_ubo_read_ends`)

- Un tampon de constantes n'est borné que si **toutes** ses lectures ont un décalage constant ; un seul décalage calculé, et il est copié en entier, comme avant 0115.
- La borne est arrondie à 64 octets, plus une marge de 64 octets.

## Installé

- Patch appliqué (journal Mesa) ; **BOOT :** relecture `5f65648e8482f4356e3a840cb5f8561da743abecbebdf41c7132e15f78814155` (BOOT-OK). Redémarré vers le système.
- **Retour en arrière :** `out/b1l-metal-0122/boot.img` avec SYSTEM v72 (avant les vues partielles).

## À surveiller

- Fautes GPU (`did not finish`) : doivent revenir à 0.
- `B2L UBO` : moins de Ko évités qu'en 0115 ; la fluidité doit rester proche.

## Résultat (journal `out/logs/0124-boot.txt`)

- **Propriétaire : toute l'interface clignote** (icônes, heure, transitions) ; Centre de contrôle très cassé, en partie transparent et en partie gris ; dock et tiroir d'apps toujours noirs.
- **46 fautes GPU** (11 en 0123), dont 20 sur des tampons **déjà libérés** ; 38 refus de tampons GPU revenus. La borne des constantes n'était donc pas en cause : les fautes viennent du rendu dans les niveaux de mipmap (0123). → B1L-0125 revient au comportement de 0122.

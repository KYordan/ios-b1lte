# Build B1L-0114 : échantillonneur étendu (pourquoi la copie fait des fautes)

Date : 2026-11-08. Base : B1L-0113. Noyau seulement, ensemble `b2l-metal-v68`. Flashé mais pas testé seul : B1L-0115 (Mesa) a été flashé juste après et le contient.

## Changement (`osfmk/arm64/b2l_prof.c`)

- Échantillons pris dans le noyau : pc et lr **du noyau** (`B2L PROF KPC`), et l'exception par laquelle le fil est entré (`far`, `esr` de son état utilisateur). Histogramme par classe d'exception et code de faute (`B2L PROF EXC: ec … iss-low …`) : faute de traduction, d'« access flag » ou de permission.
- Pour chaque adresse chaude : `x0`/`x1` (destination et source du `memcpy`), `far`, `esr`.
- Nombre de fautes de page de backboardd par fenêtre de 10 s (`B2L PROF FAULTS`).
- Pile (8 retours) des fils **en cours d'exécution** aussi (`B2L PROF RUN`), exacte quand le fil est dans le noyau : elle dira qui appelle `-replaceRegion:`.

## Installé

- **Vérifié :** messages `B2L PROF KPC`, `B2L PROF FAULTS` dans le noyau.
- **BOOT :** relecture `43f5c0b05d81d9dab8a8c1331097274e989ad079d81c418433e35c26e2b2e626` (BOOT-OK). Pas redémarré (0115 suit).

# B1L-0193 — Wi-Fi : étapes chronométrées pour situer le gel sans texte

Base : `b2l-metal-v95`, `--no-libs`. Seul `B2LWifi.cpp` change par rapport à 0192.

Le gel du 0192 n'affiche aucun texte de panic : c'est un blocage (verrou ou bus), pas un panic.
Après la première annonce du lien à iOS :
- 0–30 s : étape 0, lien annoncé, aucune trame dans un sens ni dans l'autre ;
- 30–75 s : étape 1, les trames reçues montent à iOS ;
- au-delà : étape 2, iOS émet aussi.
Chaque passage écrit `B2L WIFI STAGE n` (au premier usage de l'étape), donc la dernière étape
écrite plus de 5 s avant le gel la situe.

## Résultat
Gel dès la connexion : aucune ligne `ASSOCIATE`/`JOIN`/`STAGE` écrite, le journal s'arrête sur un appui (son relâchement jamais écrit). Le gel vient dans les ≤5 s après l'appui, donc à l'étape 0, sans trame : l'annonce du lien suffit.

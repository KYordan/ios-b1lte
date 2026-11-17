# Build B1L-0112 : le pilote garde en vie les ressources qu'il a encodées

Date : 2026-11-08. Base : B1L-0111. Pilote Metal seulement (le noyau n'est recompilé que pour le nom de l'ensemble).

## Diagnostic (journal `out/logs/0111-boot.txt` et ceux de 0104 à 0109)

- **Le receveur fautif n'est pas un command buffer.** Tous les command buffers du journal sont rangés sur une grille fixe de l'allocateur : des multiples de 0x300 dans des blocs de 0x4000 (`0x4417b4300`, `0x4419fc600`…). Dans les 8 plantages de 0104 à 0111, `x0` tombe hors de cette grille : `0x441bdf410`, `0x447e00310`, `0x43b70b4f0`, `0x146517d40`…
- **Le plantage est dans `-[HakiosMetalCommandBuffer commit]`, pas dans `-commitAndWaitUntilSubmitted`.** La pile du noyau (`backtrace_user`, `bsd/kern/kern_exit.c`) suit la chaîne des `fp` et non LR. Comme `objc_msgSend` n'a pas de cadre, le « frame 1 = +0xa16c » est l'adresse de retour enregistrée par `-commit`. Vérification : `fp − sp = 0x8e0`, ce qui correspond exactement au cadre de 0x8f0 octets de `-commit`. L'appel fautif part donc de la boucle compute de `-commit`, la seule partie intégrée à la fonction (`runDraw`/`runClear` ont leur propre cadre) : `-mesaBuffer`, `-mesaTexture`, `-mesaSampler` ou `-compute` envoyé à un objet libéré.
- **Cause.** `HakiosMetalWork` et les encodeurs gardent de simples pointeurs vers les tampons, textures, samplers et pipelines, sans les retenir. Le pilote annonce pourtant `retainedReferences:YES`, donc QuartzCore relâche ses objets juste après l'encodage et compte sur Metal pour les garder en vie. Ça touche les vues de texture, les tampons de `-setBytes:` (marqués `autorelease`) et les samplers d'un seul passage. Le dispatch compute de `compute_average_luma` et les vues de texture tournent depuis 0104 : c'est la date où le plantage est apparu. À l'ouverture d'une app, l'envoi arrive plus d'une seconde après l'encodage (compilation des shaders), et l'objet est libéré entre-temps.
- Les essais 0106 à 0111 retenaient le command buffer, qui n'était pas l'objet libéré.
- `FAULTREG x19` vaut toujours 0, dans tous les plantages : valeur non fiable.

## Changement (`hakiosmetal_air.m`)

- `HakiosMetalWork` retient tout ce qu'il nomme : pipeline de calcul ou de dessin, cibles de couleur, profondeur, état profondeur/stencil, index buffer, vertex buffers, tampons, textures, samplers (`-hakiosRetainReferences`). Il les relâche dans son nouveau `-dealloc`.
- La retenue est faite à un seul endroit, `-[HakiosMetalCommandBuffer recordWork:]`, par lequel passent tous les dessins, effacements et dispatchs. Les ressources sont relâchées quand le command buffer vide son travail (après ses rappels, depuis 0111) ou à sa destruction.
- Les contournements de 0107 à 0111 (retenues de 30 s et de 2 s) restent en place pour ce build, afin de ne changer qu'une chose à la fois.

Ensemble `b2l-metal-v68`.

## Installé

- **Vérifié :** `hakiosRetainReferences` présent dans `libhakiosmetal.dylib` (v68) ; aucun nouvel avertissement de compilation.
- **SYSTEM :** SYSTEM-OK en 54 s (6 blocs sur 1750). **BOOT :** relecture `f91107f037905a757732f84f96e80a2bcf2caf95309120bf9ac93985ff3fc8ac` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `out/b1l-metal-0111/boot.img` (ou `out/b1l-metal-0104/boot.img`).

## Résultat (journal `out/logs/0112-boot.txt`)

- **Propriétaire : les apps se lancent, plus de plantage ; un peu plus rapide, mais encore très lent.**
- Journal : **un seul démarrage de backboardd, aucun `B2L CRASH`**. Le plantage de 0104 à 0111 est réglé.
- Pour la vitesse :
  - GPU occupé 4 à 19 % (souvent 8 %) : il attend, ce n'est pas lui le goulot ;
  - 77 tampons GPU refusés (`no memory`, 5 à 36 Mo) et des textures RGBA16Float plein écran refusées ;
  - `compute_average_luma` échoue 63 fois (`reads texture 0 and 1 texture was bound`) ;
  - gros cœurs bridés à 1 248–1 378 MHz dès 60–67 °C.
- Les mesures `B2L PERF` étant coupées, on ne sait pas où backboardd passe son temps. → échantillonneur dans B1L-0113.

# Build B1L-0015 : variables CoreAnimation injectées par le kernel (+ contenu de B1L-0014)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `302713cf…` identique ; pas encore testé.** BOOT seulement.

Principe (choix du propriétaire) : corriger par le kernel plutôt que par la racine.

- Plantage du clavier / de l'interface (B1L-0013) : backboardd s'arrête sur `ogl-context.cpp:1186`
  `found != slist->end ()` dans `CA::OGL::Context::push_surface` ← `AsynchronousNode::retain_surface` (rendu logiciel,
  nœuds asynchrones). QuartzCore lit `CA_DISABLE_ASYNCHRONOUS` / `CA_DISABLE_ASYNC`.
- `xnu-overlay/bsd/kern/b2l_exec_graphics_diag.h` : `b2l_exec_render_env()` ajoute ces deux variables à l'environnement
  de `/usr/libexec/backboardd` au moment de l'`exec` (chaînes déjà copiées, valeur de l'appelant conservée, limites
  `ip_argspace`/`ip_strspace` respectées). Coupe-circuit : boot-arg `b2l_ca_env=0`. Journal : `B2L CA ENV:`.
  Non vérifié : que QuartzCore respecte ces variables sur une version non interne.
- Contenu B1L-0014 conservé : essai DVFS « buffer » et sonde GPU G1 (lecture seule).
- Métadonnées d'affichage : déjà fournies par `XnuLoader/DeviceTree.c` (/product : idiome, sous-type 2868, gamut sRGB,
  `graphics-featureset-class GLES2,0`).
- Apps / APFS : aucune erreur `clonefileat`/`fclonefileat` dans le journal ; pas d'émulation APFS sans preuve.

Le patch racine équivalent (`set-launchd-env.sh`) a été appliqué à une copie puis **non flashé** (flash arrêté pendant le
calcul des empreintes, aucun bloc écrit) ; copie : `out/live/ios26-userdata-live-envpatch-unflashed.img`.
L'image de travail est revenue à l'état flashé (clone 0013).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-caenv-0015/boot.img` (25 910 056 octets) | `302713cf4735481525254dffff03ab2748663b1cea7031d822d7c9fd485a0660` |
| Kernel | `61722fd5ac8ce774e184e4226581ed38425a3cc4e9b0d3039b3d0613553b96bb` |

## Résultat observé (B1L-0015)

- **Clavier : fonctionne** (lent). `B2L CA ENV` présent : QuartzCore respecte bien `CA_DISABLE_ASYNCHRONOUS`/`CA_DISABLE_ASYNC`.
- **DVFS : fonctionne.** Écriture directe dans la case du canal 5 + `INTGR0` ; l'APM répond dans `rx_base` avec le numéro de
  séquence et la fréquence (`00290000 001c2af0`, `002a0000 0021b8d0`, `002b0000 001fbd00`). Mesure PMU après envoi :
  **A55 1846, A75 2210, M4 2080 MHz** (avant : 1053 / 1222 / 1378).
- **GPU G1 : validé.** `pd-embedded_g3d conf=1 status=1` (déjà allumé au démarrage). `GPU_ID=0x72110000` (Mali-G76),
  `L2_FEATURES=0x07120206`, `SHADER_PRESENT=0x00770077` (12 cœurs), `TILER=1`, `L2=1`, `AS_PRESENT=0xff` (8), `JS_PRESENT=7` (3).
- Apps : toujours beaucoup d'apps absentes.

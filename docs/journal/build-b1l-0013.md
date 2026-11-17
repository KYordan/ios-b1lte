# Build B1L-0013 : fréquence CPU (envoi ACPM corrigé) + 50 apps système

Date : 2026-11-04. Statut : **construit, pas installé.** BOOT + USERDATA.

## 1. DVFS (BOOT)

- `B2LACPM.h` : lit `type` et `ap_poll` ; accepte `q_len=1` pour TYPE_BUFFER (test hôte mis à jour, OK).
- `B2LCPU.cpp` : envoi identique à `__acpm_ipc_send_data()` (contrôle de file pleine, copie au front, publication, sonnette) ;
  réponse attendue seulement sur les canaux `ap_poll`. Le canal DVFS ne répond pas : le résultat est vérifié par la
  sonde PMU (`B2L CPUFREQ after-dvfs`).
- Cibles (fvmap pas encore localisée ; table des plugins vidée pour la suite) : valeurs listées par le DTB du S10.
  A55 1 846 000 kHz (un cran sous 1 950 000), A75 2 210 000 (sous 2 400 000 / 2 314 000), **M4 2 080 000 = `max-freq`
  = `pm_qos-booting`**, la valeur que le noyau Samsung applique à chaque démarrage. Coupe-circuit `b2l_dvfs=0`.

## 2. Apps système (USERDATA, `install-b1l-0013-apps.sh`)

Constat (`mobile_installation.log.0`, `/private/var/log/b2l-system-app-bootstrap-v2.log`) : au premier démarrage,
`SystemAppMigrator` a été lancé par `b2l-system-app-bootstrap-v2` (staged=50), a mis les 50 apps à l'état 6, puis
`nativeResult=0 installedAfter=0` ; `/private/var/staged_system_apps` est vide depuis. Les bundles n'existent plus dans la racine.

Correctif (renommages seulement, rien n'est effacé) :
1. copie des 50 apps de `OS.dmg:/private/var/staged_system_apps` (680 Mo) dans `/Applications`, propriétaire de `Preferences.app` ;
2. placeholders correspondants déplacés dans `/System/Library/AppPlaceholders.b2l-before-0013/` ;
3. `/private/var/installd/Library/MobileInstallation` → `.b2l-before-0013` (état installd vierge) ;
4. bases LaunchServices `com.apple.LaunchServices*.csstore` renommées (reconstruites par lsd) ;
5. `nc-static macho-apply` sur `/Applications`, contre-vérification, `fsck_hfs`.

Attendu : Appareil photo, Safari, Messages, Photos, Horloge, Météo… et les apps de fonds d'écran
(EmojiPoster, GradientPoster, KaleidoscopePosterApp, PridePosterApp, UnityPosterApp, WeatherPosterApp) — possible
retour du fond d'écran (PosterBoard). Non vérifié.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-dvfs-apps-0013/boot.img` (25 910 056 octets) | `c99b3477028bac914e8a8aa79ae16c936251eb3cc36db8a93ca5342d33716b51` |
| Kernel | `ee328edb9f813e2e7aa5c2712f52a0e682a732f51f049721f69aedbe2a735d60` |

Retour arrière : racine `out/live/ios26-userdata-live-0012.img`, BOOT `out/b1l-ent-0012/boot.img`.
Écritures matérielles : SRAM APM (file du canal 5) + `INTGR0`, comme le pilote Samsung ; aucune écriture de stockage côté kernel.

## Installation (2026-11-04)

- `install-b1l-0013-apps.sh` : 50 apps copiées (propriétaire 0:80 de Preferences.app), 50 placeholders mis de côté,
  `MobileInstallation` renommé, 1 base LaunchServices renommée ; 234 binaires traduits (3 939 675 mots), 0 restant ; `fsck_hfs` OK.
- BOOT (sda14) : relecture `c99b3477…` identique.
- USERDATA (sda34) : 64/64 morceaux vérifiés (11 écrits). Pas encore testé.

## Résultat observé (B1L-0013)

- 1er démarrage : retour au logo Apple pendant la migration (`datamigrator`, `lsd` à 100 %) ; aucun plantage nouveau.
- 2e démarrage : écran verrouillé atteint. Apps visibles : Santé, Réglages, Raccourcis, Messages, Notes, Téléphone (6).
  Toujours pas de fond d'écran. Le clavier fait planter l'interface (il posait déjà problème avant, selon le propriétaire).
- DVFS : `parse=0 type=2 poll=0 len=1 front=0 rear=0 fvmap=29000` puis `tx full` sur les trois clusters : **rien envoyé**,
  fréquences inchangées (1053/1222/1378 MHz). Plugin DVFS : n° 4, base 0x1ec00, `fvmap_offset` 0xa400 → fvmap 0x29000.

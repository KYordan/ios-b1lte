# Documentation du projet — Port iOS 26 sur Galaxy S10 (beyond1lte)

## Partie 1 — Présentation, état actuel et arborescence

---

### 1. Qu'est-ce que ce projet ?

Ce projet porte **iOS 26** (build `23G90`, produit `iPhone17,2`, iPhone 16 Pro Max) sur un **Samsung Galaxy S10 (SM-G973F, codename beyond1lte, SoC Exynos 9820)**.

Il adapte le port existant `beyond2lte-ios` (fait pour le Galaxy S10+, SM-G975F, même SoC Exynos 9820) au modèle plus petit S10 (beyond1lte). C'est une **démonstration de recherche**, pas un téléphone quotidien : pas d'appels, pas de 4G, pas de caméra, pas d'audio, pas de veille. Aucune donnée personnelle ne doit être placée sur cette installation (pas de chiffrement, pas de SEP, pas de signature réelle).

---

### 2. Chaîne de boot (du bouton power à iOS)

```
S-Boot (Samsung) → BootShim → Mu UEFI (beyond1ltePkg) → XnuLoader (EFI) → XNU (kernel Apple) → HFS root dans USERDATA → launchd → SpringBoard
```

| Maillon | Rôle |
|---------|------|
| **S-Boot** | Chargeur d'origine Samsung. Charge la partition `BOOT` (`sda14`). Pas de fastboot. |
| **BootShim** | Petite couche de transition entre S-Boot et l'UEFI. |
| **Mu UEFI** | Firmware UEFI basé sur Mu-Silicium (EDK II), adapté au S10 par un *overlay* (`beyond1ltePkg`). Gère l'écran, l'UFS, l'ACPI, le tactile. |
| **XnuLoader** | Application EFI embarquée qui lit XNU depuis la partition BOOT, construit les `boot_args` et l'arbre de périphériques Apple, puis passe la main à XNU. |
| **XNU** | Noyau Apple open-source (version `rel/xnu-12377`), patché pour le board B2L (Exynos 9820, arm64e sans PAC hardware). |
| **HFS root** | Système de fichiers iOS (HFS+ journalisé, pas APFS natif) installé dans `USERDATA` (`sda34`). |
| **launchd / SpringBoard** | Userspace iOS : lance les services et l'interface graphique. |

---

### 3. État actuel au 2026-11-10 (build 0139)

D'après `engineer.md`, `metal-progress.md` et les journaux 0136-0139 :

**Ce qui fonctionne :**
- Boot vérifié sur le S10 : texte XNU visible, 8/8 CPU initialisés (4×A55, 2×A75, 2×M4), timers et interruptions OK.
- UFS natif : lecture/écriture directe depuis XNU.
- Racine HFS : montée en lecture/écriture, journal replay OK.
- Userspace iOS 26 : lance, SpringBoard apparaît, écran de verrouillage et tactile fonctionnent.
- Réglages : s'ouvrent (depuis build 0099 sur S10+, en stabilisation sur S10).
- Écran d'accueil, centre de contrôle, contacts, clavier s'affichent (rendu GPU Metal).
- Liquid Glass (flous, verre) rendu sur le bouton Focus du centre de contrôle (build 0127-0128).
- Rendu GPU Metal actif : backboardd dessine sur le Mali-G76 via le pont Metal → libhakiosmetal → Mesa panfrost → Bifrost → Mali.
- Throttle thermique CPU : la puce plafonne à ~62-66 °C au lieu de 76 °C (build 0030).
- GPU : DVFS à 676 MHz par ACPM (build 0030), premier job GPU exécuté (build 0017), service HakiosMali démarré (build 0029).
- Fréquences CPU relevées après DVFS : A55 1846, A75 2210, M4 2080 MHz (build 0015).

**Ce qui ne fonctionne pas encore :**
- Glyphes des boutons ronds du centre de contrôle (lampe, minuteur, calculatrice, appareil photo, enregistrement), lune de Concentration, glyphe du curseur de volume.
- Icône Horloge : aiguilles présentes, chiffres du cadran absents.
- Icône Calendrier : entièrement vide.
- Widgets de l'écran d'accueil : rectangles blancs sans contenu.
- Dock qui clignote parfois.
- Un glyphe en haut à gauche s'affiche en hachures (mauvaise largeur de ligne).
- apps non installées (blocage HFS/APFS, erreur 166).
- Pas de Wi-Fi, Bluetooth, audio, cellulaire, caméra, veille.
- Le chemin Metal ne consomme pas les surfaces `240x240 LA08` des instantanés d'icône vivant (Calendar, Timer) — diagnostic en cours (builds 0136-0139).

**Build en cours :** B1L-0139, ensemble `b2l-metal-v88` (instrumentation userspace pour tracer la consommation des textures 240×240 par le chemin Metal). Résultat en attente.

---

### 4. Arborescence des dossiers

#### Dossier de travail principal : `~/Developer/s10-port/`

| Dossier | Rôle |
|---------|------|
| `beyond2lte-ios/` | Cœur du projet : patches XNU, loader EFI, scripts de build/packaging, tooling iOS. Branche locale `b1l` (adaptation S10). |
| `beyond2lte-uefi/` | Overlay UEFI pour le S10+ (référence). Contient `beyond2ltePkg`, `yap.sh`. |
| `uefi-touch-42412d0/` | Autre copie de l'overlay UEFI (branche touch, commit 42412d0), protocoles tactile et diagnostic attendus par le loader. |
| `Mu-Silicium/` | Base firmware EDK II / Mu (commit `a8ca6f0d`, Mu Basecore `762a65d`). |
| `mu-work-b1l-touch/` | Arbre de travail Mu jetable pour construire le firmware S10 (beyond1lte). |
| `mu-work-b1l/` | Arbre de travail Mu précédent. |
| `device-s10/` | Données du téléphone réel : DTB (`base.dtb`, `merged.dtb`, `rev26.dtbo`), dumps TWRP, sauvegardes de partitions (efs, boot, vbmeta, etc.). |
| `b1l-tools/` | Scripts d'installation, flash, construction de la racine iOS, cache natif, traduction arm64e. |
| `build-gl/` | Pont OpenGL ES / OSMesa (référence S7, plusieurs versions). |
| `build-mesa/` | Builds Mesa (Panfrost pour Mali). |
| `build-metal/` | Builds Metal → Vulkan (dada / scion). |
| `deps/` / `deps-src/` | Dépendances de build (KDK, SDK, bibliothèques). |
| `progress/` | Fichiers de statut de build (un `.status` par build NN, un `current` pointant sur le dernier). |
| `saved-builds/` | Builds conservés (ex: `0128/`). |
| `static-work/` | Travail de traduction arm64e statique (fichiers `orig/`, `out/`, comparaisons). |
| `tools-src/` | Outils supplémentaires (macOS UnifiedLogs). |
| `*.log` / `run-*.sh` / `flash-*.log` | Journaux de builds et flashs passés, scripts d'exécution historiques. |

#### Dossier de documentation et planification : `~/Port iOS 26 to S10/`

| Fichier / dossier | Rôle |
|-------------------|------|
| `PLAN.md` | Plan complet du port : stratégie, phases 0-7, récupération de l'iOS 26, matériel, risques, vérification. |
| `engineer.md` | Brief d'ingénieur : cible, état actuel, politique de stockage, environnement de build, workflow, priorités, diagnostics. |
| `gpu.md` | Plan d'accélération GPU Mali-G76 (étapes G0-G7). |
| `docs/` | Journaux de build détaillés (`build-b1l-XXXX.md`), captures d'écran, plans GPU, plan de phase 4, faits mesurés du S10 (`docs/device-facts.md`), référence GPU S7 (`docs/gpu/herolte-reference.md`). |
| `DOCUMENTATION_PROJET_PART{1,2,3}.md` | Ce document (présentation, build, sécurité, journaux, GPU, référence). |

---

### 5. Rôle des différents scripts

#### Dans `beyond2lte-ios/scripts/` (build et tooling)

| Script | Rôle |
|--------|------|
| `build-kernel.sh` | Compile le noyau XNU (target `DEVELOPMENT ARM64 B2L`, SDK iPhoneOS 26.5). Vérifie la révision XNU (`ac9718fb…`), intègre libpthread/BearSSL/HFS, lance `make`. Nécessite `KDKROOT` et `B2L_DEPSROOT`. |
| `build-boot-firmware.sh` | Construit le firmware UEFI (Mu-Silicium) pour un board donné (`B2L_BOARD`). |
| `build-efi.sh` | Construit les applications EFI standalone. |
| `build-mesa.sh` | Cross-compile Mesa (Panfrost) pour iOS arm64. |
| `build-metal.sh` / `build-metal-scion.sh` | Construit les bibliothèques Metal et le pont Air (scion). |
| `build-userspace-bootstrap.py` | Génère le payload userspace à partir de la racine iOS. |
| `prepare-xnu.sh` | Prépare les sources XNU (applique l'overlay B2L). |
| `fetch-xnu.sh` | Récupère les sources XNU épinglées. |
| `package-boot.py` | Empaquette le `boot.img` Android v1 (firmware UEFI + noyau XNU + loader + trailer). Vérifie l'en-tête, le trailer `B2LXNU02`, la graine 256 octets. |
| `make-board-platform.py` | Dérive le platform package `beyond1ltePkg` de l'overlay S10+. |
| `prepare-boot-firmware.py` | Prépare le firmware de boot. |
| `b2l_board.py` | Table des cartes, identité par `ro.boot.em.model`. |
| `test-loader.sh` | Teste le loader EFI sur l'hôte. |
| `test-macho.sh` | Teste les fichiers Mach-O. |
| `test-arm64e.sh` | Teste l'émulation arm64e. |
| `test-native-cache.sh` | Teste le cache de code natif. |
| `test-ufs-transport.sh` | Teste le transport UFS. |
| `pull-b2l-log.py` | Récupère le journal noyau depuis TWRP. |
| `pull-b2l-unified-logs.py` | Récupère les journaux unifiés iOS (tracev3) et rapports de crash `.ips`. |
| `inspect-kernel.sh` | Inspecte un noyau XNU (symboles, sections). |
| `apply-board-patches.py` | Applique les patches spécifiques au board. |
| `decode-b2l-diag.py` | Décode les diagnostics B2L. |
| `stage-libpthread.py` / `stage-bearssl.py` / `stage-hfs.py` | Intègre libpthread, BearSSL et le pilote HFS dans XNU. |
| `extract-b2lmod.py` | Extrait les modules B2L. |
| `grow-native-cache.py` / `init-native-cache.py` | Gère le cache de code natif. |
| `check-native-cache-codegen.py` | Vérifie la génération du cache natif. |
| `audit-kernel-rcpc.py` | Audite le noyau. |
| `build-air-host.sh` | Construit l'hôte Air. |
| `build-firehose.sh` | Construit le protocole Firehose (non utilisé ici). |
| `build-springboard-dylib.py` | Construit une dylib SpringBoard. |
| `stage-hfs.py` | Stage le pilote HFS. |
| `unsign-branches.py` | Dés-signe les branches. |

#### Dans `b1l-tools/` (installation, flash, racine iOS)

| Script | Rôle |
|--------|------|
| `b1l-build.sh NNNN SET [--mesa] [--no-libs] [--no-kernel]` | Chaîne complète d'un build Metal : mesa → libs → kernel → package → system → boot. Écrit un fichier de statut (`progress/NNNN.status`) lisible par `watch-build.sh`. |
| `flash-boot.sh IMAGE SHA256` | Écriture gardée de la partition BOOT (`sda14`) avec readback et vérification SHA-256. Vérifie TWRP, root, modèle SM-G973F, bloc et non-montage. |
| `flash-chunked.sh IMAGE NAME BLOCK BYTES` | Écrit une grande image (USERDATA, SYSTEM) en chunks de 512 Mio vérifiés, résumable (skip si déjà écrit). |
| `flash-delta.sh` / `flash-saved.sh` | Variantes de flash delta / sauvegardes. |
| `hash-device.sh` | Calcule/vérifie les hash des partitions du téléphone. |
| `build-ios-root.sh` | Construit l'image racine iOS (HFS+) à partir de l'IPSW. Nécessite sudo. |
| `build-system-image.sh` | Construit l'image SYSTEM (contient le cache natif + journal). |
| `set-root-prefs.sh <profile>` | Définit les préférences de racine (ex: `BKNoWatchdogs`). |
| `install-translated-cache.sh` | Installe le cache de code traduit. |
| `set-launchd-env.sh` | Configure l'environnement launchd. |
| `system-add-fast.sh` / `system-add-gl.sh` | Ajoute des fichiers à l'image SYSTEM (chunks modifiés uniquement). |
| `pull-chunked.sh` | Récupère une image depuis le téléphone par chunks (pour sauvegarde / rollback). |
| `system-pull-changed.sh` | Récupère uniquement les chunks modifiés. |
| `copy-tree.py` / `dt-table.py` / `dts-nodes.py` / `fdt2json.py` | Outils de manipulation d'arbres de fichiers et de DTB. |
| `aea-key.py` | Gestion des clés AEA (images iOS chiffrées). |
| `install-availability.sh` | Installation des frameworks de disponibilité. |
| `install-b1l-0011-root.sh` … `install-b1l-0018-native-apps.sh` | Scripts d'installation historiques par build. |
| `watch-build.sh` / `watch-flash.sh` / `watch-progress.sh` / `watch-system-flash.sh` | Affichage en direct de l'avancement des builds/flashs. |

#### Scripts d'exécution historiques (`run-*.sh` à la racine de `s10-port/`)

Ce sont des scripts d'exécution passés (builds 0069 à 0081 et au-delà), chacun exécutant une séquence : build noyau → package → flash SYSTEM → flash BOOT. Exemple `run-0081.sh` :
1. Copie les headers graphiques dans XNU
2. Construit le noyau avec `build-kernel.sh`
3. Empaquette `boot.img` avec `package-boot.py`
4. Ajoute le système avec `system-add-fast.sh`
5. Flashe BOOT avec `flash-boot.sh`

---

### 6. Règle d'or du projet

> **Une seule modification par build de test. Chaque build a son journal `docs/journal/build-b1l-XXXX.md`. « Verified » signifie observé sur le S10.** Ne jamais revendiquer un résultat qui n'a pas été observé sur l'appareil.

La partie 2 couvre l'environnement de build, le processus de build pas à pas, et la politique de sécurité/stockage.
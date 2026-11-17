# Documentation du projet — Port iOS 26 sur Galaxy S10 (beyond1lte)

## Partie 2 — Environnement de build, processus de build et sécurité

---

### 7. Matériel et environnement de build

Tout le travail se fait sur un **ThinkPad T470s (Intel i7-7500U, hackintosh, macOS 26 Tahoe, 12 Go RAM)**.

| Composant | Détail |
|-----------|--------|
| Ordinateur | ThinkPad T470s, hackintosh macOS 26 |
| RAM | 12 Go (16 Go conseillé) |
| CPU | 2 cœurs / 4 threads |
| Espace disque | ~150 Go libres pour l'image iOS, SSD externe USB-C possible |
| Kernel / iOS | Xcode 26.5 + KDK 26.5 (`25F71`), SDK iPhoneOS 26.5 |
| UEFI / outils | micromamba env `~/Developer/tools/mamba/envs/s10` : clang 23, lld, llvm-tools, cmake, ninja, meson, dtc, bash 5, Python 3.12, cryptography |
| adb | `~/Developer/android/platform-tools/adb` |
| Téléphone | Galaxy S10 SM-G973F, TWRP 3.7.1, root ADB, série `<SERIAL>` |

**Contraintes :**
- Homebrew n'est **pas** disponible sur Intel macOS 26.
- L'agent n'a **pas de sudo**. Les étapes nécessitant la root (éditions de la racine iOS avec préservation des propriétaires) sont lancées par le propriétaire avec `sudo`.
- L'UEFI et Mesa se construisent dans un **conteneur Linux** (OrbStack, Colima ou Docker) sur le hackintosh.
- Le packaging `boot.img` et les scripts Python sont natifs sur macOS.
- Le flash et les logs utilisent `adb` + TWRP depuis macOS.
- Windows sert uniquement de secours (Odin pour restaurer le firmware stock ou réinstaller TWRP).

---

### 8. Processus de build — étape par étape

Le workflow officiel est décrit dans `engineer.md` section 5 et `beyond2lte-ios/README.md`.

#### Étape 0 — Prérequis (une seule fois)

1. Récupérer les sources XNU épinglées :
   ```sh
   scripts/fetch-xnu.sh
   ```
2. Appliquer l'overlay et les patches B2L :
   ```sh
   scripts/prepare-xnu.sh
   ```
3. Préparer les dépendances externes (KDK, SDK, bibliothèques dans `deps/` et `deps-src/`).

#### Étape 1 — Construire le noyau XNU

```sh
KDKROOT=/Library/Developer/KDKs/KDK_26.5_25F71.kdk \
B2L_DEPSROOT=~/Developer/s10-port/deps \
BUILD_JOBS=4 \
BUILD_MEMORY_BYTES=12884901888 \
BUILD_DIR=$PWD/out/xnu-b2l-ref \
bash scripts/build-kernel.sh CWARNFLAGS=-w CXXWARNFLAGS=-w
```

Ce script :
- Vérifie que le dépôt XNU est à la révision `ac9718fb1af618d5ce8678d0dc6e8a58f252216f`.
- Vérifie que l'overlay B2L est appliqué (`pexpert/pexpert/arm64/B2L.h` présent).
- Intègre libpthread, BearSSL et le pilote HFS dans XNU.
- Produit `out/xnu-b2l-ref/obj/DEVELOPMENT_ARM64_B2L/mach.development.b2l`.

#### Étape 2 — Construire le firmware UEFI

Dans un arbre de travail Mu jetable préparé par `scripts/make-board-platform.py` :

```sh
B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 \
[B2L_FB_LOGICAL=WxH B2L_DISPLAY_SCALE=N] \
scripts/build-boot-firmware.sh
```

Produit :
- `Build/beyond1ltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd` (firmware UEFI, 4 Mio)
- `Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi` (loader EFI)

Pour le S10+ : `bash yap.sh --clean` / `--tar` / `--acpi` depuis `beyond2lte-uefi/`.

#### Étape 3 — Empaqueter le boot.img

```sh
python3 scripts/package-boot.py \
  --mu $W \
  --firmware $W/Build/beyond1ltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd \
  --kernel $K \
  --loader $W/Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi \
  --output-dir out/b1l-metal-NNNN \
  --max-image-bytes 57671680 \
  --boot-partition-bytes 57671680 \
  --clang $E/clang --objcopy $E/llvm-objcopy
```

Vérification : `python3 scripts/package-boot.py --verify /path/to/boot.img`

Le `boot.img` contient : en-tête Android v1, firmware UEFI (4 Mio), noyau XNU compressé, loader EFI embarqué, trailer `B2LXNU02` avec graine 256 octets, DTB à la fin du champ noyau.

#### Étape 4 — Construire la racine iOS (première fois uniquement)

```sh
sudo bash b1l-tools/build-ios-root.sh
```

Montre l'IPSW iOS 26 (`23G90`, iPhone 16 Pro Max), extrait `OS`, `SystemOS` et `AppOS` selon `BuildManifest.plist`, et les copie dans une image **Journaled HFS+ sensible à la casse** (32 Gio), en préservant propriétaires, liens et hard links.

Puis : `b1l-tools/build-system-image.sh` (construit l'image SYSTEM contenant le cache natif `b2l-cache.bin` d'environ 5,52 GiB et une queue de journal de 16 Mio).

#### Étape 5 — Éditer la racine (sur une copie)

1. Récupérer la racine live :
   ```sh
   b1l-tools/pull-chunked.sh ios26-userdata.hfs.img userdata /dev/block/sda34 499641200640
   ```
2. Cloner comme point de rollback : `cp -c`
3. Éditer le clone avec `sudo` :
   ```sh
   sudo bash b1l-tools/set-root-prefs.sh <profile>
   sudo bash b1l-tools/install-translated-cache.sh
   ```
4. Réecrire uniquement les chunks modifiés :
   ```sh
   b1l-tools/system-add-fast.sh <set>
   ```

---

### 9. Chaîne complète automatisée : `b1l-build.sh`

```sh
bash b1l-build.sh 0139 b2l-metal-v88 --mesa
```

Stages exécutés (chacun écrit dans `progress/NNNN.status`) :
1. **mesa** — `scripts/build-mesa.sh` (seulement avec `--mesa`)
2. **libs** — `scripts/build-metal-scion.sh` dans `build-metal/SET`
3. **kernel** — `scripts/build-kernel.sh` (headers graphiques copiés dans XNU avant)
4. **package** — `scripts/package-boot.py` → `out/b1l-metal-NNNN/boot.img`
5. **system** — `system-add-fast.sh SET` (SYSTEM : seuls les blocs modifiés, relus à chaque exécution)
6. **boot** — `flash-boot.sh` (BOOT, avec gardes et read-back)

Le fichier `progress/$N.status` contient une ligne par stage : `nom|état|début|fin|détail` (état : pending, running, done, failed). `progress/current` pointe vers le statut le plus récent.

---

### 10. Politique de stockage et sécurité

Il n'y a **pas de boot en RAM** sur ce téléphone. S-Boot charge le champ noyau de la partition **BOOT**, et iOS a besoin de sa racine dans **USERDATA**.

#### Partitions où l'on peut écrire (accord explicite du propriétaire pour chaque installation)

| Partition | Bloc (S10) | Taille | Contenu | Écrit par |
|-----------|------------|--------|---------|-----------|
| **BOOT** | `sda14` | 57 671 680 octets (55 Mio) | `boot.img` (BootShim + UEFI FD + DTB + trailer XNU) | hôte via TWRP `dd`, puis readback |
| **USERDATA** | `sda34` | ~465 Gio | Racine HFS+ iOS (32 Gio image) | hôte via `flash-chunked.sh` |
| **SYSTEM** | `sda25` | 7 340 032 000 octets | HFS+ journalisé contenant `b2l-cache.bin` + queue journal 16 Mio | hôte via `flash-chunked.sh` |

#### Partitions à NE JAMAIS écrire

`bota*`, `efs`, `sec_efs`, `cpefs`, `radio`, `keystorage`, `keydata`, `keyrefuge`, `param`, `up_param`, `uh`, `uhcfg`, `steady`, `persistent`, `misc`, `recovery`, `dtb`, `dtbo`, `vbmeta`, la GPT, ou tout ce qui n'est pas dans le tableau ci-dessus.

#### Gardes obligatoires avant chaque écriture sur l'appareil

1. L'appareil est en TWRP (`adb get-state` = `recovery`) avec root (`id -u` = 0).
2. `ro.boot.em.model` = `SM-G973F`. `ro.product.device` n'est **pas** fiable (les ROM custom le réécrivent).
3. `/dev/block/by-name/<nom>` résout vers le bloc attendu, et `blockdev --getsize64` correspond exactement.
4. La cible n'est pas montée. TWRP monte USERDATA à `/data` et `/sdcard` : démonter d'abord.
5. Le hash de l'image est vérifié sur l'hôte et de nouveau après transfert.
6. Après l'écriture : vider les caches, relire, comparer SHA-256. Tout enregistrer dans le journal de build.
7. Les grosses images utilisent `flash-chunked.sh` (chunks de 512 Mio vérifiés, resumable). Ne jamais streamer une grosse image via `adb exec-in` (cela stalle après ~5 Go).

#### La racine live est la source de vérité

Le cache natif réécrit les pages traduites **à l'intérieur des fichiers iOS sur le téléphone**. Ne jamais réécrire l'image originale par-dessus une racine utilisée. Procédure :
1. Lire la racine en retour avec `pull-chunked.sh` (passer l'image originale comme base pour sauter les chunks inchangés).
2. Cloner (`cp -c`) comme point de rollback.
3. Éditer le clone. Utiliser les scripts `sudo` pour les éditions préservant les propriétaires.
4. Réecrire uniquement les chunks modifiés.

#### Sauvegardes existantes

`~/Developer/s10-port/device-s10/backup-20261103/` : efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param, keystorage, steady, persistent, misc — tous SHA-256 vérifiés. L'Android SYSTEM n'a **pas** été sauvegardé (choix du propriétaire), et Android userdata est disparu. Un retour à Android nécessite Odin avec le firmware stock `G973F`.

---

La partie 3 couvre les journaux de build détaillés (état actuel, GPU, référence S7, faits du S10).
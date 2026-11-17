# Port d'iOS 26 sur Galaxy S10 (SM-G973F, beyond1lte, Exynos 9820)

État des sources au 2026-11-01. Rien n'est encore construit ni testé.
`[VERIFY]` = à prouver sur ton téléphone ou dans le DTB. `[HYPOTHÈSE]` = mon déduction, pas un fait documenté.

## 1. Stratégie

**On ne part pas de zéro : on adapte `beyond2lte-ios` du S10+ au S10.** C'est le seul dépôt à combiner le même SoC et iOS 26.

| Dépôt | Rôle | Ce qu'on en prend |
| --- | --- | --- |
| `orhunnx/beyond2lte-ios` | **Base** (XNU 12377, iOS 26, Exynos 9820) | Tout : patchs, loader EFI, packaging, émulation arm64e, CPU, UFS, HFS, tactile |
| `orhunnx/beyond2lte-uefi` (branche `master`) | Référence firmware | Overlay UEFI du S10+ |
| `Project-Silicium/Mu-Silicium` | Firmware S10 | `Platforms/Samsung/beyond1ltePkg` existe déjà |
| `wardevsa/ios-a5xelte` (`herolte`) | **Référence GPU Mali** | Service kernel de jobs, mise à l'échelle GPU, méthode d'optimisation. iOS 16 et XNU 8796 : à adapter, pas à copier |
| `superboyyy/hakios-arm64e-dada` | **Référence Metal et audio** | Chaîne `metal2vulkan` → Vulkan → kernel, plugin Metal |
| `oosexgang/hakios-xnu-kernel` | Référence Wi-Fi | Intégration du Wi-Fi à iOS. XNU 8796, Qualcomm seulement |
| `UniXNU/UniXNU` | Référence d'architecture | Découpage des pilotes. Pas d'Exynos 9820 au catalogue |

**Point de départ réel (S10+, build 0100) :** boot, 8 cœurs, ~7 Go de RAM, UFS, root HFS, SpringBoard, tactile et Réglages fonctionnent. Lock screen à ~90 s. Icônes mal dessinées, fond d'alertes incorrect, assertion SpringBoard, aucune app installée. Pas de GPU, Wi-Fi, Bluetooth, audio, USB sous iOS, ni veille.

## 2. Ce qui change la donne

1. **Tout iOS 26 est arm64e et l'Exynos 9820 n'a pas de PAC matériel.** Les instructions manquantes passent par des traps kernel et un cache de code natif (~5,5 Gio). Le premier boot sera très lent, et ce coût restera la limite de fluidité, même avec le GPU.
2. **iOS 26 n'est pas iOS 16 côté GPU.** Le pont OpenGL ES/EAGL du S7 fonctionne sur iOS 16. Sur iOS 26, dada passe par Metal → Vulkan. Pour Mali, la voie probable est `metal2vulkan` → PanVK (Mesa) → service kernel Mali `[HYPOTHÈSE]`. Personne n'a publié ça sur Mali-G76 : c'est de la recherche, pas un portage. La partie Metal n'est pas garantie.
3. **Aucun des dépôts n'a de Wi-Fi utilisable sur cette puce.** La puce du S10 est une Broadcom PCIe, probablement BCM4375 `[VERIFY]`, alors que les pilotes existants visent Qualcomm. Il faudra un contrôleur PCIe Exynos, un pilote de puce et un raccord au réseau iOS. Comme le code `bcmdhd` de Samsung est sous GPL, on s'en sert pour comprendre le protocole, comme dada l'a fait avec Linux, sans le recopier.
4. **Les installations d'apps sont bloquées sur HFS** (erreur 166, iOS exige APFS). Il faudra une voie de staging compatible HFS pour peupler l'écran d'accueil, ou copier les apps directement dans l'image comme le S7.
5. **BOOT fait 55 Mio sur ton S10** (`sda14`), contre une limite par défaut de 64 Mio au packaging. Il faut vérifier que firmware + kernel y tiennent.
6. **`ro.product.device` vaut `r13s` sous ArtisanROM.** Une garde de flash basée sur cette propriété serait fausse. Il faut la remplacer : `ro.boot.em.model` sous Android, l'identité TWRP en recovery.

## 3. Phases

Règle sur tout le projet : **une seule modification par boot de test, chaque build a son journal `docs/build-XXXX.md`.** « Verified » signifie observé sur ton S10.

### Phase 0 : Sécurité et faits (bloquante)
- Lire en **Download mode** : `OEM LOCK`, `KG STATE`, `RMM STATE`. Les propriétés Android sont peut-être falsifiées par la ROM.
- Sauvegarder depuis TWRP : `efs`, `sec_efs`, `cpefs`, `boot`, `recovery`, `dtb`, `dtbo`, `vbmeta`, en vérifiant les hash. Ce contrôle n'a jamais été fait (Gate 0 du projet ColorOS toujours ouvert).
- Tester un vrai retour au stock par Odin.
- Copier ailleurs ce qui compte : **`userdata` sera effacé**.

**Critère :** restauration testée et sauvegardes hors du PC de travail.

### Phase 1 : Chaîne de build
- Reproduire le S10+ **sans modification** : kernel, EFI, `boot.img`.
- Le kernel exige macOS, Xcode et un KDK Apple 26.5 : il sera compilé sur le T470s (voir section 5). **Premier test à faire, car il décide de toute la chaîne :** compiler XNU 12377 non modifié, puis la cible S10+ sans changement.
- UEFI : l'ancienne session a échoué ici. Le lien `MdePkg/Library/ArmLib` de Mu-Silicium était cassé dans WSL. Cause probable : clone ou sous-modules incomplets `[HYPOTHÈSE]`. Remède : clone récursif frais dans un conteneur Linux sur le T470s, comme dada.

**Critère :** `scripts/test-loader.sh` et `package-boot.py --verify` passent.

### Phase 2 : Cible `beyond1lte`
- Comparer les DTB G973F et G975F, puis `common/memory/*`, cartes RAM, panneau, tactile, GPIO, GPT.
- Créer le board `B1L` en gardant le S10+ intact, avec un kernel console seul, **sans iOS**.
- Build avec `--boot-partition-bytes` réel, garde de flash corrigée, relecture du hash après écriture.

**Critère :** texte XNU à l'écran, UFS natif lu, 8 cœurs, journal récupérable depuis TWRP. Android reste intact à part BOOT.

### Phase 3 : iOS 26 jusqu'au rendu
- Construire le root HFS (section 4), l'écrire dans USERDATA.
- Atteindre le lock screen, puis répéter les corrections du S10+ jusqu'à l'équivalent de 0100.
- Installer la correction 0101 (compilée, jamais testée).

**Critère :** lock screen, tactile, Réglages s'ouvrent.

### Phase 4 : Priorité 1, patch graphique
- Trouver l'origine des icônes et alertes incorrectes, à partir des journaux CoreImage et CoreGraphics déjà instrumentés.
- Résoudre l'assertion SpringBoard.
- Peupler les apps : d'abord copie dans l'image, HFS staging ensuite.
- Réduire la résolution logique si la mesure le justifie (72 % de pixels en moins sur le S7).
- Mesurer avant et après chaque optimisation : temps de boot, FPS, CPU au repos. Retirer les démons qui bouclent, comme `BTAudioHALPlugin` sur le S7.

**Critère :** écran d'accueil correct, apps qui s'ouvrent, 30 min sans panic.

### Phase 5 : Priorité 2, Wi-Fi
- Identifier la puce et le bus dans le DTB G973F et les sources Samsung.
- Extraire le firmware et la NVRAM de **ton** téléphone (jamais dans le dépôt).
- Démarrer PCIe, charger le firmware, faire répondre la puce, scanner, s'associer, faire du DHCP.
- Étudier d'abord comment HakiOS relie le Wi-Fi à iOS sur `redwood`.

**Critère :** `ping`, puis une page Safari. Attendu : long, jalon par jalon, sans date garantie.

### Phase 6 : Priorité 3, accélération GPU
Paliers, chacun testable seul :
1. Alimenter le Mali-G76 et lire son ID.
2. Exécuter un job de test kernel, comme `herogpu=3` sur le S7, adapté de Midgard à Bifrost.
3. Mettre à l'échelle via le GPU pour alléger le CPU (gain possible sans Metal).
4. Compiler Mesa pour iOS.
5. `metal2vulkan` + PanVK, plugin Metal, composition CoreAnimation.

**Critère :** paliers 1 à 3 considérés réalistes, 4 et 5 considérés recherche ouverte. Si le palier 5 bloque, le palier 3 reste une amélioration réelle.

### Phase 7 : Option Bluetooth
Seulement après les phases 4 à 6. Même famille de puce, même bus à étudier `[VERIFY]`.

## 4. Récupérer iOS 26 (guide)

**Image cible : IPSW iPhone 16 Pro Max (`iPhone17,2`), iOS 26.6.2, build `23G90`.** Le dépôt S10+ cite ce build et s'annonce en `iPhone17,2`, mais ne nomme pas explicitement l'IPSW source. À confirmer auprès de l'auteur par une issue avant de télécharger `[VERIFY]`. **Garder exactement cette version** : les patchs sont calés dessus, et une autre version déplace tout. iOS 26 n'existe pas en arm64 simple.

1. Télécharger l'IPSW depuis les liens officiels Apple (index : ipsw.me).
2. Extraire `OS`, `SystemOS` et `AppOS` selon `BuildManifest.plist`.
3. Monter les images en lecture seule sur le T470s (macOS), propriétaires activés. Une partie des images peut être au format `.aea`, à vérifier sur le fichier réel `[VERIFY]`.
4. Copier dans une image **Journaled HFS+ sensible à la casse** (32 Gio au S10+, ~20 Go de données), en conservant propriétaires, liens et hard links.
5. Placer SystemOS sous `private/preboot/Cryptexes/OS`, AppOS sous `private/preboot/Cryptexes/App`.
6. Lancer `scripts/prepare-ios-root.py` en `--dry-run`, puis pour de vrai, puis `--verify`.
7. Démonter proprement, passer `fsck`, noter le hash.
8. Écrire l'image complète depuis TWRP, zéros compris. Une image sparse mal écrite corrompt le volume.

Détails : `docs/ios-userspace-root.md` du dépôt S10+. Le guide complet, commande par commande, sera écrit dans `docs/INSTALL.md` à partir de ce qui aura réellement marché.

## 5. Matériel et prérequis

- **Mac de build : ThinkPad T470s (i7, Intel), hackintosh sous macOS 26 Tahoe.** Il sert pour :
  - le kernel : Xcode 26, SDK iPhoneOS, KDK 26.5 installé localement ;
  - l'image iOS : montage APFS natif, `prepare-ios-root.py` tel quel ;
  - plus tard, le code arm64e et Metal côté iOS, et l'analyse des binaires avec les outils Xcode.
- **Écart avec la référence :** le S10+ est compilé avec Xcode 27.1 et le SDK macOS 27 sur Apple Silicon. Ici, c'est Xcode 26 sur Intel, donc il faudra peut-être adapter les scripts ou les avertissements `[VERIFY]`. Si ça ne compile pas, GitHub Actions sert de secours, avec un dépôt public et sans rien d'Apple dedans.
- **Limites du T470s :** 2 cœurs / 4 threads, donc le premier build complet sera long et les builds incrémentaux suffiront ensuite. Pour l'image iOS, il faut ~150 Go libres, ou un SSD externe USB-C. La RAM est à vérifier (16 Go ou plus conseillé).
- **Tout le projet se fait sur le T470s, sauf le secours Odin.**
  - UEFI (Mu-Silicium/EDK2) et Mesa : dans un conteneur Linux, avec OrbStack, Colima ou Docker. Le même conteneur évite le problème de liens cassés vu sous WSL. Il faut que VT-x soit actif sur le hackintosh `[VERIFY]`.
  - Packaging `boot.img`, scripts Python : natifs sur macOS.
  - Flash et logs : `adb` + TWRP depuis macOS (`android-platform-tools`), avec `dd` de BOOT et USERDATA, comme le S10+.
- **Windows reste en secours uniquement :** Odin pour remettre le firmware stock ou réinstaller TWRP. Heimdall existe sur macOS mais n'est pas fiable sur les Samsung récents `[VERIFY]`, donc on ne s'appuie pas dessus.
- **Contrôle à distance :** activer « Session à distance » (SSH) sur le T470s, pour que Claude lance les builds depuis ce PC.
- TWRP, Odin, platform-tools : déjà présents dans `iOS S10\s10 rom`.
- Une prise de photo de l'écran : pas de série, donc le premier debug passe par des photos et les journaux lus depuis TWRP.

## 6. Risques

- **Conflit avec ColorOS :** le même téléphone sert aux deux projets et iOS efface `userdata`. Il faut alterner, avec restauration entre les deux.
- **Brique :** on n'écrit que BOOT et USERDATA, et `SYSTEM` pour le cache. Jamais `bota*`, `efs`, `sec_efs`, `radio`, `keystorage`.
- **Sécurité inexistante :** pas de chiffrement, pas de SEP, pas de signature réelle. Aucune donnée personnelle sur cette install.
- **Chaleur et batterie :** pas de veille, et aucune limite thermique validée. Ne pas laisser l'appareil tourner sans surveillance.
- **Juridique :** la licence d'Apple lie iOS au matériel Apple, et la réponse dépend du pays. Rien d'Apple, aucun firmware, IMEI ou MAC dans le dépôt. Les services Apple (activation réelle, iCloud, App Store) sont hors périmètre.
- **Résultat attendu :** une démonstration de recherche, pas un téléphone quotidien. Pas d'appels, de 4G, de caméra ni d'audio dans ce plan.

## 7. Vérification

| Niveau | Preuve |
| --- | --- |
| Hôte | Tests du dépôt, `package-boot.py --verify`, `fsck`, hash de l'image |
| Flash | Relecture de BOOT octet par octet, modèle confirmé G973F |
| Téléphone | Photo de l'écran + journal kernel/`tracev3` récupéré depuis TWRP |
| Régression | Le board S10+ continue de compiler et ses tests passent |

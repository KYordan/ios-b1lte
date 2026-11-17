# Build B1L-0017 : surfaces YUV (IOSurface multi-plan) + premier job GPU (G2b)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `d60b1791…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0016.

## Analyse du rendu CPU (vue depuis le kernel)

- backboardd n'a pas de périphérique Metal : il compose avec `CA::OGL::SWContext` (rendu logiciel annexe d'iOS). Effets
  modernes (matériaux, vibrance, flous de fond) incomplets → Control Center vide, couleurs fausses ; tout l'écran
  1440×3040 composé sur le CPU puis copié (17,5 Mo par image). Correction de fond : pilote GPU.
- IOSurface (B1L-0016, journal kernel) : **toutes les créations multi-plan échouent** (`chronod … fmt 0x78663230 'xf20'
  planes 2 -> 0xe00002c7`, `'420f'`) : `B2LSurface.cpp` refusait volontairement `planeCount != 0`.
  Images HEIC/JPEG décodées, fonds d'écran, photos, vidéo et vignettes de widgets utilisent ces formats.
- IOMobileFramebuffer : sélecteurs refusés identifiés dans `IOMobileFramebuffer.framework` (12 RequestPowerChange,
  15 SetDebugFlags, 22 SetDisplayDevice, 25 GetDigitalOutState, 27 GetGammaTable, 29 GetDisplayArea,
  33 EnableDisableVideoPowerSavings, 50 SetBrightnessCorrection/ICC, 57 GetColorRemapMode, 68 SetParameter,
  79 GetBlock, 84 SetContrast). Fonctions secondaires : non simulées sans preuve.

## 1. IOSurface multi-plan

Disposition du résultat de verrouillage relevée dans `IOSurface.framework` (iOS 26, client +0x70 = résultat +0) :
nombre de plans +0x58 ; plan n à +0x88 + 0x98·n : largeur +0, hauteur +8, décalage +0x18, octets/ligne +0x20,
taille +0x28, octets/élément +0x30 (u16), largeur/hauteur d'élément +0x32/+0x33, compression +0x6c (laissée à 0).
Cohérent avec les champs mono-plan déjà utilisés (+0x20/+0x28/+0x30/+0x60). Le refus est supprimé.
Test hôte `tests/test_surface_properties.py` mis à jour : PASS.

## 2. GPU G2b : premier job

Tampon contigu de 6 pages (tables L0..L3 AArch64 4K, descripteur, résultat), caches CPU nettoyés au PoC.
AS0 : `TRANSTAB`, `MEMATTR = 0x48…` (non cacheable), `TRANSCFG = AARCH64_4K | INA_BITS(7) | PTW_NC`, `UPDATE`.
Job `WRITE_VALUE` (type 2, IMM32, valeur `0xB21CAFE5` vers VA 0x105000) sur la file 0, `JS_CONFIG = AS0 | flush
début/fin | priorité 8`. Sondage `JOB_IRQ_RAWSTAT` ≤ 200 ms ; journal `B2L GPU G2b:` (statut de file, faute MMU, valeur lue).
En cas d'échec : soft reset de récupération. Coupe-circuit `b2l_gpu_job=0` (et `b2l_gpu=0` pour tout le GPU).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-yuv-gpujob-0017/boot.img` (25 910 152 octets) | `d60b1791c1bb3aacf1f109369b80e95fc19fbed00bfc86787e299f9b09135292` |
| Kernel | `337e79f9c5e4a3182cc3263510289e291a5fee57c13bdb8b44c7721f77debc0e` |

Retour arrière : `out/b1l-gpu-g2-0016/boot.img`.

## Résultat observé (B1L-0017)

- **G2b validé** : `as=ok jobirq=00000001 js_status=00 mmuirq=0 as_fault=0 desc_exc=00000001 result=b21cafe5 (WRITE_VALUE executed)`.
  Premier job exécuté par le Mali à travers une table de pages AArch64 4K programmée par notre kernel.
- **Surfaces YUV créées** : `420f`, `xf20`, `444f`, `xf44` (2 plans) → `0x0`.
- Rien de changé à l'écran : pas de fond d'écran, pas de widgets, 6 apps, horloge mal rendue, gestes inopérants.
- Control Center : s'affiche ; vide car seuls « Mode Avion » et « Partage de connexion » sont proposés.
  → contrôles, widgets et fonds d'écran viennent des apps et de leurs extensions, qui ne sont pas enregistrées.

## Diagnostic des apps

- `IconState.plist` : Réglages, Santé, Messages, Notes, Raccourcis (+ Téléphone).
- Base LaunchServices du téléphone : 413 apps, mais `Calculator.app`, `MobileSafari.app`, `EmojiPoster.app`, `Weather.app` absentes.
- `UIRequiredDeviceCapabilities` identique (`arm64e`) pour les 50 apps : pas un filtre de capacités.
- iOS livre `System/Library/CoreServices/com.apple.launchservices.lsd.csdb` (base pré-calculée) où ces apps sont des apps
  « en attente » installées par installd dans des conteneurs : les copies de B1L-0013 dans `/Applications` sont écartées.
→ B1L-0018 : rendre la main à l'installeur natif (`install-b1l-0018-native-apps.sh`).

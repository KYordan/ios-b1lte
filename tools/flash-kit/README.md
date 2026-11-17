# Flash-kit — Sauvegarde et flash des images iOS sur Galaxy S10 (SM-G973F)

## Contenu

| Fichier | Rôle |
|---------|------|
| `check-device.sh` | Vérifie que le téléphone est en TWRP, root, modèle SM-G973F, et liste les partitions cibles |
| `backup-partitions.sh` | Sauvegarde les partitions importantes avant toute modification |
| `flash-images.sh` | Flashe boot.img, system.img et userdata.img dans les partitions existantes |
| `images/` | Dossier où placer les 3 images (boot.img, system.img, userdata.img) |
| `docs/partition-map.md` | Carte des partitions du S10 et ce qu'il ne faut JAMAIS écrire |

## Prérequis

- Téléphone en **TWRP 3.7.1** avec root ADB
- Série du téléphone : `<SERIAL>`
- `adb` disponible : `~/Developer/android/platform-tools/adb`
- Mac de build : ThinkPad T470s, macOS 26

## ⚠️ Règles de sécurité (non négociables)

1. **N'écris que** BOOT (`sda14`), SYSTEM (`sda25`) et USERDATA (`sda34`).
2. **N'écris JAMAIS** : `bota*`, `efs`, `sec_efs`, `cpefs`, `radio`, `keystorage`, `keydata`, `keyrefuge`, `param`, `up_param`, `uh`, `uhcfg`, `steady`, `persistent`, `misc`, `recovery`, `dtb`, `dtbo`, `vbmeta`, la GPT.
3. **Avant chaque écriture** : vérifier TWRP, root, `ro.boot.em.model = SM-G973F`, chemin by-name, taille exacte, partition non montée, hash SHA-256, readback après écriture.
4. **Backup obligatoire** avant tout flash : utilise `backup-partitions.sh`.
5. Ce kit ne **repartitionne pas** le téléphone. Le repartitionnement d'un Samsung S10 se fait avec Odin + fichier PIT, c'est une opération séparée et très risquée.

## Comment obtenir les 3 images

| Image | Comment l'obtenir |
|-------|-------------------|
| `boot.img` | `scripts/package-boot.py` dans `beyond2lte-ios/` (produit `out/b1l-metal-NNNN/boot.img`) |
| `system.img` | `b1l-tools/build-system-image.sh` (contient le cache natif + journal) |
| `userdata.img` | `b1l-tools/build-ios-root.sh` depuis l'IPSW iOS 26 `23G90` (iPhone 16 Pro Max) |

Place-les ensuite dans `flash-kit/images/` avec les noms exacts : `boot.img`, `system.img`, `userdata.img`.

## Ordre d'utilisation recommandé

1. `./check-device.sh` — vérifier l'état du téléphone
2. `./backup-partitions.sh` — sauvegarder tout avant de toucher à quoi que ce soit
3. Placer les images dans `images/`
4. `./flash-images.sh` — flasher les 3 images (avec gardes et readback)
5. `adb reboot` — redémarrer vers iOS

## Dossier `images/`

Vide par défaut. C'est toi qui dois y placer les 3 images générées par la chaîne de build. Ne commit pas les images (trop grosses, contiennent potentiellement des données personnelles).
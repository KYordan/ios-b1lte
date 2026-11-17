# Carte des partitions — Galaxy S10 SM-G973F (beyond1lte)

Données mesurées depuis TWRP root (`docs/s10-device-facts.md`).

## Partitions où l'on peut écrire (avec ce kit)

| Nom | Bloc | Taille | Contenu | Écrit par |
|-----|------|--------|---------|-----------|
| `boot` | `/dev/block/sda14` | 57 671 680 octets (55 Mio) | `boot.img` : BootShim + UEFI FD + DTB + trailer XNU | `flash-images.sh` |
| `system` | `/dev/block/sda25` | 7 340 032 000 octets | HFS+ journalisé : cache natif `b2l-cache.bin` + queue journal 16 Mio | `flash-images.sh` |
| `userdata` | `/dev/block/sda34` | 499 641 200 640 octets (~465 Gio) | Racine HFS+ iOS (32 Gio image) | `flash-images.sh` |

## Partitions à NE JAMAIS écrire

`bota*`, `efs`, `sec_efs`, `cpefs`, `radio`, `keystorage`, `keydata`, `keyrefuge`, `param`, `up_param`, `uh`, `uhcfg`, `steady`, `persistent`, `misc`, `recovery`, `dtb`, `dtbo`, `vbmeta`, la GPT.

## Sauvegardes existantes

`~/Developer/s10-port/device-s10/backup-20261103/` contient déjà : efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param, keystorage, steady, persistent, misc (tous SHA-256 vérifiés).

L'Android SYSTEM n'a pas été sauvegardé (choix du propriétaire). Android userdata est disparu. Un retour à Android nécessite Odin avec le firmware stock `G973F`.
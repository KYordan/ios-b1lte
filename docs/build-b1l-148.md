# Build 148 : WiFi, configuration de la puce, BAR, identifiant interne, EROM et OTP (build regroupé)

**Exception à « un changement par build »**, accordée par le propriétaire le 11/11/2026 : plusieurs étapes, chacune exécutée seulement si la précédente a réussi, avec une trace avant chacune.

## Diagnostic

Le build 147 a monté le lien PCIe (LTSSM L0, Gen1 x1). La puce n'a pas encore été lue.

## Changement

Dans `B2LWifi.cpp`, après `LINK: up` :

1. `probeEndpoint()` : iATU viewport 1 en CFG0 sur `0x14FFE000` (bus 1), lecture de l'ID, de la commande, de la classe, de l'en-tête, du sous-système, tailles des 6 BAR (écriture `~0`, lecture, restauration). **Arrêt si l'ID n'est pas `0x447514e4`** (BCM4375).
2. Placement des BAR mémoire dans la fenêtre DT `0x14000000`–`0x14FEFFFF` (alignés sur leur taille, BAR 64 bits avec moitié haute à 0) ; fenêtre non préfetchable du pont racine (DBI `0x20`), fenêtre préfetchable fermée ; iATU viewport 0 en MEM identité ; commande de la puce = MEM + MASTER.
3. `probeChip()` : par la fenêtre 0 de BAR0 (`PCI_BAR0_WIN`, config `0x80`), lecture de chipcommon (`0x18000000`) : chipid, capacités, pointeur EROM. **Arrêt si la puce n'est pas `0x4375`.** Copie brute de l'EROM (8 mots par ligne jusqu'au marqueur de fin).
4. OTP selon `read_otp_from_bp` (`dhd_custom_cis.c`, `BCM4375_CHIP`) : sauvegarde de OTP Control 1 (`0x18010324`) et du masque PMU min (`0x18012618`), écriture `0x00fa0000` / `0x0e4fffff`, lecture des 22 mots CIS (`0x18011120`), restauration et relecture.

Calcul de placement des BAR vérifié hors ligne (BAR0 32 Kio → `0x14000000`, BAR2 4 Mio → `0x14400000`). Compilation à blanc OK.

## Installé avec hash BOOT

- Build : **148**, SET `b2l-metal-v88` (`--no-libs`).
- BOOT relu : `b5f9594cab7109d6652bfe44939a82f6fadbe6f3d9bf16faf3d2bbffc1dd7b5b` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 59 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

**Toutes les étapes réussies, boot iOS sans régression** (aucun `B2L CRASH`, journal `out/logs/0148-boot.txt`).

```
B2L WIFI EP: id=0x447514e4 cmd=0x100000 class=0x2800005 hdr=0 sub=0x437514e4
B2L WIFI EP: bar=<0x4 0 0x4 0 0 0> size=<0xffff0004 0xffffffff 0xffc00004 0xffffffff 0 0>
B2L WIFI BAR: bar0=0x14000000/0x10000 bar2=0x14400000/0x400000 cmd=0x100006 bridge=0x14f01400
B2L WIFI CHIP: chipid=0x10054375 chip=0x4375 rev=5 pkg=0 socitype=1 caps=0x58400009 erom=0x18111000
B2L WIFI OTP: ctl=0 minres=0x64fffff restored=0/0x64fffff
```

- **Puce** : BCM4375 (`0x4475`, Broadcom), classe réseau `0x0280`, BAR0 64 bits 64 Kio, BAR2 64 bits 4 Mio. Révision 5, backplane AI.
- **EROM décodée** (`scratchpad erom.py`) : chipcommon `0x18000000` ; deux D11 (`0x18001000`, `0x18004000`) ; **ARM CR4 rev 15** : registres `0x18002000`, wrapper unique `0x18102000`, TCM `0x0`–`0x150000` et **`0x170000`–`0x300000`** ; **PCIe2 rev 64** : `0x18003000`, wrapper `0x18103000` ; GCI `0x18010000`, PMU `0x18012000`.
- **OTP** : tuples CIS lus, `boardtype = 0x088f`, **VID = `53 33` → `semco_sem_e43_cs53`** (`vid_info` de `dhd_custom_cis.c`) → table de nommage `e43_cs53` → **NVRAM `nvram.txt_CS01_semco_b1`**, firmware `bcmdhd_sta.bin_b1`. Le `boardtype` de cette NVRAM vaut aussi `0x088f`.
- Registres OTP et PMU restaurés à l'identique.

Suite : build 149 (chargement du firmware et démarrage du CR4).

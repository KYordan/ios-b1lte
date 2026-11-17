# Bluetooth du S10 : revue de l'existant et plan (2026-11-13)

But : l'interrupteur Bluetooth de Réglages s'allume, la recherche trouve des appareils, l'appairage d'un casque tient
après un redémarrage et le son passe en A2DP.

## 1. Revue de l'existant

### Matériel (mesuré dans le DTB live `device-s10/dump/live.dtb` et le noyau Samsung 4.14 `ref-kernel/k9820`)

| Élément | Valeur | Source |
| --- | --- | --- |
| Puce | BCM4375, Bluetooth en UART HCI (H4), Wi-Fi en PCIe | prompt, `bcm43xx.c` |
| UART | `uart@10840000` (alias `uart1`), USI v2, `samsung,rts-gpio-control`, FIFO 256 o, IRQ 0x198 | DTB live |
| Broches UART | gpq0-0..3, fonction 2, pull-up (pinctrl@15850000 +0xA0) | DTB live, `pinctrl-exynos-arm64.c` |
| BT_EN | gpm8-0, sortie (pinctrl@15C30000 +0x100) | DTB `/bluetooth gpios[0]` |
| BT_WAKE | gpg2-0, sortie (pinctrl@10430000 +0xC0) | `gpios[1]` |
| HOST_WAKE | gpa2-3, entrée, pull-down, front montant (pinctrl@15850000 +0x40) | `gpios[2]` |
| Horloge | CMU_PERIC1 0x10800000 : mux `UART_BT_USER` +0x1c0 (bit 4 : 0 = OSCCLK 26 MHz), div +0x1814, portes +0x2020/+0x2098/+0x209c, QCH +0x3038 | `cmucal-sfr.c` |
| Mise sous tension | BT_WAKE=1, BT_EN=1, 100 ms ; arrêt : BT_EN=0 | `bcm43xx_bt_rfkill_set_power` |
| Micrologiciel | `bcm4375B0_semco.hcd`, `B0_murata`, `B1_semco`, `B1_semco_sem`, `B1_murata` déjà copiés (hors git) dans `device-s10/wifi-bt-fw/vendor-firmware/` | extraction Wi-Fi |

Le choix du `.hcd` dépend de la révision de la puce (B0/B1) et du module (Semco/Murata) : la session Wi-Fi a lu
la révision (`B2L_WIFI_EP_ID`, OTP/CIS) ; à confirmer avant l'étape 3.

### Ce que le port a déjà
- `B2LPlatformExpert.cpp` publie `mac-address-bluetooth0` (synthétique).
- `B2LWifi.cpp` : modèle à suivre (service IOKit sur IOResources, fil noyau, journal `b2l_diag_record`,
  micrologiciel lu depuis SYSTEM par `b2l_userspace_seed.h`). Rien pour le Bluetooth, aucun accès à CMU_PERIC1 ni à l'UART.
- Personne d'autre ne touche gpq0, gpm8, gpg2 ni gpa2 (B2LInput et B2LWifi utilisent d'autres bancs du même bloc).
- Attention : `B2L BT` dans le journal = pile d'appel noyau. Le Bluetooth journalise en `B2L BLUETOOTH`.

### Références
- **ios-pipa** (QCA6390, iOS 26), résumé « BLUETOOTH WORKS IN iOS » : noyau = alimentation + micrologiciel + point H4
  (`kernel control` + nœud de registre `bluetooth` avec `local-mac-address`, `transport-speed` en **OSData 4 octets**) ;
  BlueTool remplacé par un répondeur XPC (`RunBuiltinScript boot/init/deepsleep` → 0) ; trou +0x60 de la table H4
  de bluetoothd ; synchro des disques toutes les 10 s pour garder les appairages ; UART à 921600 pour l'A2DP.
  Leurs réponses aux commandes vendeur Broadcom (0xfcxx) et 0x204a viennent de ce que leur puce est Qualcomm :
  **la nôtre est une Broadcom, comme celles d'Apple** ; bluetoothd lui parle peut-être sans ces faux. À mesurer.
- **HakiOS** `drivers/bluetooth/hakios.bt.c` : seulement une lecture des broches ; utile pour la méthode
  (une étape par niveau de boot-arg, ne jamais toucher un bloc dont l'horloge est coupée).

## 2. Plan

| Étape | Contenu | Signe de réussite |
| --- | --- | --- |
| 1 (B1L-0210) | `B2LBluetooth.cpp` : lecture DT/broches/CMU ; si l'horloge UART_BT est sur OSCCLK et ouverte : UART 115200 8N1 RTS/CTS, BT_EN, `HCI_Reset`, `Read_Local_Version`, puis BT_EN=0 | `THE CHIP ANSWERS`, `manufacturer=0xf` |
| 2 | `.hcd` copié dans SYSTEM (comme `b2l-wifi/`) et lu par la graine ; `HCI_Download_Minidriver` (0xfc2e), enregistrements `Write_RAM` (0xfc4c), `Launch_RAM` (0xfc4e), 250 ms, `HCI_Reset` ; version relue | `lmp_subver` change après le chargement |
| 3 | Débit : `Update_UART_Baud_Rate` (0xfc18) + UART à 921600 puis 3 Mbit/s (mux sur CLKCMU_PERIC1_IP, à lire avant) | `Read_Local_Version` répond au nouveau débit |
| 4 | Point H4 pour bluetoothd (kernel control + nœud `bluetooth`, `transport-speed` OSData), BlueTool remplacé, fil de réception sur l'IRQ 0x198 ou en scrutation 1 ms | `H4: connected`, interrupteur de Réglages |
| 5 | Recherche, appairage, A2DP, appairages gardés (sync des disques, à voir avec la session stockage) | casque appairé après redémarrage |

Règles tenues : une étape par build, aucun écrit sur le stockage, aucun registre CMU écrit à l'étape 1,
tout registre écrit est journalisé avec sa valeur d'avant, `b2l_bt=0` coupe tout, `b2l_bt=1` = lecture seule.

## 3. Étape 1 : ce qui part dans le build 0210

Fichier créé : `xnu-overlay/iokit/Kernel/arm/B2LBluetooth.cpp` (vérifié : compile sans avertissement avec les options
de `B2LWifi.cpp`, `-Wall -Wextra`). À ajouter côté `xnu/` par la session qui construit (même forme que B2LWifi) :
- `xnu/iokit/conf/files` : `iokit/Kernel/arm/B2LBluetooth.cpp optional iokitcpp b2l`
- `xnu/iokit/KernelConfigTables.cpp`, après la ligne `B2LWifi` :
  `"   {'IOClass'=B2LBluetooth;'IOProviderClass'=IOResources;'IOMatchCategory'=B2LBluetooth;},"`

Déroulé : au démarrage du service, un fil lit `/bluetooth`, les quatre bancs de broches et les registres CMU de UART_BT.
Si une porte d'horloge est coupée, ou si le mux n'est pas sur OSCCLK, il s'arrête là (aucune écriture). Sinon il attend
que la graine Wi-Fi soit prête puis 30 s (pour ne pas lever BT_REG_ON pendant le démarrage du Wi-Fi), configure l'UART,
allume la puce, envoie `HCI_Reset` et `Read_Local_Version`, et la rééteint.

Écritures matérielles (toutes journalisées avant/après) : gpq0 CON/PUD (broches 0..3 seulement), gpm8-0 et gpg2-0
(DAT puis CON), registres de l'UART_BT (USI_CON, USI_OPTION, UINTM, UINTP, ULCON, UBRDIV, UFRACVAL, UCON, UFCON, UMCON).
Aucune écriture sur le stockage, ni sur CMU, PMU, ou broches d'un autre composant.

Lignes à lire dans le journal :
- `B2L BLUETOOTH: /bluetooth present, gpios <…>` et `B2L BLUETOOTH PINS[boot]` ;
- `B2L BLUETOOTH CMU: mux=… div=… gate … qch=…` puis `UART_BT clock on, 26000000 Hz` (ou la raison de l'arrêt) ;
- `B2L BLUETOOTH UART[before]: …` ;
- **`HCI_Reset -> 7 byte(s): 04 0e 04 01 03 0c 00`** et **`THE CHIP ANSWERS`**, puis
  `hci=… rev=… lmp=… manufacturer=0xf lmp_subver=…` (0xf = Broadcom) ;
- en échec : `not sent (CTS …)` (la puce ne lève pas CTS) ou `no HCI answer, uerstat=…` ;
- `PINS[off]` avec gpm8 dat bit 0 à 0, puis `step 1 done`.

Ce qui doit rester intact : Wi-Fi (join, IP), écran, GPU, tactile, UFS. Si la moindre régression apparaît, `b2l_bt=0`.

### Résultat observé au build 0210 (journal `out/logs/0210-boot.txt`)
Wi-Fi intact (join + IP). L'étape s'est arrêtée avant toute écriture : la puce n'a pas été allumée.
- `/bluetooth missing` : le DT d'iOS est fabriqué par `uefi/XnuLoader/DeviceTree.c`, qui ajoute `/wlan` à la main mais
  pas `/bluetooth`. Sans effet : les adresses viennent du DTB live Android.
- Valeurs de départ (à garder pour toute écriture) : gpm8 con=0 dat=0 pud=0x1 ; gpg2 con=0 dat=0xc pud=0x1110111 ;
  gpa2 con=0 dat=0x14 pud=0x11101111 (hostwake 0) ; gpq0 con=0x2220000 dat=0x70 pud=0x3331111.
- CMU_PERIC1 UART_BT : **mux=0x10** (sur CLKCMU_PERIC1_IP, débit inconnu), mux2=0, div=0, portes rst/ipclk/pclk=0x200000
  (manuel 0, valeur 1 : ouvertes), qch=0x3f0002 (requête logicielle, horloge forcée).

## 4. Étape 1b (build suivant)
Même fichier, deux changements :
- `/bluetooth` n'est plus exigé, il est seulement signalé dans le journal.
- Avant de toucher l'UART : mux `UART_BT_USER` (0x108001c0) 0x10 → 0x00 (OSCCLK 26 MHz), attente du bit BUSY, et
  remise à 0x10 à la fin. Ce mux n'alimente que l'UART_BT. 26 MHz suffit jusqu'à 921600 bauds (écart 0,75 %),
  le débit qu'ios-pipa a retenu pour l'A2DP.
Lignes attendues en plus : `UART_BT gates open, 26000000 Hz once on OSCCLK (mux now CLKCMU_PERIC1_IP)`,
`write cmu uart_bt mux +0x1c0 0x10 -> 0 (read back 0)` puis, à la fin, `… 0 -> 0x10 (read back 0x10)`.

### Résultat observé au build 0211 : LA PUCE RÉPOND
Wi-Fi resté connecté. `HCI_Reset -> 04 0e 04 01 03 0c 00`, puis ROM : **hci=9 rev=0 lmp=9 manufacturer=0xf (Broadcom)
lmp_subver=0x1111**. CTS levé par la puce (`umstat=0x11`). Le mux est revenu à 0x10.
Sans danger : les écritures DAT relues inchangées tant que la broche est en entrée (la relecture donne le niveau de la
broche, le verrou s'applique au passage en sortie : `PINS[on]` montre bien dat=1). À la fin, gpm8-0 et gpg2-0 restent
en sortie à 0 : c'est l'état où le pilote Linux `bcm43xx` les met au démarrage (`gpio_direction_output(…, 0)`).

## 5. Étape 2 (build suivant) : chargement du micrologiciel
- Fichier : `bcm4375B1_semco.hcd` (sha256 8c8fb286…), copié en `device-s10/wifi-bt-fw/system/bt.hcd` (hors git) ;
  `system-add-fast.sh` copie déjà tout ce dossier dans `SYSTEM:/b2l-wifi/`. Choix : le Wi-Fi tourne avec
  `nvram.txt_CS01_semco_b1`, donc puce **B1**, module **Semco**. Il reste deux candidats : `bcm4375B1_semco.hcd`
  (« Beyond E43 Ant0 ») et `_semco_sem.hcd` (« Beyond SEMCO BT EPA E43 Ant0 », amplificateur externe). J'ai pris le
  premier, la configuration de base ; à confirmer plus tard en lisant la règle de choix dans le `libbt-vendor` du
  vendor Android (lecture seule).
- Noyau : `b2l_bt_fetch()` (dans `B2LBluetooth.cpp`) lit `b2l-wifi/bt.hcd` au moment de la graine. Deux lignes
  à ajouter dans `bsd/kern/b2l_userspace_seed.h` : la déclaration et l'appel juste après `b2l_wifi_fetch();`.
- Déroulé, après la version ROM : `Read_Local_Name`, `Download_Minidriver` (0xfc2e), 50 ms, les 1261 enregistrements
  (1260 × `Write_RAM` 0xfc4c, puis `Launch_RAM` 0xfc4e), chacun attendant son Command Complete, 250 ms, `HCI_Reset`,
  version et nom relus, puis extinction comme avant. Environ 30 s à 115200 bauds.
- Coupures : `b2l_bt=2` = étape 1 seule (sans micrologiciel), `b2l_bt=1` lecture seule, `b2l_bt=0` rien.
- Succès : `THE FIRMWARE RUNS (lmp_subver 0x1111 -> …)` et `Read_Local_Name` = « BCM4375B1 Beyond E43 Ant0 … ».

### Résultat observé au build 0212 : LE MICROLOGICIEL TOURNE
1261 enregistrements envoyés en 17 s. Après `Launch_RAM` : révision HCI **0 → 0xad**, nom
« BCM4375B1 Beyond E43 Ant0 [Baseline: 0173] ». `lmp_subver` reste 0x1111 sur cette puce, donc le test affichait à
tort « not confirmed ». Wi-Fi intact.

## 6. Étape 3 (build suivant) : la puce reste allumée et passe à bluetoothd
> **En pause (2026-11-14, demande du propriétaire)** : jamais flashée, rangée dans
> `~/Developer/s10-port/bluetooth-step3-parked/`. État et reprise : `bluetooth-etat.md`.
Lecture de bluetoothd 23G90 (désassemblage, lecture seule) :
- `getenv("BT_HCI_TRANSPORT")` (0x10029798c), format `transport:pilote:chemin` séparé par `:`. Transports connus :
  USB, H4DS, H4BC, **H4**, H5, APPLEBT, VIRT, PCIE, PCIESW, PCIESUN, APPLEPCIE ; pilotes : USB, **POSIX**, APPLEBT,
  VIRT, PCIE… Sans la variable, bluetoothd lit une plist (« BT_HCI_TRANSPORT not set »).
- Le pilote POSIX lit `nom,vitesse` (0x100192f90) ; nom par défaut **`com.apple.uart.sk.bluetooth`**, ouvert par
  `socket(PF_SYSTEM)` + `ioctl(CTLIOCGINFO)` : c'est un **kernel control**, pas un service IOKit ni AppleConvergedIPC.
- bluetoothd demande à BlueTool (XPC `com.apple.BlueTool`) `RunBuiltinScript` boot/init/deepsleep et lit la réponse
  `result` (0 = bon), `firmware`, `manufacturer`.

Ce qui change :
- **Critère corrigé** : la valeur comparée est `lmp_subver<<16 | révision HCI` ; succès = elle a changé.
- **Noyau (`B2LBluetooth.cpp`)** : niveau par défaut 4 = puce laissée allumée. Après le micrologiciel :
  `Write_BD_ADDR` (0xfc01) avec l'adresse synthétique de B2LPlatformExpert (administrée localement, pas celle du
  téléphone), relue par `Read_BD_ADDR` (seul le statut est journalisé). Puis kernel control
  `com.apple.uart.sk.bluetooth` (flux, non privilégié) et entrée de registre `bluetooth` (local-mac-address 6 octets,
  transport-speed OSData 4 octets = 115200, compatible `brcm,bcm4375`, `B2LBluetoothReady`). Un fil relaie en
  scrutation 1 ms : FIFO RX de l'UART → client, anneau TX de 16 Kio → UART.
- **Crochet d'exécution (`b2l_exec_bluetooth.h`)** : bluetoothd reçoit
  `BT_HCI_TRANSPORT=H4:POSIX:com.apple.uart.sk.bluetooth,115200` et `BT_BAUDRATE=115200` ; BlueTool reçoit
  `DYLD_INSERT_LIBRARIES=/usr/local/lib/b2l-bluetool-v1.dylib` si le fichier existe, et son pid est ajouté à la
  politique dyld d'AMFI.
- **Remplaçant de BlueTool (`userspace/b2l-bluetool.c`, construit par `scripts/build-bluetool-dylib.py`)** :
  interpose `getopt` ; quand BlueTool appelle `getopt("c:hf:F:R")` en tête de main, la dylib écoute
  `com.apple.BlueTool` et ne rend plus la main. boot/init : attend (jusqu'à 60 s) que le kernel control existe puis
  répond 0 (19 si la puce n'est jamais montée) ; tout le reste : 0. Ses lignes de journal passent par
  `setsockopt(0xb2)` sur le kernel control.
- **Graine (`b2l_bt_seed.h`)** : `b2l_bt_fetch()` puis publication de la dylib dans `/usr/local/lib` (SYSTEM).
  **Rien sur USERDATA.**
- Coupures : `b2l_bt_ios=0` (ni dylib ni variables, la puce reste quand même allumée), `b2l_bt=3` (micrologiciel
  puis extinction, comme 0212), `b2l_bt=2`, `b2l_bt=1`, `b2l_bt=0` comme avant.
- Risques connus : bac à sable de BlueTool (socket ou XPC refusés), plantage de bluetoothd à l'extinction (case +0x60
  de la table H4 vue par ios-pipa), 115200 bauds trop lent pour l'A2DP (921600 plus tard), appairages à
  synchroniser sur disque.

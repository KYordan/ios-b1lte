# Audio : revue de l'existant et plan (2026-11-13)

Mission : le son d'iOS 26 par les haut-parleurs du S10, puis par la prise jack (avec la détection du casque).
Prompt : `docs/prompt-session-audio.md`. Session audio, premier changement embarqué dans le build **0210** de la session Wi‑Fi.

## 1. Ce qui existe dans le port

- **Aucun pilote audio.** Seule trace : `b2l_exec_graphics_diag.h` (B1L-0052) bloque quatre synthétiseurs vocaux
  (KonaSynthesizer, MauiAUSP, MacinTalkAUSP, SiriAUSP) qui plantaient en boucle.
- Briques réutilisables :
  - `B2LInput.cpp` : transferts HSI2C (même IP Samsung que les amplis), mais sur un contrôleur que l'UEFI a déjà
    configuré (`r(0x60) && r(0x64)`) ;
  - `B2LWifi.cpp` : méthode « audit en lecture seule des portes CMU / QCH, puis écriture » et lecture des bancs GPIO ;
  - `B2LCPU.cpp` : sondes au démarrage (`b2l_gpu_probe`), mailbox ACPM (`b2l_mfd`) qui donnera accès au PMIC
    (alimentations du codec) ;
  - aucune entrée `conf/files` ni `KernelConfigTables.cpp` n'est nécessaire tant que le code tient dans un en-tête
    inclus par un fichier déjà construit (comme `B2LThermal.h`, `B2LACPM.h`).

## 2. Matériel du S10 (DTB live `device-s10/dump/live.dtb`, vérifié)

| Élément | Où | Détails |
| --- | --- | --- |
| ABOX (DSP audio) | `abox@18c50000` | sfr 0x18C50000, sysreg 0x18C10000, sram 0x18D00000 (0x59000), timer 0x18CE0000 ; 2 cœurs CA32 (PMU 0x2D80 / 0x2E00) ; firmware `calliope_sram.bin`, `calliope_dram.bin`, `calliope_sram_2.bin`… ; RDMA 0–11 0x18C51000+n·0x100, WDMA 0–7 0x18C53000+n·0x100, UAIF0–3 0x18C50500+n·0x10 ; IOMMU ; horloges PLL_AUD0/1, AUDIF, DMIC, BUS, CNT |
| Domaine d'alimentation | `pd-aud@15861900` (PMU) | `need_smc = 0x18c62000` : Linux appelle le monde sécurisé à l'allumage |
| CMU audio | CMU_AUD 0x18C00000 | dans pd-aud : ne pas lire tant que pd-aud est éteint |
| Broches UAIF | `pinctrl@18C60000` (gpb0–2) | dans pd-aud |
| **Haut-parleur** (bas) | CS35L40 `0x40` sur `hsi2c@109A0000` | `cs35l40-bot`, canal droit, reset **gph1-1**, IRQ gpm24 |
| **Écouteur** (haut) | CS35L40 `0x41`, même bus | `cs35l40-rcv`, reset **gph1-2** |
| Bus des amplis | `hsi2c@109A0000` = **USI17_I2C** (CMU_PERIC1 0x10800000) | 400 kHz, broches gph0-6 (SCL) / gph0-7 (SDA), pinctrl PERIC1 0x10830000 (gph0 0xE0, gph1 0x100) |
| Lien audio des amplis | **UAIF1**, format `dsp_a`, horloge bit inversée | TDM, comme la leçon d'ios-pipa |
| Codec (jack, micros) | CS47L93 sur `spi@10470000` (cs 0) | reset **gpg1-0** (PERIC0 0x10430000 + 0xA0), IRQ banc gpa2 (ALIVE), alims phandles 0x32c / 0x32d (PMIC, à identifier) ; UAIF0 = AIF1 (i2s), UAIF2 = AIF3 |
| Jack | `cirrus,accdet` du CS47L93 | `jd-use-jd2`, `hpd-pins <2 5 0 0>`, `micd-ranges`, sortie HPOUT2L/R |

Sources du noyau Samsung (LineageOS, clone partiel `ref-samsung/k9820`, lues avec `git show`) :
`sound/soc/codecs/cs35l41.c` (gère aussi `cirrus,cs35l40`, DEVID 0x00000000 = **0x35A40**, REVID 0x4),
`sound/soc/codecs/{madera,cs47l92}.c`, `drivers/mfd/madera-core.c`, `drivers/extcon/extcon-madera.c`,
`sound/soc/samsung/abox/*` (dont `abox_rdma.c`, `abox_soc.c`, `abox_core.c`), `sound/soc/samsung/beyond_madera.c`.
Registres CMU : `ref-kernel/k9820/drivers/soc/samsung/cal-if/exynos9820/cmucal-sfr.c`.

## 3. Références lues

- **HakiOS** `drivers/audio/` + `docs/driver-review/sound.md` : `/dev/hakios-audio` nourri par CoreAudio, garde des
  haut-parleurs (`hakios.guard.c` : passe-haut 200 Hz, limites de puissance 0,2 / 3 / 60 s, limiteur de crête),
  sondes par boot-arg (`speaker_probe=1` lit seulement). Revue : 16 défauts, dont S-01 (volume non borné → amplis au
  maximum, garde aveugle) et S-04/S-05 (amplis laissés allumés après un échec I2C) : **à corriger avant de reprendre
  ce code.**
- **ios-pipa** (`NOTES.md`, étapes AUDIO A–E, patchs 0138/0139) : côté iOS, un `AudioServerPlugIn`
  (`HakiosAudio.driver` dans `/System/Library/Audio/Plug-Ins/HAL`, sortie « Speaker » → `/dev/hakios-audio`),
  `VirtualAudio.plugin` remis en place, rien d'autre à patcher dans mediaserverd. Leurs CS35L41 (cousins des nôtres)
  sont pilotés en **I2C bit-bang par GPIO**, amplis allumés 42–46 ms après le premier son, éteints après 3 s de
  silence. Leur DSP (Qualcomm) ne nous sert pas.

## 4. Plan

Un changement par build jusqu'à 0212, puis lots groupés (accord du propriétaire, 2026-11-13) ; chaque étage conditionné au précédent ; aucun registre d'un composant inconnu écrit
avant d'avoir relu et noté sa valeur de départ.

| Étape | Build | Contenu | Réussi si |
| --- | --- | --- | --- |
| **A0** | **0210** | Sonde **en lecture seule** (`B2LAudio.h`) : PMU pd-aud et cœurs ABOX, portes CMU_CMU (AUD, PERIC1), portes/QCH CMU_PERIC1 (GPIO, USI17_I2C), bancs gph0/gph1/gpg1/gpa2, registres du contrôleur HSI2C si son horloge est ouverte | ligne `B2L AUDIO: verdict` dans le journal, aucun blocage |
| A1 | 0211 | Lire l'identité des deux CS35L40 (DEVID/REVID, lecture I2C seule) en I2C simulé par GPIO sur gph0-6/7 (méthode ios-pipa), reset gph1-1/2 relâché le temps de la lecture, puis broches remises comme au départ | `DEVID=0x35a40` pour 0x40 et 0x41 |
| A2 | 0212 | pd-aud est déjà allumé (0210/0211) : lire en lecture seule CMU_AUD (PLL_AUD0, horloges ABOX/UAIF1/GPIO_AUD), puis l'ABOX (version, routage, SPUS, UAIF0–3, RDMA0) et les broches AUD gpb0–2 si leurs horloges sont ouvertes | ligne `abox-verdict` |
| A3 | 0212 (lot groupé 1) | Régler les deux amplis par I2C (errata A0, OTP, ASP dsp_a esclave 4×32 bits, 48 kHz, PLL sur SCLK 6,144 MHz, boost 4100 mA), **étage de sortie et boost coupés, volume en sourdine** ; chaque écriture : avant / écrit / relu, journalisé | `amp-prep … \| ready` pour bot@40 et rcv@41 |
| A4 | 0212 (même lot) | Horloge UAIF1 6,144 MHz sortie de l'ABOX (diviseur CMU, mux, broches gpb1 fn2, CTRL0/CTRL1, QCH), puis GLOBAL_EN des amplis : PUP_DONE prouve qu'ils voient l'horloge ; puis tout est défait et les amplis remis en reset | `uaif1-verdict amps-clocked-by-uaif1 \| abox-cmu-pins-undone=1` |
| A5 | lot 2 | Premier bip : RDMA0 → SPUS → SIFS0 → UAIF1 programmés par l'AP (sans Calliope), 1 kHz −10 dB 0,7 s sur bot@40 seul ; A3 recopie aussi l'OTP d'usine des amplis. Voir §10 | bip audible au haut-parleur du bas, `beep played=1` |
| B1 | | `/dev/b2l-audio` (sur le modèle de `hakios.audio.c`, défauts S-01/S-02/S-04/S-05 corrigés) + garde des haut-parleurs | écriture PCM → son |
| B2 | (USERDATA, demander) | `AudioServerPlugIn` sur le modèle de HakiosAudio.driver, VirtualAudio remis en place | clic du clavier, sonnerie, musique |
| C1 | | Jack : détection JD2 par le CS47L93 (SPI), bascule de sortie UAIF0/HPOUT2 | branchement vu, son au casque |
| C2 | | Volume par les boutons, sans crépitement | |

Questions ouvertes (à trancher par les journaux, pas par supposition) :
1. L'ABOX peut-il sortir un flux RDMA → UAIF sans son firmware Calliope ? À lire dans `abox_rdma.c` / `abox_cmpnt_v20.c`
   avant A3. Sinon, charger `calliope_*.bin` depuis la partition `vendor` (lecture seule depuis TWRP).
2. Faut-il le firmware de protection des CS35L40 (`cs35l40-dsp1-spk-prot.wmfw` dans `vendor`) ? Sans lui : mode
   bypass + garde logicielle à niveau bas.
3. Alimentations du CS47L93 (phandles 0x32c / 0x32d) : quel LDO du PMIC, et sont-elles allumées par S-Boot ?

## 5. Étape A0 (build 0210)

Fichiers : `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` (nouveau) et `xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp`
(deux lignes : `#include "B2LAudio.h"` après `B2LThermal.h`, `b2l_audio_probe();` après `b2l_gpu_probe();`).

- **Aucune écriture** de registre, de stockage ou de GPIO. Coupure : boot-arg `b2l_audio=0`.
- Ordre de lecture pour ne jamais toucher un bloc sans horloge : PMU et CMU_CMU (toujours allumés) ; CMU_PERIC1
  seulement si la porte `GATE_CLKCMU_PERIC1_BUS` est ouverte ; broches PERIC1 seulement si la porte et le QCH de
  `GPIO_PERIC1` sont ouverts ; contrôleur HSI2C seulement si ses portes IPCLK/PCLK et son QCH sont ouverts. Rien
  dans pd-aud n'est lu.
- Testé sur le Mac avec une mémoire simulée (portes ouvertes, bus PERIC1 fermé, QCH I2C fermé, coupure) :
  test temporaire hors dépôt (le code est inclus tel quel, `ml_io_map` et le journal simulés), 4 cas OK, `clang++ -Wall -Wextra` sans avertissement. Pas compilé dans le noyau par cette
  session (c'est la session Wi‑Fi qui construit 0210).

Lignes à chercher (préfixe `B2L AUDIO:`) :
```
B2L AUDIO: pd-aud conf=… status=… (on|off) core0=…/… core1=…/…
B2L AUDIO: cmu_top aud_cpu=… peric1_bus=… peric1_ip=…
B2L AUDIO: gpg1(codec reset=0) con=… dat=…
B2L AUDIO: gpa2(codec irq) con=… dat=…
B2L AUDIO: cmu_peric1 …            (si le bus PERIC1 est ouvert)
B2L AUDIO: gph0(amp scl=6 sda=7) … / gph1(amp reset bot=1 rcv=2) …   (si GPIO_PERIC1 ouvert)
B2L AUDIO: hsi2c@109a0000 …         (si USI17_I2C ouvert)
B2L AUDIO: verdict pd-aud=… amp-bus=… scl_fn=… sda_fn=… amp_reset bot=fnX/Y rcv=fnX/Y codec_reset=fnX/Y i2c_regs=…
```
Lecture du verdict : `amp-bus` = `peric1-bus-gated` | `peric1-gpio-gated` | `i2c-gated` | `i2c-untimed` (contrôleur
non réglé par S-Boot) | `i2c-timed` (réglé). `fnN/D` = fonction de la broche (0 entrée, 1 sortie, 2 fonction I2C) / niveau.
Échec : plus aucune ligne après `B2L AUDIO: cmu_top` (lecture bloquée) → reflasher 0209 et passer `b2l_audio=0`.

## 6. Résultats

### Build 0210 (journal `beyond2lte-ios/out/logs/0210-boot.txt`)

La sonde a tourné jusqu'au verdict, sans blocage. Trois lignes sont illisibles : `b2l_early_trace` écrit caractère par
caractère et `B2L THERMAL` écrivait en même temps sur un autre CPU (corrigé en A1 : `b2l_diag_record`, une ligne
réservée d'un bloc). Ce qui est lisible :

| Mesure | Valeur | Lecture |
| --- | --- | --- |
| pd-aud | conf=00000001, `on` | le domaine audio est **allumé** par S-Boot |
| CMU_PERIC1 GPIO | pclk=00200000, qch=003f0002 | porte ouverte, horloge forcée |
| hsi2c@109a0000 | ctl=00000008 conf=980110ff fs1=0180ff00 fs2=030003f8 **usi_con=00000001** | contrôleur cadencé mais **USI en reset** |
| gph0-6/7 (SCL/SDA) | fonction 0 (entrée) | broches **pas** en fonction I2C (3) |
| gph1-1/2 (resets amplis) | fonction 0, niveau 0 | amplis **en reset** (ligne tirée bas) |
| gpg1-0 (reset codec) | fonction 1, niveau 0 | S-Boot tient le CS47L93 en reset |

Conclusion : S-Boot n'a pas touché aux amplis. Valeurs de départ notées pour A1 : gph0 et gph1 en entrée sur les
quatre broches. Les registres CON/DAT complets de gph0/gph1 n'étaient pas lisibles (mélange) : A1 les relit et les
journalise avant d'écrire.

## 7. Étape A1 (build 0211)

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` seul (B2LCPU.cpp inchangé depuis 0210).

- Boot-arg `b2l_audio` : 0 coupé, 1 = A0 seul (lecture), **2 = défaut** = A0 + A1.
- A1 ne démarre que si les quatre broches sont encore dans l'état mesuré en 0210 (entrées), sinon
  `amp-id skipped`.
- Écritures, toutes dans le banc GPIO PERIC1 (composant connu), uniquement sur les bits de ces quatre broches :
  verrous DAT de gph0-6/7 à 0 (une seule écriture) ; si les deux lignes sont hautes une fois relâchées (pull-ups de
  la carte), reset gph1-1/2 en sortie bas 1,1 ms puis haut 2,1 ms (séquence `cs35l41-core.c`), puis I2C simulé : une
  ligne est soit en entrée (relâchée), soit en sortie à 0 ; **jamais poussée à l'état haut**. Lectures : DEVID (0x0)
  et REVID (0x4) de 0x40 puis 0x41. **Aucune écriture I2C vers les amplis**, qui ne sont donc jamais activés.
- Fin : gph1 puis gph0 reprennent leurs bits CON/DAT de départ (les amplis retournent en reset), contrôle relu
  (`pins-restored=1`).
- Si les lignes ne sont pas hautes au repos : rien n'est piloté (`bus-not-idle`).
- Testé sur le Mac avec un bus I2C simulé et deux CS35L40 simulés (qui ne répondent que reset relâché) : amplis
  présents, bus tenu bas, amplis sans alimentation, mode lecture seule, broches hors état de départ. Le test a trouvé
  un bug corrigé avant envoi : DAT relit le niveau des broches en entrée, deux écritures lecture-modification
  successives auraient remis un 1 dans le verrou de SCL.

Ligne à chercher :
```
B2L AUDIO: amp-id idle scl=1 sda=1 | bot@40 err=0 id=00035a40 rev=… err=0 | rcv@41 err=0 id=00035a40 rev=… err=0 | both-cs35l40 | pins-restored=1
```
`err` : 0 réussi, 1 adresse non acquittée (ampli muet, sans alimentation ou toujours en reset), 2–5 octet de
registre non acquitté, 6 relecture refusée, 7 SCL bloqué bas, 9 non tenté. Verdicts : `both-cs35l40`, `no-answer`,
`bus-not-idle`, `scl-stuck`.

### Build 0211 (journal `beyond2lte-ios/out/logs/0211-boot.txt`)

Lignes entières (plus de mélange). **Les deux amplis répondent** : `bot@40` et `rcv@41` DEVID `0x35a40`, REVID
`0xa0` (CS35L40 révision A0), broches remises à l'état de départ (`pins-restored=1`). Le Wi‑Fi est resté connecté.

Valeurs de départ complètes (avant toute écriture) :

| Registre | Valeur | Lecture |
| --- | --- | --- |
| pd-aud | conf=1, status=1 | allumé |
| CMU_CMU aud_cpu / peric1_bus / peric1_ip | 00200000 ×3 | portes ouvertes |
| gph0 (bus amplis) | con=00000000 dat=c0 pud=11111111 drv=22222222 | entrées, pull-down interne (1) mais lignes hautes : pull-ups de la carte plus forts |
| gph1 (resets amplis) | con=00000000 dat=00 pud=00011111 | entrées tirées bas : amplis en reset |
| gpg1 (reset codec) | con=10000001 dat=d6 pud=01000001 | gpg1-0 sortie à 0 : CS47L93 en reset |
| gpa2 (IRQ codec) | con=00000000 dat=14 pud=11101111 | |
| USI17_I2C | ipclk/pclk 00200000, qch 003f0002, mux 00000010 | horloge présente ; contrôleur `usi_con=1` (en reset) |

Pour plus tard (vrai bus I2C) : le DTB veut gph0-6/7 en fonction 3 sans pull (`pud 0`) ; aujourd'hui le pull-down
interne est actif.

## 8. Étape A2 (build 0212)

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` seul (B2LCPU.cpp inchangé).

- Boot-arg `b2l_audio` : 0 coupé, 1 A0, 2 A0+A1, **3 (défaut) A0+A1+A2**. A1 continue de tourner à chaque démarrage
  (identité des amplis, broches remises).
- **Aucune écriture.** CMU_AUD (0x18C00000, dans pd-aud) n'est lu que si pd-aud est allumé et la porte AUD_CPU de
  CMU_CMU ouverte ; les registres ABOX (0x18C50000) seulement si la porte ACLK et le QCH ACLK de l'ABOX sont ouverts ;
  les broches AUD (0x18C60000) seulement si la porte et le QCH de GPIO_AUD le sont.
- Registres ABOX lus (`abox_soc.h`, v2) : IP_INDEX, VERSION, SYSPOWER_CTRL/STATUS, SYSTEM_CONFIG0, DYN_CLOCK_OFF(1),
  QCHANNEL_DISABLE, ROUTE_CTRL0–2, SPUS_CTRL0/1, UAIF0–3 CTRL0/CTRL1/STATUS, RDMA0 CTRL0/CTRL1/STATUS (0x30). Aucun
  FIFO ni registre à lecture destructive.
- Testé sur le Mac (registres simulés) : chemin complet, horloge ABOX coupée (ABOX non lu), pd-aud éteint (rien du
  domaine AUD lu), niveau 2 (pas d'A2), plus les cas A0/A1 précédents.

Lignes à chercher :
```
B2L AUDIO: cmu_aud pll_aud0=… mux_cpu_user=… div_bus=… div_uaif1=… aclk=… qch_aclk=… bclk_uaif1=… qch_bclk1=… gpio_pclk=… qch_gpio=… sysreg_pclk=…
B2L AUDIO: abox ip=… version=… syspower=…/… config0=… dyn_clk_off=…/… qch_disable=… route=…/…/… spus=…/…
B2L AUDIO: abox uaif0=…/…/… uaif1=…/…/… uaif2=… uaif3=… rdma0=…/…/…
B2L AUDIO: gpb0(mclk=0) … / gpb1(uaif1 0..3) … / gpb2 …
B2L AUDIO: abox-verdict abox_regs=0|1 aud_pins=0|1 uaif1_pins_fn=NNNN bclk_uaif1=open|gated
```
Échec : plus aucune ligne après `cmu_aud` (lecture bloquée) → `b2l_audio=2`.

Ce que A2 décide pour A3 (premier bip) : si l'ABOX répond (version lisible) et PLL_AUD0 tourne, A3 programme
RDMA → UAIF1 directement ; sinon il faut d'abord la séquence d'horloges de cmucal (PLL_AUD0, diviseurs UAIF1).

### Correctif à A2 (dans le lot 0213)

A2 lisait les registres ABOX dès que la porte ACLK et son QCH étaient ouverts. Or l'horloge du bus ABOX
(DIV_CLK_AUD_BUS) divise PLL_AUD0 : si cette PLL était éteinte, la lecture pouvait bloquer le bus. Depuis 0213, A2 ne
lit l'ABOX que si PLL_AUD0 a ENABLE (bit 31) **et** STABLE (bit 29). La ligne `cmu_aud` donne aussi la fréquence
calculée de la PLL (0831x : 26 MHz × (M + K/65536) / (P << S)), DIV_AUDIF et MUX_UAIF1. A2 passe maintenant **avant**
A1 (toujours en lecture seule) pour que A4 connaisse l'état de l'ABOX.

Si 0212 a bloqué après la ligne `cmu_aud` : c'était ce cas, corrigé par 0213.

## 9. Lot groupé A3 + A4 (build 0213)

Accord du propriétaire : regrouper les étapes audio, chacune conditionnée à la précédente. Fichier :
`xnu-overlay/iokit/Kernel/arm/B2LAudio.h` seul (B2LCPU.cpp inchangé).

Boot-arg `b2l_audio` : 0 coupé, 1 A0, 2 +A1, 3 +A2, 4 +A3, **5 (défaut) +A4**.

**Ordre et conditions**

1. A0 (lecture), puis A2 (lecture, garde PLL ci-dessus).
2. A1 : reset des amplis relâché, DEVID/REVID. A3 seulement si les deux répondent 0x35a40.
3. A3, pour chaque ampli (code de `cs35l41.c` pour cette carte) :
   - errata rév. A0 entre les clés 0x40 (0x5555/0xAAAA … 0xCCCC/0x3333) : 0x3854, OTP_TRIM_30 0x7418, 0x3014,
     BSTCVRT_DCM_CTRL 0x381C, 0x54, IRQ1_DB3, IRQ2_DB3 ;
   - attente OTP_BOOT_DONE (IRQ1_STATUS4 bit 1, ≤ 100 ms) ;
   - BSTCVRT_PEAK_CUR = 0x42 (4100 mA, DTB), SP_HIZ_CTRL = 3, SP_FORMAT esclave dsp_a SCLK inversé largeur 32/32,
     FRAME_RX_SLOT (bot = canal droit → RX1 slot 1 ; rcv → RX1 slot 0), GLOBAL_FS 48 kHz, SP_RATE_CTRL 0x28,
     **PWR_CTRL2 : AMP_EN = 0 et BST_EN = 0**, volume PCM = sourdine (0x4CF), SP_ENABLES ASPRX1 ;
   - PLL_CLK_CTRL dans l'ordre de `set_sysclk` (boucle ouverte, REFCLK 0x28, arrêt, source SCLK, boucle fermée,
     marche) ;
   - chaque registre : lu avant, écrit, relu ; journalisé `reg:avant>après`.
   - `otp_unpack` (recopie des réglages d'usine OTP) n'est **pas** porté : inutile tant que l'étage de sortie reste
     coupé ; à faire avant A5.
4. A4 seulement si A3 est `ready` sur les deux amplis et A2 a trouvé ABOX + broches AUD + PLL verrouillée, et si
   DIV_AUDIF donne une fréquence multiple de 6,144 MHz à 0,1 % près (sinon rien n'est écrit) :
   - PMU 0x1920 bit 11 (ABOX_PAD_NORMAL, `abox.c` pour 9820) mis à 1 s'il ne l'est pas — seul réglage laissé en place,
     comme le fait `abox.c` ;
   - DIV_UAIF1 = AUDIF / 6,144 MHz − 1, MUX_UAIF1 = diviseur, porte BCLK_UAIF1 ouverte si elle était forcée fermée ;
   - gpb1-0..3 en fonction 2 ;
   - UAIF1 CTRL1 = 0x1B4DF7DF (dsp_a, IB_NF, 4 × 32 bits, `abox_if.c`), CTRL0 MODE maître, QCHANNEL_DISABLE bit 3
     (horloge toujours active, `set_tristate(0)`), puis SPK_ENABLE (aucune source : des zéros) ;
   - pour chaque ampli : patch de mise sous tension puis GLOBAL_EN ; attente PUP_DONE (IRQ1_STATUS1 bit 24) ; état IRQ1
     1–4 avant/après ; puis GLOBAL_EN = 0, patch d'arrêt, attente PDN_DONE (bit 23) ;
   - tout ce qu'A4 a écrit dans l'ABOX, le CMU et les broches reprend sa valeur de départ (vérifié).
5. Fin : broches de reset et du bus remises comme au départ, les amplis retournent en reset.

**Ce que ce lot ne fait pas** : aucun son. L'étage de sortie (AMP_EN) et le boost restent coupés du début à la fin.
Il prouve que les amplis sont réglés et qu'ils voient l'horloge UAIF1 de l'ABOX.

Réserve : PUP_DONE pourrait ne pas venir si l'ampli l'attend avec AMP_EN = 1 (Samsung met AMP_EN avant GLOBAL_EN).
Dans ce cas, `amps-not-clocked` serait un faux négatif : les valeurs brutes IRQ1_STATUS1–4 « after » permettent de
trancher (horloge absente → erreurs ou rien ; PLL verrouillée → bits d'état PLL dans STATUS3).

Testé sur le Mac (bus I2C et amplis simulés, `t/test2.cpp`) : chemin complet ; horloge qui n'arrive pas aux amplis ;
PLL_AUD0 éteinte (ABOX ni lu ni écrit) ; DIV_AUDIF incompatible (rien écrit) ; niveaux 4, 3, 2, 1 ; autre révision
d'ampli ; bus tenu bas ; amplis sans alimentation ; broches pas dans l'état de départ ; horloge ABOX coupée ; pd-aud
éteint. Vérifié à chaque fois : aucune ligne I2C tirée à 1, broches et registres AUD revenus à leur valeur de départ,
AMP_EN jamais à 1, volume toujours en sourdine.

Lignes à chercher (`grep -a "B2L AUDIO"`) :
```
B2L AUDIO: abox-verdict pll_aud0=locked abox_regs=1 aud_pins=1 …            (A2)
B2L AUDIO: amp-id … | both-cs35l40                                          (A1)
B2L AUDIO: amp-errata bot@40 … readback-same i2c=ok                         (A3)
B2L AUDIO: amp-prep bot@40 otp_boot=1 reg:avant>après … | ready             (A3, idem rcv@41)
B2L AUDIO: uaif1-on pad=…>… div_uaif1=…>… … bclk=6144…                      (A4)
B2L AUDIO: amp-power bot@40 io=0 pup_done=1(Nms) pdn_done=1(Nms) irq1 before=… after=… err=00000000
B2L AUDIO: uaif1-verdict amps-clocked-by-uaif1 | abox-cmu-pins-undone=1      (A4)
B2L AUDIO: amps-off pins-restored=1 …                                       (fin)
```
Échecs prévus : `| i2c-error` ou `| readback-mismatch` (A3, A4 non lancé), `uaif1 skipped (…)` (condition non
remplie, rien écrit), `amps-not-clocked` (voir la réserve). Blocage → `b2l_audio=4` (sans A4) ou `=3` (lecture seule).

**Pour A5 (premier son)** : il manque la source des données. Le RDMA de l'ABOX (adresse du tampon, format,
déclenchement) est programmé par le firmware Calliope via IPC, pas par l'AP. Deux voies :
(a) démarrer Calliope (`calliope_*.bin` de la partition vendor) et parler son IPC (`abox_ipc.h`) ;
(b) programmer RDMA0 directement (BUF_STR/END/OFFSET connus dans `abox_soc.h`, SysMMU de l'ABOX en contournement) et
la chaîne SPUS → UAIF1 (ROUTE_CTRL0, SPUS_CTRL0/1), en partant des valeurs lues par A2.
Besoin pour trancher : `calliope*.bin`, `cs35l40*.wmfw/.bin` et `mixer_paths*.xml` copiés en lecture seule depuis vendor.

## 10. Lot 2 : premier bip (A5)

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` (seul). Défaut `b2l_audio=6` ; `=5` = lot 1 sans bip.

**Voie choisie : (b), l'AP programme le RDMA lui-même.** Les accesseurs du firmware Calliope
(`vendor-audio/calliope_dram.bin`, désassemblé) donnent les champs que le firmware écrit pour démarrer un RDMA :
VOL_FACTOR, BUF_STR, BUF_END = début + taille, STR_POINT, BUF_OFFSET, BURST 8, FORMAT, bit 5 = 1, puis ENABLE. Les SBANK
sont en octets (STR 13:4, SIZE 29:20 en unités de 16 octets) ; le mixeur par défaut du firmware est à 0x3000, 512 octets.
SYSPOWER_CTRL bit 0 est la demande « DRAM allumée » (attendre STATUS bit 0).

Ajouts :
- **A3** : après OTP_BOOT_DONE, recopie de l'OTP d'usine (`cs35l41_otp_unpack` de Samsung) : clé de test, OTPID, 32 mots
  OTP_MEM, contrôle d'en-tête, 99 champs écrits par lecture-modification-écriture. Valeurs jamais journalisées (0x17040/44
  = lot et plaquette de la puce). Ligne `amp-otp … result=ok writes=N`. Échec d'en-tête ou d'OTPID → pas de bip, le reste
  continue ; erreur I2C → ampli non prêt.
- **A5**, dans A4 une fois UAIF1 lancé :
  1. RDMA0 doit être arrêté au départ (sinon `beep skipped (rdma0 busy …)`, rien écrit) ; tampon de 3840 octets contigu
     sous 4 Go (240 trames × 4 voies × 32 bits, 1 kHz à −20 dBFS dans la voie 1 = droite = bot), cache nettoyé ;
  2. 13 registres ABOX lus puis écrits (SYSPOWER, SBANK RDMA0/mixeur, SPUS_CTRL0/1, SIFS0_CNT, ROUTE_CTRL0, RDMA0) ; la
     ligne `beep-path` donne chaque `avant>après` ; le pointeur de lecture du RDMA doit bouger (`rdma-moving`) ;
  3. bot@40 seul, avant GLOBAL_EN, dans l'ordre DAPM : source PCM = ASPRX1, CLASS H, sortie non coupée, AMP_EN ; **boost
     coupé** (VBST = batterie par l'inductance, comme le chemin écouteur) ; rcv@41 reste AMP_EN = 0 ;
  4. après PUP_DONE sans bit d'erreur : volume −10 dB avec rampe, 700 ms, bits d'erreur IRQ1 lus toutes les 100 ms
     (erreur → sourdine immédiate), sourdine, 60 ms, GLOBAL_EN coupé, AMP_EN = 0 ;
  5. RDMA arrêté (UAIF1 tourne encore), attente de PROGRESS = 0, les 13 registres remis à l'envers (SYSPOWER en dernier),
     tampon libéré seulement si le RDMA est à l'arrêt ; puis A4 défait comme avant, amplis remis en reset.

Lignes à chercher en plus de celles du §9 :
```
B2L AUDIO: amp-otp bot@40 otpid=… result=ok writes=N                        (A3, idem rcv@41)
B2L AUDIO: beep-path ring_pa=… dram=ok 0010:…>… … 1000:…>… status=…>… | rdma-moving
B2L AUDIO: beep-amp bot@40 04c00:…>00000008 0201c:…>…10 02024:…>… 02018:…>…01 | ready
B2L AUDIO: beep played=1 ms=700 vol_up=1 vol_down=0 err=00000000 … | rdma-idle-ring-freed=1 abox-path-restored=1
```
À l'oreille : un bip de 0,7 s à 1 kHz, faible, au haut-parleur du bas, pendant le démarrage.

Échecs prévus :
- `dram=TIMEOUT` ou `rdma-NOT-moving` : l'ABOX ne lit pas le tampon (SysMMU active, horloge, champ inconnu) ; rien n'est
  envoyé aux amplis, tout est remis ;
- `played=0 err=…` : erreur d'ampli pendant le bip, sourdine immédiate ;
- `played=1` sans son : le RDMA tourne peut-être une seule fois (comparer `rdma_status_end` à `status`), ou les données
  n'arrivent pas sur la voie 1 ;
- `rdma-idle-ring-freed=0` : le RDMA ne s'arrête pas ; tampon gardé (4 Ko perdus), registres quand même remis ;
- blocage → `b2l_audio=5` (lot 1 seul), puis `=4`.

Risques connus : bit 4 de RDMA CTRL0 de sens inconnu (laissé à sa valeur) ; SysMMU de l'ABOX supposée en contournement
(jamais activée depuis le reset), sinon défaut de page DMA que XNU ne traite pas ; ampli sans firmware de protection (DSP),
d'où le volume bas, la durée courte et le boost coupé.

Testé sur le Mac (`t/test3.cpp`, amplis, ABOX, RDMA et DMA simulés), 11 cas : bip complet ; OTPID 1 et 2 comparés à
l'algorithme Samsung recopié tel quel ; niveau 5 sans bip ; RDMA immobile ; DRAM refusée ; erreur d'ampli pendant le bip ;
en-tête OTP faux ; OTPID inconnu ; RDMA déjà occupé ; RDMA qui ne s'arrête pas ; pas de mémoire contiguë ; amplis sans
horloge. Vérifié à chaque fois : boost jamais allumé, AMP_EN et volume −10 dB seulement sur bot pendant le bip, registres
ABOX/CMU/broches revenus à leur valeur de départ, amplis remis en reset, lignes < 640 octets.

## 11. Lots 3 et 4 : iOS joue le son (préparé, build après B1L-0270)

**Fichiers**
- `xnu-overlay/iokit/Kernel/arm/B2LAudio.h` : lot 3 (A6).
- `xnu-overlay/bsd/kern/b2l_audio_seed.h` (nouveau) : lot 4, côté noyau.
- `b2l_userspace_seed.h` : 2 lignes (`#include` + `b2l_audio_seed(ctx)` après `b2l_bt_seed`).
- Plug-in HakiOS (MIT) : `audio/plugin/` → `build-audio/b2l-audio-v1/` → SYSTEM:/b2l-audio-v1/.

**Boot-arg `b2l_audio`** : … 5 = A4 sans chemin · 6 = bip audible de 0213 · **7 (défaut) = A6, silencieux**.
- Autres interrupteurs : `b2l_audio_max` (plafond en 1/8 dB sous 0 dB, défaut 48 = −6 dB, minimum 24) et `b2l_audio_ios=0` (pas de /dev, pas de plug-in).

**A6 (lot 3)**
- Même vérification qu'en 0213 (chemin ABOX, ampli prêt, RDMA qui avance), mais l'anneau reste à zéro : aucun bip.
- Si tout est bon, le chemin reste ouvert : RDMA0 → SIFS0 → UAIF1 tourne en silence. Les deux amplis restent alimentés, bot en veille (GLOBAL_EN = 0), rcv jamais allumé.
- Anneau : 65 280 o = 4080 trames × 4 voies × 32 bits (85 ms), période 4080 o (moins de 4 Ko, comme abox_rdma_pointer). Mono (L+R)/2 dans la voie 1 (bot).
- `b2l_audio_sink()` écrit 1024 trames (21 ms) devant le RDMA, sans jamais attendre ; ce qui ne rentre pas est perdu.
- Resynchronisation dans trois cas : premier appel ; écart trop petit ou trop grand ; RDMA qui a dépassé l'écrivain pendant un silence. Ce dernier cas est détecté par le fil, qui suit l'avance de l'écrivain toutes les 10 ms.
- Fil noyau toutes les 10 ms :
  - remet à zéro ce qui a été joué ;
  - allume bot quand un son non nul arrive (ordre DAPM de Samsung, patch PUP, GLOBAL_EN, PUP_DONE, puis volume avec rampe) ;
  - l'éteint après 3 s de silence ;
  - applique le volume d'iOS sous le plafond ;
  - lit les erreurs toutes les 100 ms.
- Erreur d'ampli : coupé pendant 30 s. Au bout de 3 erreurs, le haut-parleur reste coupé jusqu'au reboot. Boost toujours coupé.
- Journal : `ios-ready`, `amps-kept`, `ios-play`, `ios-stop`, `ios-volume`, `ios-fault`.

**Lot 4**
- `/dev/hakios-audio` (0666) : write = s16 stéréo 48 kHz → `b2l_audio_sink` ; ioctl 'r' 1 = volume, 'r' 2 = ligne de journal du plug-in (`B2L AUDIO HAL:`, 400 lignes au plus).
- Le plug-in HakiOS, inchangé, est publié une fois par le noyau dans `/System/Library/Audio/Plug-Ins/HAL/HakiosAudio.driver/`. Rien sur USERDATA.

**Testé sur l'hôte** (`audio/tests/test4.cpp`, `run.sh`, ASan + UBSan) :
- niveau 7 : chemin gardé sans aucun son, service démarré ;
- écriture dans la voie 1, avance du RDMA, écritures trop grosses ;
- zéros derrière le RDMA, volume et sourdine, arrêt après 3 s ;
- trou puis resynchronisation, 3 erreurs donnant l'arrêt définitif, plafond ;
- RDMA immobile et amplis sans horloge : tout est défait ;
- niveau 6 : bip de 0213 puis tout défait ; niveaux 5, 3, 0 ; bus bloqué ; amplis sans courant.

**Risques**
- VirtualAudio d'iOS 26 peut ne pas router vers un appareil nommé « Speaker » d'un plug-in tiers.
- Le bac à sable d'audiomxd peut refuser /dev/hakios-audio (b2l_sandbox est permissif, à confirmer).
- Le chemin reste allumé en permanence : un peu de consommation.

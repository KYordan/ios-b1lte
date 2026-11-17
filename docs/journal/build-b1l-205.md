# B1L-0205 : télémétrie de trap sans kexts

## Pourquoi
Le gel à l'ouverture d'une app avec Wi-Fi (0204) est une panique noyau : la télémétrie de trap parcourt
`gLoadedKextSummaries`, qui vaut NULL ici (aucun kext chargé). Voir `build-b1l-204.md`.

## Changements (hors périmètre Wi-Fi, cause racine)
- `libkern/c++/OSKext.cpp` (sauvegarde `../OSKext.cpp.204bak`) : les boucles de `foreachKextInBacktrace`,
  `summaryForAddress` et `findSummary` testent d'abord `gLoadedKextSummaries`. Sans liste, elles ne trouvent rien.
- `osfmk/kern/trap_telemetry.c` (sauvegarde `../trap_telemetry.c.204bak`) : `B2L TRAPTEL: type N code 0x…` à chaque
  enregistrement soumis, pour savoir quel trap déclenche la télémétrie (diagnostic, à retirer ensuite).

## Résultat
Plus de plantage : Stocks reste sur son écran de chargement, Météo se lance et Safari ne plante plus, mais rien ne charge (aucune donnée reçue par les apps). Prochaine étape : lire le journal (battement RX/TX, `B2L TRAPTEL`) pour savoir où les paquets s'arrêtent : envoi, réception, DNS ou TCP.

### Lecture du journal (`out/logs/0205-boot.txt`, USERDATA `out/live/ios26-userdata-net-205.img`)
- Le réseau marche au niveau IP : `en0` a envoyé 8 145 trames (1,1 Mo) et en a reçu 21 680 (29 Mo). DNS
  (mDNSResponder) répond, 123 négociations TLS 1.3 aboutissent. 744 trames sortantes ont été perdues pendant le pic
  (anneau d'envoi plein), ce que TCP rattrape.
- `B2L TRAPTEL` : uniquement type 4 code 0xff00, un piège de télémétrie douce, sans gravité. Le log est retiré en 0206.
- Côté iOS (tracev3 décompressés avec `scratchpad/tv3.py`) : le trousseau échoue partout,
  `ks_crypt: e00002c7 failed to 'oe' item (class 6..11)` : AppleKeyStore refuse d'emballer les clés (sélecteurs 10/11).
  Aussi 22 refus TLS « certificate unknown » avec `TemporalValidity` sur leaf/ca1/root (non expliqué : l'heure est
  bonne, fuseau US/Pacific), des délais `NSURLErrorDomain -1001`, et MobileActivation sans dossier d'activation.

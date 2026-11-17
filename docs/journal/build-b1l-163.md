# Build 163 : connexion Wi-Fi (chemin de données BCM + WPA2 côté hôte)

## Diagnostic

Jusqu'au 162, `join` du pilote BCM4375 répond « not yet » (EOPNOTSUPP), et aucune trame ne passe (`transmit` les jette). Le firmware Android n'a pas de supplicant intégré (`sup_wpa` = BCME_UNSUPPORTED, build 151) : la poignée de main WPA2 doit se faire sur l'hôte.

La requête `ASSOCIATE` d'iOS 26 fait **908 octets** (IO80211 0x25d48006c : type 20, longueur 0x38c). Son début a la disposition 16.5 (mode à +4, longueur du SSID à +12, clé à +0x48). `set_join` lit ces 136 premiers octets ; à vérifier dans le journal (`a join: a name of N bytes ... a key of K bytes of kind 10`).

## Changement

**Scan** : `escan` passe en version 2 (`wl_escan_params_v2_t`, 84 octets). Le firmware répondait `BCME_VERSION` (-37) au format v1 (build 162). La lecture de `wl_bss_info_v109` a été revérifiée contre `wlioctl.h`.


`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (fichier Wi-Fi), d'après bcmdhd_101_16 (`dhd_msgbuf.c`, `wl_cfg80211.c` ; LineageOS exynos9820, lecture seule) :
- **Bloc DMA de données** (0x24000, < 4 Gio, en cache avec maintenance explicite) : une flowring TX de 64 × 48 octets, 32 tampons RX de 2 Kio postés sur l'anneau RXPOST, 32 tampons TX.
- **Réception** : complétions `RX_CMPLT` (0x12) et `TX_STATUS` (0x10), avec le même contrôle epoch/XOR que l'anneau de contrôle. Les trames EAPOL (0x888e) vont à la poignée de main, les autres à `hakios_net_input` une fois connecté. L'interrogation se fait par le `poll` de l'interface (2–10 ms) et par le fil du pilote (50 ms).
- **Émission** : `TX_POST` (0x0F) sur la flowring 2 (en-tête Ethernet dans le message, charge utile par adresse DMA).
- **Connexion** : choix du point d'accès le mieux entendu (scan dirigé si absent), puis `INFRA`, `AUTH` ouvert, `WSEC` AES (+TKIP si le groupe du point d'accès l'est), `WPA_AUTH` PSK, `wpaie` (notre élément RSN), et `WLC_SET_SSID` avec le BSSID. On attend `WLC_E_LINK`, puis on lit le BSSID et on crée la flowring (`FLOW_RING_CREATE`, réponse 0x04).
- **WPA2** : `bsd/dev/b2l_wpa.c` + `bsd/sys/b2l_wpa.h`, copiés du `lx_wpa.c` d'ios-pipa (MIT, HakiOS authors) en ne changeant que les includes. Ils calculent la PMK (PBKDF2), traitent les messages 1 et 3 et déballent la GTK (AES key wrap). Le message 4 part avant l'installation des clés : PTK puis GTK via `wsec_key`, avec le RSC. Les renouvellements de la clé de groupe sont traités par le fil du pilote.
- **État** : `JOINING` puis `JOINED` avec le réseau ; `hakios_net_link_state(true)` (configd lance le DHCP) ; perte du lien → `hakios_sta_lost`. Une mauvaise phrase de passe donne `EAUTH`, qu'Apple80211 traduit en « invalid PMK ».
- `xnu/bsd/conf/files` : ajout de `bsd/dev/b2l_wpa.c`.

Aucune clé ni phrase de passe n'est écrite dans le journal (longueurs et statuts seulement).

## Attendu

`B2L WIFI DATA: block ... ok, 32 RX buffers posted`. Puis, quand on choisit un réseau dans Réglages : `B2L WIFI JOIN: WPA2-PSK ...`, `B2L WIFI FLOW: created ring 2 status=0`, `EAPOL in/out`, `WPA: keys in: pairwise=0 group=0`, `JOIN: joined, frames pass`. Ensuite le DHCP et une adresse IP.

## Installé avec hash BOOT

- Build : **163**, SET `b2l-metal-v88` (`--no-libs`). Premier essai : une erreur de compilation (`strstr` absent du noyau), remplacé par `strncmp`.
- BOOT relu : `fdc46d2a3a1403d9bed2e00ebf35ece4ec7066afc758dc989ccb9b42c8d5aa53` (`BOOT-OK`).
- SYSTEM : `SYSTEM-OK` en 62 s.
- Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

Scan OK : 7 à 11 réseaux entendus, la liste s'affiche. Les 8 `ASSOCIATE` sont refusés en `EINVAL` (« a name of 8 bytes ... a key of 1 bytes of kind 32 ») : la disposition 16.5 supposée est fausse. Voir le build 167.

# Build 167 : disposition iOS 26 de la requête ASSOCIATE

## Diagnostic

Journal du boot 163 (époque 173, relue dans les anciennes pages de la zone de journal) : scan OK. Les 8 connexions ont échoué en `error 22` (EINVAL), avec `a name of 8 bytes ... authentication 0/0x1, a key of 1 bytes of kind 32`, que le réseau soit ouvert ou protégé.

Le désassemblage du constructeur IO80211 (0x25d48006c, 23G90) montre que les champs d'authentification sont passés en 32 bits. Tout ce qui suit est donc décalé de 4 octets par rapport à 16.5 :

| Champ | 16.5 | iOS 26 |
|---|---|---|
| auth_lower / auth_upper | +6 / +8 (u16) | +8 / +0xc (u32) |
| ssid_length / ssid | +0xc / +0x10 | +0x10 / +0x14 (lus par la dérivation PBKDF2 à 0x25d48133c) |
| bssid | +0x30 | +0x34 |
| key_version / key_length / cipher | +0x38 / +0x3c / +0x40 | +0x3c / +0x40 / +0x44 (cipher 10 = phrase, écrit à 0x25d48140c) |
| key | +0x48 | +0x4c (memcpy à 0x25d481414) |

Le « name of 8 » était auth_upper = 8 (WPA2-PSK), le « key of 1 » était key_version.

## Changement

`xnu-overlay/bsd/dev/b2l_hakios_apple80211.c` : `struct apple80211_assoc` prend la disposition iOS 26 (908 octets ; `_Static_assert` sur les décalages). `set_join` est inchangé.

## Attendu

`a join: a name of N bytes ... a key of K bytes of kind 10` (ou 0 pour un réseau ouvert), puis `B2L WIFI JOIN`, `FLOW: created`, EAPOL, clés et DHCP.

## Installé avec hash BOOT

`4b2be70ebe20ab147fad3fad1f76e6e99ec9cc1cd95d4f03a49db510955dfd4a` (relu, BOOT-OK ; SYSTEM-OK en 123 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

ASSOCIATE accepté (« a name of 9 bytes ... a key of 32 bytes of kind 6 » : PMK du trousseau). Le firmware s'associe : événements AUTH, ASSOC, LINK up, puis flowring créée (status 0). Mais aucune trame n'arrive ensuite, pas même le message 1 du point d'accès : timeout de la poignée de main (error 60), trois fois, en 5 GHz. Un réseau ouvert a gelé tout le téléphone dans les 5 s, avant l'écriture du journal. Voir le build 168.

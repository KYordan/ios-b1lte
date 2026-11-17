# Build B1L-0012 : droits de Réglages + îlots de stubs + vidage ACPM

Date : 2026-11-04. Statut : **construit, pas installé.** BOOT + USERDATA (cache seulement, 9 fichiers).

## 1. « Informations » : limite de taille des droits (BOOT)

Le XML des droits de `/Applications/Preferences.app/Preferences` fait **75 435 octets** (DER : 52 207), au-dessus de
`B2L_AMFI_XML_MAX` = 65 536. Sans droits, `SecTaskLoadEntitlements` échoue : d'où la double libération de MediaServices
(B1L-0011) puis l'arrêt TCC. Relevé de la racine : Réglages est le seul fichier au-delà de 64 Kio (suivant : assistantd, 57 467).
`b2l_amfi_metadata.h` : `B2L_AMFI_XML_MAX` et `B2L_AMFI_DER_MAX` passent à **262 144**.

## 2. Îlots de stubs du cache (USERDATA)

`nc-static apply` réécrit aussi, dans les mappings exécutables, la paire exacte `ldr x16,[x17]` + `braa x16,x17` →
`br x16` : **108 164 stubs**. Total : 41 502 070 mots (687 PACGA), corecrypto exclu (5 187). `compare` : 0 changement
inattendu hors les 2 NOP MediaServices. 9 fichiers diffèrent de `out2`.

## 3. DVFS : vidage ACPM en lecture seule (BOOT)

La file du canal 5 lue avec la disposition du source LineageOS annonce `len=1` (inutilisable par l'algorithme Samsung) :
la disposition du firmware diffère. B1L-0012 journalise `B2L DVFS DUMP init+..` et `chan+...` (en-tête et table des canaux)
sans rien envoyer d'autre que les vérifications existantes (qui refusent).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-ent-0012/boot.img` (25 910 112 octets) | `da886c33252fb87217020528947f4d8502a1c4e8705c8b19fc49a29190d68e7a` |
| Kernel | `cd4863b79bee5ea1f3570fd826e0702192a73ea0141d21b65b6400615dcfbce9` |

Racine : `sudo bash b1l-tools/install-b1l-0012-root.sh` (cache depuis `static-work/out3`).
Retour arrière : `out/live/ios26-userdata-live-0011.img` + BOOT `out/b1l-pacga-0011/boot.img`.

## Installation (2026-11-04)

- Racine : 9 fichiers de cache réécrits, `fsck_hfs` OK.
- BOOT (sda14) : relecture `da886c33…` identique.
- USERDATA (sda34) : 64/64 morceaux vérifiés (16 écrits). Pas encore testé.

## Résultat observé (B1L-0012)

- **Réglages › Général › Informations s'ouvre.** Réglages reste lent.
- Pièges : 23,5 M à 345 s puis **31,97 M à 825 s** (croissance presque nulle à la fin), contre 196 M (0011) et 58 M (0006).
- Vidage ACPM : la disposition de `framework.h` est la bonne. Canal 5 : id 5, owner 4, **type 2 (TYPE_BUFFER)**, `q_len=1`,
  `elem=16`, `ap_poll=0`, tx 0x7bc8/0x7bcc/0x7bd0. Samsung envoie sur ce canal sans attendre de réponse.

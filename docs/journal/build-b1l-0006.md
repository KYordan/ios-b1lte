# Build B1L-0006 : cache dyld traduit en statique (arm64e → arm64)

Date : 2026-11-04. Statut : **installé dans USERDATA, pas encore testé sur le téléphone.**
Une seule modification par rapport à B1L-0004 : les fichiers `dyld_shared_cache_arm64e*` de la racine vivante
sont remplacés par leur version traduite. BOOT (B1L-0003, `0fbcf3ba…`) et SYSTEM sont inchangés.

## Pourquoi

- Le kernel piège puis émule chaque instruction arm64e (PAC/AUT, RETAA, BRAA, LDAPR…). Le cache natif ne les apprend
  qu'une fois exécutées, d'où une première utilisation très lente (Réglages : `B2L LIVE: traps=103424606`, pid Preferences).
- Test de l'hypothèse sur le plantage de « Informations » : si l'émulation corrompt une valeur, le plantage doit disparaître.

## Méthode

Outil hôte `b1l-tools/static-translate/nc-static` : même règle que le kernel (`b2l_nc_native`), même exclusion
corecrypto (`b2l_xresolve`). Seules les sections Mach-O marquées instructions (`flags & 0x80000400`) sont réécrites.

| Mesure | Valeur |
| --- | --- |
| Mots convertibles dans le cache d'origine | 41 501 504 (PAC/AUT 13,27 M, hint 11,70 M, RETAA 5,58 M, BRAA 10,82 M, LDAPR 134 k) |
| Mots exclus (corecrypto) | 5 187 |
| Changements appris par le téléphone vérifiés | 122 177, **0 inattendu** |
| Traduction appliquée au cache vivant | 4 317 images, 12 152 sections, 41 225 014 mots |
| `compare` origine → résultat | 41 395 052 mots traduits, **0 inattendu** |

Installation dans l'image : `sudo bash b1l-tools/install-translated-cache.sh` (écriture sur place, même taille,
même inode, même propriétaire), puis `fsck_hfs` : « The volume B2LIOSROOT appears to be OK ».
Relecture du montage : 84 fichiers identiques, 0 différent.

## Écriture sur le téléphone

`flash-chunked.sh live/ios26-userdata-live.img userdata /dev/block/sda34 499641200640` (garde-fous : TWRP root,
`SM-G973F`, by-name, taille, non monté). Résultat : **64/64 morceaux vérifiés**, rc=0. Morceaux déjà identiques sautés :
11–14, 41–42, 50–64.

Seule partition écrite : USERDATA (`sda34`).

## Retour arrière

Réécrire `out/live/ios26-userdata-live-0004.img` avec la même commande (seuls les morceaux changés sont réécrits).

## À observer

1. Temps de démarrage jusqu'à l'écran verrouillé.
2. Réglages : ouverture, fluidité, et Général › Informations (plante-t-il encore ?).
3. Depuis TWRP : `pull-b2l-log.py` et comparer `B2L LIVE: traps=` avec B1L-0004.

## Résultat observé

- Démarre ; premier démarrage lent (le cache natif saute 43 877 enregistrements devenus caducs : `applied=6564 already=4357 skipped=43877`).
- Réglages s'ouvre puis se fige un moment ; **Général › Informations plante toujours**, même pile (`objc_release` dans
  `+[MSVEntitlementUtilities _checkEntitlement:…]` ← `ICCloudServiceStatusMonitor` ← `+[MPMediaLibrary requestAuthorization:]`,
  adresse invalide `0x504d72cb0`). L'hypothèse « l'émulation corrompt une valeur du cache » est écartée.
- Cache natif (fin du log, ~845 s) : `queued=102876 learned=47455 dropped=659595` contre `queued=240942 learned=90417 dropped=9611766` en B1L-0004.
- Pièges : ~58 M à 845 s, dont **≥ 24 M paires PACGA** (`paired=24207233`). PACGA (SipHash noyau) n'est pas traduisible
  à l'identique et domine maintenant le coût d'émulation.
- Pas de barre Home ni de geste de sortie d'app (le bouton Bixby sert de Home).
- `undo archive unavailable (5)` : déjà présent en B1L-0004.

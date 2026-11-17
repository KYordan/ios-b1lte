# Plan : traduire le PAC de corecrypto sans casser son auto-test (pour plus tard)

Date : 2026-11-08. **À ne pas lancer maintenant** : le propriétaire le fera démarrer plus tard.

## Décision du propriétaire

- **Méthode 1 :** traduire les instructions PAC du texte mesuré de corecrypto, puis **recalculer l'empreinte d'intégrité** (HMAC) qu'il vérifie et la remplacer dans le fichier. Il se vérifie et ne voit rien. Aucune sécurité désactivée.
- **D'abord extraire du téléphone (USERDATA) le cache partagé actuel**, pour partir d'une copie à jour, pas de `static-work/out`.

## Pourquoi

- B1L-0098 : backboardd passe ~88 s sur 90 dans le noyau, 20 000 à 80 000 interruptions PAC par seconde, ≈ 11 µs chacune.
- Le cache traduit n'a plus de PAC, sauf les **5 187 mots du texte mesuré de corecrypto** (`__TEXT,__text` de `libcorecrypto.dylib`, gardés d'origine par `b2l_cache_exclusion.h` et `nc-static`).
- Le cache natif n'apprend plus que ces instructions (`measured` ≈ 43 000 par boot).
- **À confirmer d'abord avec le profil de B1L-0101** (`B2L PACPROF`) : si un seul appelant sollicite corecrypto en boucle, le corriger peut suffire.

## Étapes

### 0. Confirmer la cible (journal de B1L-0101)

- Faire correspondre les PC de `B2L PACPROF` avec le cache : le glissement se trouve en faisant coïncider les instructions relevées avec celles du fichier.
- Vérifier que les places les plus fréquentes sont bien dans la plage mesurée de corecrypto (`dyld_shared_cache_arm64e.31`, `0x1516c00 + 0x874d0` d'après `B2L NCACHE EXAMPLE`), et noter les appelants (LR).

### 1. Extraire le cache partagé de USERDATA (lecture seule)

- Téléphone en TWRP, gardes habituelles : TWRP root, `ro.boot.em.model` = SM-G973F, chemin by-name, taille, partition non montée.
- Lire `sda34` par morceaux avec hash, avec `b1l-tools/pull-chunked.sh` ou équivalent, vers `out/live/` sur le Mac, **hors de git**. USERDATA contient des données personnelles.
- Monter l'image **en lecture seule** sur le Mac (`hdiutil attach -readonly`) et copier :
  - `private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64e*` ;
  - le même nom sous `/System/Library/Caches/com.apple.dyld/`, s'il existe. Le cache natif du noyau lit ce chemin ; vérifier lequel dyld utilise vraiment.
- Comparer avec `static-work/out` (`cmp` par fichier) et noter les différences : traductions apprises par le téléphone, NOP voulus, etc.
- Garder cette copie intacte comme **retour en arrière**.

### 2. Comprendre l'auto-test d'intégrité

- Dans `libcorecrypto.dylib` (cache extrait), trouver la fonction d'intégrité FIPS (`fipspost_post_integrity` dans le code source de corecrypto publié par Apple), puis :
  - **les zones mesurées** (le texte ; peut-être aussi des constantes) ;
  - **la clé HMAC** (constante) ;
  - **l'emplacement de l'empreinte attendue** (une section dédiée, par exemple `__fips_hmacs`) ;
  - **ce qui se passe en cas d'échec** (abort, refus des opérations, ou rien).
- Écrire un vérificateur sur le Mac qui recalcule l'empreinte **sur le cache d'origine** : elle doit être égale à la valeur rangée. C'est la preuve qu'on a bien compris avant de modifier quoi que ce soit.

### 3. Traduire et recalculer

- `nc-static` sans l'exclusion corecrypto : `RETAA`/`RETAB` → `RET`, `BRAA`/`BLRAA` → `BR`/`BLR`, `LDAPR` → `LDAR`, etc. Même règle que `b2l_nc_native`. Les `LDRAA` qui ne se traduisent pas restent.
- Recalculer le HMAC sur le texte traduit et l'écrire à la place de l'empreinte attendue.
- Vérifier avec le vérificateur de l'étape 2 (empreinte égale) et avec `nc-static compare` (seuls les mots prévus ont changé).

### 4. Le noyau ne doit pas défaire la traduction

- `b2l_native_cache.c`, à l'application avant userspace, **remet d'origine** le texte mesuré de corecrypto (`integrity_restored`, `b2l_xfilter`). Il faut le désactiver, avec un boot-arg ou du code, sinon il annulera la traduction au boot suivant.
- Le noyau panique si les métadonnées de corecrypto sont « dangereuses » (`integrity_open`) : vérifier que le cache modifié passe ce contrôle.

### 5. Installer sur le téléphone (écriture USERDATA, avec l'accord du propriétaire au moment venu)

- N'écrire que les blocs de `sda34` qui changent (le fichier `.31`, même taille, même inode), avec toutes les gardes : TWRP root, modèle, by-name, taille, non montée, hash, relecture.
- Retour en arrière : réécrire les mêmes blocs depuis la copie extraite à l'étape 1.

### 6. Mesurer

- `B2L EMU CLASS` et `B2L PACPROF` : nombre d'interruptions par seconde avant et après.
- `caught burning CPU` de backboardd : la part noyau doit s'effondrer. Fluidité ressentie de l'interface.

## Risques

- Si l'auto-test échoue, tous les processus qui utilisent la cryptographie peuvent refuser de démarrer, y compris des services du démarrage. D'où l'étape 2 (preuve sur l'original) et la copie de retour en arrière.
- Le cache est partagé par tout le système : une instruction mal traduite fait planter n'importe quel processus. D'où `compare` (zéro changement inattendu).

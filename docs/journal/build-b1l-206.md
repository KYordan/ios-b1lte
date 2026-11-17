# B1L-0206 : trousseau (emballage des clés)

## Pourquoi
Le propriétaire veut que le Wi-Fi se reconnecte seul au démarrage suivant. Le journal iOS du 0205 montre que le
trousseau ne garde rien : `ks_crypt: e00002c7 failed to 'oe' item`, AppleKeyStore refuse les sélecteurs 10 et 11.
Sans trousseau, wifid ne peut pas enregistrer le mot de passe, et d'autres services (IDS, accountsd) échouent aussi.

## Changements
- `iokit/Kernel/arm/b2l_aks_core.c` (sauvegarde `../b2l_aks_core.c.205bak`) : sélecteur 10 (emballer : scalaires
  {handle, classe}, clé ≤ 64 octets → marque `B2wr` + classe + clé mélangée à un motif fixe, 8 octets de plus, comme
  RFC 3394) et 11 (déballer). Même schéma qu'ios-pipa 0134 et HackiOS MiniAppleKeyStore. Ne protège rien (pas de
  SEP, pas de code), mais stable d'un démarrage à l'autre. Testé sur le Mac (`scratchpad/akstest.c` : aller-retour,
  marque fausse refusée, place insuffisante refusée).
- `iokit/Kernel/arm/B2LKeyStore.cpp` (sauvegarde `../B2LKeyStore.cpp.205bak`) : les échecs AppleKeyStore ont leur
  propre budget de 64 lignes `B2L AKS`, pour voir les sélecteurs encore manquants.
- `osfmk/kern/trap_telemetry.c` : remis à l'original (log `B2L TRAPTEL` retiré).

## Résultat
Après redémarrage, le Wi-Fi se reconnecte seul (le mot de passe est gardé). Les apps ne chargent toujours rien.

Piste trouvée dans `timesync` (copie USERDATA 0205) : à chaque démarrage, l'heure murale vaut 1970-01-01 00:00:47
(pas d'horloge RTC lue). Elle n'est corrigée que vers 290 s après le démarrage (synchro réseau). Avant ça, toute
vérification TLS échoue en `TemporalValidity` (leaf, ca1 et root). C'est ce qui explique les refus du 0205.

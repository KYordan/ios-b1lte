# Build B1L-0020 : capacité arm64e annoncée + lanceur de migration v3

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `172037fc…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0019.

- `patches/0101-b2l-report-arm64e-host-subtype.patch` (`osfmk/kern/host.c`) : `host_info(HOST_BASIC_INFO).cpu_subtype`
  (et donc `sysctl hw.cpusubtype`) annonce `CPU_SUBTYPE_ARM64E` quand la compatibilité arm64e est active. Le choix des binaires
  à l'`exec` (`cpu_subtype()`, `grade_binary`) ne change pas. Coupe-circuit `b2l_report_arm64e=0`.
- Profil MobileGestalt **v5** : régénération du cache de capacités.
- Lanceur `userspace/system-app-bootstrap.m` : reçu v2, jusqu'à 3 tentatives si l'installation n'est pas terminée,
  journal non tamponné à chaque étape. Publié sous de nouveaux chemins (le kernel ne remplace jamais un fichier existant) :
  `/usr/local/libexec/b2l-system-app-bootstrap-v3`, `dev.b2l.system-app-bootstrap-v3.plist`, cache launchd
  `/private/var/db/b2l-launchd-bootstrap-v4.plist`, journal `/private/var/log/b2l-system-app-bootstrap-v3.log`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-arm64ecap-0020/boot.img` (25 910 264 octets) | `172037fca451431e9d5152b7d085e198fc0fa69c4374f5f601c11d6e878a7b28` |
| Kernel | `22090d5a5816c1754e6c24fe01def3573e7a1efeec451b3181fbe72104bdff7a` |

Retour arrière : `out/b1l-apfsname-0019/boot.img`.

## Résultat observé (B1L-0020)

- Toujours 2 apps. Lanceur v3 : `attempt 1 of 3` → `invoking … staged=50` → **`nativeResult=0 installedAfter=0`** après 4 min 30.
- **Erreur arm64e (code 67) disparue** du dernier démarrage (`boot F5A6600E`).
- L'erreur APFS (code 166) reste (24 fois) : elle vient du processus 354 (`xpcproxy` → démon d'installation), dont le nom
  `installcoordinationd` est tronqué à 16 caractères (`installcoordinat`) dans `p_comm` : la comparaison exacte de B1L-0019 ne le visait pas.

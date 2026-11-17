# Build B1L-0021 : APFS annoncé aussi à installcoordinationd (nom tronqué)

Date : 2026-11-04. Statut : **installé dans BOOT (sda14), relecture `8c537048…` identique ; pas encore testé.** BOOT seulement. Base : B1L-0020.

`xnu-overlay/bsd/vfs/b2l_fstype.h` : `strncmp(name, "installcoordinationd", MAXCOMLEN)` (p_comm limité à 16 octets).
Le lanceur v3 fera sa tentative 2 sur 3.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-apfsname2-0021/boot.img` (25 910 264 octets) | `8c5370484f1ab8da49caf98b33bb80f03236b478059f20120860381e58e0c651` |
| Kernel | `9192e93854402209874977fd47d5b6c50bba8ae27232d585d6ee5a8d33c5c624` |

Retour arrière : `out/b1l-arm64ecap-0020/boot.img`.

# Build B1L-0014 : essai DVFS « buffer » + sonde GPU G1 (lecture seule)

Date : 2026-11-04. Statut : **construit, pas installé.** BOOT seulement.

- DVFS : canal 5 TYPE_BUFFER à une case, laissé `front=rear=0` par S-Boot. L'algorithme de file Samsung ne peut pas y écrire.
  Essai : écrire la commande dans l'unique case (`tx_base`) puis `INTGR0`, sans contrôle de file ; journal du contenu
  `rx_base` après 500 µs. Le firmware APM reste seul juge des tensions. Mesure PMU `after-dvfs` si envoyé.
- GPU G1 : lecture de `pd-embedded_g3d` (0x15862100 conf, +4 état, bit 0). Registres Mali (GPU_ID, L2_FEATURES,
  SHADER_PRESENT, TILER/L2/AS/JS_PRESENT) lus **seulement** si le domaine est déjà allumé. Aucune écriture.
- Décision GPU : voir `gpu-g0.md` (rendu GLES présent dans QuartzCore d'iOS 26).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-gpu-g1-0014/boot.img` (25 910 056 octets) | `3519d9c0a8c5d012f5ea241c2d862254a563292673319d12a75b590d92f45354` |
| Kernel | `398c626b72035b4c1146e266533b8b360aee492e14e758fc4bc1f67544c25664` |

Retour arrière : `out/b1l-dvfs-apps-0013/boot.img`.

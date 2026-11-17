# B1L-0201 — traceur d'ioctl

Base : `b2l-metal-v95`, `--no-libs`. Comme 0200, avec ces changements :
- IPv6 réactivé sur en0 ;
- test de l'événement au démarrage retiré.

`xnu/bsd/kern/sys_generic.c` est modifié directement dans `xnu/` (sauvegarde `../sys_generic.c.bak`). Dans
`ioctl()`, pendant 30 s après chaque montée du lien, chaque ioctl (hors apple80211) est écrit :
`B2L IOCTL n: proc[pid] fd cmd`. Après chaque ligne, le journal est vidé (`b2l_log_kick`) et le noyau attend 50 ms.
`b2l_ioctl_trace_until` est armé dans `b2l_hakios_net.c` ; `b2l_log_kick` vient de `b2l_persistent_log.c`.

## Résultat
Flashé et redémarré.

Mise à jour : finalement testé, gel au join. Journal (`out/logs/0201-boot.txt`) :
- `configd` envoie l'ioctl 116 `0xc030694c` puis l'ioctl 119 `0x805869a0` = **SIOCLL_CGASTART** (IPv6 link-local CGA →
  `in6_cga.c` → `SHA1Init`) ;
- configd n'envoie plus aucun ioctl ensuite ; le journal s'arrête environ 1 s plus tard.
Cela confirme la cause corrigée en 0202.

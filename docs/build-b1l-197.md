# B1L-0197 — lien caché à iOS (comme 184) sur la base metal v95

Base : `b2l-metal-v95`, `--no-libs`. `g_link_to_ios = false` : `hakios_net_link_state` n'est jamais appelé,
en0 reste down pour iOS, aucune trame vers iOS. Journal 1 s.
But : 184 (base v88, même réglage) ne gelait pas. Si 197 gèle, le gel ne vient pas de l'annonce du lien.

## Résultat
Aucun gel en 8 joins WPA2 (wifid se déconnecte faute de lien, comme 184). Le gel exige donc un événement de lien envoyé à iOS (196 : le `KEV_DL_LINK_OFF` de la déconnexion qui suit l'échec ; 195 : le `KEV_DL_LINK_ON` à T+20 s).

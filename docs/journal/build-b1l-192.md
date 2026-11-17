# B1L-0192 — Wi-Fi: mbuf de réception refait (correctif du gel), pauses retirées

Base : `b2l-metal-v95` (session Metal, build 0191), `--no-libs`.

## Changements
- `bsd/dev/b2l_hakios_net.c` : `hakios_net_input_unit` construit la trame dans un seul mbuf
  à cluster (`mbuf_gethdr` + `mbuf_mclget` + `memcpy` + `mbuf_setlen`/`mbuf_pkthdr_setlen`),
  comme le patch 0076 d'ios-pipa. Avant : `mbuf_allocpacket` + `mbuf_copyback` puis retrait de
  l'en-tête, qui panique « mbuf validity check: len -14 » quand le premier mbuf a une longueur 0.
  Cela correspond au gel de 183 (premier paquet arrivé à « bpf next », puis gel).
- Toutes les pauses diagnostic (`net_watch`, étapes d'entrée, étape d'annonce du lien) sont retirées.
- `B2LWifi.cpp` : `g_link_to_ios = true` (iOS voit de nouveau le lien) ; `wpa->rsn_length = 0`
  après le join (un message 1 de rekey copiait 22 octets depuis nullptr).

## Résultat
Gèle encore. Le journal s'arrête sur des appuis tactiles, sans aucune ligne `ASSOCIATE` ni `B2L WIFI JOIN` :
la connexion (1–2 s au build 184), l'annonce du lien et le gel tiennent dans les ≤5 s jamais écrites.
Le correctif mbuf ne suffit pas. Prochaine étape : photo de l'écran au gel (bande `B2L BT` / `B2L FAULT`),
à symboliser avec `out/mach-0192.unstripped`.

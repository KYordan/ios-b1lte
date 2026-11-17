# B1L-0195 — journal écrit toutes les secondes (accord du propriétaire)

Base : `b2l-metal-v95`, `--no-libs`. Code Wi-Fi identique au 0194 (étapes 0/20/40/60 s, repère toutes les ~2 s).

- `osfmk/arm64/b2l_persistent_log.c` (accord du propriétaire, 2026-11-12) : le thread du journal écrit toutes les
  **1 s** au lieu de 5 ; les lignes de progression (EMU PERF, LIVE, ...) restent espacées d'environ 30 s.
  Le boot-arg `b2l_log_sec=1..5` remplace la valeur. Les boot-args sont en dur dans `uefi/XnuLoader/XnuLoader.c`,
  et ce chargeur est **intégré au FD UEFI** (`package-boot.py` refuse un chargeur qui n'y est pas) : changer un
  boot-arg demande de reconstruire le FD. Essai fait puis annulé, chargeur et source remis à l'identique.

## Résultat
Le journal à 1 s montre enfin la séquence : join WPA2 réussi ; étape 0 (en0 up, sans événement) tenue 20 s sans souci ;
à T+20 s `net: en0's link event sent`, puis configd retrouve en0 (IOKit), ouvre HakiosWLAN et lit CARD_CAPABILITIES,
SSID, BSSID, AUTH_TYPE, puis 387/552/563 REFUSED : **dernière ligne**. Le gel suit donc l'événement noyau
`KEV_DL_LINK_ON` à ≤1 s, avant toute trame dans un sens ou dans l'autre (étape 1).

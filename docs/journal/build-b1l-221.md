# B1L-0221 : trace du gel de Safari, deuxième essai

Identique à 0220, sauf `b2l_wk_trace()` :
- `emu` : 100 lignes au plus, sans attente ;
- appels GPU, IOSurface et framebuffer, et traps de cache : 400 lignes écrites d'avance (40 ms chacune), 4000 au plus.

BOOT `42dd9720…9493` : BOOT-OK. SYSTEM-OK (6 blocs).

## Résultat

Le gel est arrivé de nouveau. La **dernière ligne** de `out/logs/0221-safari.txt` est le premier trap de cache de
WebKit : `B2L WK 0: ic pid 365 0x5 0x80ae18000 0x9b00f0000`. Cette ligne est écrite après `pmap_find_pa`, donc le gel
survient pendant la fenêtre de copie ou pendant l'opération de cache. La version `phystokv()` de 0218 gelait déjà au
même endroit. Le chemin du trap de cache n'a donc jamais fonctionné. Voir 0222.

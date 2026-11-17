# Outils de symbolisation du cache partagé iOS 26 (23G90)

Tous travaillent sur le cache d'origine `~/Developer/s10-port/static-work/orig` (lancer depuis ce dossier).

- `dscsym.py DIR pc...` : trouve le glissement (slide) du cache à partir d'une pile `B2L BT` (les adresses de retour doivent suivre un BL/BLR), puis donne l'image et le décalage de chaque adresse.
- `dscname.py DIR va_non_glissee...` : symbole local le plus proche (depuis `dyld_shared_cache_arm64e.symbols`).
- `rd.py` : lecture d'une adresse non glissée dans le cache (`rd(va, n)`), utilisé par les autres.
- `iomfbexp.py DIR` : exports d'IOMobileFramebuffer et sélecteur passé à IOConnectCall (heuristique `mov w1, #N`).

Désassembleur : `~/Developer/tools/mamba/envs/s10/bin/llvm-mc --disassemble -triple=arm64e-apple-ios` (octets en hexa `0x..` sur l'entrée standard).
Exemple : `cd ~/Developer/s10-port/static-work/orig && python3 ~/Developer/s10-port/b1l-tools/dsc/dscsym.py . 0x19f75b420 0x1a361b044 ...`

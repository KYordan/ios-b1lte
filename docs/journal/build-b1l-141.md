# Build 141 : `/wlan` et pinctrl dans le device tree synthétique

## Diagnostic

Le build 140 a montré que `B2LWifi` s'instancie et que `WL_EN` est bien asserté, mais que `SecureDTLookupEntry("/wlan")` échoue (`dt=0`). La cause est établie : XNU ne reçoit pas le DTB Samsung complet. `uefi/XnuLoader/DeviceTree.c` construit un arbre synthétique minimal (`/chosen`, `/product`, `/cpus`, `/arm-io` avec GIC et écran, `/framebuffer`, `/aliases`) et c'est cet arbre qui est passé au noyau. Le DTB beyond1lte présent dans l'image BOOT n'est donc pas visible depuis XNU.

Le nœud `/wlan` réel de `beyond1lte.dtb` (révision 26) contient :

- `compatible = "samsung,brcm-wlan"`, `status = "okay"` ;
- `gpios = <0x8e 5 1 0x134 7 1>` : `gpg0-5` (`cfg_wlanen`) et `gpa0-7` (`wlan_host_wake`) ;
- `pinctrl-names = "default"`, `pinctrl-0 = <0x17c 0x13a>` : `cfg_wlanen` et `wlan_host_wake`.

Les phandles viennent du DTB beyond1lte : `gpg0 = 0x8e`, `gpa0 = 0x134`, `cfg_wlanen = 0x17c`, `wlan_host_wake = 0x13a`. Les offsets de registres correspondants ont été confirmés au build 140 (`gpg0` à `0x080` dans PERIC0, `gpa0` à `0x000` dans ALIVE).

## Changement

Un seul changement, dans `uefi/XnuLoader/DeviceTree.c` : ajout au device tree synthétique de

- `/pinctrl@10430000` (`compatible = "samsung,exynos9820-pinctrl"`, `reg = <0x10430000 0x1000>`) avec `gpg0` (`gpio-controller`, `#gpio-cells = <2>`, phandle `0x8e`) et `cfg_wlanen` (`samsung,pins = "gpg0-5"`, fonction 1, pull 0, drive 3, phandle `0x17c`) ;
- `/pinctrl@15850000` (`reg = <0x15850000 0x1000>`) avec `gpa0` (phandle `0x134`) et `wlan_host_wake` (`samsung,pins = "gpa0-7"`, fonction 15, pull-up, phandle `0x13a`) ;
- `/wlan` (`compatible = "samsung,brcm-wlan"`, `status = "okay"`, `gpios`, `pinctrl-0`).

Aucun pilote noyau n'est modifié, aucun registre matériel n'est touché. Le chargeur `XnuLoader.efi` et le firmware `SILICIUM_UEFI.fd` ont été reconstruits pour intégrer le nouvel arbre.

## Installé avec hash BOOT

- Build : **141**, SET `b2l-metal-v88` (libs réutilisées, `--no-libs`).
- `XnuLoader.efi` : 38 Kio, reconstruit le 10/11/2026 ; les chaînes `samsung,brcm-wlan`, `cfg_wlanen`, `wlan_host_wake`, `gpg0-5`, `gpa0-7` et `samsung,exynos9820-pinctrl` sont présentes dans le binaire.
- Firmware : `SILICIUM_UEFI.fd`, 4 Mio, hash SHA-256 `b84fea7af3d8cc2cdf207066e90ac8f39c97d1484c921abf4e835b63f00ece79`.
- Chargeur : hash SHA-256 `bcf747b12dc5aba2bf5bf080914b119c48758c64839f7d3ee589028dd02aac07`.
- BOOT : `out/b1l-metal-141/boot.img`, 26 895 456 octets, hash `3dd7b2811c3630bdaf23351004a40d91c949a80e219a005ebea6a36bbc1cfe05`, relecture identique (`BOOT-OK`).
- SYSTEM : 1 bloc modifié depuis le build 140, 5 blocs sur 1750 à réécrire (20 Mio), relecture complète (`SYSTEM-OK` en 91 s).
- Flash effectué le 10/11/2026 depuis TWRP.

## Résultat

**Succès, boot iOS sans régression.** Le téléphone a démarré normalement ; le journal contient 2 997 lignes `hakiosmetal[backboardd ...]` et des swaps de framebuffer continus, donc l'écran et le rendu fonctionnent.

La trace persistante du pilote est maintenant :

```
B2L WIFI: dt=1 gpio=<0x8e 5 0x1 0x134 7 0x1> gpg0[con=0 dat=0x1 pud=0x11131113 drv=0] wl_en=0/0/0 wake=0/0/0
B2L WIFI: WL_EN asserted con=0x100000 dat=0x21 host_wake=0
```

- `dt=1` : `SecureDTLookupEntry("/wlan")` réussit ; le nœud synthétique est bien celui que voit XNU.
- `gpios = <0x8e 5 0x1 0x134 7 0x1>` : les valeurs lues correspondent exactement au DTB beyond1lte (`gpg0-5` et `gpa0-7`).
- `WL_EN` reste correctement asserté (`con=0x100000`, `dat=0x21`) et `host_wake` reste à 0, comme au build 140 (puce non alimentée/horlogée).
- La ligne `IOLog` `/wlan present, gpios <...>` est également présente, mais entrelacée avec les traces SMP multi-cœurs ; la trace `b2l_diag_record` ci-dessus est la référence fiable.

Le mécanisme de device tree synthétique est donc validé : les prochains pilotes peuvent déclarer leurs nœuds et propriétés dans `DeviceTree.c` et les lire depuis XNU. Aucun retour arrière nécessaire.

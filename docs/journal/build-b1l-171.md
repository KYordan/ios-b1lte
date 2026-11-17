# Build 171 : mémoire SCB côté hôte (HSCB)

## Diagnostic

Build 170 : la mémoire est contiguë et le bus PCIe ne signale aucune erreur. Le firmware refuse donc lui-même de remonter les trames de données, et l'erreur est classée dans sa couche PCIe (`EL: 30`).

Dans bcmdhd_101_16, `dhd_prot_init` appelle `dhd_alloc_host_scbs` : si `flags2` (pciedev_shared + 80) porte `PCIE_SHARED2_HSCB` (0x800), le firmware garde ses fiches de station (SCB) en mémoire hôte. L'hôte alloue alors `host_scb_size` octets (+72), en écrit l'adresse dans `host_scb_addr` (+64) et ajoute `HOSTCAP_HSCB` (0x02000000) à host_cap. Nous ne le faisions pas. Pour une trame de données, le firmware cherche la fiche du point d'accès et ne la trouve pas, ce qui cadre avec l'échec observé.

## Changement

`xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp`, juste après host_cap : lecture de `flags2` et de `host_scb_size`. Si le firmware demande HSCB, allocation d'un bloc contigu de cette taille, mis à zéro, puis adresse et `HOSTCAP_HSCB` donnés au firmware. Trace : `B2L WIFI SHARED2: flags2=.. host_scb_size=.. hscb=given|FAILED|not asked`.

## Attendu

`hscb=given`, puis `RXC ok=1`, `EAPOL in` et la suite de la poignée de main. Si `not asked`, la valeur de flags2 oriente la suite.

## Installé avec hash BOOT

`ea027643ded9a00d68b5e39cd04abd78a246a716c7d676db1a1b1caaf5c5b87a` (relu, BOOT-OK ; SYSTEM-OK en 67 s). Flashé le 11/11/2026, téléphone redémarré vers iOS.

## Résultat

`flags2=0x40801001` (EXTENDED_TRAP_DATA, EDL_RING, ETD_ADDR_SUPPORT, D2H_D11_TX_STATUS), `host_scb_size=0` : le firmware ne demande pas HSCB. Même échec. Au passage, j'ai vérifié qu'Android mappe le SysMMU PCIe en identité (`arch/arm64/mm/dma-mapping.c` : iova = adresse physique), donc les adresses vues par le dongle sont les mêmes que chez nous. Voir le build 172.

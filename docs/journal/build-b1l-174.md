# B1L-0174 — première trame envoyée pas à pas

Fichier : `xnu-overlay/iokit/Kernel/arm/B2LWifi.cpp` (copié dans `xnu/`).

Le 173 gèle à l'envoi du message 2 WPA. Le format de la TXP est le même que dans bcmdhd (`dhd_prot_txdata`).

Changement : `b2l_wifi_ring_submit` prend `bell` (vrai par défaut). Pour la première trame seulement, avec `g_wifi_lock` pris si possible et une pause de 6 s entre chaque étape :
- TX1 : avant tout ;
- TX2 : élément et index écrits, sans sonnette ;
- TX3 : sonnette (DB0) ;
- TX4 : relecture des drapeaux partagés (trap 0x400), de l'index de lecture du flowring et de l'index d'écriture txcpl, puis erreurs PCIe ;
- TX5 : toujours vivant.

La dernière ligne présente dans le journal donne l'étape qui gèle.

Flashé : SYSTEM 55 s, BOOT relu 98e12a94e2ea7c31c986c2289090e1fea8b1ca2f1e76b8046bcfd4a2ab284f1e.

Résultat : gel quand même, mais **pas à l'envoi**. Dans le journal :
- TX1, TX2, TX3 présents ;
- TX4 : `shared flags=0x30040007 flow r=1 txcpl w=1`, donc la puce a pris la trame et rendu son TX_STATUS, sans trap (0x400 absent) ;
- PCIe propre ;
- le système vit encore quelques secondes après (rapports PROF) ;
- TX5 manque.

Le gel vient donc plus tard : traitement de l'accusé, suite de la poignée de main, ou événements.

# Audio — état au 2026-11-14 (en pause, demande du propriétaire)

Le propriétaire a demandé de s'arrêter là pour l'audio, sans build. On y reviendra plus tard.

## Ce qui marche (sur le téléphone, build 0213)
- **Premier bip entendu** : au démarrage, le noyau joue un bip de 1 kHz à −10 dB pendant 0,7 s sur le haut-parleur du bas (CS35L40 bot@40), puis coupe tout.
- Chemin utilisé : l'AP programme lui-même RDMA0 → SPUS → SIFS0 → UAIF1 de l'ABOX, sans le firmware Calliope. Le tampon audio est en DRAM (ring_pa 0x84b8c000 en 0213).
- Les deux amplis CS35L40 (0x40 haut-parleur, 0x41 écouteur, id 35a40, rév. A0) répondent sur un I2C simulé par GPIO (gph0-6/7). Ils reçoivent les errata de la rév. A0 et leur OTP d'usine (cs35l41_otp_unpack), et l'ASP est en dsp_a esclave, 48 kHz.
- PLL_AUD0 verrouillée à 1,18 GHz, ABOX v2.0.1, UAIF1 à 6,144 MHz (bclk).
- Après le bip, tout est remis à sa valeur de départ, sauf PMU 0x1920 bit 11 (rétention des broches ABOX levée). Les amplis retournent en reset.
- Le Wi-Fi n'a jamais été gêné.

## Étapes et journal (build par build)
- 0210 A0 : lecture seule. 0211 A1 : identité des amplis. 0212 A2–A4 : ABOX lu, amplis réglés, cadencés par UAIF1. 0213 A5 : le bip.
- Journal : `grep -a "B2L AUDIO"` dans `beyond2lte-ios/out/logs/021x-boot.txt`.
- Détails techniques complets : `docs/audio/review-and-plan.md` §5 à §10.

## Ce qui est installé (dans 0213)
- `iokit/Kernel/arm/B2LAudio.h` (sha 5112968d…), appelé par `b2l_audio_probe()` dans `B2LCPU.cpp` juste après `b2l_gpu_probe()`.
- Boot-arg `b2l_audio` : 0 coupé · 1 A0 · 2 +A1 · 3 +A2 · 4 +A3 · 5 +A4 · 6 (défaut) +bip.

## À vérifier
- Le registre 0x4820 vaut 0x1 sur bot et 0x100 sur rcv. C'est voulu : bot est sur la voie 1 et rcv sur la voie 0, comme chez Samsung.
- ABOX v2.0.1 : la position du RDMA se lit aux bits 27:12.
- Le sens du bit 4 de RDMA CTRL0 est inconnu.
- La SysMMU de l'ABOX est supposée en contournement.
- Pas de firmware de protection des amplis (`cs35l40-*-spk-prot.wmfw`) : volume et durée à garder bas.

## Travail en cours rangé (jamais construit)
- La session Audio préparait le branchement à iOS (lots 3–4 : `/dev/b2l-audio`, puis un pilote vu par coreaudiod/audiomxd). Son `B2LAudio.h` inachevé est rangé dans `~/Developer/s10-port/audio-next-parked/B2LAudio.h.lot3-unfinished` (sha d76cae82…). Il n'est pas relu et peut ne pas compiler. Seul ce fichier avait changé dans l'overlay ; `b2l_audio_seed.h` n'a jamais été créé.
- Plug-in audio côté iOS (dérivé de HakiOS, MIT) : source et `build.sh` dans `Port iOS 26 to S10/audio/plugin/`, compilé dans `~/Developer/s10-port/build-audio/b2l-audio-v1/`. Pas sur SYSTEM.
- `xnu-overlay/` est remis à l'état 0213 pour l'audio.

## Données
- `~/Developer/s10-port/vendor-audio/` (hors git) : calliope*.bin, calliope2.dt, cs35l40-{bot,rcv}-dsp1-spk-prot.{wmfw,bin}, *-calib.bin (propres à ce téléphone) et mixer_paths.xml. Copiés en lecture seule depuis vendor (erofs). Empreintes dans SHA256SUMS.

## Pour reprendre
1. Repartir du 0213 : le bip marche.
2. Lot suivant prévu : `/dev/b2l-audio` avec une garde de niveau, puis le côté iOS (AudioServerPlugIn ou équivalent). Si quelque chose doit aller sur USERDATA : accord du propriétaire d'abord.
3. Plus tard : charger le firmware de protection des amplis avant de monter le volume.

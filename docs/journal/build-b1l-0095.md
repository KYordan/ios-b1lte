# Build B1L-0095 : cache de shaders sur le téléphone + mesures du temps noyau

Date : 2026-11-08. Base : B1L-0094 (qu'il contient : 0094 n'a pas été démarré seul).

Deux changements, à la demande du propriétaire (chaque boot prend plus de 10 minutes) : le cache, et des mesures qui ne changent aucun comportement.

## 1. Cache de shaders (`apply.py` B1L-0095, `build-mesa.sh`)

- Mesa construit avec `-Dshader-cache=enabled` (le dossier de build est reconfiguré une fois, marqueur `.b2l-shader-cache`).
- `pan_disk_cache.c` : la version du pilote est l'**UUID Mach-O** (`LC_UUID`) de la bibliothèque au lieu de la note build-id ELF (absente sur iOS, ce qui aurait planté). `ld` change l'UUID quand le code change : un compilateur corrigé ne reçoit jamais les binaires d'un ancien. Sans UUID, pas de cache.
- `air_metal.c` (constructeur) : `MESA_SHADER_CACHE_DIR=/b2l-dump/mesa-cache`, `MESA_SHADER_CACHE_MAX_SIZE=1G` (dossier sur USERDATA que le noyau ouvre depuis B1L-0086, `error=0` au boot 0093).
- Ce qui est gardé : le binaire Mali de chaque shader (clé : SHA-1 du NIR). La traduction AIR → NIR se fait encore à chaque boot.
- **Le 1er boot remplit le cache (aussi long qu'avant) ; le gain se voit au 2e.**

## 2. Mesures (`B2LPerf.h`, `B2LFramebuffer.cpp`, `B2LMaliService.cpp`)

Toutes les 10 s : `B2L PERF: 10 s: blit N x, T ms, MB | flush N x, T ms, MB | submit N x, T ms`
- **blit** : la copie CPU d'une surface vers l'écran (`b2l_blit`) ;
- **flush** : le nettoyage du cache CPU d'une surface (`FlushPoC_DcacheRegion`, `flushSurface`), fait pour chaque surface nommée par chaque envoi au GPU ;
- **submit** : le temps CPU de l'envoi lui-même (dont ces nettoyages).

Suspect principal : le nettoyage de surfaces de 17,5 Mo à chaque envoi.

Ensemble `b2l-metal-v51`, plug-in `B2LMetalv51`. Mesa toujours avec ses assertions signalées.

## Installé

- **Vérifié :** `libhakiosair.dylib` contient `/b2l-dump/mesa-cache` et `disk_cache_create` (Mesa compilé avec `ENABLE_SHADER_CACHE`) ; le noyau contient `B2L PERF`.
- **SYSTEM :** SYSTEM-OK en 70 s. **BOOT :** relecture `5131e0fe6b65b64a01a3ce377b9458ed0f206d6b2ae5121183abc8631539d5ea` (BOOT-OK).
- **Retour en arrière :** `out/b1l-metal-0093/boot.img`.

## Résultat (photo `b1l-0095-centre-controle.jpg`, journal `out/logs/0095-boot.txt`, un boot)

- Plus rapide selon le propriétaire. **Les éléments apparaissent petit à petit à chaque rafraîchissement.** Animation à l'ouverture d'une app, pas à la fermeture. Liquid Glass correct dans les apps et sur l'écran de verrouillage tiré ; tuiles du Centre de contrôle encore sombres. Artefacts à l'ouverture des apps.
- **0 faute 0x51** (112 à 220 avant) : le correctif de 0094 marche. 0 assertion, 0 plantage, 1 démarrage de backboardd.
- Reste **26 fautes MMU** (`status 0x8`, statut 0xff côté file) dans des lots de textures et de verre (`glass_background_*`, `Tcim*`, `Timg*`) : lectures hors des tampons (14 sur un tampon déjà libéré, 9 sur un tampon vivant).
- **Mesures `B2L PERF`** (par 10 s, interface utilisée) :
  - copie vers l'écran : 4 à 51 copies, **16 ms chacune** (17,5 Mo) ;
  - nettoyage de cache : 170 à 820 surfaces, **3,5 ms par surface de 17 Mo**, **jusqu'à 2,9 s par 10 s** ;
  - envoi au GPU : presque tout ce temps est ce nettoyage.
- Le cache de shaders n'a pas pu être jugé (un seul boot dans le journal).

## Cause de l'affichage progressif (lue dans le code)

`HakiosMaliDrawn()` (attendre que le GPU ait fini une surface) existe mais **personne ne l'appelait** : l'écran copiait des surfaces à moitié dessinées. Et la surface 1 que backboardd dessine est la mémoire même que l'écran lit : on voit le dessin en direct. Corrigé en B1L-0096.

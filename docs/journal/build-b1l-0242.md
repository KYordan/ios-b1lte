# Build B1L-0242 : l'app « Test Metal » passe dans Dictaphone

Date : 2026-11-14. Base : B1L-0241. Ensemble `b2l-metal-v113`.

## Pourquoi (journaux 0200 à 0240)

Délai entre le lancement d'une app (« signature verification bypassed for … ») et sa première surface :

| App | Délai |
|---|---|
| Horloge (MobileTimer) | 30 s (0231) ; rien après 26 s (0239) |
| Calendrier (MobileCal) | aucun dessin vu (0237) |
| Dictaphone (VoiceMemos) | 2 s (0205) |
| Journal | 2 s (0208) |
| Bourse (Stocks) | 4–5 s |
| Météo, Réglages, Safari | 3–8 s |

Horloge et Calendrier sont les deux seules lentes (cause commune probable, pas cherchée ici : hors pilote GPU).

## Changement

- `b2l_exec_graphics_diag.h` : `B2L_APP_HOST` `/MobileTimer.app/MobileTimer` → `/VoiceMemos.app/VoiceMemos`.
  Horloge redevient l'app d'Apple sans rien d'ajouté.
- Commentaires de `b2l_metal_suite.m` mis à jour.

## Installé

- **Vérifié :** `/VoiceMemos.app/VoiceMemos` dans `out/b1l-metal-0242/boot.img`.
- **SYSTEM :** SYSTEM-OK. **BOOT :** relecture `121842cd8330c91f7908c6356e07abf77f9fcc7857c878eb56d6dc9df83cbba8` (BOOT-OK). Redémarré par `adb reboot`.

## Remarque du propriétaire : les deux apps lentes sont celles des icônes en panne

- Juste : Horloge et Calendrier ont des icônes « vivantes ». Dans 0231 (Horloge) et 0237 (Calendrier), pendant que l'app
  attend sa fenêtre, le seul travail GPU est **iconservicesagent qui redessine l'icône de cette app** par RenderBox
  (tampons sans copie de 169 728 o = 204×832, recopies). Piste : le lancement attend cette icône, que notre chemin
  RenderBox rend mal ou lentement, jusqu'à un délai (~30 s). Non vérifié.
- Mémoire : en 0231 et 0239, 1,4 à 1,8 Go de tampons GPU vivants, 16–17 allocations refusées, parfois 65 Mo libres. À suivre.

## Résultat (journal `out/logs/0242-boot.txt`)

- Dictaphone s'ouvre et l'app de test tourne : 6 passages (boutons « Relancer »), toujours **24 OK, 9 ÉCHEC, 1 SAUTÉ**, 0,4–0,6 s (9 s au premier, compilation comprise).
- **OK** : propriétés, file, **compilation de shaders depuis le source sur le téléphone** (`newLibraryWithSource`, 2142 caractères → `_MTLLibrary`), tampons, blit privé, `fillBuffer`, calcul 1 M additions, threadgroup + barrière + atomique, textures 8 formats, mipmaps, effacement, triangle, indexé + instances, mélange alpha, viewport + scissor, profondeur, échantillonnage nearest et linear, `setFragmentBytes` vers RGBA16F, dessin dans une IOSurface, dessin dans une texture posée sur la mémoire de l'app, handlers, 200 command buffers (0,45–0,6 ms chacun).
- **ÉCHEC** :
  1. `MTLCreateSystemDefaultDevice` nil et `MTLCopyAllDevices` vide : une app ordinaire n'a pas le GPU (choix de 0185).
  2. `supportsFamily` : aucune famille (Apple1…, Common1…) ; beaucoup d'apps et de moteurs s'arrêtent là.
  3. MSAA 4x : non annoncé, et le dessin 4x + résolution donne un gris (`0xff808393`) au lieu du rouge.
  4. `newBufferWithBytesNoCopy` : ce que le GPU copie (blit) dans le tampon n'arrive pas dans la mémoire de l'app (la recopie de 0236 ne couvre que les textures posées sur un tampon).
  5. Blit tampon → texture : texel 0.
  6. Calcul qui écrit une texture : `0xffffffff` partout.
  7. `MTLSharedEvent` : nil.
- **Écran** : `CAMetalLayer` animé, **11 à 18,6 images/s** (90×90 points).
- SAUTÉ : `newDefaultLibrary` (normal, l'app n'a pas de `.metallib`).

## Retour en arrière

- `b2l_exec_graphics_diag.h.before-0242`.

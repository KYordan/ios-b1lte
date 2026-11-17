# Build B1L-0230 : le shader déclare tous les samplers qu'il utilise (`fa16dhmha4` refusé par le GPU en 0229)

Date : 2026-11-14. Base : B1L-0229. Ensemble `b2l-metal-v101` (noyau reconstruit), Mesa recompilé.

## Changement (`apply.py`, bloc B1L-0230)

- `panfrost/lib/pan_shader.c` : `sampler_count` = le plus grand numéro de sampler des instructions `tex` + 1 (au lieu
  du nombre de textures). Les numéros de texture et de sampler de Metal sont indépendants. Le `nir_gather_info` de ce
  Mesa ne remplit pas `samplers_used`, d'où le parcours des instructions.
- Le pilote lie déjà chaque sampler à son numéro (samplers en ligne, sampler par défaut depuis 0228) : la table des
  samplers a donc les 14 entrées.

## Installé

- **Vérifié :** `air-b2l: a shader counts the samplers it uses (B1L-0230)` dans `out/build-mesa-0230.log`.
- **SYSTEM :** 7 blocs sur 1750, SYSTEM-OK. **BOOT :** relecture `3c308e955a147fee05ce900c01082f330d9b4ee3b360d69f7eee64a3aaf0f86a` (BOOT-OK). Redémarré par `adb reboot`.

## Résultat (propriétaire, photo `b1l-0230-accueil.jpg`)

- **Progrès : Calendrier (« Sat 14 ») et Horloge (aiguilles) sont dessinés en direct.** Le cadran de l'Horloge est
  couvert de stries horizontales grises au lieu des traits noirs.
- Les icônes disparaissent dès qu'on change de page puis qu'on revient.
- Le centre de contrôle n'a toujours pas ses icônes.
- Journal `out/logs/0230-boot.txt` : **plus aucun job GPU refusé, plus aucun « GPU DID NOT FINISH », aucun plantage.**
  `fa16dhmha4` tourne dans SpringBoard et iconservicesagent.
- Reste signalé : « a texture over a buffer was asked for » (SpringBoard et iconservicesagent), présent depuis 0226 au
  moins. Ce message n'est écrit qu'une fois par processus, donc le nombre de demandes est inconnu.
  `-newTextureWithDescriptor:offset:bytesPerRow:` répondait nil.
- Suite : B1L-0231.

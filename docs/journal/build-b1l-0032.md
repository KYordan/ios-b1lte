# Build B1L-0032 : GPU G6 option 1, le serveur d'affichage d'iOS 26 forcé sur OpenGL ES

Date : 2026-11-05. Statut : **installé : SYSTEM (7 blocs relus) et BOOT (relecture `c653a4df…`) ; pas encore testé.** Base : B1L-0031. Plan : `docs/gpu-plan.md` §4c.

## Principe

`CA::WindowServer::AccelServer::renderer()` (QuartzCore 23G90) abandonne sans Metal. `userspace/b2l_accel_gles.c`, ajouté à `libb2l-gl.dylib` et chargé seulement dans backboardd, fait deux modifications au démarrage :

1. **`cbz x0` en 0x183f3bdf0 devient NOP** : la suite de `renderer()` s'exécute malgré l'absence de périphérique.
2. **L'entrée de `CA::OGL::new_metal_context` (0x18415a0c0) saute** vers `b2l_gles_context()`. Cette fonction :
   - crée un `[EAGLContext initWithAPI:3]`, pris en charge par le pont `eagl.c`, avec Mesa Panfrost derrière ;
   - appelle `gles_new_context` de QuartzCore (0x184109d38), avec `{type=3, eagl}`, qui alloue 0xb10 octets et appelle `GLESContext(eagl)`.

Le reste de `renderer()` (création du `Renderer`, compression, couleurs) reste celui d'Apple.

**Sécurité :**
- chaque mot modifié est comparé aux valeurs de 23G90 (`b4000a20`, `b9400008`, 4 mots de `new_metal_context`) ; au moindre écart, rien n'est écrit ;
- écriture par `vm_protect(VM_PROT_COPY)`, ce qui donne une page privée au processus, puis `sys_icache_invalidate`.

**Journal :** `eagl.c` et le crochet écrivent dans le journal noyau (`kern.hakios.say`), avec les lignes `b2l-gl: …` et `hakios-gl: …`.

## Livraison

- `scripts/build-gl.sh` (V=b2l-gl-v2) produit `libOSMesa.8.dylib` (`990f29af…`) et `libb2l-gl.dylib` (`2a7cd1f3…`).
- SYSTEM : copie fraîche du téléphone (`52ee4123…`, vérifiée), plus `/b2l-gl-v2/`, puis flash-delta de 7 blocs.
- Noyau : chemins passés en `b2l-gl-v2` (publication et injection).

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glhook-0032/boot.img` (26 054 000 octets) | `c653a4dfc8a383f293d27d407cb620a6963ab61e1aa39a3e02b215ac0731fcae` |
| Kernel | `185e377963f29fe8beef5e87733e3d31460571697861134d06f99f2fb4337209` |

## Retour arrière

Écran noir, ou backboardd qui boucle : reflasher `out/b1l-thermgpu-0030/boot.img`. Ce noyau n'injecte rien dans backboardd ; les fichiers sur SYSTEM sont alors ignorés.

# Build B1L-0035 : plus de recherche Metal dans le serveur d'affichage, filet de sécurité dès le départ

Date : 2026-11-06. Statut : **installé : SYSTEM (7 blocs relus, copie `fb1e5b1f…`) et BOOT (relecture `4f3af624…`) ; pas encore testé.** Base : B1L-0034.

## Constat sur B1L-0034

- Crochet posé, mais « renderer asked » n'apparaît jamais : notre fonction n'a pas été appelée. Le filet de sécurité, lancé dans cette fonction, ne s'est donc pas déclenché.
- Pile du thread principal de backboardd, relevée par les échantillons du noyau (`B2L WAITBT`) et symbolisée avec le cache 23G90, slide 0x19f20000 :
  `-[CAWindowServer _detectDisplays]` → `AppleDisplay::new_server` → `AccelServer::AccelServer` → `renderer()` → `CAMetalContextCreate` → bloc `dispatch_once` → `IOServiceGetMatchingServices` → `io_service_get_matching_services_bin`.
- L'attente est `kind=mach-recv` sur le port de réponse, de 214 à 234 s et plus : le noyau ne répond pas. Sans crochet (0031, 0032), ce thread n'est jamais échantillonné en attente. Mécanisme côté noyau non établi ; la copie des pages du cache partagé (unnest) est suspectée.

## Changements (`userspace/b2l_accel_gles.c`, bibliothèques `b2l-gl-v5`)

- `bl CAMetalContextCreate` en 0x183f3bde8 (`940b76ec`, vérifié avant écriture) devient `mov x0, #0` : plus de recherche Metal, chemin direct vers `new_metal_context` → GLES.
- Le chien de garde démarre dès la pose des patchs : sans rendu GLES 20 s après, marqueur pour ce boot, `abort`, puis retour au rendu CPU au redémarrage de backboardd.
- `libOSMesa.8.dylib` `b6beafc5…`, `libb2l-gl.dylib` `449bac66…`.

| Artefact | SHA-256 |
| --- | --- |
| `out/b1l-glhook4-0035/boot.img` | `4f3af62447ed1a34fbca50089fc4bc03af1d61c11193e62e0e7100f138b4407a` |
| Kernel | `4666c57b3e58d1ccabb73197c6490a123f3389ffa1ebacc60409439f1913ac00` |

Retour arrière : `out/b1l-thermgpu-0030/boot.img`.

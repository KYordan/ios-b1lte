# B1L-0252 — Dictaphone redevient Dictaphone (app Test Metal seulement sur demande)

## Pourquoi

Le propriétaire (via la session noyau) : « ya toujours le teste sur voice memo ». Depuis 0242, le noyau injecte
`libb2l-metal-app.dylib` (Test Metal) dans `/VoiceMemos.app/VoiceMemos` par défaut.

## Changement (`b2l_exec_graphics_diag.h`, `b2l_exec_app_inject`)

Injection coupée par défaut (`enabled = 0`). Boot-arg `b2l_metal_app=1` pour relancer le test dans Dictaphone.
Noyau seul (`--no-libs`), set v119 inchangé. Inclut 0251.

## Installé

- SYSTEM-OK, BOOT `46587916e26f1ff96e1ad3184b252a874b45247712e1568b515f6c175c670da3`, BOOT-OK.

## Résultat

- En attente : Dictaphone doit s'ouvrir normalement.

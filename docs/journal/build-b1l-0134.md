# Build B1L-0134 : le noyau accepte les appels IOSurface de chronod

Date : 2026-11-09. Base : B1L-0133. Ensemble `b2l-metal-v83` (pilote et Mesa inchangés).

## Diagnostic

- **Hypothèse :** les widgets (blancs) et les boutons ronds du Centre de contrôle (contrôles WidgetKit depuis iOS 18) sont produits par `chronod`.
- **Indices :**
  - dans le journal 0133, `chronod` recrée 15 fois ses paires de surfaces YCbCr ;
  - il reçoit `kIOReturnUnsupported` pour les sélecteurs 11, 20, 21 et 54.
- **Sélecteurs**, lus dans `IOSurface.framework` du cache (heuristique `mov w1, #N` en début de fonction exportée) :
  - 20 : `IOSurfaceClientSetPurgeable` ;
  - 21 : `IOSurfaceClientSetOwnership` ;
  - 54 : `IOSurfaceClientSetCoreVideoBridgedKeys` (les surfaces des CVPixelBuffer) ;
  - 11 : une requête de transaction ou de câblage.

## Changement (`B2LSurface.cpp`, `IOSurfaceRootUserClient::externalMethod`)

- Les sélecteurs 11, 20, 21 et 54 réussissent sans rien faire : les sorties scalaires et la structure de sortie sont mises à zéro.
- Trace `B2L SURF: chronod accepted N`, une fois par sélecteur.

## Installé

- **SYSTEM :** SYSTEM-OK en 58 s. **BOOT :** relecture `6e9d4664466069452865bb8d7069bc90ad9eb954bb342347581f7bd576c83a28` (BOOT-OK). Redémarré vers le système par `adb reboot`.
- **Retour en arrière :** `bash ~/Developer/s10-port/b1l-tools/flash-saved.sh 0128`.

## Résultat

- **Propriétaire : aucun changement.** Suite : nouvelle session avec `docs/prompt-session-rendu.md`.

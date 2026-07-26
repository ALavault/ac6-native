# AC6 cycle 250 — contrats restants des hooks D3D

> **Correction cycle 252.** La fonction `0x821DD188` n'est pas un bind de
> vertex declaration. Le consommateur indexé `0x821DF2C0` lit son champ
> `device+0x308C` comme un buffer (`+0x00`, `+0x18`) : il s'agit du bind
> d'index-buffer. Les instructions et chemins relevés ci-dessous restent
> valides, mais leur ancienne étiquette vertex declaration est obsolète.

## Identité

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6` ;
- checkout AC6Recomp : `.tools/ac6-recomp-reference`, base
  `c5b089fb6988ac504ba394db611543bda2fb2c96`.

Toutes les preuves ont été collectées avec Ghidra headless en lecture seule.
Aucun XEX, projet Ghidra, fichier généré ou configuration XenonRecomp n'a été
modifié. Aucune session Xenia, GUI, VNC ou humaine n'a été utilisée.

## Index buffer (étiquette corrigée au cycle 252)

La fonction physique `0x821DD188` reçoit `device` dans `r3` et le nouvel
index-buffer dans `r4`. Elle les conserve dans `r31/r29`, lit l'ancienne valeur
à `device+0x308C`, puis publie toujours `r29` à `0x821DD20C`.

Le chunk configuré `0x821DD1C8` n'est pas une frontière universelle : il est
contourné lorsque l'ancien buffer est nul ou lorsque certains
drapeaux device prennent le chemin rapide. Il reste donc pass-through et ne
peut pas maintenir un shadow state exact.

`0x821DE7D0` n'est pas un bind. Il appartient au helper de création
`0x821DE7A8`, qui reçoit une séquence d'éléments dans `r3` et un objet alloué
dans `r4`. L'ancien hook alimentait donc `vertex_declaration` avec le pointeur
de destination d'un constructeur et mémorisait le pointeur source comme
device. Cette capture a été supprimée.

Le bind d'index-buffer est confirmé, mais aucune capture complète n'est revendiquée tant
qu'un chunk universel à l'entrée ou à `0x821DD20C` n'est pas ajouté par une
modification de configuration suivie d'une régénération vérifiée.

## Captures fausses retirées

Trois autres adresses configurées n'exposaient pas l'ABI annoncée :

- `0x821D95C8` est un store d'état interne de `0x821D9588`; `r3` désigne son
  owner et il n'existe pas de couple index/surface live ;
- `0x821D9D38` est une branche d'un dispatcher `0x821D9CF8` qui compare `r3`
  au sélecteur `0x22`; `r3` n'est pas un device ;
- `0x821DA698` appartient au validateur/flush `0x821DA658`, appelé après la
  publication réelle du viewport, et ne reçoit pas un ABI stable
  `x/y/width/height`.

Ces trois hooks conservent leur continuation générée mais ne modifient plus le
shadow state et n'incrémentent plus de compteurs trompeurs.

## Viewport rétabli à sa frontière d'entrée

La vraie fonction `0x821DCEE8` reçoit `device` dans `r3` et un pointeur vers
quatre `uint32` dans `r4`. Elle publie les quatre valeurs à
`device+0x317C..0x3188`, calcule les registres viewport et appelle ensuite
`0x821DA658`.

Le chunk déjà configuré `0x821DCF28` intervient avant la réutilisation de `r4`.
Le hook source lit donc les quatre mots guest à cette frontière et alimente
`viewport`, sans modifier la configuration ni les sorties générées.

## Resolve et Clear

À `0x821E2BB8`, le wrapper Resolve a déjà remappé ses arguments. La capture
utilise désormais les copies qualifiées :

```text
device = r31
args   = r25, r26, r20, r27, r5, r22, r21
scale  = f30
```

L'ancien code relisait `r4..r10/f1`, dont plusieurs avaient déjà changé.

Pour Clear, les valeurs robustes déjà sauvegardées sont maintenant utilisées
pour `device`, `flags` et `depth` : `r29`, `r28` et `f31`. Les autres arguments
sont encore intacts au chunk `0x821E2380`.

## Draws indexés

Les deux hooks indexés étaient eux aussi situés après réutilisation de certains
registres volatils :

- à `0x821DEF18`, le contrat correct est
  `device/primitive/start/count = r31/r25/r21/r22` ;
- à `0x821DF300`, le contrat correct est
  `device/primitive/flags/start/count = r31/r16/r15/r19/r17`.

Le draw primitif `0x821DEA48` est antérieur à la réutilisation de `r3/r4/r5`
et reste inchangé.

## Validation

- index buffer (script au nom historique) : **20/20** assertions headless lors
  du cycle 250 ; contrat étendu et revalidé au cycle 252 ;
- frontières D3D restantes : **31/31** assertions headless ;
- draws : **18/18** assertions headless ;
- `d3d_hooks.cpp` et le bridge de capture passent Clang 21 en C++23 avec
  `-fsyntax-only -Wall -Wextra -Werror`.

Artefacts principaux :

- `artifacts/ac6-cycle250-vertex-declaration-audit.log` ;
- `artifacts/ac6-cycle250-vertex-declaration-callers.log` ;
- `artifacts/ac6-cycle250-vertex-declaration-validation.log` ;
- `artifacts/ac6-cycle250-remaining-d3d-boundaries.log` ;
- `artifacts/ac6-cycle250-remaining-d3d-validation.log` ;
- `artifacts/ac6-cycle250-draw-hook-boundaries.log` ;
- `artifacts/ac6-cycle250-draw-hook-validation.log` ;
- `artifacts/ac6-cycle250-hook-sources-final-syntax.log`.

## Frontières ouvertes

- trouver la vraie frontière vertex declaration ;
- identifier les vraies fonctions de bind render-target et depth-stencil ;
- conserver `render_targets`, `depth_stencil` et `vertex_declaration` à zéro
  plutôt que d'y injecter de faux pointeurs tant que ces frontières manquent ;
- reprendre ensuite seulement la jointure material/shader/draw.

La nouvelle archive AC6 annoncée n'était toujours pas visible lors de ce
cycle ; aucun contenu n'en est donc revendiqué comme inspecté.

# Premier diff de reachability D3D valide, et deux lectures refusées

Date : 2026-08-18

## L'instrument, enfin bon

`651e7878` avait rejeté un diff fondé sur `control_flow.edges`, qui ne couvre
que les appels indirects. Avec l'atlas complet de `probe --atlas` (2 779
fonctions, appels directs compris) et l'ensemble HIR de l'oracle, la comparaison
est valide.

Plage D3D `0x821A0000..0x821D0000`, confirmée comme telle par les noms XDK de
`1a5c4f76` :

```text
l'oracle exécute   931 fonctions
le natif atteint   429
oracle seul        502
natif seul           0
```

**Zéro fonction atteinte en natif que l'oracle n'atteint pas.** Le port est un
sous-ensemble strict, ce qui est exactement la forme attendue d'un arrêt en
cours de route, et non d'une divergence de chemin.

## Ce que l'oracle exécute et nous jamais

Échantillon de 14 adresses réparties sur les 502, nommées à fenêtre 32 — dont
le plancher mesuré est 0/40 (`b417938f`) :

```text
0x821C5D90  D3D::InitializeApiState(CDevice*)            29/60 d3d9 ET d3d9i
0x821B6A50  D3D::SetTileAndDepthClear(...)               22/60 et 9/60
0x821AFDB0  D3DDevice_SetSamplerState_MinFilter          17/60 et 4/60
0x821AE670  D3DDevice_SetRenderState_BlendOp             16/60
0x821BD5D8  D3D::CCapture::SavePages                      5/60 et 4/60
0x821C0B48  D3D::CCommandBuffer::GrowableAllocate         2/60
```

## Première lecture tentante, refusée

`VdInitializeEngines` est appelée une fois avec `r4 = 0x821C5D70`, et le
bridge l'implémente en `return 0` sans regarder cet argument. `sub_821C5D70`
s'avère être une fonction de neuf instructions qui écrit deux registres Xenos
(`0x7FC83214 = 7`, `0x7FC83408 = 2048`) avec barrières — la forme exacte d'une
callback que le noyau doit invoquer.

Contrôle : **`0x821C5D70` n'est pas dans l'ensemble oracle non plus.** Xenia ne
l'exécute pas davantage. La piste ne tient pas et n'est pas publiée comme
défaut.

## Seconde lecture tentante, refusée

`SetTileAndDepthClear` est appelée **directement** par `sub_821B6FD0`, atteinte
**11 754 fois** — une par image. Un appelant qui tourne à chaque image sans
jamais atteindre son appelé ressemble à un verrou.

Lecture de la garde :

```c
r25 = r23 & 0x200;
if (r25 == 0) goto loc_821B7388;   // saute l'appel
```

`0x200` est un bit du jeu de drapeaux de resolve passé par l'appelant. Le jeu
demande donc des resolves **sans** ce drapeau, et sauter l'appel est correct.
Que l'oracle l'atteigne s'explique par une scène différente, pas par une
capacité qui nous manque. C'est vraisemblablement une **conséquence** de ne pas
rendre, pas une cause.

## Ce qui reste, et qui est réel

`D3D::InitializeApiState` — 29/60 dans deux archives, le score le plus élevé de
tout ce travail d'appariement — est exécutée par l'oracle et jamais par nous.
Elle n'a **aucun appelant direct** dans le C++ généré : elle est donc atteinte
par pointeur ou table, et son absence ne s'explique par aucune garde lisible
d'ici.

## Non établi

- Comment `InitializeApiState` est appelée, donc pourquoi elle ne l'est pas.
- Si les 502 manquantes contiennent une cause ou seulement des conséquences.
  L'échantillon de 14 ne tranche pas, et le dire est plus utile que de
  généraliser depuis six noms.

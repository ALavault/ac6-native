# Cycle 1629 — neutral contre START au readback runtime

Date : 2026-08-15  
Cible : démo Xbox LIVE PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Neutral et START ont été rejoués depuis deux stores neufs avec le même binaire,
le même backend Vulkan et la même borne de 300 ticks. START est injecté une
seule fois au tick 252 (`buttons=0x0010`, contrôleur connecté).

Les deux routes atteignent exactement :

- 5 shader loads, 26 draws, 1 present ;
- 4 modules Vulkan, 2 pipelines ;
- 1 draw normal qualifié et 1 resolve neutral qualifié ;
- le même état scheduler et les mêmes milestones à tick 300 ;
- le même objet `graphics`, SHA-256 normalisé
  `a6492238651d85b547b77fe42e53f07a8b5dccf874beb230dca0933be93ac9db` ;
- le même readback noir 1280×720
  `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.

START ne provoque donc aucune transition visuelle ou milestone persistante dans
cette fenêtre et n'est pas promu comme entrée frontend légitime.

## Preuve XAM

Deux movies XAM complets de 48 appels ont été enregistrés en headless :

| Route | movie SHA-256 | payload | normalisé |
|---|---|---|---|
| neutral | `cb55e845…580a` | `b974b48c…935f` | `7eb955be…342d` |
| START | `bd61f6a0…63d7` | `b83d641b…7e7c` | `c5193f1b…ba1` |

La divergence est exacte et bornée :

- ordinal 0, tick 252, thread 1, caller LR `0x822F616C` : état START
  `00000001001000000000000000000000` ;
- ordinal 1 puis suivants, caller LR `0x822F60A8` : boutons relâchés et packet
  number 2, `00000002000000000000000000000000`.

Le guest reçoit donc START exactement une fois ; l'absence de transition ne
provient ni du HID ni d'un défaut de movie/replay. Les agrégats de contrôle
convergent avant tick 300 et ne suffisent pas à identifier le premier consumer
transitoire.

## Qualification

- `demo-qualified` : injection XAM, callers, ticks, PM4 et pixels ;
- `unknown` : première branche guest qui consomme le bit START et état logique
  qui l'annule ou le rejette ;
- aucune preuve retail fusionnée, aucun actif propriétaire suivi.

## Prochain checkpoint

Instrumenter uniquement le retour de `XamInputGetState` au caller
`0x822F616C`, capturer les premières lectures/branches dépendant des 16 octets
d'état et leur writer éventuel, puis joindre le premier état guest persistant.
Ne pas prolonger aveuglément la durée ni réinjecter START avant cette preuve.

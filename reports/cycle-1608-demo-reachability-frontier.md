# Cycle 1608 — reachability neutral/START de la démo

## Résultat

Le hook d'entrée agrégé est désactivé par défaut et activé uniquement par
`probe --atlas FILE`. Il enregistre l'adresse exacte de fonction, les premier
et dernier ticks et le compteur, sans écriture guest. Le writer atomique exige
un movie XAM record ou replay et refuse toute collision avec trace, rapport ou
movie.

Deux replays START depuis des stores neufs produisent un atlas byte-identique,
SHA-256 `525e9b31e6684adcf87536e8b4f9dcac80b6f5e1579331f31f5d798d52a842e0` :
2 285 fonctions, 12 591 995 entrées et ticks 0..252. Le schéma
`ac6-demo-reachability-atlas/v1` est validé.

Le contrôle neutral, lui aussi depuis un store neuf, atteint exactement les
mêmes 2 285 fonctions. Son atlas vaut
`5913b6426f90a8671f6ee9c5338e6964083b394b5c0b90593a7d1a0ee3cf5dee`.
La seule différence de compte est +167 appels neutral pour chacun des wrappers
d'une instruction `0x822E1DF0` et `0x822E1DF8`, qualifiés statiquement comme
queues vers `RtlEnterCriticalSection` et `RtlLeaveCriticalSection`.

## Frontière

Les adresses `0x82170FCC` et `0x82185210` sont des instructions internes. Leurs
frontières indépendantes exactes sont respectivement `0x82170F58` et
`0x82185198`, toutes deux slot 4 de vtables RTTI qualifiées. Aucune n'est
atteinte par neutral ou START. Un replay XAM sans HID prolongé jusqu'au tick
260 conserve le même ensemble de 2 285 fonctions et 115 appels PRESENT, sans
frontend qualifié. Il n'y a donc ni writer ni consumer persistant à revendiquer.

## Validation

- CTest codegen OFF : 14/14 ;
- CTest codegen ON : 13/13 ;
- tests unitaires du hook : activation, agrégation, ordre et rejet d'un nom
  invalide ;
- atlas START record/replay : byte-identique ;
- atlas neutral et START : schéma v1 valide ;
- replay prolongé : movie XAM strict, aucun HID.

L'interface publique `replay TRACE --xam-movie-replay MOVIE` est maintenant
implémentée. Un replay complet de 254 ticks depuis un store neuf reproduit la
trace RTPLY-v4 (`deterministic=true`, 766 événements), consomme exactement le
movie START et refuse tout événement XAM divergent ou restant. Le test CLI
codegen OFF prouve que ni trace, ni store, ni movie ne sont lus sans guest lié.

Le prochain lot doit qualifier le producteur ou dispatcher de la tâche
virtuelle avant ces deux slots, ou localiser l'invariant scheduler qui empêche
leur activation. Aucune synthèse d'état frontend n'est autorisée. Les six lanes
jouables restent ouvertes.

# Cycle 1655 — premier accès guest après activation post-`NtSetEvent`

## Résultat

Le hook `AC6_DEMO_WATCH_EVENT_POST_SET` reste opt-in et read-only. Il arme un
marqueur lors de l’activation scheduler et consigne le premier load/store du
thread repris. Deux stores neufs, neutral et START (entrée au tick 252), ont
été rejoués jusqu’au tick 300 avec le même binaire codegen ON.

Les deux routes produisent exactement 351 lignes mémoire, le même ordre
d'opération/adresse/largeur/thread/LR/entry et le digest
`1fb44bae233f8499cbd9e284a1e58248d799ac68b41b48befb327fca76ca7c61`. Chaque
ligne conserve `tick == set_tick == resume_tick`; le lien
`event_wake → activation → premier accès` est donc qualifié au niveau
événementiel, sans attribuer une sémantique aux adresses.

## A/B

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| lignes mémoire post-set | 351 | 351 |
| delta tick nul | 351/351 | 351/351 |
| digest lignes mémoire | `1fb44bae…6ca7c61` | `1fb44bae…6ca7c61` |
| opérations | 10/13/246/47 loads 8/16/32/64; 4/6/20/5 stores 8/16/32/64 | identique |
| entries | 0x821A7160=53; 0x821C4970=149; 0x82320560=97; 0x822EE158=39; 0x821A1D10=13 | identique |
| PRESENT | 163 | 163 |
| frontend / mission / terminal | non / non / non | non / non / non |

Les stderr ont chacun le SHA `de03c62b…2de843b`, les traces RTPLY restent
`2e49ae67…ad9c115` (neutral) et `179db68a…235345b` (START), et les rapports
`a11feac6…2a4340` / `c74ca0ea…3012593`.

## Accès observés

| entry | premier accès représentatif | thread | LR |
|---|---|---:|---|
| `0x821A7160` | `load64 0x7F0409D8`; puis service `load16 0x7F0407CC` | 1 | `0x821A69CC` / `0x82278F80` |
| `0x821C4970` | `load32 0x82000608` | 18 | `0x821C4A28` |
| `0x82320560` | `load32 0x2EB40EE8` | 21 | `0x821A877C` |
| `0x822EE158` | `load64 0x1678CDF0` | 14 | `0x821A69CC` |
| `0x821A1D10` | `store16 0x102C5AC0` dans la fenêtre observée | 9 | `0x82278F80` |

Au total, 56 adresses guest distinctes sont observées. Les stores liés au
service portent parfois le label généré `__imp__sub_823273E0`; ce label reste
un repère de recompilation, pas une preuve de rôle.

## Qualification et blocage

- `demo-qualified` : égalité A/B de la séquence mémoire, bornage à un accès par
  activation, ticks et threads, et cinq frontières statiques PAL déjà jointes.
- `demo-observed` : adresses, types de load/store, LR et familles d'entry.
- `xenia-generic` : aucune preuve fusionnée.
- `unknown` : sémantique des adresses, producteur/consumer exact de l'IB
  `0x1274A000`, relation à Xenos/PM4, pixels, frontend et mission.

Le hook ne modifie aucun état guest et n'arme pas de readback. Le prochain test
minimal est de joindre une seule adresse à un writer/reader PAL exact puis à
une soumission PM4, avec arrêt immédiat si le champ ou la provenance est
inconnu. Une screencap n'est toujours pas qualifiée.

## Validation

- A/B frais : deux routes, 300 ticks, 351/351 lignes mémoire, digest identique.
- Le hook est désactivé par défaut et borné à 8 192 lignes.
- C++ généré, Ghidra, Xenia/ReXGlue, microcodes et actifs propriétaires non
  modifiés ni suivis.
- `pytest tests/test_build_demo.py -q` : `49 passed, 4 subtests passed`.
- CTest codegen ON : `17/17`; build démo : `18/18`.
- `audit_demo_sources.py` et `audit_cpp_complexity.py --check` : `pass`;
  `guest_bridge.cpp` reste à 1 200 lignes.

Capsule : `analysis/demo/ac6-demo-event-post-set-memory-join-v1.json`.

# Cycle 1649 — consumers guest observés des handles PAL

## Résultat

Deux processus codegen-ON frais ont été exécutés depuis le store démo PAL
`.build/ac6-demo-store-test-3`, neutral puis START `0x0010` au tick 252, avec
le hook opt-in `AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS=1`. Le hook observe les
lectures `AC6_PPC_LOAD_U32` dont la valeur est un handle bridge connu ou un
handle de thread guest. Il ne modifie ni la mémoire, ni l’ordonnanceur, ni
l’entrée.

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| PRESENT | 163 | 163 |
| lignes consumer | 3 927 | 3 927 |
| SHA lignes consumer | `19a7e07c…7ff8474` | `19a7e07c…7ff8474` |
| SHA stderr complet | `9f19ca80…e45355dc9b` | `9f19ca80…e45355dc9b` |
| groupes (adresse, valeur, LR) | 370 | 370 |
| frontend / mission / terminal | non / non / non | non / non / non |

Le reçu durable est
[`ac6-demo-event-handle-consumer-probe-v1.json`](../analysis/demo/ac6-demo-event-handle-consumer-probe-v1.json).

## Lectures observées et jointure statique

Les premières lectures de la table d’initialisation sont observées au tick 0 :
`0x7F040B58 → E1000000/E1000004` avec `context_lr_observed=0x821A8D00`,
et `0x7F040B28 → E0000000/E0000004` avec `context_lr_observed=0x821A6364`.
Les couples répétés utilisés par les chemins d’attente sont notamment :

| adresse guest lue | valeur | LR observé | occurrences | ticks | threads |
|---|---|---|---:|---|---|
| `0x82934748` | `E0000048` | `0x822EEE38` | 351 | 1–299 | 1,12 |
| `0x8293474C` | `E000004C` | `0x822EEE44` | 351 | 1–299 | 1,12 |
| `0x82934748` | `E0000048` | `0x822E3F1C` | 299 | 1–299 | 12 |
| `0x82933F98` | `E0000054` | `0x822EEEA0` | 211 | 0–299 | 1,14 |
| `0x82933F9C` | `E0000058` | `0x822EEEA0` | 106 | 0–299 | 1,14 |

Le tableau complet des groupes fréquents et les 3 927 lignes normalisées sont
résumés par hash dans le JSON. Les valeurs `context_lr_observed` sont jointes
aux frontières de l’atlas statique (`Function_821A8CB8`,
`Function_821A62F0`, `Function_822EEE10`, `Function_822E3EC0`,
`Function_822EEE68`, `Function_822E4080`). Cette jointure qualifie une
fonction contenante, pas l’instruction exacte de chargement.

## A/B, provenance et limites

- `demo-qualified` : adresse de lecture, valeur chargée, tick, thread, stream
  A/B identique et frontières PAL de la fonction contenante ; le hook est
  désactivé par défaut et strictement read-only.
- `demo-observed` : `context_lr_observed`, valeurs `E000…/E100…` et fréquence
  des couples.
- `xenia-generic` : uniquement l’interprétation des handles `F800…` et la pile
  POSIX documentée par l’archive Xenia ; aucune sémantique PAL n’en est déduite.
- `unknown` : PC exact de l’instruction `load`, type d’objet PAL, payload et
  arête causale writer→consumer.

Les lignes consumer sont byte-identiques entre neutral et START ; START ne
peut donc pas encore être promu comme transition guest ou visuelle. Le
readback demeure noir et aucun frontend guest-owned n’est atteint : aucune
screencap n’est produite.

## Prochain checkpoint

Tracer un seul couple prioritaire (`0x82934748/0x8293474C`) dans une fenêtre
exacte avec PC guest/LR/thread/tick, puis rejoindre son payload au site writer
du cycle 1648. Le test doit rester A/B, opt-in et fail-closed ; en cas de
chargement hors plage ou d’objet non résolu, arrêter sans resynchronisation.

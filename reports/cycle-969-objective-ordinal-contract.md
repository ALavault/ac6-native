# Cycle 969 — ordinal campagne des objectifs

## Défaut fermé

La voie campagne appelait `can_complete_objective` et
`complete_objective` avec `objective_id - 1`. Cette hypothèse n’est valide que
si les IDs commencent à 1 et sont contigus ; elle n’appartient pas au contrat
des manifestes d’objectifs, qui séparent l’identité stable du rang.

## Correction

`MissionScenario::objective_index` retourne l’ordinal des objectifs présents,
sur leur snapshot trié et donc de manière déterministe. `MissionExecution`
utilise cet ordinal pour les deux appels campagne et refuse un ID absent avant
toute mutation.

## Preuve

Le test runtime charge les IDs `10` et `20`, les active et les complète dans
une mission campagne à deux objectifs, puis vérifie l’état `Completed`.

```text
cmake --build build -j2
DISPLAY=:98 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
```

La correction est générique et ne qualifie pas encore les IDs retail de la
Mission 1 ; ceux-ci restent à fournir par le catalogue d’assets qualifié.

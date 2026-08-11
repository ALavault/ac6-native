# Cycle 1519 — cadence du scheduler de mission retail

## Delivered

`RetailSessionConfig::advance_script_each_tick` ajoute le chemin produit du
signal -2 retail : `RetailSession::tick` appelle le driver du script avant le
tick de simulation fixe, puis les transitions de sous-mission activent et
complètent les objectifs natifs. `retail play` et `retail replay` activent ce
mode ; les ouvertures payload-only restent explicitement pilotables par le
caller pour conserver les fixtures de cadence et les tests de parser.

La progression n’est plus forcée par une cadence choisie par la commande : la
Mission 01 qualifiée épuise ses six étapes au tick 6 et atteint le débrief avec
ses quatre objectifs complétés. Les 15 bundles du cache qualifié ont été
ouverts avec ce scheduler et exécutés pendant huit ticks fixes ; les traces
restent bornées et aucune ouverture ne dépend d’un manifeste.

## Validation

- `cmake --build reconstruction/ace-combat-6/build -j16 --target
  ac6-retail-session-tests ac6-native` : passe.
- Tests ciblés `ac6-retail-session` et `ac6-retail-session-replay` : 2/2.
- Validation cache `/tmp/ac6-retail-v2-smoke` (index
  `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`) :
  missions 1–15 ouvertes et tickées ; missions 1–14 terminent leur script
  dans la fenêtre de sonde, mission 15 reste en gameplay après 8 ticks avec sa
  trace intacte.

## Boundary retained

Les gardes runtime de `0x822ED708` et les producteurs de compteurs issus de
l’IA/combat ne sont pas encore qualifiés ; les conditions tag-7 suivent donc
le lecteur de compteurs existant et ne sont pas remplacées par une valeur
inventée. Les succès humains JP, les captures JG et la campagne complète
restent ouverts.

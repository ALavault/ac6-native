# Checkpoint 4n — classification de l’écart de producteur

Date : 2026-08-12

Le comparateur `ac6.execution-first-divergence.v2` distingue maintenant deux
cas : `value_divergence` pour une valeur comparable différente, et
`producer_schema_mismatch` lorsqu’un champ est absent d’un payload et présent
dans l’autre. Le comparateur reste fail-closed (code retour 1), mais le rapport
n’attribue plus à la simulation un champ dont le contrat n’est pas partagé.

Sur les captures courantes, la comparaison donne :

```text
execution_trace_compare=producer_schema_mismatch sequence=1 tick=1 \
domain=simulation_snapshot path=events[1].payload.active_units
```

Le natif publie `active_units=230`, compteur des unités actives construites
depuis le scénario retail et resynchronisées depuis `CombatWorld`. L’oracle
publie à cette frontière des mots de transformée joueur/enfant bruts, sans
compteur sémantique qualifié. Aucun adaptateur de schéma n’est donc introduit.

Le test du comparateur couvre la classification et le test raster vérifie que
la conversion RGBA réutilise la capacité et l’adresse du buffer appelant.

# Réparation des preuves agrégées de la branche `infos`

## Archives retirées

La branche réparée ne contient plus :

```text
analysis/ac6-demo-campaign-result-save-gallery-evidence-20260817.zip
analysis/ac6-demo-orchestration-service-debriefing-evidence-20260817.zip
analysis/ac6-demo-ranking-board-evidence-20260817.zip
```

Le fast-forward de `infos` vers le `main` actuel importe les membres récupérables
déjà versionnés en clair et supprime les trois blobs ZIP tronqués du tree courant.

Cette passe complète la réparation avec :

```text
analysis/ac6-demo-ranking-board-20260817/ranking_ui_callback_contract.csv
analysis/ac6-demo-campaign-result-save-20260817/mission_result_layout.csv
analysis/ac6-demo-result-checkpoint-20260817/act_start_condition_summary.csv
```

`ranking_ui_callback_contract.csv` est restauré exactement depuis la copie locale
valide. `mission_result_layout.csv` est régénéré depuis le rapport et ses
consommateurs PPC ; sa provenance est déclarée explicitement. Le grand tableau
des conditions de démarrage d'Act reste reproductible depuis le blob de mission
et l'extracteur ; le remote publie ici un résumé exhaustif de ses cardinalités et
des arêtes discriminantes plutôt qu'un nouvel agrégat binaire.

## Statut canonique

Les CSV, JSON, Markdown et scripts en clair sont les seules preuves canoniques.
Les anciens ZIP subsistent dans l'historique Git, comme toute erreur déjà
commise avec assez d'assurance, mais ils sont absents du tree courant.

# AC6 PAL démo — reachability des consommateurs START, cycle 1776

Verdict : **START-CONSUMER-NEGATIVE / FRONTEND-NO-GO**, `supported=false`.

La capsule fraîche [menu-consumer-reach-ab](../analysis/demo/ac6-demo-menu-consumer-reach-ab/sha256/b5a4a75b7347fe942cda22e6217a835b4765dea3b959ec53e729b6b10012aca3/receipt.json), basée sur le binaire `cf90f551…e87b0`, rejoue `neutral` et `buttons16` jusqu'au tick 253 avec movies XAM enregistrés et atlas de fonctions activé.

| route | fonctions | `0x82170F58` | `0x82185198` | ticks/PRESENT | résultat |
|---|---:|---|---|---:|---|
| `neutral` | 2286 | absent | absent | 253/116 | `rc=4`, readback noir |
| `buttons16` | 2286 | absent | absent | 253/116 | `rc=4`, readback noir |

Les deux routes ont le même ensemble de fonctions atteintes. Les propriétaires
virtuels de `0x82170FCC` (`CModeTaskDemoBase::update`) et `0x82185210`
(`CModeTaskMissionTitle::update`) ne sont donc pas construits/atteints dans
ce chemin scheduler. `frontend`, `mission` et `terminal` restent faux ; aucune
observation MCP supplémentaire n'est exposée.

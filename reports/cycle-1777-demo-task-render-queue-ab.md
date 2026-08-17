# AC6 PAL démo — producteur/consommateur de file de rendu, cycle 1777

Verdict : **TASK-PRODUCER-QUEUE-QUALIFIED / MENU-CONSTRUCTION-NO-GO**, `supported=false`.

La capsule [task-render-queue-ab](../analysis/demo/ac6-demo-task-render-queue-ab/sha256/d710d620a66f3578520f8281e97b868323a424f2db5d95b1d691206da5ad0e02/receipt.json) rejoue deux stores frais au binaire `cf90f551…e87b0`, jusqu'au tick 600.

| route | writes indices | stores slots | snapshots 96 o | producteur | consommateur | résultat |
|---|---:|---:|---:|---:|---:|---|
| `neutral` | 2090 | 2440 | 696 | 695 | 0 | `rc=4`, readback noir |
| `buttons16` | 2090 | 2440 | 696 | 695 | 0 | `rc=4`, readback noir |

Les stderr sont byte-identiques. Les snapshots de slots sont tous nuls et
identiques (`3ef591eb…b1def`). Les stores exacts mappent les frontières
primaire `0x820FF8D8`, worker `0x820FFCA0` et le producteur
`0x820FF710` aux bytes PAL ; le worker réinitialise les indices après chaque
publication. Aucun objet correspondant aux propriétaires menu
`0x82170F58`/`0x82185198` n'est construit dans ce chemin.

Au callsite dispatcher `0x82259D74`, les trois entrées dynamiques restent
`CModeTaskStartUpDemoOffline` (`0x2E7F0080/0x8201130C`), `CTaskLoading`
(`0x18BA2BF4/0x8200F388`) et `CTaskModeManager`
(`0x18980000/0x82011694`), 348 appels chacun entre les ticks 252 et 599 ;
aucune vtable menu n'apparaît.

`frontend`, `mission`, `terminal` restent faux à `PRESENT=463`. La file est
qualifiée comme frontière scheduler, pas comme renderer ou menu ; aucune
observation MCP supplémentaire n'est exposée.

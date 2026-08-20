# AC6 PAL démo — valeurs de payload de file, cycle 1778

Verdict : **QUEUE-INDEX-ONLY / FRONTEND-NO-GO**, `supported=false`.

Deux imports froids de `Default.xex` PAL (`de917873…05da8`) ont atteint le
tick 600 en headless avec les traces de slots de file activées. START au tick
252 (`0x10`) ne modifie aucun des 2 440 stores : stderr identique
(`8a9fd9fa…0b082`), 697 stores non nuls dans chaque route, et mêmes états
graphics/scheduler/imports.

Le premier non-nul est `0x8238CD9C = 1` au tick 221. Les premiers indices
producteur/consommateur sont `0x8238CD90 = 1` (thread 1) et
`0x8238CD94 = 1` (thread 25) au tick 252. Ce sont des compteurs de file, non
un payload de commande ou une construction de tâche menu.

Les deux routes terminent à `max_ticks=600`, `PRESENT=463`, avec
`frontend=false`, `mission=false` et `terminal=false`. Les digests complets
sont consignés dans
[`ac6-demo-queue-payload-ab-v1.json`](../analysis/demo/ac6-demo-queue-payload-ab-v1.json).

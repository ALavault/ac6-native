# AC6 PAL démo — portée de la fenêtre START, cycle 1780

Les A/B courts à START tick 252 qualifient seulement le bootstrap : ils ne
testent pas le parcours title. Les preuves PAL déjà qualifiées placent la
fenêtre title au tick 3000, avec réponse du guest au tick 3001 :
`0x820DC224 → 0x820D32D0` puis `0x820D3AC8`.

Conséquence : les verdicts queue et mode-manager jusqu’au tick 600 restent
des refus corrects pour leur fenêtre, mais ne peuvent pas réfuter la route
title. La prochaine exécution doit être un replay froid avec START au tick
3000, un atlas et une movie XAM scellée, puis doit exiger une transition
guest persistante et un readback non noir avant toute promotion frontend.

Cette note ne promeut aucun jalon : les preuves title existantes concluent
encore à l’absence de transition menu/mission qualifiée.

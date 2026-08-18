# START arrête la boucle d'attract, sans la faire transiter

Date : 2026-08-18
Guest : jeu de frontières à 349 chunks confirmés (`478e31ed`), les deux runs
sur le **même** binaire

## Les deux runs

```text
contrôle, sans entrée   6 transitions : 222 2429 4255 6432 8258 10435
START au tick 3000      2 transitions : 222 2429, puis titre tenu jusqu'à 12000
```

Les deux terminent en `max_ticks` à 12 000 ticks, 11 863 présentations,
2 soumissions au ring, `frontend` faux, aucune trap.

## Ce qui est établi

L'appui est reçu et **arrête la boucle d'attract**. Le contrôle tourne sur le
même guest, donc l'écart n'est pas imputable à l'expansion de frontières : une
mesure antérieure comparait un run avec appui au build neuf contre un run sans
appui au build ancien, ce qui n'aurait rien prouvé.

L'expansion, elle, a fait ce qu'on lui demandait : `0x820D3710` et ses 194
voisines sont émises, et l'appui **ne trape plus**. Il traverse désormais le
code qu'il ne pouvait pas exécuter.

## Ce qui n'est pas établi

Le mode ne transite pas. L'écran-titre cesse d'alterner mais ne cède la place
à rien, et rien n'est soumis au ring.

La lecture cohérente avec le reste de la session — sans être démontrée — est
que le mode suivant exige le sous-système de mission,
`CX360MissionManager` → `CX360UnitManager`, dont les sept sites de
construction sont précisément ceux que ce port n'atteint jamais
(`reports/AC6_DEMO_RENDER_GATE_RAISER.md`). Un seul manque expliquerait alors
les deux symptômes que la campagne poursuivait séparément : l'écran noir et le
titre immobile.

Ce qu'il faudrait pour trancher : savoir ce que l'écran-titre attend après
l'appui. La réponse n'est pas dans l'appui, elle est dans ce que son
consommateur fait ensuite.

# Une file de rendu avec un producteur et aucun consommateur

Date : 2026-08-18

## Ce que l'instantané d'ordonnanceur disait déjà

Il était dans chaque rapport de sonde de cette session, et je ne l'avais pas
lu :

```text
threads 23   runnable 0   blocked 23   finished 0
render_queue  producer_changes = 7495
              consumer_changes = 0
              last_producer_tick = 3999   last_consumer_tick = 0
              max_pending = 1
```

Le producteur de la file de rendu avance **7 495 fois** sur 4 000 ticks. Le
consommateur ne bouge **pas une seule fois**. C'est l'expression la plus directe
de la panne obtenue jusqu'ici, et elle ne demandait aucune fouille dans les
internes D3D.

## Où les threads attendent

Vingt-trois threads bloqués, aucun exécutable. Les LR d'attente :

```text
0x821A8C88   16 threads    (workers)
0x821A69CC    2
0x821C4A28    2            <- l'attente GPU de sub_821C4970
0x821A877C    2
0x82350130    1            (client XAudio)
```

Les deux threads en `0x821C4A28` attendent les événements `0x100446F4` et
`0x10044744`. C'est exactement le `KeWaitForSingleObject` dont `c401042f` a
montré qu'il expire, empêchant le `KeResetEvent` que l'oracle appelle 2 246
fois. Le log de l'oracle porte `KeResetEvent(400046F4)` — mêmes seize bits de
poids faible, autre espace d'adressage.

## Un suspect éliminé

L'explication évidente serait que le port ne délivre jamais l'interruption
graphique. Elle est fausse :

```text
0x821B9710  callback d'interruption graphique   atteinte 12 001 fois
```

Une fois par tick. Le port **délivre** l'interruption ; le callback s'exécute et
ne signale rien, ce qui est cohérent avec un GPU qui n'a franchi aucune nouvelle
barrière puisque l'anneau n'a pas avancé depuis le tick 0.

Écarter cette hypothèse vaut le détour : c'était le premier réflexe, et il
aurait coûté une itération.

## L'état, dit d'une phrase

Le jeu produit du travail de rendu — 7 495 avances de producteur, 163 930
écritures de buffer indirect — et rien ne le consomme, parce que les threads qui
le consommeraient attendent des événements que seul un GPU ayant avancé
signalerait.

## Non établi

- Lequel de ces deux faits précède l'autre. « L'anneau n'avance pas donc les
  événements ne partent pas » et « les consommateurs n'avancent pas donc
  l'anneau n'est pas rempli » sont deux lectures du même cycle, et rien ici ne
  les départage.
- Ce que fait `0x821B9710` à chacun de ses 12 001 appels.

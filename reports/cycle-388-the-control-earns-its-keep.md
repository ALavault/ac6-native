# Cycle 388 — le témoin fait exactement son travail

## 1. Correction apportée

Deux lignes ajoutées à la condition d'omission : un compteur atomique et une
journalisation périodique (`[ac6-skip] skipped draw #N binding base=...`).

## 2. Ce qu'il révèle immédiatement

Rejeu du protocole du cycle 387, à l'identique :

```
skip fired : 0
deltas image : 0, 0, 0, 0, 0, 0, 0
```

**L'omission ne s'est jamais déclenchée.** Le zéro du cycle 387 était donc
l'explication (2) — « la sonde n'a pas tourné » — et non la confirmation
cherchée. Sans ce témoin, ce zéro aurait pu être lu comme « les dessins de
`03514000` ne produisent aucun pixel », c'est-à-dire exactement la conclusion
souhaitée, et fausse.

Seconde exécution, valeur passée en décimal (`55591936`) au cas où l'analyseur
de cvar rejetterait l'hexadécimal :

```
skip fired : 0
deltas image : 19860, 19689, 19155, 20298, 2891, 13350, 19154
```

Toujours zéro déclenchement — mais les deltas non nuls montrent que **cette
exécution n'a pas atteint l'écran de sauvegarde** : elle est restée sur un écran
animé. `03514000` n'y est jamais lié, donc l'omission n'avait rien à omettre.

La question « hexadécimal ou décimal » reste **non tranchée** : les deux
exécutions ont échoué pour des raisons différentes, et aucune n'a atteint l'état
où le test porte.

## 3. Valeur de ce cycle

Il ne fait avancer aucune hypothèse sur le défaut. Il fait quelque chose d'autre,
et de nécessaire : il **empêche une fausse conclusion** que le cycle 387 aurait
pu produire, et il rend le prochain essai lisible.

C'est la huitième fois que la règle du canal vivant intervient dans cette
enquête. Les sept premières corrigeaient après coup ; celle-ci a été posée
*avant* la lecture, et c'est la seule différence qui compte.

## 4. Ce qu'il reste à faire, sans ambiguïté

1. atteindre l'écran de sauvegarde de façon fiable — la séquence
   `A,A,A / Start / A / A` n'y parvient pas systématiquement ;
2. confirmer par le journal `[ac6-skip]` que l'omission se déclenche ;
3. alors seulement lire les deltas d'image.

Tant que (2) n'est pas vert, aucun résultat de ce test n'est interprétable.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

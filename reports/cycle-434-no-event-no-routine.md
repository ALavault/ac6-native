# Cycle 434 — mesuré : l'`overlapped` du sélecteur n'a ni événement ni routine

## 1. Relevé

```
[ovl] A33001AC result=00000000 event=00000000 routine=00000000 context=F8000020 len=0
[ovl] A33001D0 result=00000000 event=FEFEFEFE routine=FEFEFEFE context=F8000020 len=49
XamShowDeviceSelectorUI(…, A33000B8, A33000BC)
[ovl] A33000BC result=00000000 event=00000000 routine=00000000 context=F8000020 len=0
```

Pour `A33000BC`, l'`overlapped` du sélecteur : **événement = 0, routine de
complétion = 0**, résultat écrit à 0 (`SUCCESS`).

## 2. La piste du cycle 433 est morte

L'enchaînement proposé — champs à `FEFEFEFE`, routine lue comme non nulle, APC
adressée à un contexte invalide, donc jamais déposée — **ne s'applique pas
ici**. Il n'y a ni événement à signaler ni routine à appeler : rien à déposer.

`FEFEFEFE` existe bien, mais sur `A33001D0`, un autre appel. C'est une anomalie
réelle et distincte, à traiter séparément ; elle n'a aucun rapport avec le
sélecteur.

Le cycle 433 énonçait trois issues possibles et disait laquelle tuerait la
piste. C'est celle-là. La mesure a tranché en un cycle.

## 3. Ce que cela implique

Sans événement ni routine, l'invité **ne peut vouloir qu'une chose** : relire
lui-même l'`overlapped`. C'est le schéma normal d'une attente par scrutation.

Or le cycle 432 a montré que la routine de mise à jour, voyant `[obj+84] ≠ 0`,
**retourne immédiatement sans rien lire**. Personne ne scrute donc
l'`overlapped`, et personne ne remet l'état à 0.

Les deux faits, désormais tous deux mesurés, se rejoignent : l'invité a préparé
une attente par scrutation, et le code qui devrait scruter ne s'exécute pas.

## 4. La question, réduite à sa dernière forme

Quel code, chez l'invité, devrait relire `A33000BC` — et pourquoi n'est-il pas
atteint ? C'est un unique maillon, et les deux extrémités sont connues et
vérifiées.

`context = F8000020` est identique sur les trois `overlapped` : c'est le
descripteur de fil posé par `CompleteOverlappedDeferredEx`, pas une valeur de
l'invité. Sans intérêt pour la suite.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# Cycle 433 — `FEFEFEFE` dans une structure `overlapped`

## 1. Ce que le journal montre

```
12:13:16.602  Deferred overlapped A33001D0: running completion
12:13:16.602  CompleteOverlappedEx: missing XEvent for handle FEFEFEFE
```

`0xFEFEFEFE` est le motif classique de **mémoire non initialisée**. C'est
exactement la valeur que la sonde de dialogue relevait déjà dans les champs de
l'objet au cycle 417 (`+96 = FEFEFEFE`, `+100 = FEFEFEFE`, …).

## 2. Pourquoi cela pourrait compter

`CompleteOverlappedEx` fait trois choses avec la structure : écrire résultat,
erreur et longueur ; signaler l'événement s'il existe ; **et déposer une APC si
`XOverlappedGetCompletionRoutine` est non nul.**

Si les champs de l'`overlapped` contiennent `FEFEFEFE` :

- la recherche d'événement échoue — c'est l'avertissement observé ;
- la routine de complétion est lue comme `0xFEFEFEFE`, donc **non nulle** ;
- l'APC est alors adressée au contexte `FEFEFEFE`, `LookupObject` échoue, et
  **aucune APC n'est déposée**.

La routine de complétion de l'invité ne s'exécuterait donc jamais — et c'est un
candidat naturel pour remettre `[obj+84]` à 0, le champ dont le cycle 432 n'a
trouvé aucun autre écrivain.

## 3. Réserves, importantes

- L'avertissement porte sur `A33001D0`, **pas** sur `A33000BC`, l'`overlapped`
  du sélecteur. Aucun avertissement n'apparaît pour ce dernier, donc son champ
  événement valait vraisemblablement 0.
- Je n'ai **pas** relevé le contenu des champs de `A33000BC`. L'enchaînement
  ci-dessus est cohérent mais **non mesuré**.

C'est donc une piste, pas un diagnostic. Après trois lectures de code erronées
en quatre cycles, je préfère le dire nettement.

## 4. Vérification, courte et décisive

Journaliser, dans `CompleteOverlappedEx`, les champs lus de l'`overlapped` —
événement, routine de complétion, contexte — pour `A33000BC`. Trois issues :

- champs à zéro → pas d'APC attendue, la piste tombe ;
- champs à `FEFEFEFE` → l'invité n'a pas initialisé la structure, et il faut
  comprendre pourquoi ;
- routine valide mais APC non déposée → le défaut est dans notre dépôt d'APC.

Chacune désigne une suite différente et sans ambiguïté.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

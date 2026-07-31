# Cycle 430 — la routine de mise à jour de l'écran est identifiée

## 1. Chaîne complète, du bouton à l'écran

| niveau | fonction | rôle | statut |
|---|---|---|---|
| noyau | `XamShowDeviceSelectorUI` | modale | ✔ se termine, `SUCCESS` (417-418) |
| invité | `sub_821CE8A8` | **lance** le sélecteur, pose `[obj+84]` | ✔ décodé (428-429) |
| invité | **`sub_821C56F8`** | **mise à jour de l'écran** | ← ici |

`sub_821C56F8` appelle `sub_821CE8A8` depuis **deux** sites (`0x821C57B8` et un
second), en lui passant l'objet sélecteur `[r31+4]` et `r4 = [r31+396]`.

## 2. Structure entrevue

Au site examiné :

```
r11 = [this+396] ; r5 = 1 ; r4 = r11
bl sub_821CE8A8
[this+72] = r23
```

et juste après, une lecture conditionnelle qui choisit entre `[r3+76]` et
`[r3+72]` selon que `[r3+80] == 1`. La routine porte donc plusieurs champs
d'état, distincts de `[obj+84]`.

## 3. Ce que je ne conclus pas

`sub_821C56F8` commence ligne 16509 et s'étend bien au-delà de 17013 : c'est une
fonction longue, à branches multiples. **Je n'en ai lu que quarante lignes.**

Je ne dis donc pas où elle bloque, ni si c'est elle qui devrait constater la
complétion. Ce qui est établi est seulement qu'elle est l'appelante, et donc le
prochain endroit à lire — pas la coupable désignée.

C'est la précaution qui manquait aux cycles 428 et 412, où une lecture partielle
a produit une affirmation fausse dans les deux cas.

## 4. Reprise, ordre exact

1. lire `sub_821C56F8` **en entier**, étiquettes comprises, sans filtrer la
   sortie ;
2. y chercher les lectures de `[obj+84]` — l'état posé à 2 par le lanceur ;
3. si aucune n'existe, la complétion n'est constatée nulle part et le défaut est
   nommé ;
4. si elle existe, relever à l'exécution ce qu'elle lit.

Le point 3 est le résultat le plus probable au vu du symptôme, et il serait
directement actionnable — mais il reste à vérifier, pas à supposer.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

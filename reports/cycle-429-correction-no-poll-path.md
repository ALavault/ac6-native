# Cycle 429 — correction : `0x821CE8D4` n'est pas un chemin de scrutation

## 1. Ce que j'ai mal lu

Le cycle 428 présentait `0x821CE8D4` comme « le chemin pris quand l'état n'est
pas nul, qui scrute la complétion ». **C'est faux.**

Le flot réel :

```
r11 = [this+84]
si r11 != 0 -> loc_821CE8D4        ; saute UNIQUEMENT le stockage suivant
[this+84] = 1
loc_821CE8D4:                      ; retombée commune
  r30 = 0 ; [this+88] = 0 ; ...
  bl sub_821F4658                  ; XamShowDeviceSelectorUI
  si r3 == 997 : [this+84] = 2
  sinon        : [this+84] = 0
```

Le branchement n'évite que l'écriture `[this+84] = 1`. Les deux voies se
rejoignent immédiatement et **appellent toutes deux le sélecteur**. Il n'y a
aucune scrutation de complétion dans cette fonction.

Mon erreur vient d'avoir déduit la structure d'une étiquette de saut sans lire
ce qui la suivait — j'avais filtré la sortie sur les commentaires et perdu les
étiquettes.

## 2. Ce qui reste acquis du cycle 428

Ces éléments-là sont lus correctement et tiennent :

- `[this+84]` est l'état de l'écran ;
- `997` = `ERROR_IO_PENDING` fait passer à l'état 2 ;
- toute autre valeur remet à 0 ;
- `sub_821F4658` enveloppe `XamShowDeviceSelectorUI`.

## 3. Ce que la correction implique

Cette fonction **lance** le sélecteur ; elle ne l'attend pas. Le journal ne
montre qu'un seul appel à `XamShowDeviceSelectorUI` sur toute l'exécution
(cycle 417), donc elle n'est appelée qu'une fois, l'état passe à 2, et
**personne ne l'en fait sortir**.

La scrutation de complétion, si elle existe, est **ailleurs** : dans l'appelant
de `sub_821CE8A8`, ou dans une routine de mise à jour de l'écran qui examine
`[this+84]`.

## 4. Suite

Trouver qui lit `[this+84]` en dehors de cette fonction. C'est ce lecteur — ou
son absence — qui décide si l'écran sort un jour de l'état 2.

## 5. Note

Deuxième lecture de code erronée en deux cycles (après le registre `r11` du
cycle 412). Les deux viennent d'avoir filtré la sortie avant de la comprendre.
Lire le flot complet, étiquettes comprises, coûte quelques lignes de plus et
évite un cycle entier.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

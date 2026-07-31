# Cycle 447 — un seul mot bouge, et c'est un flottant : la région balayée est la mauvaise

## 1. Exclusion retirée, descente au mot

Plus aucun bloc n'est exclu. Les blocs qui diffèrent entre référence et
déclenchement sont décomposés au mot.

## 2. Résultat, sur 256 Mo denses

```
[ac6-trig] blk 0xA33D0000 (1 words shown)
[ac6-word] 0xA33DA914 : 0x4577E0C3 -> 0x4577D76F
```

**Un seul mot** change au passage de l'appui.

Décodé en flottant : `0x4577E0C3` ≈ 15992,19 et `0x4577D76F` ≈ 15989,86. Une
valeur qui **décroît lentement** — compteur de temps ou accumulateur
d'animation. Ce n'est pas un drapeau de sélection, qui prendrait deux valeurs
discrètes.

L'adresse appartient à la page `0xA33DA000`, celle qui remue depuis le cycle 443.

## 3. La conclusion, et l'erreur qu'elle révèle

L'état de sélection **n'est pas** dans `0xA0000000`–`0xB0000000`.

Et la raison est visible depuis le début : **cette plage est le tas**. Or l'image
du jeu est basée à `0x82000000`, et les données statiques — variables globales,
états d'interface persistants — vivent autour de `0x82xxxxxx`–`0x83xxxxxx`.

Je n'ai **jamais balayé cette région**. Cinq cycles de balayage (443-447) ont
tous porté sur le tas, parce que les objets que je suivais s'y trouvaient. Un
drapeau de sélection d'un dialogue global n'a aucune raison d'y être.

## 4. Ce que cela clôt et ce que cela ouvre

Clos : le tas est innocenté, densément et de façon synchrone. C'est un résultat
solide, obtenu avec un instrument désormais correct.

Ouvert : la région des données statiques, jamais examinée, où l'adresse de base
de l'image donne directement l'intervalle à couvrir.

## 5. Reprise, en une ligne

`kScanOrigin = 0x82000000`, étendue 32 Mo. Le reste du dispositif — dense,
déclenché, sans exclusion, descente au mot — est déjà en place et validé.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

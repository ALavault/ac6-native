# Cycle 1141 — les trous réduits de 120 Ko : rien de neuf

Date : 2026-08-08. Cycle autonome. Il exécute la prise nommée au cycle 1140 et
en rapporte un résultat nul.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Écriture dans `ghidra-projects-xenon/ac6-xenon` seul**, corpus de travail
  non canonique. `ghidra-projects/ace-combat-6` n'a jamais été ouvert en
  écriture, ici ni ailleurs dans cette série.
- **Statique seul.** Aucun oracle.

## Ce qui a été fait

Le cycle 1140 avait montré que tous les balayages de la série ne lisent que des
instructions décodées, et que 13,77 % de `.text` ne l'est pas. `Ac6FillGaps.java`
parcourt les 1 637 trous d'au moins 8 octets et tente un désassemblage à chacun.

```
gaps_attempted    1637
gaps_disassembled 1637
decoded_before    2 967 444  (86,30 %)
decoded_after     3 087 816  (89,80 %)
gained_bytes      120 372
```

**Cent vingt mille octets de code en plus**, soit environ trente mille
instructions qu'aucun balayage de cette série n'avait jamais lues.

## Ce que les balayages en disent

Les trois instruments relancés sur le corpus élargi :

| balayage | avant | après |
| --- | ---: | ---: |
| écritures de ligne de transformation | 108 | **110** |
| dont assemblées sur la pile | 34 | **34** |
| triplets `stfsx` indexés | 21 | **21** |
| appels à `memcpy` | 394 | **402** |
| dont vers une ligne de transformation | 7 | **7** |

Deux écritures de transformation et huit appels à `memcpy` de plus, et **aucun
nouveau chemin de données** : zéro nouvelle écriture assemblée, zéro nouveau
triplet indexé, zéro nouvelle copie vers une ligne de transformation, **zéro
nouveau site dans le groupe de mission**.

## Ce que cela vaut

C'est un résultat nul, et il vaut d'être publié : il **retire une hypothèse**.
Le cycle 1140 supposait que le mécanisme manquant se cachait dans le code non
décodé. Pour 120 Ko de ce code — le quart du total manquant — c'est faux.

Il reste **10,20 %** de `.text` non décodé, soit environ 350 000 octets. Les
trous qui subsistent sont ceux où le désassembleur n'a rien produit : selon toute
apparence, de vraies données — réservoirs de flottants, tables de chaînes, tables
de sauts — et non du code.

## Décision de cycle

Ne pas insister sur les 350 Ko restants par la même méthode. Le rendement vient
de tomber à zéro sur le quart le plus accessible, et rien n'indique que le reste
soit plus riche ; s'entêter serait dépenser sans hypothèse.

## Ce que la série laisse, définitivement pour ce cycle

Dix-sept cycles. Cinq idiomes de magasin énumérés et fermés. Cinq instruments
corrigés. Trois lectures corrigées, dont deux de moi. Une hypothèse tuée par
mesure. Le corpus élargi de 3,5 points sans rien changer.

**La position initiale d'une unité de la Mission 01 n'est pas trouvée**, et
l'espace où elle peut se cacher a été réduit autant que la méthode statique le
permet sans nouvelle idée.

`ctest 24/24`, la porte JF reste verte.

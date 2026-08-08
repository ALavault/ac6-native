# Cycle 1137 — l'angle mort fermé sans propagation, et ce qu'il reste

Date : 2026-08-08. Cycle autonome. Il ferme la frontière que trois cycles
avaient chiffrée et laissée entière.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Ce qui bloquait, et pourquoi c'était mal posé

Les cycles 1133, 1135 et 1136 ont buté sur les **659 magasins indexés** dont
l'index n'est pas une constante suivable, et ont tous refusé d'ajouter de la
propagation de valeurs — à raison, puisque le classificateur était encore faux.

Mais la propagation n'était pas nécessaire. Écrire une position en trois
flottants indexés a une **signature** qui ne dépend pas de la valeur des index :

> trois `stfsx` ou plus sur **la même base**, vers **trois destinations
> distinctes**, avec **au moins deux valeurs différentes**, dans une courte
> fenêtre.

Trois destinations distinctes écarte le champ réécrit trois fois ; deux sources
au moins écartent la mise à zéro en bloc — les deux faux positifs des cycles 1126
et 1128, évités par construction plutôt que découverts après coup.

## Le résultat

```
indexed_vector_writes=21
```

**Vingt et un** sur tout le binaire, regroupés dans huit fonctions :
`0x820B3290`, `0x82107C20`, `0x8213AAA8`, `0x821461C0`, `0x82154FC0`,
`0x823BC700`, plus deux sites sans fonction déclarée.

**Aucune n'est dans le groupe de mission `0x822xxxxx`.**

## L'énumération, maintenant close par idiome

| idiome | sites | verdict |
| --- | ---: | --- |
| `stvx128`, ports `+0x50` et `+0xA0`, index suivable | 108 | 74 copies, 34 assemblées ; 3 lisent un enregistrement — événement, rejeu, pool `0x2CF4` — **aucune n'est la naissance** (cycle 1136) |
| `stfs` à déplacement littéral, trois sources distinctes | 12 | tous expliqués : memsets, tables de constantes, copies (cycles 1128, 1133) |
| `stfsx` indexé, signature de triplet | 21 | **aucun dans le code de mission** |

Soit, en une phrase :

> **Aucun code du groupe de mission n'écrit la position d'une unité depuis des
> données de mission, par aucun des idiomes de magasin que cette campagne sait
> énumérer.**

## Ce que cela laisse

Deux possibilités, et il faut les écrire toutes les deux :

1. **La position n'est jamais authorée.** Les unités naissent à l'origine
   (cycle 1124, constante vérifiée au cycle 1130) et le programme d'ordres les
   pilote de là (cycle 1125). C'est cohérent avec tout ce qui précède, et **rien
   ne l'établit** — l'absence de preuve du contraire n'est pas une preuve.
2. **Elle est écrite par un mécanisme hors de ces idiomes.** Le candidat nommé et
   non testé est la **copie mémoire** : un `memcpy(objet+0x50, source, 12)`
   n'apparaît dans aucun des trois balayages. C'est la prise suivante, et elle
   est décidable — il suffit d'énumérer les appelants des primitives de copie
   dont le troisième argument est 12, 16 ou 64.

## Décisions de cycle

1. **Ne pas ajouter de propagation de valeurs.** Trois cycles l'avaient reportée
   par prudence ; celui-ci montre qu'elle n'était pas la bonne réponse à la bonne
   question. La signature a coûté trente lignes et a fermé l'angle mort.
2. **Ne rien porter.** Aucun des 21 sites ne concerne la Mission 01.

`ctest 24/24`, la porte JF reste verte.

# Cycle 1139 — l'hypothèse de route, tuée par une mesure

Date : 2026-08-08. Cycle autonome. Court, et négatif.

## Qualification

- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.** Aucun oracle.

## L'hypothèse

Les cycles 1136 à 1138 ont fermé l'énumération sur quatre idiomes de magasin
sans trouver d'écriture de position. Le rapport `WORLD_POSITION_DEBT.md` gardait
alors deux lectures, dont celle-ci :

> Les unités ne sont jamais placées : elles naissent à l'origine et le programme
> d'ordres les déplace de là, `PLAD` désignant l'entrée de route du joueur.

Elle a une conséquence mesurable. Si une unité tient sa place de son premier
ordre d'étiquette 2, alors **les unités sans ordre d'étiquette 2 ne peuvent pas
être des unités mobiles** — ce seraient des cibles fixes, d'une autre classe.

## La mesure

| | unités | octet de classe | faction |
| --- | ---: | --- | --- |
| **avec** un ordre d'étiquette 2 | 122 | `{1: 9, 2: 113}` | `{0: 78, 1: 22, 2: 22}` |
| **sans** | 108 | `{0: 1, 1: 31, 2: 75, 4: 1}` | `{0: 62, 1: 20, 2: 26}` |

**La coupure n'existe pas.** La classe 2 est des deux côtés — 113 avec, 75 sans —
et la classe 1 aussi, 9 contre 31. Les factions se répartissent pareillement.
Soixante-quinze unités de la même classe que les unités routées n'ont aucun
ordre d'étiquette 2, donc aucune position d'aucune sorte.

## Ce que cela fait

L'hypothèse est **réfutée**. Elle avait été écrite au cycle 1132 « pour pouvoir
être tuée » ; c'est fait, et par une mesure de trente lignes plutôt que par un
raisonnement.

Et cela laisse la question dans un état pire, ce qu'il faut dire : la seconde
lecture — « la position est écrite par un mécanisme hors de la liste » — est
maintenant **la seule debout**, alors que quatre idiomes ont été énumérés et
fermés. Il manque donc quelque chose à l'énumération, et le cycle 1138 nommait
déjà les deux candidats restants : une copie de **taille variable**, ou une
boucle écrite à la main.

## Décision de cycle

Publier un résultat négatif court plutôt que l'enfouir dans le rapport suivant.
Cette série a corrigé cinq instruments et trois lectures ; une hypothèse tuée
proprement vaut mieux qu'une hypothèse qu'on cesse discrètement de mentionner.

`ctest 24/24`, la porte JF reste verte.

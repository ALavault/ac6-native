# Cycle 449 — le couple répond à Droite, jamais à A

## 1. Séquence mesurée

| entrée | `0x82A53428` | `0x82A5342C` |
|---|---|---|
| arrivée | `40` | `04` |
| **Gauche** | `40` | `04` — inchangé |
| **Droite** | `04` | `40` — **échange** |
| **Gauche** | `04` | `40` — pas de retour |
| **A** | `04` | `40` — inchangé |
| repos | `04` | `40` — inchangé |

## 2. Ce qui est confirmé

Le couple **réagit bien à une entrée de navigation** : Droite provoque
l'échange `40`↔`04` relevé au cycle 448. Ce n'est donc pas une coïncidence de
balayage ; ces deux mots participent à l'état de l'interface.

Et surtout, le résultat central se répète : **A ne les modifie pas.** La
validation ne laisse aucune trace, ici comme partout ailleurs depuis le cycle
421.

## 3. Ce qui n'est pas confirmé

Le test décisif que j'avais annoncé — l'échange doit se **défaire** au retour —
**échoue** : après Droite, un Gauche ne rétablit rien. Le couple ne se comporte
donc pas comme une bascule à deux positions.

Deux lectures possibles, non départagées :

1. ces mots sont un état de sélection, mais la navigation n'est pas symétrique
   (butée, ou liste de plus de deux entrées dont l'affichage ne montre que
   deux) ;
2. ce ne sont pas la sélection, mais autre chose que Droite a modifié en même
   temps.

Le premier Gauche resté sans effet est cohérent avec les deux : sélection déjà
en butée à gauche, ou simple absence de lien.

## 4. Statut honnête

Le cycle 448 annonçait « champ de sélection trouvé ». **C'était prématuré.** Ce
qui est établi est plus faible et plus sûr : deux mots statiques qui changent
avec une entrée de navigation et jamais avec la validation.

C'est réel et exploitable, mais ce n'est pas l'identification que j'avais
annoncée, et le test que j'avais moi-même désigné comme décisif ne l'a pas
confirmée.

## 5. Reprise

Écrire directement dans ces mots pour forcer l'échange, et observer l'écran. Si
le surlignage bascule, le lien est prouvé ; s'il ne bouge pas, l'hypothèse tombe
et il faut revenir aux cinq autres blocs relevés au cycle 448, encore inexplorés.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

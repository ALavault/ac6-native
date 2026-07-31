# Cycle 451 — 120 mots relevés ; un décalage cohérent, et un contrôle utile

## 1. Contrôle : la manette apparaît, comme prévu

`0x8290DE18`, `0x8290DE50`, `0x8290DE58`, `0x8290DE60` passent de `0` à `4`.

`0x8290DE3C` est **l'objet d'emplacement de manette** identifié aux cycles
412-413, et `0x4` est le bit `DPAD_LEFT`. Ces mots sont donc l'entrée elle-même,
pas une conséquence.

C'est un bon signe pour l'instrument : il voit ce qu'il doit voir, à l'endroit
déjà connu. Écartés de l'analyse.

## 2. Le motif intéressant : un décalage régulier

Bloc `0x82A53Cxx`, valeurs avant → après :

| adresse | avant | après |
|---|---|---|
| `0x82A53CBC` | `0x44` | `0x00` |
| `0x82A53CE0` | `0x48` | `0x04` |
| `0x82A53D04` | `0x4C` | `0x44` |
| `0x82A53D28` | `0x50` | `0x48` |
| `0x82A53D4C` | `0x54` | `0x4C` |
| `0x82A53D70` | `0x58` | `0x54` |
| `0x82A53D94` | `0x5C` | `0x58` |

Les entrées, espacées de 36 octets, **prennent la valeur de leur voisine
précédente**. C'est un **décalage d'une position** — la signature d'une liste
qui défile, pas d'un simple surlignage.

Les deux premières sortent du motif (`0x44`→`0`, `0x48`→`4`), ce qui est
cohérent avec un rebouclage en tête de liste.

## 3. Ce que cela suggère, sans le démontrer

L'écran ne serait pas un simple dialogue à deux boutons mais une **liste
défilante**, dont YES et NO seraient deux entrées visibles. Le pas de 36 octets
se retrouve dans trois blocs distincts (448, 450, ici), ce qui indique des
structures parallèles décrivant les mêmes éléments.

Je ne conclus pas : rien n'a été écrit ni vérifié. Le cycle 450 vient de montrer
qu'un motif convaincant — deux mots qui s'échangent — pouvait n'avoir aucun
effet sur l'affichage.

## 4. Test à faire, identique au précédent

Forcer le décalage sur ces sept entrées et mesurer la bande de boutons. Le seuil
est établi : ~131 pour une navigation réelle, 2 à 4 au repos, et 4,4 pour le
faux candidat du cycle 450.

Et cette fois **avec journalisation active**, pour vérifier que l'écriture a
bien eu lieu — la réserve laissée ouverte au cycle 450.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

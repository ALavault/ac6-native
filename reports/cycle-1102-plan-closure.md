# Cycle 1102 — fermeture des points restants du plan

Date : 2026-08-09. Trois points du plan approuvé n'étaient pas fermés : la
preuve d'aller-retour de S3, le patron transactionnel de S5.2, et la relecture
statique de la frontière `0x8237CC58` que le plan avait mise de côté.

## S3 — analyser, ré-émettre, exiger le même SHA-256

Le plan demandait « la preuve la plus forte disponible hors exécution ». Un
parcours qui lit sans se plaindre ne prouve presque rien : il peut sauter une
table, mal lire un compte, ne jamais atteindre un sous-arbre, et paraître
réussir. Reconstruire le fichier et exiger le même condensat rend ces échecs
bruyants.

`tools/roundtrip_ac6_scenario.py` sépare explicitement ce qui est reconstruit de
ce qui est recopié :

- **tout mot de structure est recalculé** depuis le modèle, jamais recopié. Les
  deux décalages d'un nœud sont redérivés comme `data_absolu − nœud` et
  `table_absolu − nœud` ; le compte d'une table et chacun de ses décalages
  d'enfant de la même façon. Une table mal lue revient fausse et le condensat
  bouge.
- **tout bloc de données est recopié tel quel**. Aucun schéma ne prétend
  modéliser ce qu'un bloc de données contient ; le ré-émettre ne prouverait rien.

Sur la charge utile Mission 01 :

```
identique                oui        (3 477 248 octets, même SHA-256)
nœuds                 62 031
tables                28 218
blocs de données      57 811
octets de structure  857 240        recalculés, pas recopiés
octets non réclamés  694 472        dont non nuls : 0
                                    88 745 plages, la plus longue de 12 octets
```

Le contrat retenu n'est donc pas « tout octet est réclamé » — 20 % du fichier ne
l'est pas — mais **« tout octet non réclamé est un zéro »**. Un seul octet non
nul hors du modèle serait de l'information que le parcours n'a jamais vue. Les
plages non réclamées font au plus 12 octets : c'est le bourrage d'alignement
entre blocs, et c'est dit comme tel plutôt que masqué.

Cinq cas synthétiques, sans aucune donnée retail, tiennent le reste : identité
sur un conteneur construit dans le test, bourrage nul, un octet non nul glissé
dans le bourrage qui **casse** l'identité, une falsification du modèle (et non
des octets) qui déplace la sortie — ce qui prouve que la structure est bien
recalculée — et un compte négatif conservé sans être suivi.

## S5.2 — le patron transactionnel, dans les deux fichiers nommés

Le plan nommait `tests/combat_runtime_tests.cpp` et
`tests/campaign_progression_tests.cpp`. Les cycles précédents avaient ajouté des
fichiers de test séparés ; les deux fichiers nommés portent maintenant leur
part, sur le patron de `manifest_loader_transaction_tests.cpp` — ligne invalide,
échec, état préservé :

- côté vagues : les 230 enregistrements chargés, la distribution de factions
  140 / 42 / 48 vérifiée, puis une ligne invalide et un doublon **refusés
  entiers**, l'état chargé survivant aux deux ;
- côté objectifs : les 4 sous-missions chargées en `Manual` et `Pending`, un
  identifiant stable vide et un doublon refusés, et **rien qui complète un
  objectif** — ni le chargement, ni les refus.

Les deux moitiés retail sont sautées si les manifestes ne sont pas là. Pour
vérifier qu'elles s'exécutent bien plutôt que de se fier au vert, chaque
assertion a été temporairement faussée : `230 → 231` échoue à la ligne 32,
`4 → 5` échoue à la ligne 42, et les deux repassent une fois rétablies.

## La frontière `0x8237CC58`, relue

Détaillé dans l'addendum de `reports/cycle-1082-savegprlr-analysis-gap.md`, où
le plan demandait qu'il soit noté. En bref : la fonction n'est plus tronquée —
`[0x8237CC58, 0x8237D1B3]`, 1372 octets — et deux adresses citées par les cycles
460 et 514 se requalifient. `0x8237CEF0` n'est pas une fonction ni un appelant :
c'est l'adresse de retour du `bctrl` de `0x8237CEEC`, intérieure à
`0x8237CC58`. `0x8237D0FC` est l'adresse de retour d'un `bl 0x8237CC58` — un
appel **récursif** le long de `r30 = *(r30 + 0x18)`, ce qui confirme par le
listing la lecture que le cycle 460 avait dû faire sur un vide.

**Aucun run N3 n'a été engagé**, conformément au plan.

## État du plan

| étape | état |
| --- | --- |
| S0 | fait (cycle 1082, plus l'addendum ci-dessus) |
| S1 | fait (cycles 1084–1088) |
| S2 | fait (cycle 1083) |
| S3 | **fait ici** — parcours piloté par le schéma et aller-retour octet pour octet |
| S4 | fait (cycles 1089–1092) |
| S5 | fait (cycles 1096–1097), le patron transactionnel **complété ici** |

Le seul livrable du plan délibérément non produit reste `ai.tsv` : le format
natif est une règle de tir périodique, la charge utile retail n'en porte pas, et
en fabriquer une serait exactement le piège que l'objectif nommait.

## Ce que cela n'établit pas

L'aller-retour prouve que le parcours voit toute l'information structurelle du
fichier. Il ne dit rien de ce que les blocs de données **signifient** — il les
recopie sans les lire, et c'est précisément pourquoi il ne peut pas se tromper à
leur sujet.

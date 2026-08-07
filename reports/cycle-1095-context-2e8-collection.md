# Cycle 1095 — G1.2 : ce que `0x8226EBD0` fait de `contexte+0x2E8`

Date : 2026-08-08. Statique seul, aucun oracle.
XEX PAL `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## La question

Le cycle 1093 avait laissé `contexte+0x2E8` en suspens : le chargeur y publie
`contexte + 0x123C40` juste après l'étiquette `'Obj & Unit'`, et
`mission_manager_update` la relit sous garde `état == 8` pour la passer à
`0x8226EBD0` avec le delta de frame. Restait à savoir ce que celui-ci en fait.

## La réponse

```c
do {
  count = *(int *)(region + 4);
  chunk[0] = count * i       >> 5;      // début de tranche
  chunk[1] = count * (i + 1) >> 5;      // fin de tranche
  chunk[2] = (float)delta;              // le delta de frame
  Function_821D4938(0x8275A494, region, chunk);   // dispatch
  i++;
} while (i < 0x20);                                // 32 tranches
Function_821D49D8(0x8275A494);                     // jointure

*(int *)(region + 0x33e4) += 1;                    // curseur de frame
if (DAT_823f9c7c <= cursor) *(region + 0x33e4) = 0;
Function_82265F00(region + 0x1050);
```

`contexte+0x2E8` désigne donc **une collection comptée** :

| champ | rôle |
| --- | --- |
| `+0x04` | le nombre d'éléments |
| `+0x1050` | une sous-structure, traitée après la jointure |
| `+0x33E4` | un curseur de frame, borné par `DAT_823F9C7C` |
| taille | au moins `0x33E8` octets |

Le compte est découpé en **32 tranches** dispatchées en parallèle contre l'objet
de travail `0x8275A494`, puis jointes — un parcours par frame de tous les
éléments.

## Ce que cela vaut

C'est **la forme d'un registre d'unités** : une collection comptée, mise à jour
chaque frame, en parallèle, sous garde d'état de mission.

Ce n'est **pas** une preuve que ses éléments sont les enregistrements `Obj`
analysés. Le compte vit en `+0x04`, en mémoire d'exécution : le comparer aux
230 enregistrements statiques demande un run. C'est exactement le genre
d'observable que la politique d'oracles réserve à N3.

`H-STATIC-OBJ-AND-UNIT-REGION-IS-THE-PARSED-BUFFER` reste donc `proposed`, et
`retail_units_and_waves` reste ouvert.

## Ce que cela change pour le passage N3

Le contrat pré-enregistré `H-N3-OBJ-SLOT-CONSUMER-INSERTS-INTO-UNITMANAGER`
gagne un **contrôle supplémentaire et bon marché** : lire `*(région+0x04)` avant
et après les trois appels à `0x820A7070`. Si le compte passe de 0 à 230, la
jonction est faite en une lecture, sans instrumenter la boucle.

C'est le contrôle à tenter en premier lors du run, avant le hook par itération.

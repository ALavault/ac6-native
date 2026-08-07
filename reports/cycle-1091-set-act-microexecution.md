# Cycle 1091 — micro-exécution de `ActBin::read` et `SetBin::read`

Date : 2026-08-08. La chaîne `Set → Act → Order` est fermée de bout en bout.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Aucun oracle** : ni émulateur de console, ni bridge, ni exécution du produit
  natif.
- Détection d'écriture par union de deux passes de poison, corrigée au
  cycle 1090.

## Ce qui a été ajouté au parseur natif

Deux lecteurs et trois dimensionneurs, tous dérivés de la décompilation :

- `ActBin::read` `0x82330688` — en-tête de liste, réserve `count × 0x2C`, met à
  zéro **onze `u32`** par élément, puis appelle `OrderBin::read` et avance du
  `OrderBin::getReadBuffSize` de chaque ordre ;
- `SetBin::read` `0x8232F5F8` — même forme, foulée `0x08`, appelle
  `ActBin::read` et avance de `ActBin::getReadBuffSize` ;
- `OrderBin::getReadBuffSize` `0x823310E8` — 4 pour les étiquettes 0, 1 et 3
  à 9 ; 8 plus un bloc imbriqué pour l'étiquette 2 ; 0 sinon ;
- `ActBin::getReadBuffSize` `0x82330540` et `SetBin::getReadBuffSize`
  `0x8232F4D0`.

> Un chemin mort est modélisé et signalé comme tel : quand un enfant d'ordre est
> absent, le dimensionneur retail déréférencerait un pointeur nul. Aucun enfant
> d'ordre n'est absent dans cette charge utile, donc le chemin ne s'exécute
> jamais ; le parseur natif retourne 0 plutôt que de deviner une faute.

## Résultats

| cas | nœuds | résultat |
| --- | ---: | --- |
| `ActBin::read` `0x82330688` | **24** | `pair_equal` |
| `SetBin::read` `0x8232F5F8` | **24** | `pair_equal` |

Les deux passent **du premier coup**, sans la correction qu'avait exigée
`ObjBin` au cycle 1089.

Profondeur atteinte :

| lecteur | pas | entrées sous-parseur | tampon écrit |
| --- | --- | ---: | --- |
| `ActBin` | 357 – **12 681** | 30 – 668 | 96 – 2 680 o |
| `SetBin` | 892 – **17 780** | 75 – 956 | 152 – 2 584 o |

Une seule exécution `SetBin` traverse donc `SetBin → ActBin → OrderBin` et ses
variantes, jusqu'à près de mille entrées de sous-parseur, et l'image mémoire
produite coïncide au bit près avec celle du parseur natif. L'échantillon inclut
délibérément les actes extrêmes de la distribution, jusqu'à **52 ordres**.

**Total cumulé : 132 comparaisons, 132 `pair_equal`, 0 divergence.**

| classe | nœuds micro-exécutés |
| --- | ---: |
| `ObjBin` | 25 |
| `OrderBin` | 32 |
| `ManeuverBin` | 25 |
| `ActBin` | 24 |
| `SetBin` | 24 |
| `ComTblBin` | 1 |
| `ComBin` | 1 |

## Balayage complet, côté natif

Le parseur natif a ensuite traversé **la totalité** de la charge utile depuis
les 230 nœuds `SetBin` :

```
sets parcourus                 230
actes traversés                492
chemins d'échec déclenchés       0
descentes étiquette 2            0
```

Aucun débordement de région, aucun chemin d'échec fermé, aucune descente
d'étiquette 2. Le retail parse ses propres données sans jamais toucher une de
ses chaînes d'erreur, sur l'intégralité du scénario.

## Tests

Quatre cas synthétiques ajoutés : foulée `0x2C` des ordres dans le tampon,
sortie anticipée d'un acte déclarant zéro ordre, échec fermé quand le compteur
déclaré dépasse la table, et l'égalité `act_size == records + ordres`. Suite
complète : `python3 -m unittest discover -s tools/tests` → **55 tests, OK**.

## État de la micro-exécution

| classe | schéma | micro-exécuté |
| --- | :---: | :---: |
| `ObjBin` | oui | **oui** |
| `OrderBin` | oui | **oui** |
| `ActBin` | oui | **oui** |
| `SetBin` | oui | **oui** |
| `ManeuverBin` | oui | **oui** |
| `ComTblBin` | oui | **oui** |
| `ComBin` | oui | **oui** |
| `SubMisTblBin` | oui | non |
| `SubMisBin` | oui | non |
| `RadioTblBin` | oui | non |

Les sept classes du chemin `Obj & Unit` — celui du domaine `retail_units_and_waves`
— sont couvertes. Les trois restantes appartiennent à d'autres slots de la
racine.

## Ce que cela n'établit toujours pas

C'est le point à ne pas perdre de vue : **132 comparaisons octet pour octet ne
disent rien de la sémantique**. Les parseurs recopient des pointeurs sans les
interpréter, et le parseur natif fait de même ; leur accord prouve que la
structure est lue correctement, pas que l'on sait ce qu'elle signifie.

Restent hors d'atteinte :

- ce que désigne chaque pointeur résolu ;
- ce qu'un `Act` regroupe, ce qu'un `OrderStopBin` arrête ;
- les étiquettes `OrderBin` 4 et 9, absentes de la Mission 01 ;
- le sous-lecteur `0x82331D98` de l'étiquette 2.

`retail_units_and_waves` et `retail_objectives` restent **ouverts** dans le
contrat v2, et rien dans ce cycle ne les rapproche : ils demandent une identité
de vague et une condition d'objectif, pas une disposition d'enregistrement.

## Prochaine tranche

Deux directions distinctes, et il faut choisir :

1. **Terminer la micro-exécution** — `SubMisTblBin`, `SubMisBin`, `RadioTblBin`.
   Peu coûteux, complète le tableau, n'ouvre aucun domaine.
2. **Attaquer la sémantique** — suivre un pointeur résolu jusqu'à son
   consommateur runtime, par exemple depuis un enregistrement `Obj` jusqu'à la
   factory `0x820A7F48` que le cycle 1073 avait isolée. C'est la seule voie qui
   touche `retail_units_and_waves`, et la seule qui puisse encore exiger un
   passage N3.

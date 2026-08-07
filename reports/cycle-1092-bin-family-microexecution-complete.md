# Cycle 1092 — la micro-exécution de la famille `*Bin` est complète

Date : 2026-08-08. Dernières classes : `SubMisTblBin`, `SubMisBin`,
`RadioTblBin`.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Aucun oracle.** Détection d'écriture par union de deux passes de poison.

## Une couche de plus, sans nom

`SubMisBin` descend dans une liste que le cycle 1088 avait signalée comme
n'ayant **aucune chaîne d'erreur dans tout son sous-arbre**. La micro-exécution
a obligé à la modéliser, puisque les quatre sous-missions de la Mission 01
l'exercent.

| élément | fonction | forme |
| --- | --- | --- |
| liste | `0x8232ED10` / `0x8232EC08` | en-tête `0x08`, compteur `u8`, foulée **`0x28`** |
| élément | `0x8232F9B8` / `0x8232F8B0` | union étiquetée de 10 mots |

L'union de l'élément a exactement la forme de `OrderBin`, mais **aucune de ses
variantes ne peut être nommée** :

| étiquette | 0 | 1 | 2 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mot écrit | `[1]` | `[5]` | `[6]` | `[2]` | `[3]` | `[4]` | `[8]` | `[9]` |

Seules les étiquettes **0 (×4) et 1 (×2)** apparaissent dans les six éléments de
la Mission 01. L'étiquette 2, qui descend encore d'un niveau
(`0x823308E0`/`0x82330810`), n'est jamais employée ; le parseur natif reproduit
sa précondition et **lève** plutôt que de deviner.

## Quatrième particularité de dimensionneur

`SubMisTblBin` a divergé au premier essai — dérive de curseur de 8 octets à
partir de la deuxième sous-mission.

`0x8232EC08` termine par :

```
uVar6 = total + 0xF;
return ((uVar6 >> 4) ...) << 4;
```

C'est un **arrondi au multiple de 16**, et c'est le **seul dimensionneur de la
famille qui aligne**. Sur une liste de deux éléments il vaut 8 octets :
`2 × 0x28 + 2 × 4 = 88` devient 96.

Quatrième asymétrie relevée, après `SetBin` (cycle 1088), le bloc de manœuvres
(cycle 1089) et le chemin mort du dimensionneur d'ordres (cycle 1091). Aucune
n'est un défaut observable ; toutes changent la disposition du tampon.

Après reproduction exacte : `pair_equal`.

## Résultats

| cas | nœuds | pas | résultat |
| --- | ---: | ---: | --- |
| `SubMisTblBin::read` `0x82309758` | 1 | 2 848 | `pair_equal` |
| `SubMisBin::read` `0x8232C8A8` | **4** | 365 – 575 | `pair_equal` |
| `RadioTblBin::read` `0x823094D8` | 1 | 2 118 | `pair_equal` |

`RadioTblBin` est passé du premier coup. Son tampon vaut **216 octets** pour
54 entrées de foulée `0x10` : le lecteur d'entrée n'écrit qu'**un mot** par
entrée, soit `54 × 4`, et les douze octets restants de chaque entrée ne sont
jamais touchés. Le compteur `u16` et le dimensionneur nul du cycle 1088 sont
ainsi confirmés par exécution.

## La famille, close

| classe | schéma | micro-exécuté | nœuds |
| --- | :---: | :---: | ---: |
| `ObjBin` | oui | oui | 25 |
| `OrderBin` | oui | oui | 32 |
| `ActBin` | oui | oui | 24 |
| `SetBin` | oui | oui | 24 |
| `ManeuverBin` | oui | oui | 25 |
| `ComTblBin` | oui | oui | 1 |
| `ComBin` | oui | oui | 1 |
| `SubMisTblBin` | oui | oui | 1 |
| `SubMisBin` | oui | oui | 4 |
| `RadioTblBin` | oui | oui | 1 |

**Total cumulé : 138 comparaisons, 138 `pair_equal`, 0 divergence.**

Les dix classes de la famille sont désormais reproduites octet pour octet par un
parseur écrit depuis les schémas seuls, sur des nœuds réels de la Mission 01,
sans aucun oracle.

## Tests

Trois cas synthétiques ajoutés sur l'arrondi : la valeur pour une et deux
entrées, l'équivalence avec `(total + 0xF) & ~0xF` sur plusieurs tailles, et le
cas déjà aligné qui doit rester inchangé. Suite complète :
`python3 -m unittest discover -s tools/tests` → **58 tests, OK**.

## Bilan des quatre particularités

| # | où | règle | écart |
| ---: | --- | --- | ---: |
| 1 | `SetBin::getReadBuffSize` | réserve `0x2C` par acte, le lecteur écrit `0x08` | `0x24`/acte |
| 2 | bloc de manœuvres `0x82330A30` | part de `0x60`, écrase par `taille0 + 0x6C` | `0x60` |
| 3 | dimensionneur d'ordres `0x823310E8` | déréférencerait un pointeur nul sur enfant absent | chemin mort |
| 4 | liste `0x8232EC08` | arrondit le total au multiple de 16 | ≤ `0x0F` |

Aucune n'a été trouvée en lisant le code : **les quatre ont été révélées par la
comparaison sur plusieurs nœuds**. Un seul nœud par classe aurait donné 10/10 et
un schéma faux.

## Ce que cela n'établit toujours pas

Rien de sémantique. Les dix parseurs recopient des pointeurs sans les
interpréter ; le parseur natif fait de même ; leur accord prouve que la
structure est lue correctement, pas qu'on sait ce qu'elle signifie.

Restent inconnus : ce que désigne chaque pointeur résolu, ce qu'un `Act`
regroupe, ce qu'un `OrderStopBin` arrête, ce que porte la liste `0x28` sans nom,
et le lien entre une entrée `RadioTblBin` et une clé radio.

`retail_units_and_waves` et `retail_objectives` restent **ouverts**.

## Prochaine tranche

La sémantique. Suivre un pointeur résolu jusqu'à son consommateur runtime :
depuis un enregistrement `Obj` vers la factory `0x820A7F48` que le cycle 1073
avait isolée sans pouvoir la joindre à des données. C'est la seule voie qui
touche `retail_units_and_waves`, et la première depuis le cycle 1082 qui puisse
encore exiger un passage N3.

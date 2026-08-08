# Cycle 1123 — les 1 619 rejets, nommés : aucune classe perdue

Date : 2026-08-08. La seconde dette nommée par le goal.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
  ouvert **en lecture seule**.
- **Statique seul.** Aucun oracle.

## Ce qui était reproché

Le cycle 1116 l'écrivait sans détour : « *toute vtable* est en réalité *toute
vtable qui résout*. Le balayage nomme 811 tables et **rejette 1 619 candidats**
qui échouent à une étape. Ils sont comptés et non nommés. » Un compte n'explique
rien : il pourrait aussi bien cacher 1 619 classes perdues.

`scripts/Ac6RttiRejects.java` rejoue **le balayage identique** — même parcours,
mêmes prédicats — et enregistre, pour chaque candidat refusé, **quelle étape** l'a
refusé. Le compte se reproduit exactement :

```
WROTE vtables=811 rejected=1619
```

## La partition, exhaustive

| étape ayant refusé | candidats |
| --- | ---: |
| `name_not_rtti` — les octets lus ne commencent pas par `.?A` | 1 096 |
| `name_unreadable` — pas de chaîne du tout à cet endroit | 522 |
| `hierarchy_out_of_range` | 1 |
| **`slot_not_code` — le mot suivant n'est pas du code** | **0** |
| **`slot_unreadable`** | **0** |

La dernière ligne est le résultat. **Aucun candidat n'a atteint le test de la
vtable.** Tous ont échoué sur le nom, c'est-à-dire que le mot ne pointait pas sur
un descripteur de type. Un candidat portant un vrai nom RTTI et refusé faute
d'un premier emplacement exécutable — le seul cas où le balayage aurait perdu une
classe — **n'existe pas dans ce binaire**.

## Ce que les rejets sont réellement

Sur les 1 096 refus `name_not_rtti`, **1 083 (98,8 %)** lisent, là où un nom
devrait commencer, **un pointeur dans l'image**. Et la longueur lue avant le
premier octet nul se groupe sur des multiples de 4 : 641 à 4 octets, 182 à 8,
121 à 12, 124 à 16. Le balayage n'a pas lu un nom tronqué ; il a marché dans un
**tableau de pointeurs** jusqu'au premier mot nul.

Les 1 619 refus ne visent d'ailleurs que **802 cibles distinctes** : le même mot
sans intérêt est atteint depuis deux endroits en moyenne, ce à quoi ressemble un
tableau de pointeurs référencé plusieurs fois.

## Le contrôle : un modèle nul par décalage

Dire « ce sont des coïncidences » ne coûte rien. Le mesurer, si. Le prédicat de
rejet est délibérément lâche — le mot en tête du candidat doit valoir 0 ou 1, et
celui en `+0x0C` doit tomber dans une image de 11 Mo. S'il se déclenche par
hasard, alors **décaler le pointeur candidat d'une constante** — ce qui détruit
toute relation réelle sans toucher aux statistiques des données — doit laisser le
compte de rejets du même ordre, tout en effondrant le compte d'acceptations.

```
décalage  acceptées  rejetées
   0x0        811      1619
  0x10          0       136
  0x40          0       857
 0x100          7      1263
 0x400         33      1110
0x1000         44      1516
0x10000         0       241
```

**811 → 0 d'un côté, 1 619 → des centaines de l'autre.** La chaîne complète ne se
déclenche jamais par accident ; le prédicat de rejet, lui, se déclenche à peu
près au même rythme sur des pointeurs délibérément faux. Les 1 619 sont des
coïncidences du pré-filtre, pas des vtables manquées de peu.

Les 7, 33 et 44 acceptations résiduelles méritent leur mot : les localisateurs
sont **denses** dans `.rdata`, si bien qu'un décalage de `0x100` peut tomber sur
un *autre* localisateur véritable. La chaîne résout alors, mais l'appariement
table/localisateur est faux. C'est le décalage nul qui donne le bon.

## Et l'hypothèse jamais mesurée

Le balayage saute les blocs exécutables, avec pour seule justification un
commentaire : « les vtables vivent dans les données en lecture seule ». C'est une
hypothèse. Mesurée :

```
EXEC_SUMMARY scanned=876487 found=0
```

876 487 mots parcourus dans les blocs exécutables, **zéro** chaîne complète.
L'hypothèse tient sur ce binaire.

## Ce qui est livré

- `analysis/class-map-rejects.tsv` — les 1 619 refus, un par ligne, avec
  l'adresse, la cible, l'étape qui a refusé, le bloc, les octets lus à la place
  du nom (en hexadécimal, parce qu'ils sont binaires) et le mot suivant.
- `tools/audit_ac6_class_map.py --rejects` — l'explication devenue barrière :
  chaque refus doit nommer une étape du balayage, aucun ne peut porter un nom
  décoré, aucun ne peut avoir été refusé au test de la vtable, et le registre ne
  peut pas être plus court que le compte. Quatre refus vérifiés :

```
étape inventée            → fail: 'because' is not a step of the sweep
refus au test de vtable   → fail: ... refused at slot_not_code, which means a
                                   resolved class name was dropped
refus portant un nom .?A  → fail: ... carries the decorated name '.?AVCFake@@'
registre tronqué          → fail: only 41 refusals ledgered, the sweep counted 1619
```

- L'en-tête de `analysis/class-map.tsv` ne dit plus « délibérément non nommés ».

`ctest 24/24`, une ignorée. La porte JF reste verte.

## Ce que cela n'établit pas

- **Que la carte contient toute classe du binaire.** Elle contient toute classe
  que la RTTI MSVC déclare et que ce motif atteint. Une classe sans localisateur
  complet — RTTI désactivée sur une unité de compilation, table construite à la
  main — resterait invisible, et rien ici ne la chercherait.
- **Que 811 est le bon compte de vtables** au sens du compilateur : 811 tables
  pour **721 noms distincts**, donc 90 classes portent plus d'une table, ce qu'on
  attend d'un héritage multiple. Aucune n'a été vérifiée une par une.
- Les octets lus à la place des noms sont rendus tels quels ; **rien n'identifie
  les tableaux de pointeurs** qu'ils traversent. Ce serait un autre cycle, et
  aucun résultat de celui-ci n'en dépend.

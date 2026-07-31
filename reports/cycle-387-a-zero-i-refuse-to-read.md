# Cycle 387 — un zéro que je refuse d'interpréter

## 1. Mesure

Omission alternée (2 s sur 4) des dessins liant `0x03514000`, à l'intérieur
d'**une seule exécution** — la correction de protocole demandée au cycle 386.
Dix captures à 1 s d'intervalle sur l'écran de sauvegarde :

```
f1->f2 : 0 pixels modifiés      f6->f7  : 0
f2->f3 : 0                      f7->f8  : 0
f3->f4 : 0                      f8->f9  : 0
f4->f5 : 0                      f9->f10 : 0
f5->f6 : 0
```

## 2. Pourquoi ce résultat n'est pas lu

Trois explications l'expliquent également bien :

1. les dessins de `03514000` ne produisent **aucun pixel visible** — cohérent
   avec un échantillon nul, et ce serait la confirmation cherchée ;
2. l'omission **ne s'est jamais déclenchée** ;
3. la bascule s'est produite **entre** les captures, sans jamais tomber de part
   et d'autre d'un intervalle.

Aucune journalisation ne distingue ces cas : **j'ai omis le témoin de vivacité**
dans la sonde d'omission, alors que je l'ai exigé dans chacune des sondes des
cycles 352, 359, 364 et 366.

C'est la septième fois dans cette enquête qu'un zéro se présente sans canal
prouvé. Les six précédentes ont été rattrapées ; celle-ci l'est aussi, mais
seulement parce que la règle est devenue un réflexe — pas parce que l'outil
était correct.

## 3. Correction à apporter

Deux lignes dans la condition d'omission :

```c
static std::atomic<uint32_t> skipped{0}, kept{0};
// journaliser périodiquement skipped et kept
```

Sans elles, l'expérience ne peut rien rendre. Avec elles, le même protocole
tranche l'attribution du cycle 385 en une exécution.

## 4. État

L'outil et le protocole intra-exécution sont en place et compilés ; le témoin
manque. L'attribution « lots multi-quads = texte manquant » reste **déduite,
non vérifiée**, et vingt-sept éliminations continuent de reposer dessus.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

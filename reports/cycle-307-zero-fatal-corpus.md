# AC6 cycle 307 — corpus sans aucun `REX_FATAL`

Le cycle 306 avait éliminé les 26 pièges `Unresolved branch` restants et désigné
les **205 `Unresolved call`** comme blocage suivant. Ce cycle les traite.

**Résultat : 0 `REX_FATAL` de toute classe** dans les 48 unités de traduction,
21 415 fonctions émises, exécutable lié et smoke passé.

## 1. Attribution

Les 205 pièges se répartissent en deux populations très inégales, lisibles dans
le commentaire que le générateur émet juste avant :

| Origine | Occurrences |
| --- | ---: |
| `b` (branchement inconditionnel), `no CallTarget in FunctionNode` | **204** |
| `bl` (appel), `unresolved bl target` | **1** |

Les 204 se réduisent à **141 paires distinctes** et **124 cibles distinctes**.

Deux mesures fixent la nature du problème :

- **0 des 124 cibles** est une fonction émise ;
- **119 des 124 cibles** sont un `loc_` à l'intérieur d'une **autre** fonction.

Ce ne sont donc ni des adresses fantaisistes ni un défaut de comptabilité du
graphe : ce sont des sauts vers du code réel, situé au milieu d'une fonction
voisine. C'est la même famille que les `Unresolved branch` du cycle 305 — une
coupure `[functions]` traverse un saut — vue depuis l'autre côté. `build_b`
classe la cible en `Function`, appelle `emit_function_call`, qui n'a aucune
arête pré-résolue pour ce site, et échoue.

**Pourquoi cette famille avait survécu au cycle 305 :** toute l'instrumentation
de retrait en masse extrayait les pièges avec le motif `Unresolved branch from`.
Les `Unresolved call` n'ont jamais été soumis à la même analyse, alors qu'ils
relèvent de la même cause et cèdent à la même méthode. C'est la seconde fois en
deux cycles qu'un filtre trop étroit masque une population entière.

## 2. Traitement des 204

Application de la règle du cycle 305 — retirer les entrées `[functions]`
strictement comprises entre la cible et la source, la cible elle-même si elle
est déclarée — étendue cette fois au motif `Unresolved call from`.

Les 141 paires ont **toutes** une coupure candidate ; aucune n'exige d'analyse
manuelle. Cela désigne **167 entrées** distinctes.

Un seul passage de codegen après retrait :

| | avant | après |
| --- | ---: | ---: |
| `Unresolved call` | 205 | **1** |
| `Unresolved branch` | 0 | **0** |

Le retrait ne réintroduit aucun piège de branche : les deux familles sont
compatibles.

## 3. Traitement du dernier `bl`

`bl 0x8238F434` depuis `rex_sub_8238F350`. La cible n'est ni une fonction, ni un
label, ni une entrée de configuration.

Le désassemblage de `sub_8238F414` — une amorce de pile — se lit ainsi :

```
8238F414  std   r31,-8(r1)
8238F418  addi  r31,r12,-112
8238F41C  std   r30,-16(r1)
8238F420  mflr  r12
8238F424  stw   r12,-24(r1)
8238F428  stwu  r1,-112(r1)
8238F42C  lwz   r30,132(r31)
8238F430  b     0x8238F44C
8238F434  <- cible du bl
```

`0x8238F434` suit **immédiatement un branchement inconditionnel**. Aucune
exécution ne peut l'atteindre par continuité ; elle n'est atteignable que par le
`bl`. C'est donc une entrée de fonction au sens strict, et le GapFill l'avait
absorbée à tort dans `sub_8238F414`, dont le corps saute d'ailleurs directement
de `b 0x8238F44C` à `loc_8238F44C` sans jamais émettre `0x8238F434`.

Ajout d'une seule entrée `0x8238F434 = { name = "rex_sub_8238F434" }` :
**0 `Unresolved call`, 0 `Unresolved branch`**.

C'est le seul ajout de tout le cycle ; les 167 autres changements sont des
retraits.

## 4. État final du corpus

| Mesure | Valeur |
| --- | ---: |
| `REX_FATAL`, toutes classes | **0** |
| unités de traduction | 48 |
| fonctions émises | 21 415 |
| entrées `[functions]` | 8 572 |

Les **1 239 `ppc_trap`** restants ne sont pas des échecs du générateur : ils
proviennent d'instructions de piège PowerPC réelles du jeu — 762 `twi`,
423 `twllei`, 33 `twlgei`, 16 `tdllei`, 5 `tdlgei` — c'est-à-dire les
vérifications de bornes et de division émises par le compilateur d'origine. Les
traduire autrement serait une erreur.

## 5. Compilation, édition de liens, exécution

Même arbre isolé et mêmes options qu'au cycle 306.

| | valeur |
| --- | --- |
| compilation, 48 unités | **0 erreur** |
| édition de liens | **réussie, 163,6 Mo** |
| smoke `xvfb`, 60 s | **exit 124 (survit), 2 fois sur 2** |

Activité mesurée à 40 s d'exécution : 60 fils, 782 Mo de RSS, 4,65 Go lus,
environ 256 % de CPU, `DATA00.PAC` et `DATA01.PAC` ouverts. Profil identique à
celui du corpus cycle 306, ce qui écarte une régression silencieuse.

Le clone de référence reste intact : `rexglue` au hash d'origine, `generated/`
toujours à 26 pièges, **aucune** modification sous `src/codegen` ni `include`.

## 6. Ce qui n'est pas prouvé

- Les 167 retraits ne sont **pas qualifiés** par contrat headless. Ils sont
  justifiés par la résolution de sauts mesurés, ce qui reste plus faible qu'une
  preuve de frontière. L'ajout de `0x8238F434` est en revanche argumenté
  structurellement (instruction suivant un branchement inconditionnel, cible
  d'un `bl`).
- Zéro `REX_FATAL` signifie qu'aucun piège de traduction ne peut plus être
  atteint. Cela ne dit rien du **comportement** du code traduit : le smoke
  atteint le chargement des données, pas une mission jouable.
- `recompiler-generated` n'est pas `verified`.

## 7. Reproduction

- Générateur : `patches/rexglue-unresolved-branch-gapfill-retracer-20260726.patch`
  (cycle 306, requis).
- Retraits cycle 306 : `patches/ac6recomp-config-empty-table-entries-to-remove-20260726.txt` (35).
- Retraits cycle 307 : `patches/ac6recomp-config-unresolved-call-splits-to-remove-20260726.txt` (167).
- Ajout cycle 307 : `patches/ac6recomp-config-entry-to-add-20260726.txt` (1).

Ordre : appliquer le patch générateur, puis les retraits, puis l'ajout.

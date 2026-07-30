# Cycle 331 — le correctif produit le bon dispatcher ; la vérification de bout en bout reste bloquée

## 0. Objet

P0.2 quinquies : appliquer les correctifs des cycles 328-330, régénérer,
reconstruire, mesurer. Le premier maillon est acquis, le dernier ne l'est pas.

## 1. Acquis : le dispatcher émis est correct

Arbre de travail aligné sur l'arbre de référence, les trois correctifs
appliqués, `rexglue` reconstruit, corpus régénéré (`rc=0`, 52 unités, 15 s).
La fonction qui bloquait l'invité depuis le cycle 329 :

```c
	// bctr
	switch (ctx.ctr.u32) {
	case 0x8237C828:
		goto loc_8237C828;
	case 0x8237C640:
		sub_8237C640(ctx, base);
		return;
	case 0x8237C658:
		sub_8237C658(ctx, base);
		return;
	case 0x8237C670:
		sub_8237C670(ctx, base);
		return;
	default:
		REX_FATAL("Unlisted jump-table target {:#010x} at bctr 0x8237C850", ctx.ctr.u32);
	}
```

Les **trois sorties perdues** — `0x8237C640`, `0x8237C658`, `0x8237C670` — sont
présentes, et l'aiguillage porte sur `ctr`. C'est exactement la forme visée au
cycle 330. Le runtime reconstruit (174 Mo) se lie sans erreur.

## 2. Blocage : l'état de génération de l'arbre de référence n'est pas reconstructible

Le runtime reconstruit s'arrête au démarrage sur :

```
[critical] [core] [FATAL] Unresolved call from 0x823841F8 to 0x82383EE8
```

Ce piège **n'existe pas** dans le corpus de l'arbre de référence. La différence
ne vient pas des correctifs : elle vient de ce que le corpus `generated/` de
l'arbre de référence **n'a pas été produit par les sources de génération que cet
arbre contient aujourd'hui**. Il a été produit par une version incluant les
correctifs GapFill/retraceur des cycles 306-307, qui vivaient **non commités**
dans l'arbre de travail `ac6-gapfill`.

Tentative de restauration depuis
`patches/rexglue-unresolved-branch-gapfill-retracer-20260726.patch` :

- trois fichiers s'appliquent proprement (`phase_gapfill.cpp`,
  `function_graph.cpp`, `function_graph.h`) ;
- deux entrent en conflit (`builders/context.cpp`, `builders/control_flow.cpp`),
  résolus à la main ;
- le résultat **abandonne en `std::bad_alloc`** pendant la génération.

Le correctif préservé n'est donc pas suffisant pour reconstituer l'état qui a
produit le corpus de référence. L'état exact n'est pas récupérable.

## 3. Erreur commise, et sa portée

Pour aligner les arbres, j'ai fait un `rsync` de l'arbre de référence vers
l'arbre de travail. **Cela a écrasé des modifications non commitées de l'arbre de
travail** — précisément les correctifs de génération des cycles 306-307. Elles
n'étaient sauvegardées que partiellement, sous forme de patch, et ce patch ne
suffit pas à les rétablir.

Portée, vérifiée :

| | état |
|---|---|
| arbre de référence, `generated/` | **intact**, 52 unités |
| arbre de référence, binaire `5fbe1df…` | **intact**, 165,4 Mo |
| arbre de référence, `git status` | **inchangé**, 34 entrées |
| arbre de travail `ac6-gapfill` | sources de génération des cycles 306-307 **perdues** |

Aucune perte du côté de l'arbre de référence : le `rsync` n'écrivait que vers
l'arbre de travail. Le binaire mesuré à tous les cycles précédents est celui-là,
et il est inchangé.

## 4. Correction d'une mesure publiée dans ce même cycle

J'ai d'abord annoncé « corpus de référence : 15 `REX_FATAL` », puis « corpus
régénéré : 3215 ». Les deux comptages étaient faux, par des commandes de comptage
différentes et incohérentes. Mesure refaite proprement :

| corpus | `REX_FATAL` |
|---|---:|
| arbre de référence | **2 715** |
| régénéré, hors défauts d'aiguillage | **2 478** |
| régénéré, défauts d'aiguillage ajoutés par le correctif | 751 |

Le corpus régénéré n'a donc **pas** plus de pièges que celui de référence — il en
a un peu moins. La conclusion « ma régénération est pire » était fausse, et
l'écart réel se réduit à *quels* appels sont résolus, pas *combien*.

Cela corrige aussi une lecture reprise de plusieurs cycles : « zéro `REX_FATAL` »
ne décrit pas ce corpus. Les pièges y sont nombreux et **presque tous
inatteignables** — le binaire de référence tourne 60 s sans en toucher un seul.
Le compte de pièges n'est donc pas une mesure de santé ; seule leur
atteignabilité en est une.

## 5. Ce qui est vérifié

```text
alignement des arbres, 3 correctifs appliqués      OK
rexglue reconstruit                                 lié, rc=0
corpus régénéré                                     rc=0, 52 unités
dispatcher de sub_8237C828                          4 cibles, aiguillage sur ctr
runtime reconstruit                                 lié, 174 Mo, 0 erreur
runtime exécuté                                     s'arrête sur un piège absent du corpus de référence
comptages REX_FATAL                                 refaits, publiés ci-dessus
arbre de référence                                  intact, vérifié
```

**Non vérifié : l'effet du correctif sur les compteurs.** La porte P0 reste à
`eop` 34, `host_swap_presents` 12, mesurés au cycle 328 sur le binaire de
référence, inchangés.

## 6. Front suivant

Deux routes, exclusives :

1. **Route config.** Repartir des sources de génération de l'arbre de référence
   telles quelles, plus le correctif `bctr` seul — combinaison qui régénère à
   `rc=0` — et traiter les pièges *atteignables* un par un par entrées
   `[functions]`, comme aux cycles 306-307. Le premier est
   `0x823841F8 -> 0x82383EE8`. Risque : cascade.
2. **Route reconstruction.** Refaire le correctif GapFill/retraceur depuis son
   rapport de cycle plutôt que depuis son patch, contre les sources actuelles.
   Plus coûteux, mais rend l'arbre de travail à nouveau capable de produire un
   corpus équivalent à celui de référence.

La route 1 est la moins chère et se teste immédiatement.

## 7. Règles ajoutées

1. **Ne jamais `rsync` par-dessus un arbre portant des modifications non
   commitées.** Commiter, ou `git stash push -u` avec étiquette, *avant*
   d'aligner deux arbres. Un patch exporté n'est pas une sauvegarde : il peut
   cesser de s'appliquer, et ici il ne suffit plus.
2. **Un corpus généré est un artefact, pas une sortie reproductible de l'arbre.**
   `generated/` de l'arbre de référence ne peut pas être reproduit par les
   sources de cet arbre. Tant que la chaîne de génération n'est pas
   reproductible, toute mesure faite sur le binaire de référence est
   ininterprétable en termes de sources.
3. **Vérifier une commande de comptage avant d'en publier le résultat.** Deux
   comptages de `REX_FATAL` incompatibles ont été publiés dans ce cycle avant
   d'être refaits ; c'est la même classe d'erreur que les regex majuscules du
   cycle 305.

`recompiler-generated` n'est pas `verified`.

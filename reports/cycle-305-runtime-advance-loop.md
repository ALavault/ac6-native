# AC6 cycle 305 — boucle d'avancement runtime automatisée

## Objet

Le cycle 304 a établi que l'abort runtime est un `REX_FATAL` de branche non
résolue, causé par une coupure `[functions]` traversée par une branche arrière,
et que le retrait de la coupure fait avancer la frontière. La méthode était
manuelle. Ce cycle l'automatise et la fait tourner.

## Outillage

`tools/ac6-advance-loop.sh` exécute la boucle complète :

1. régénérer le corpus depuis une configuration de travail ;
2. reconstruire le runtime ;
3. l'exécuter sous `gdb`, capturer le `REX_FATAL` atteint ;
4. localiser l'entrée `[functions]` **strictement** entre cible et source ;
5. la retirer ; recommencer.

`tools/ac6-next-split.py` fait l'analyse seule (étapes 3--4) et sert au
diagnostic ponctuel. Aucun des deux ne modifie la configuration du clone
vendorisé : la boucle travaille sur une copie.

Conditions d'arrêt, toutes explicites :

- plus d'abort dans le code recompilé — succès ;
- l'abort n'est pas une branche non résolue — travail manuel requis ;
- aucune coupure candidate entre cible et source — travail manuel requis ;
- budget d'itérations épuisé.

## Résultats mesurés

Coût par itération : environ 15 s de codegen, 2 min de build, 15 s
d'exécution — **environ 2 min 30 au total**.

La frontière traverse des fonctions **distinctes** à chaque avancée, ce qui
confirme une progression réelle dans le chemin de démarrage et non un
piétinement :

| Itération | Fonction en abort | Coupures retirées | Pièges restants |
| ---: | --- | --- | ---: |
| 1 | `sub_82345100` | `0x823452A8` | 4 857 |
| 2 | `sub_82345100` | `0x82345250`, `0x82345260`, `0x82345300` | 4 850 |
| 3 | `sub_82348FC8` | `0x82349050` | 4 838 |
| 4 | `sub_82349310` | `0x823493B0` | 4 836 |
| 5 | `sub_821F6080` | `0x821F60C0` | 4 833 |
| 6 | `sub_8239E628` | `0x8239E728` | 4 831 |
| 7 | — | — | 4 821 |

Le retrait d'une seule coupure résout parfois plusieurs branches : les baisses
observées vont de 2 à 12 pièges.

## Limites, à ne pas franchir sans preuve

- **Aucune de ces coupures n'est qualifiée** par un contrat headless. Elles sont
  justifiées par la résolution d'une branche mesurée, ce qui est plus faible que
  la preuve 28/28 produite pour `0x82345250` au cycle 302. Le clone vendorisé
  reste à l'état cycle 301 ; ces retraits ne sont **pas** promus.
- Franchir un piège ne rapproche pas mécaniquement d'un jeu jouable. Le runtime
  doit encore initialiser le graphique, l'entrée et charger une mission.
- `recompiler-generated` n'est pas `verified`.

## Ce que la boucle ne peut pas faire

Elle ne traite que les pièges causés par une coupure retirable. Un piège dont la
cible n'a aucune entrée entre elle et la source — saut calculé, données lues
comme du code, table de branchement non déclarée — arrête la boucle et exige une
analyse humaine. C'est délibéré : mieux vaut s'arrêter que retirer une coupure
au hasard.

## Le `std::bad_alloc` de ReXGlue : aléatoire et groupé

Ce défaut a coûté une semaine de gel au cycle 302, qui l'a imputé au retrait
d'une ligne de configuration. Trois hypothèses successives ont été testées et
**écartées** ici :

| Hypothèse | Test | Verdict |
| --- | --- | --- |
| causé par le retrait de `0x82345250` | codegen avec et sans, RSS relevée | **faux** — 0,1 % d'écart, aucun échec |
| combinatoire (deux retraits sûrs isolément) | bisection depuis la base intacte | **faux** — A, B et AB passent tous |
| corrélé à des codegen concurrents | rejeu seul, 110 Go libres, rien d'autre | **faux** — échoue quand même |
| déstabilisé par le retrait `0x821EC0D8` | 6 exécutions avec, 6 sans | **faux** — 0/6 échecs *avec* le retrait, 1/6 sans |

Une première bisection avait produit une configuration corrompue — 10 463
entrées reconstruites contre 10 472 à la base — et ses résultats, qui
semblaient confirmer l'hypothèse combinatoire, ont dû être jetés.

### Ce que les mesures montrent réellement

Le défaut est **aléatoire et groupé dans le temps**. Sur une configuration
strictement identique :

- une fenêtre de cinq minutes a échoué **8 fois sur 8** ;
- la même configuration passe immédiatement avant et après cette fenêtre ;
- l'état du répertoire de sortie n'est pas déterminant : vide -> échec,
  un fichier -> succès, répété.

Aucun mécanisme n'est établi, après quatre hypothèses testées et rejetées. La
mémoire disponible était de 110 Go et la charge modérée : ce n'est pas une
pénurie simple. Le taux d'échec observé va de 0/6 à 8/8 selon la fenêtre, sur
des entrées identiques.

### Cause trouvée : la réutilisation du répertoire de sortie

Une cinquième hypothèse s'est vérifiée. Les tests précédents étaient
**confondus** : ceux qui écrivaient dans un répertoire de sortie **neuf**
passaient, ceux qui réutilisaient le répertoire de la boucle échouaient. Je
n'avais pas contrôlé cette variable.

En donnant à chaque itération un répertoire de sortie neuf, les échecs
disparaissent : **0 échec de codegen** là où la même configuration échouait
8 fois sur 8 puis 3 fois sur 3.

C'est donc une **correction de cause**, pas une atténuation. Et cela explique
exactement le cycle 302 : sa commande fautive écrivait dans le `generated/`
existant — précisément le motif de réutilisation — et le rapport notait que le
répertoire avait été « vidé partiellement ».

**Règle : ne jamais faire écrire `rexglue codegen` dans un répertoire de sortie
déjà peuplé. Toujours un répertoire neuf, puis copier.**

### Règle opérationnelle

Ne jamais imputer un `std::bad_alloc` de ReXGlue à un changement sans avoir
rejoué la **même** configuration sur une fenêtre d'au moins dix minutes. La
boucle réessaie seize fois à quarante-cinq secondes d'intervalle pour couvrir
un groupe d'échecs complet.


# AC6 cycle 306 — les branches non résolues tombent à zéro

Le cycle 305 avait ramené les pièges `REX_FATAL("Unresolved branch ...")` de
4 857 à 26 par retrait de coupures `[functions]`, puis avait convergé : aucune
coupure supplémentaire ne faisait baisser le compte. Ce cycle attribue les 26
restants et les élimine.

**Résultat : 0 piège de branche non résolue**, 48 unités de traduction,
21 580 fonctions émises.

## 1. Correction d'une affirmation du cycle 305

Le cycle 305 concluait que les 26 pièges restants étaient « purement un défaut
du générateur, qu'aucun changement de configuration ne peut corriger ». C'est
faux pour une partie d'entre eux, pour une raison instructive.

La configuration contient **deux** familles d'entrées :

```
0x820F0070 = { name = "rex_sub_820F0070" }     <- 8 738 entrées nommées
0x820f5fb0 = {}                                 <- 35 entrées vides, minuscules
```

Toute l'instrumentation du cycle 305 — `ac6-next-split.py`, la boucle
d'avancement, les retraits en masse — filtrait sur `^0x([0-9A-F]{8}) = \{`.
Les 35 entrées en hexadécimal minuscule n'ont jamais été vues. Le compte
d'entrées annoncé au cycle 305 était donc incomplet, et ces 35 coupures
n'ont jamais été candidates au retrait.

Elles portent l'autorité `CONFIG` (« exact boundaries, immutable »), la plus
forte après `IMPORT`. Le générateur les respecte donc absolument, y compris
lorsqu'elles découpent une fonction `PDATA` en son milieu.

## 2. Deux défauts du générateur, distincts

### 2.1 GapFill recrée des fonctions dans les trous inter-blocs

`phase_gapfill.cpp` enregistre une fonction à toute adresse non couverte par un
**bloc**. Or une fonction possède des trous entre ses blocs : données de table
de saut, remplissage, code atteint seulement par un saut non résolu. GapFill y
crée une fonction, la découverte de blocs part de cette entrée et **retrace les
instructions du propriétaire**. La même adresse est alors émise dans deux
fonctions.

Dans la copie, une branche vers une adresse antérieure à l'entrée du retraceur
n'a pas de label : `REX_FATAL`.

`cleanupAbsorbedGapFills` était censé absorber ces doublons, mais son test
d'absorption utilise `containsAddress` (fondé sur les blocs). Par construction,
l'entrée d'un retraceur n'est dans aucun bloc du propriétaire — sinon GapFill ne
l'aurait pas créée. Le nettoyage ne pouvait donc jamais l'attraper.

Mesure instrumentée sur AC6 : **178 fonctions GAP_FILL** ont leur entrée dans
les *bornes* d'une autre fonction sans être dans ses blocs. Sur ces 178,
**15 retracent effectivement les blocs du propriétaire**. Ce sont exactement
elles qui produisent des pièges ; les 163 autres couvrent du code réellement
non couvert et doivent être conservées.

Le correctif élargit le test d'absorption à ce seul cas, et ne fusionne que les
blocs et labels que le propriétaire ne couvre pas déjà — `addBlock` et
`addLabel` ne déduplique pas, et un bloc dupliqué produirait deux fois le même
label dans une fonction, ce qui ne compile pas.

### 2.2 `classifyTarget` identifie l'appelant par adresse

`BuilderContext` connaît la fonction en cours d'émission (`fn`), mais appelait
`graph().classifyTarget(target, base, ...)`, qui redérive l'appelant via
`getFunctionContaining(base)`. Quand des fonctions se chevauchent, ce
`getFunctionContaining` renvoie un **autre** nœud que celui dont le corps est en
train d'être écrit. Une cible interne à ce corps est alors classée `Unknown`.

C'est ce qui produisait le cas relevé au cycle 305 dans
`__imp__sub_82318560` : le label `loc_82318648` est défini et atteint par deux
branches, et pourtant déclaré non résolu depuis une troisième source de la même
fonction.

Le correctif ajoute une surcharge `classifyTarget(target, const FunctionNode&,
bool)` et bascule les quatre appels des builders dessus.

## 3. Matrice de mesure

Chaque case est une exécution complète de codegen, répertoire de sortie neuf.

| | configuration cycle 305 | 35 entrées vides retirées |
| --- | ---: | ---: |
| générateur de référence | **26** | 16 |
| générateur corrigé | 8 | **0** |

Les deux changements sont **nécessaires** ; aucun ne suffit seul. Le générateur
seul plafonne à 8, la configuration seule à 16.

Le générateur reconstruit reproduit le corpus de référence **bit pour bit**
avant application des correctifs (`diff -rq` vide sur 48 unités), ce qui établit
que la matrice mesure bien les correctifs et non l'environnement de compilation.

## 4. Vérification compilation, édition de liens, exécution

Le corpus corrigé a été compilé et lié en entier, puis exécuté, dans un arbre de
compilation **isolé** (worktree git du clone vendorisé), aligné sur les options
de l'arbre de référence (`-march=x86-64-v3`, sysroot `ac6-runtime-dev-sysroot`).

| | corpus cycle 305 (26 pièges) | corpus corrigé (0 piège) |
| --- | --- | --- |
| compilation, 48 unités | 0 erreur | **0 erreur** |
| édition de liens | réussie, 157,6 Mo | **réussie, 157,6 Mo** |
| smoke `xvfb`, 60 s | exit 124 (survit) | **exit 124 (survit), 2 fois sur 2** |

Le contrôle est indispensable : le corpus cycle 305 a été compilé et exécuté
dans **le même** arbre, de sorte que la comparaison isole le correctif et non
l'environnement de compilation. Deux faux signaux ont été écartés en chemin —
un `exit 1` dû à des options de compilation différentes, puis un `exit 1` dû à
`game_data_root` résolu relativement au répertoire du binaire.

Le processus corrigé travaille réellement, mesuré à 40 s d'exécution :
60 fils, 782 Mo de RSS, 4,68 Go lus, environ 254 % de CPU, `DATA00.PAC` et
`DATA01.PAC` ouverts.

Le clone de référence est resté intact : binaire `rexglue` identique au hash
d'origine, `generated/` toujours à 26 pièges et 48 unités, aucune modification
des sources du SDK.

## 5. Ce qui n'est pas résolu

- **205 `REX_FATAL("Unresolved call ...")`** subsistent. C'est une classe
  différente — appels indirects et cibles non résolues dans le graphe — et elle
  n'est pas traitée ici. Elle devient le prochain compteur de distance.
- Les 35 entrées retirées ne sont **pas qualifiées** par un contrat headless.
  Elles sont justifiées par la résolution de branches mesurées et par le fait
  qu'elles découpent des fonctions `PDATA` en leur milieu, ce qui est plus
  faible qu'une preuve de frontière.
- Zéro piège de branche ne signifie pas jeu jouable. Cela signifie qu'aucun
  `REX_FATAL` de cette classe ne peut plus être atteint.
- `recompiler-generated` n'est pas `verified`.

## 6. Reproduction

Le correctif générateur est versionné comme patch :
`patches/rexglue-unresolved-branch-gapfill-retracer-20260726.patch`
(5 fichiers, +84/-11), applicable sur le clone vendorisé à `dev-test`.

Les 35 entrées de configuration à retirer :

```
0x820f5fb0 0x820f5fc0 0x820f5fe0 0x820f5ff0 0x820f6000 0x820f6020 0x820f6040
0x820f6070 0x820f6080 0x820f6090 0x821e27b8 0x822015b8 0x82272668 0x82272918
0x82272990 0x822729a8 0x82272c08 0x82272c20 0x82272c38 0x82272c50 0x82272c68
0x82272c98 0x82272cc8 0x822e0eb8 0x822f0bd8 0x822f7120 0x82306c88 0x82322300
0x82322330 0x82322340 0x82366a88 0x8237e3e8 0x823a6c30 0x823af140 0x823b94d0
```

Toutes de la forme `0xADDR = {}`. Le filtre à utiliser est
`^0x[0-9A-Fa-f]{8} = ` — **jamais** une classe restreinte aux majuscules.

## 7. Leçon d'outillage

Une expression régulière trop étroite a masqué 35 entrées pendant tout un cycle
et a produit une conclusion fausse sur la nature du blocage. Les comptes
d'entrées publiés au cycle 305 sont à relire avec cette réserve.

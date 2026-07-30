# Cycle 326 — l'invité ne boucle pas, il attend ; et la correction 323 fait avancer le jeu

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Binaire A, correction 323 appliquée :
  `e639a9fe5cd0504a416750c18005d3aa07a872f26763c61fa4f2dd0fa83a3b05`
- Binaire B, `HEAD` sans la correction :
  `dba7498b1cc332a821f49c63d464b9fb32658266ff6bfe4fa54516325da29b39`
- Route : `dynamic`, profilage `perf` autorisé par l'opérateur

## 0. Nouveauté de plateforme

L'opérateur a posé `kernel.perf_event_paranoid=1` et
`kernel.yama.ptrace_scope=0`. Le `PLAYABLE_PLAN.md` §3 décrivait leur absence
comme une contrainte de méthode ; elle est levée. `perf record --pid` et
l'unwinding DWARF fonctionnent. `eu-stack -p` échoue toujours par timeout sur ce
binaire LTO de 165 Mo — noté pour ne pas y revenir.

## 1. A/B de la correction du cycle 323 : elle fait avancer le jeu

Le cycle 323 avait honnêtement conclu « cette correction ne débloque pas la
présentation ». C'était vrai de ce qu'il pouvait mesurer — l'instrument n'existait
pas encore. Avec les compteurs d'anneau du cycle 325, le contraste est net.

| compteur, ~60 s | B : `HEAD` | A : avec correction | facteur |
|---|---:|---:|---:|
| `eop` (source 1) | **4** | **12** | ×3 |
| `guest_swap_requests` | 2 | 4 | ×2 |
| `host_swap_presents` | 2 | 3 | — |
| `wptr_updates` | 10 | 20 | ×2 |
| position anneau `wptr == rptr` | `0x25` | `0x43` | — |
| `primary_executions` | 7 | 11 | — |
| `pm4_interrupt` / `pm4_swap` | 4 / 2 | 12 / 4 | — |

La correction du cycle 323 **double à triple la progression de l'invité** avant
qu'il ne se bloque. C'est son premier effet runtime mesuré, et il justifie de la
conserver indépendamment de la régression isolée qui l'avait qualifiée.

**Validation croisée forte** : le binaire `HEAD` donne `VdSwap 2`,
`présentations 2`, `EOP 4` — soit **exactement** les chiffres des cycles 316
et 317. Les anciennes mesures et le nouvel instrument se confirment mutuellement,
et l'écart `eop 4 → 12` entre le cycle 317 et le cycle 324 est expliqué : c'est la
correction, pas du bruit.

Le blocage subsiste dans les deux binaires. Il est donc en aval.

## 2. Le CPU brûlé n'est pas l'invité

Profil `perf`, 8 854 échantillons sur le binaire A, 2 933 en DWARF.

Répartition par thread :

| part du CPU | thread |
|---:|---|
| **56,1 %** | **`Audio Worker`** |
| 28,8 % | `ac6recomp` (thread hôte principal) |
| 4,5 % | `XThread7087D6C0` |
| 3,0 % | `GPU Commands` |
| 1,7 % | `Main XThread` |
| ~4 % | tous les autres `XThread*` cumulés |

**Les threads invités cumulent environ 9 % du CPU.** L'invité ne boucle pas :
**il attend.**

Cela réfute la prémisse de P0.2 telle qu'écrite au cycle 325 — « quel code invité
tourne sans soumettre ? ». Il n'y a pas de boucle chaude invitée à trouver, et
l'histogramme d'appels indirects qui y était prévu aurait coûté une
recompilation complète du corpus pour mesurer un chemin froid. Prémisse corrigée
avant dépense : c'est la valeur de la mesure.

Cela révise aussi le cycle 314, qui attribuait 25,9 s de CPU à
« `XThread5487D6C0` » et en concluait « le jeu exécute une boucle réelle ». Le
thread le plus consommateur est un thread **hôte** du SDK ; l'inférence « donc
l'invité travaille » n'est pas soutenue par ce profil.

## 3. Le brûleur : `WaitMultiple`, et il est antérieur

Profil plat, binaire A :

| part | symbole |
|---:|---|
| 20,38 % | `PosixConditionBase::WaitMultiple` (51,65 % en inclusif) |
| 11,26 % | `pthread_mutex_trylock` |
| 8,32 % | `__pthread_mutex_unlock_full` |
| 4,20 % + 2,56 % | `steady_clock::now` + `clock_gettime` |
| 3,60 % + 2,61 % + 1,57 % | `free` + `malloc` + `operator new` |

Environ 55 % du CPU total est une **attente active**. `WaitMultiple` est un
sondage à 1 ms qui, à chaque tour : `trylock` le mutex de chaque handle, alloue un
`std::vector` de verrous, lit l'horloge, relâche tout, dort 1 ms, recommence.
Les allocations et l'horloge du tableau ci-dessus en sont la conséquence directe.

**Antérieur à la correction du cycle 323**, donc pas une régression que j'aurais
introduite : `WaitMultiple` pèse **20,28 %** sur `HEAD` contre **20,38 %** avec la
correction, et les huit premiers symboles des deux profils sont les mêmes aux
dixièmes de point. C'était la vérification à faire avant toute autre : le cycle
323 avait lui-même signalé `WaitMultiple` comme divergence ouverte de sa
correction, il fallait donc écarter la possibilité de l'avoir causé.

Deux défauts distincts sont ici, à ne pas confondre :

1. **performance** : un thread hôte brûle un cœur en sondant à 1 ms avec une
   allocation par tour. C'est du gaspillage, pas un blocage ;
2. **correction, toujours ouverte depuis le cycle 323** : `WaitMultiple` lit
   `signaled()` et appelle `post_execution()` sans passer par `Wait`, donc un
   `Signal()` qui remet son jeton à un attendeur `Wait` déjà en file **n'est pas
   observé** par un attendeur `WaitMultiple` du même événement. Le cas mixte reste
   une divergence.

## 4. Front P0.2, corrigé

L'invité est **bloqué en attente**, pas en boucle. La question devient :
**sur quoi chaque thread invité attend-il, et depuis combien de temps ?**

L'instrument juste est un **recensement des attentes invitées**, dans l'arbre,
derrière le cvar de télémétrie : dans `NtWaitForSingleObjectEx`,
`NtWaitForMultipleObjectsEx` et `NtSignalAndWaitForSingleObjectEx`, tenir par
thread invité le handle attendu et l'instant d'entrée, puis émettre
périodiquement « thread T attend le handle H depuis N ms ». Coût : une
reconstruction SDK, pas du corpus.

C'est l'instrument que P0.2 doit utiliser, et l'histogramme d'appels indirects du
cycle 325 est **retiré** : il mesurerait un chemin froid.

Les 4,5 % de `XThread7087D6C0` sont le seul thread invité notable ; à identifier
au même passage.

## 5. Validation exécutée

```text
sysctl perf_event_paranoid / ptrace_scope         1 / 0, perf fonctionnel
build A (correction) et B (HEAD), 2 reconstructions   rc=0
perf record 25 s, binaire A                        8 854 échantillons
perf record 25 s, binaire B                        7 794 échantillons
perf record DWARF 20 s                             2 933 échantillons, frames invitées résolues
A/B compteurs d'anneau                             eop 4→12, swaps 2→4, wptr_updates 10→20
B reproduit les cycles 316/317                     VdSwap 2, présentations 2, EOP 4
WaitMultiple, B contre A                           20,28 % contre 20,38 %
correction restaurée après l'A/B                   41 insertions présentes
fuites nettoyées                                   1 276 Mio récupérés
```

Ni preuve de jouabilité, ni preuve de parité retail.

## 6. Porte P0

**Non franchie.** Exigé : `wptr` avançant, `eop` monotone au-delà de 100,
`host_swap_presents` au-delà de 600 sur 60 s. Mesuré, meilleur binaire : `wptr`
gelé à `0x43`, `eop` 12, `host_swap_presents` 3.

Progression réelle malgré tout : le meilleur binaire va deux à trois fois plus
loin qu'au cycle 317, et la cause est identifiée et conservée.

## 7. Règle ajoutée

**Vérifier l'attribution d'un profil à un thread avant d'en tirer une conclusion
sur l'invité.** Le cycle 314 a conclu « le jeu exécute une boucle réelle » depuis
un compteur de CPU par thread sans vérifier que le thread était invité ; il ne
l'était pas. Un profil par symbole sans répartition par thread aurait ici conduit
à recompiler tout le corpus pour instrumenter un chemin froid.

# Cycle 1831 — couche 1 : l'interruption graphique est délivrée hors contrat DPC

> Artefact jumeau :
> `artifacts/goal-playable/layer1-dpc-contract-20260825/RESULT.md`
> (`artifacts/` est ignoré par politique ; ce report est la copie durable).

## Qualification

Comparaison de source à source, **sans aucun run**. Références lues dans
l'arbre Xenia Edge du workspace (`ac6_demo_work/xenia-edge-source`), citées par
fichier et ligne, pas paraphrasées de mémoire. Côté port : `HEAD` à
`0bf7d2fa`.

## Divergence 1 — pas d'impersonation DPC

Xenia délivre le callback d'interruption CP en **impersonnant un DPC**, et le
commente explicitement (`src/xenia/kernel/kernel_state.cc:1314`) :

> *in reality, our interrupt is a callback that is called in a dpc which is
> scheduled by the actual interrupt — we need to impersonate a dpc*

`BeginDPCImpersonation` (`kernel_state.cc:1298`) fait deux écritures dans le
KPCR **invité** :

```cpp
kpcr->current_irql = 2;
kpcr->prcb_data.dpc_active = 1;
```

Le port (`src/guest_bridge/lifecycle.hpp:258`, `dispatch_graphics_interrupt`)
prépare un contexte d'interruption correct par ailleurs — `r3 = source`,
`r4 = context`, pile dédiée, CPU actif forcé à 2 en `r13+268`, ce qui
correspond au `SetActiveCpu(2)` de Xenia — mais **ne touche ni `current_irql`
ni `dpc_active`**. Le seul octet du KPCR qu'il écrit est le CPU actif.

L'interruption s'exécute donc à IRQL passif, alors que le code invité qu'elle
appelle suppose le niveau DPC : sur le chemin activé par `HSIO=1`, le thread 2
appelle **2892 fois** `KeAcquireSpinLockAtRaisedIrql` et
`KeReleaseSpinLockFromRaisedIrql` — deux primitives dont le nom même énonce le
prérequis.

## Divergence 2 — l'IRQL du port est invisible pour l'invité

Xenia range l'IRQL dans le KPCR invité
(`xboxkrnl_threading.cc:1366-1382`) :

```cpp
uint32_t old_irql = pcr->current_irql;   // KeRaiseIrqlToDpcLevel
pcr->current_irql = 2;
...
if (new_irql > kpcr->current_irql) { ... }   // KfLowerIrql
```

Le port le range côté hôte (`src/guest_bridge.cpp:196`) :

```cpp
thread_local std::unordered_map<std::uint32_t, std::uint8_t> guest_irql;
```

indexé par identifiant de thread, et ses shims `KeRaiseIrqlToDpcLevel` /
`KfLowerIrql` ne lisent et n'écrivent que cette table
(`kernel_objects_dispatch_original.hpp:600-616`). **Aucune écriture n'atteint
la mémoire invitée.** Du code titre qui lit `KPCR->current_irql` directement
— ce que du code noyau-adjacent fait couramment sur 360 — observe donc une
valeur qui n'a jamais bougé.

## Pourquoi c'est la bonne couche

Les deux divergences vivent exactement dans le chemin que `HSIO=1` active et
que `HSIO=0` n'active pas. Le régime d'interruption est d'ailleurs, à la
mesure, **la seule chose qui distingue les deux exécutions** : dix arêtes
exclusives, toutes sur les threads 2 et 13, toutes issues de
`0x821B9768`.

Et le mécanisme sur lequel le blocage a été isolé — `sub_822E4018` attend une
fence que le handler d'interruption estampille — est précisément un rendez-vous
entre du code à IRQL passif et du code censé être à IRQL DPC, protégé par des
spinlocks « at raised IRQL ». Si le niveau n'est pas modélisé, l'exclusion
mutuelle que l'invité a choisie n'est pas celle qu'il obtient.

## Ce qui n'est PAS établi

- **Que ces divergences causent l'effondrement de la fence.** Elles sont dans
  le bon chemin et de la bonne nature, ce qui en fait des candidats sérieux —
  pas une démonstration. Aucun lien causal n'est mesuré ici.
- Que le titre lise `KPCR->current_irql` directement. C'est courant, ce n'est
  pas vérifié sur ce binaire.
- La cadence de l'interruption. Le port tire `source=0` une fois par tick ;
  Xenia la tire depuis `MarkVblank()`. Nominalement équivalent puisque le
  timebase du port modélise un tick à 60 Hz — mais chez Xenia le vblank est
  cadencé par le temps réel et l'horloge invitée avance **entre** deux
  interruptions, alors que dans le port l'interruption et l'horloge sont la
  même variable `tick_`. Cette coïncidence structurelle est notée ; elle n'est
  pas prouvée nuisible.

## Expérience suivante, bornée

Ajouter l'impersonation DPC au dispatch du port — les deux écritures KPCR de
Xenia, encadrant l'appel — puis rejouer l'A/B `HSIO=1` / `HSIO=0` à
3000 ticks en headless et regarder un seul chiffre : le nombre d'appels de
`NtSignalAndWaitForSingleObjectEx` par le thread 1. Il vaut 5 aujourd'hui sous
`HSIO=1` et 5611 sous `HSIO=0`.

Il faut au préalable établir l'offset de `current_irql` et de `dpc_active` dans
le KPCR de ce binaire ; le port n'utilise aujourd'hui que `r13+268`, et aucun
des deux autres offsets n'est qualifié.

## État mesuré des deux chemins, pour audit

```
chemin          presents   ring subs   blocked   runnable   slice_exh
HSIO=1 (HEAD)       1022        1116        22          1        2998
HSIO=0              2892           0        23          0         215
```

`frontend=false`, `mission=false`, `terminal=false` des deux côtés.

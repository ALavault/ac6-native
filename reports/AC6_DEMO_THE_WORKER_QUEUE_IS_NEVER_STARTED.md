# La file de travail D3D n'est jamais démarrée

Date : 2026-08-18

## La chaîne, nommée par Microsoft

```text
D3D::StartWorkerQueue(CDevice*)   0x821C4A60   JAMAIS atteinte (oracle : oui)
   pose l'événement à [objet+32], soit device+11508
D3D::WorkerThread(void*)          0x821C4970   atteinte 2 fois — les deux
   threads parqués, KeWaitForSingleObject rend 258 (timeout)
file de rendu                     producteur 7495 avances, consommateur 0
```

`StartWorkerQueue` marque **10/80 dans `d3d9.lib` et 10/80 dans `d3d9i.lib`**,
contre un plancher de bruit mesuré à 0/40 (`b417938f`). `WorkerThread` marque
9/80. Les deux threads bloqués au LR `0x821C4A28` de `5a7c3511` sont donc les
threads de travail de D3D, et ce qui les réveillerait n'est jamais appelé.

## Le fait large

Douze sites d'appel de `KeSetEvent` existent dans l'image. **Les douze sont
dans des fonctions non atteintes en natif et atteintes par l'oracle.** L'invité
ne signale donc aucun événement de tout le run ; les 32 publications observées
viennent du bridge lui-même.

Et le compte des clés :

```text
5 clés publiées      0xE0000040 0xE0000044 0xE000004C 0xE0000058 0xE0000060
18 clés attendues et jamais publiées, dont
   0x100446F4  0x10044744   les deux événements des threads de travail D3D
   douze poignées 0xE00000xx des seize threads workers en 0x821A8C88
```

Ce n'est ni une inadéquation d'espace de clés — les deux espaces figurent des
deux côtés — ni un cas isolé.

## Une limite de mes outils, dite plutôt que contournée

`0x821C4A60` n'a **aucun site d'appel repérable statiquement** : aucun
`sub_821C4A60(ctx, base)` dans le C++ généré, et son adresse n'apparaît nulle
part dans l'image hors `.pdata`. C'est normal pour un `bl` PPC, qui encode un
déplacement relatif et non l'adresse — mais mon graphe d'appels inversé lit
précisément les `bl` du code généré, et il n'en trouve aucun.

C'est la **deuxième** fonction dans ce cas, après `D3D::InitializeApiState`
(`b93f52dd`) : atteinte par l'oracle, jamais par nous, sans appelant
découvrable. Il existe donc une classe de points d'entrée D3D atteints par un
mécanisme que mes outils statiques ne voient pas, et je ne sais pas encore
lequel.

## Non établi

- Comment `StartWorkerQueue` est appelée sur console.
- Si la démarrer suffirait : les seize autres threads workers attendent douze
  poignées également jamais publiées, et rien ne dit qu'elles dépendent de la
  même cause.

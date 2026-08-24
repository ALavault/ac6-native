# Cycle 1825 — `VdIsHSIOTrainingSucceeded` arbitre le ring contre la progression

## Qualification

- Projet Ghidra `ghidra-projects/ace-combat-6-demo`, XEX
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Store `artifacts/goal-playable/brandlogo-list2-window-runtime-20260823/store`,
  **byte-identique** à `.build/ac6-demo-atomic-neutral-store` (vérifié, pas
  supposé).
- Quatre runs headless, 3000 ticks, aucune entrée, mêmes observateurs.
- Manifeste codegen `465c279f52993216` partout. Aucun oracle.
- Preuve : `artifacts/goal-playable/hsio-mode-transition-tradeoff-20260824/RESULT.md`.

## Le résultat

Un seul mot de la couche bridge décide si la démo affiche quelque chose ou si
elle progresse — et aujourd'hui, jamais les deux.

```
HSIO=1 (HEAD)   ring : 502 soumissions      mode : bloqué à StartUp, jamais Title
HSIO=0          ring : 0 soumission         mode : Title publié au tick 2369
```

`VdIsHSIOTrainingSucceeded` retournant 0 fait prendre à l'invité son chemin
logiciel de repli ; retournant 1, il prend le chemin ring. Le cycle 1810 l'a
passé de 0 à 1 pour restaurer le ring, ce qu'il a effectivement fait. Le coût
n'avait pas été mesuré : sur le chemin ring, l'invité attend des événements que
le port ne lui rend pas.

La signature est dans l'ordonnanceur, et elle est franche :

```
HSIO=0   23 threads bloqués, 0 runnable,   215 épuisements de tranche / 3000
HSIO=1   22 threads bloqués, 1 runnable,  2998 épuisements de tranche / 3000
```

Sous HSIO=0 l'invité attend proprement et le temps avance. Sous HSIO=1 un
thread tourne en boucle pendant que les autres attendent pour toujours.

## Comment on y est arrivé, et ce que cela réfute

L'observation de départ était une absence : aucune transition de mode après le
tick 223 dans les runs de ce cycle, alors que le cycle 1793 avait consigné le
Title publié au tick 2369. Trois hypothèses ont été éliminées par mesure, dans
cet ordre :

1. **Régression récente du binaire.** Réfutée : le témoin conservé du 22 août
   (`ed42da2b`) et le binaire courant (`39bc06a1`) donnent des résultats
   rigoureusement identiques sur les quatre observables — ticks de mode,
   nombre de présentations, appels natifs, état de l'ordonnanceur.
2. **Différence de store.** Réfutée : `sha256sum` des neuf fichiers, aucune
   différence avec le store neutre.
3. **Différence de code invité.** Réfutée : même manifeste codegen.

Restait la couche bridge. Le run qui transitionnait datait du 20 août, et
**tous les commits antérieurs à ce cycle datent du 20 août** : les changements
de bridge des 20–24 août n'étaient pas versionnés, et c'est le lot que le cycle
1823 a versé dans HEAD. La régression était donc contenue dans un diff connu de
8236 lignes sur 40 fichiers.

Plutôt que de bisecter à l'aveugle, la prémisse a été testée d'abord : bridge
entièrement ramené au 20 août, reconstruit, rejoué — StartUp état 2 au tick
2426, Title au 2429. La prémisse tenait. Le suspect a ensuite été isolé par
lecture du diff sur le seul axe plausible (ce qui décide du chemin pris par
l'invité), puis vérifié **à une variable près** sur HEAD.

Ce résultat réfute aussi, rétrospectivement, l'objet de plusieurs cycles :
la question « quel producteur naturel arme `manager+0x18` depuis START »
n'avait pas de réponse parce qu'elle n'avait pas d'objet. Sous HSIO=0 la chaîne
StartUp→Title se produit seule, sans START, sans producteur manquant.

## Décision prise

`kVdHsioTrainingSucceededResult` **reste à 1** dans le dépôt. Passer à 0
échangerait un blocage contre un autre : la progression contre toute sortie
renderer. Aucune des deux valeurs n'est le correctif, et choisir la moins
gênante des deux masquerait le vrai défaut.

Le correctif est de servir, sur le chemin ring, ce que l'invité attend — de
sorte que ses threads débloquent comme ils le font sur le chemin de repli.

## Non établi

- **Quel événement l'invité attend.** C'est le gate suivant, et il est
  maintenant précisément posé : comparer HSIO=0 et HSIO=1 sur le premier thread
  qui se bloque sans réveil. Le rapport de probe porte déjà `wait_key`,
  `wait_lr`, `wait_kind` et `wake_tick` par thread, plus la liste
  `event_publications` — aucune instrumentation nouvelle n'est nécessaire pour
  la première passe.
- Que la transition suffise à atteindre le menu puis le gameplay. Elle publie
  le Title ; la suite n'est pas mesurée.
- Le comportement en backend Vulkan sous HSIO=0 ; les quatre runs sont
  headless.

## État

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

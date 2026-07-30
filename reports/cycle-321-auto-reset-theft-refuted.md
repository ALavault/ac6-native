# Cycle 321 — vol de signal auto-reset réfuté par régression

> **[VERDICT INVALIDÉ PAR LE CYCLE 323 — À LIRE EN PREMIER]**
> Le verdict `AC6_AUTO_RESET_EVENT_REFUTES_THEFT 3/3` de ce cycle **ne
> s'applique pas au protocole réel**. Son banc gelait la valeur partagée à `0`
> et injectait 200 signaux indépendants ; sous ce modèle un signal volé ne peut
> effectivement que coûter du débit, puisqu'un autre signal arrive toujours. Le
> protocole invité est un ping-pong strict : `sub_8233BA78` attend `0` puis pose
> `1`, `sub_8233AD70` attend `1` puis pose `0`, et **chaque changement d'état
> n'est annoncé qu'une seule fois**. Le banc du cycle 321 ne peut pas exprimer
> cette contrainte, donc il ne pouvait pas trouver la famine.
> Le cycle 323 rejoue le protocole réel et obtient un interblocage
> **déterministe dès l'itération 1, 3/3**, avec valeur `0` et deux threads
> endormis — exactement l'état mesuré dans le runtime.
> La conclusion « la correction ne doit pas être écrite » est donc **annulée** :
> la variante à file d'attente corrige l'interblocage 3/3. Voir
> `reports/cycle-323-self-consumed-wake-and-contamination-sweep.md`.
>
> Reste valide : la lecture du code SDK (`Signal()` ne relâche bien qu'un seul
> waiter, ce n'est pas une approximation par booléen partagé) et la correction
> de registre `CURRENT.json` en fin de rapport.

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Base image : `0x82000000`
- Route : `deterministic-fast-path`

## Question

Le cycle 320 a nommé, comme candidat racine principal du gel de la boucle de
frames, le fait que l'implémentation POSIX d'événement auto-reset laisserait un
waiter tardif voler le signal destiné à un waiter éligible déjà endormi. Il
exigeait explicitement une régression SDK ciblée **avant** de modifier le
runtime. C'est cette régression.

## Ce que le code fait réellement

`PosixCondition<Event>` dans
`rexglue-sdk/src/core/threading_posix.cpp` :

```cpp
bool Signal() override {
  auto lock = std::unique_lock<std::mutex>(mutex_);
  signal_ = true;
  cond_.notify_all();
  return true;
}
inline void post_execution() override { if (!manual_reset_) signal_ = false; }
```

Tous les waiters sont réveillés, courent au mutex, et le gagnant consomme le
booléen. **Un seul waiter est relâché** : c'est bien la sémantique auto-reset,
pas une approximation. Windows relâche également un seul waiter sur un
`EventSynchronizationObject`, et l'ordre de sélection n'y est pas garanti non
plus. La différence réelle entre les deux backends n'est donc pas
« booléen partagé contre vrai événement », mais **l'équité de sélection**.

## Régression

`scripts/ac6_auto_reset_event_regression.cpp` réplique verbatim
`PosixConditionBase::Wait` et `PosixCondition<Event>`, puis rejoue la forme
observée au cycle 320 : deux waiters sur le même événement, valeur partagée
figée à `0`, donc un seul éligible. Le paramètre `reentry_delay` ralentit le
waiter éligible, condition la plus favorable au vol.

Le critère est falsifiable : si le vol de signal est le mécanisme du gel, le
waiter éligible doit pouvoir atteindre **zéro** progrès alors que les signaux
continuent d'arriver.

Trois exécutions, 200 signaux :

| retard de ré-entrée | éligible | non éligible |
|---:|---:|---:|
| 0 µs | 100 / 105 / 102 | 100 / 95 / 98 |
| 50 µs | 58 / 67 / 78 | 142 / 133 / 122 |
| 500 µs | 100 / 59 / 84 | 100 / 141 / 116 |
| 5000 µs | 25 / 24 / 22 | 175 / 176 / 178 |

```text
AC6_AUTO_RESET_EVENT_REGRESSION REFUTES_THEFT   (3/3, exit 0)
```

Même en adversarial, avec le waiter éligible 5 ms plus lent à se remettre en
attente, il obtient encore 22 à 25 réveils sur 200. Le vol de signal **dégrade
le débit, il n'affame jamais**.

## Conséquence

Deux raisons structurelles au résultat :

1. un signal volé ne fait que retarder le waiter éligible jusqu'au signal
   suivant ; il n'annule rien ;
2. un signal délivré alors qu'aucun waiter n'est présent est **conservé** dans
   le booléen `signal_`, donc aucun signal n'est perdu.

Le gel observé au cycle 320 — deux threads endormis, stables sur 30 secondes —
exige que l'événement **cesse d'être signalé**. Le vol de signal ne peut pas
produire cet état.

L'attribution du cycle 320 au « POSIX auto-reset wake ownership » est donc
**réfutée**, et sa confiance `cross-match` doit être retirée. La correction
qu'elle proposait ne doit pas être écrite : une variante FIFO à tickets a été
mesurée en préparation de ce cycle et donne au waiter éligible *moins* de
réveils que la course actuelle (environ 100 contre 104--163 sans retard).

L'observation dynamique du cycle 320 reste valide et qualifiée : deux threads
invités endormis sur le même `XEvent`, `manual_reset_ = false`, valeur partagée
`0`. Seule son interprétation causale tombe.

## Front redirigé

La question redevient : **qui doit signaler `0xF80000A8`, et pourquoi
s'arrête-t-il ?** C'est le côté producteur, pas le côté attente. Les cycles 316
« guest stops requesting frames » et 317 « interrupt source and MMIO gate »
sont les prédécesseurs directs de cette question et doivent être repris avant
toute modification du SDK.

## Correction de registre

`reports/handoff/CURRENT.json` déclarait AC6 au cycle **304**
(`cycle-304-runtime-abort-unresolved-branch.md`) alors que les rapports vont
jusqu'à 320. Le registre accusait seize cycles de retard, et sa `next_action`
— « iterate the loop from sub_82348FC8 » — était périmée : cette boucle a été
dépassée depuis. Le registre est remis à jour par ce cycle.

## Modifications

- `workspaces/ace-combat-6/scripts/ac6_auto_reset_event_regression.cpp`
  (nouveau).

Aucun code SDK, aucune sortie générée et aucun patch runtime n'ont été
modifiés : le résultat de la régression est précisément de ne pas les modifier.

## Validation exécutée

```text
g++ -std=c++20 -O2 -pthread ac6_auto_reset_event_regression.cpp    PASS
régression, 3 exécutions                              REFUTES_THEFT 3/3
```

Ce n'est ni une preuve de jouabilité ni une preuve de parité retail.

# `StartWorkerQueue` est mise en file sur le GPU, pas appelée par le CPU

Date : 2026-08-18

## La question laissée ouverte par `68936168`

« Comment `StartWorkerQueue` (`0x821C4A60`) est appelée sur console » — aucun
`bl` ne la cible dans tout le code généré, et aucun mot de l'image ne contient
son adresse hors `.pdata`.

## La réponse

Elle n'est **pas** appelée par une instruction CPU. Son adresse est composée
puis **écrite comme donnée dans le tampon de commandes**, exactement comme
n'importe quel mot du buffer que `sub_821C57D0` (la chaîne `m_pRing`/
`m_pRingGuarantee` établie dans `b8482726`) fait avancer.

```c
// sub_821B9110 (non atteinte), corps de sub_821B8ED8 (non atteinte)
r11 = 0x821C0000;
r5  = r11 + 0x4A60;          // = 0x821C4A60 = D3D::StartWorkerQueue
r3  = objet device; r4 = r27;
bl sub_821BAA78;              // ATTEINTE 23 504 fois
```

`sub_821BAA78` garde de l'espace au moyen du même couple `[objet+48]` /
`[objet+56]` que la chaîne de `sub_821C57D0`, puis appelle
`sub_821BA1F8` (**atteinte 35 367 fois**) avec l'adresse en `r6`. Cette dernière
l'écrit littéralement dans le tampon :

```c
PPC_STORE_U32(ea, ctx.r6.u32);   // ea = curseur d'écriture courant du buffer
```

`StartWorkerQueue` est donc un **paquet de type 3**, une commande destinée au
GPU, pas une adresse de retour CPU. Elle ne s'exécute que lorsque le GPU
consomme cette portion du buffer — exactement le mécanisme que `5da91f72` et
`68936168` ont établi bloqué : le pointeur d'écriture de l'anneau n'a pas
bougé depuis le tick 0.

## Le second maillon, également vérifié

`0x821C4E40` (**atteinte une fois**) compose l'adresse de `WorkerThread`
(`0x821C4970`) de la même façon et la passe à `sub_821A6B38` (**atteinte 21
fois**), une trampoline vers `sub_821A8CB8` (**atteinte 22 fois**) — la
création de thread réussit donc côté CPU, avec `WorkerThread` comme point
d'entrée. C'est cohérent avec l'instantané de `5a7c3511` : deux threads
existent et sont bloqués, précisément au LR de leur attente.

## La chaîne complète, refermée

```text
sub_821C4E40 (1)       compose WorkerThread, crée le thread réel
   -> le thread démarre, attend l'événement à device+11508

sub_821B8ED8 (jamais)  compose StartWorkerQueue
   -> sub_821BAA78 (23504)  réserve l'espace d'anneau
   -> sub_821BA1F8 (35367)  écrit l'adresse comme donnée du buffer
   -> attend le GPU pour s'exécuter
   -> le GPU n'avance pas depuis le tick 0
   -> l'événement n'est jamais posé
   -> les deux threads de travail restent parqués
```

Ce n'est pas une deuxième panne indépendante. C'est le même verrou —
l'anneau jamais déclenché — vu depuis une frontière différente : même le
démarrage du sous-système de threads de travail de D3D en dépend.

## Ce qui n'est pas généralisé

`D3D::InitializeApiState` (`b93f52dd`), l'autre fonction sans appelant
découvrable, **n'utilise pas** ce mécanisme : la même recherche de composition
d'adresse ne trouve aucun site. Les deux fonctions sans appelant CPU ne
partagent donc pas nécessairement une cause.

## Non établi

- Si `sub_821A8CB8` produit un thread valide ou trappe silencieusement ; rien
  ici ne le montre, seule sa reachability est mesurée.
- Ce qu'`InitializeApiState` utilise à la place, s'il utilise un mécanisme
  d'entrée différent.

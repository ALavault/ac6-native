# Où la démo cesse de dessiner : `sub_821BD970` n'est jamais dispatchée

Date : 2026-08-17
Cible : démo PAL `Default.xex` `de917873…405da8`
Mesures : sonde 5 600 ticks, route par défaut, atlas de réachabilité
(`probe --atlas`, 2 710 fonctions atteintes)

## La chaîne, du pixel noir vers sa cause

1. 5 463 PRESENT, readback noir constant.
2. Le ring Xenos ne reçoit que **4 écritures de pointeur, toutes au tick 0**
   (`AC6_DEMO_WATCH_RING_KICK`). Aucune soumission après le boot.
3. Les threads 18 et 19 — entrée `0x821C4970` — sont garés sur les événements
   noyau `0x100446F4` et `0x10044744`, `wait_lr = 0x821C4A28`, à l'intérieur de
   leur propre fonction d'entrée.
4. `KeSetEvent`, `KePulseEvent`, `KeResetEvent` : **zéro appel sur 5 600 ticks**,
   ces deux objets jamais touchés.
5. Trois des dix appelants statiques de `KeSetEvent` sont dans la plage
   graphique : `0x821C4A60`, `0x821C4AE8`, `0x821C4F90`. **Aucun n'est atteint.**

## Le point de divergence

`sub_821BD970` contient le garde qui réveille les workers :

```text
lwz   r3,828(r31)      ; [this+0x33C]
cmplwi r3,0
beq   0x821BDC74       ; nul  -> saute le réveil
bl    0x821C4AE8       ; sinon -> KeSetEvent
stw   r23,828(r31)
```

Mais le garde n'est jamais évalué : **`sub_821BD970` n'est pas atteinte une
seule fois**. Le problème n'est donc pas que `[this+0x33C]` soit nul ; c'est que
la fonction qui le lit n'est jamais entrée.

Elle n'a aucun appelant direct, ni dans l'atlas ni dans le C++ généré, et
apparaît une fois comme mot en `0x8201430C` — une vtable **sans RTTI**
(`whose_vtable.py` : « 1 hit with NO NAME »). C'est donc une **méthode
virtuelle jamais dispatchée**.

## Ce qui, en revanche, tourne

| fonction | appels | ticks | rôle |
|---|---:|---|---|
| `0x821C57D0` | 5 463 | 0–5 599 | exactement le nombre de PRESENT : le swap par trame |
| `0x8218A4A0` | 3 522 | 252–5 599 | update du mode de démarrage |
| `0x821C4970` | 2 | 0–0 | entrée des workers, puis blocage définitif |

Le guest a donc une boucle de trame vivante qui présente, et une machine de
modes qui progresse jusqu'à l'écran-titre, mais la moitié « construire et
soumettre une trame » de son renderer n'est jamais appelée.

## Non établi

- Quel objet porte la vtable `0x8201430C`, et si son instance existe.
- Quel site indirect devrait dispatcher `sub_821BD970`, et ce qui l'en empêche.
- Si `0x821C57D0`, atteinte 5 463 fois, est le même « swap » que `VdSwap`
  observe, ou un maillon distinct de la même trame.

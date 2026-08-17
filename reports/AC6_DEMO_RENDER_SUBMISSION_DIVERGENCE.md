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

## Suite : la machine à états du renderer ne démarre jamais

`sub_821AD378`, atteinte 5 463 fois — une par trame — est une machine à états
dispatchée par table de saut :

```text
lwz    r11,296(r30)     ; r30 = 0x827AD1C8, donc l'état est en 0x827AD2F0
addi   r11,r11,-11
cmplwi cr6,r11,8
bgt    cr6,0x821AD73C   ; hors de [11,19] -> retour immédiat
```

Mesuré sur toute la sonde (`AC6_DEMO_WATCH_RENDER_STATE`) :

```text
AC6_RENDER_STATE tick=0 state=0
```

et rien d'autre. L'état vaut **0 du tick 0 au tick 5 599**. La fonction par
trame est donc appelée 5 463 fois et retourne immédiatement 5 463 fois.

Tout le reste de la chaîne en découle : l'état n'atteint jamais 11, donc le
constructeur `sub_821BE0B0` n'est pas appelé, donc l'objet de vtable
`0x820142F4` n'existe pas, donc son slot `+0x18` (`sub_821BD970`) n'est jamais
dispatché, donc `KeSetEvent` n'est jamais appelé, donc les threads 18 et 19
dorment, donc rien n'est soumis au ring après le tick 0, donc 5 463 PRESENT
rejouent une image noire.

Deux fonctions seulement manipulent cette base : `sub_821AD378` elle-même, qui
ne fait qu'avancer la machine une fois lancée, et **`sub_821AD7C0`, jamais
atteinte**, qui est donc le démarreur. Elle n'a aucun appelant statique, ni
dans le C++ généré ni dans l'atlas, et ne figure dans aucune table de l'image :
elle est appelée par un pointeur calculé à l'exécution.

### Non établi

- Quel site indirect appelle `sub_821AD7C0`, et sous quelle condition.
- Si l'état 11 est la seule entrée valide, ou si d'autres valeurs de [11,19]
  sont des points d'entrée légitimes.

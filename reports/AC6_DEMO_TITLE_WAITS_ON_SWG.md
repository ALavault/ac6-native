# L'écran-titre attend son gestionnaire de script

Date : 2026-08-18
Sondes : 3 600 ticks, appui START au tick 3000, guest à 349 chunks confirmés

## L'état interne du mode titre ne bouge pas

```text
tick 222   mode 0x2E7F0080 (démarrage)  state 0
tick 266                                state 1
tick 2426                               state 2
tick 2429  mode 0x2E3D0100 (titre)      state 0
tick 2452                               state 1
```

et **rien d'autre** jusqu'au tick 3600. L'appui au tick 3000 ne déplace pas
l'état. Le mode de démarrage, lui, parcourt bien 0 → 1 → 2 avant de céder.

L'appui est donc consommé — 31 dispatches dans `swg::ASContext`, dont une
soustraction d'Integer au tick 3001 — et il arrête la boucle d'attract, mais
**il n'atteint pas la machine à états du mode**.

## Ce que fait l'état 1

`sub_8218A7A8`, l'update du titre, a la même forme que celui du démarrage : un
`switch` sur `[this+12]`, avec un bras par état. Celui de l'état 1 tient en
six instructions :

```powerpc
addi r3,r31,28        ; sous-objet à this+28
lwz  r11,0(r3)        ; sa vtable
lwz  r11,32(r11)      ; slot +0x20
mtctr r11 ; bctrl
b    0x8218A8E4       ; puis sortir
```

Le titre appelle donc, à chaque tick, une seule méthode d'un sous-objet et
attend qu'elle fasse avancer l'état.

## Qui est ce sous-objet

```text
vptr observé      0x82006438
whose_vtable      slot +0x00 -> 0x820CF598 -> CSwgManager
slot +0x20        0x820CE368
```

**`CSwgManager`** — le gestionnaire du sous-système `swg`, le même espace de
noms que le `swg::ASContext` dans lequel l'appui START entre. Le frontend est
piloté par script de bout en bout : le titre attend son moteur de script.

## Où cela s'arrête

`0x820CE368` **est atteinte, 5 294 fois** — le sondage a bien lieu. Son
premier geste est un test :

```powerpc
lwz    r11,4(r30)
cmplwi cr6,r11,0
beq    cr6,0x820CE504   ; nul -> sortie anticipée
```

La frontière suivante est donc la valeur de `[CSwgManager+4]`. Si elle est
nulle, le gestionnaire retourne sans rien faire 5 294 fois, et l'état 1 du
titre ne peut pas avancer.

## Non établi

- La valeur de `[CSwgManager+4]` à l'exécution ; elle n'a pas encore été lue.
- Ce que `0x820CE504`, la sortie anticipée, fait exactement.
- Le lien, s'il existe, entre ce champ et l'absence de `CX360UnitManager` :
  deux manques sont décrits, rien ne prouve encore qu'ils n'en font qu'un.

# La branche vblank fait son travail — correction de `f6f44a5e`

Date : 2026-08-18

## Ce que j'ai écrit hier

> « source=0 : 1499 fois, **écartée dès la première instruction** […] le
> callback a fait son travail **une fois en 1 500 ticks**. »

Faux. J'avais lu `if (r3 != 1) goto loc_821B97A0` comme un rejet sans regarder
ce qu'il y a en `loc_821B97A0`. Il s'y trouve la branche source 0, avec son
propre travail :

```c
loc_821B97A0:
    if (r3 != 0) goto loc_821B97C0;
    r11 = [0x7FC86544];               // registre Xenos
    if ((r11 & 1) == 0) goto loc_821B97C0;
    sub_821C5090(r30);
```

Et `sub_821C5090` est **atteinte 11 999 fois**. Le bit 0 du registre est donc
servi, la branche s'exécute, et le port n'a pas ce défaut-là.

Ce qui a tourné une fois, c'est la branche source 1.

## Les noms

```text
0x821B9710  D3D::InterruptCallback(DWORD, CDevice*)     7/80 d3d9, 6/80 d3d9i
0x821C5090  D3D::VerticalBlankInterrupt(CDevice*)       2/80 dans les deux
0x821BE2E0  D3D::Hang::Out(const char*, ...)           11/80 d3d9, 1/80 d3d9i
```

Le plancher de bruit à ce réglage est 0/40 (`b417938f`). `VerticalBlankInterrupt`
à 2/80 est faible, mais concordant dans deux archives et sémantiquement exact :
c'est la fonction appelée sur source 0.

## Une lecture spectaculaire, refusée

La branche source 1 teste le pointeur de gestionnaire contre une sentinelle et,
en cas d'égalité, appelle `D3D::Hang::Out` avec cette chaîne, lue dans l'image :

```text
ERR[D3D]: Unanticipated CPU_INTERRUPT.  Sign of a corrupt command buffer?
```

`Hang::Out` **a bien tourné une fois** dans le run. La conclusion tentante était
que le détecteur de blocage de D3D avait tiré sur notre interruption.

Deux contrôles la refusent :

- `Hang::Out` a **trois appelants atteints** — `sub_821AD378` (11 863 fois),
  `sub_821B9710` (12 001) et `sub_821C64E8` (1). Un appel unique ne s'attribue
  à aucun des trois.
- L'arête indirecte du site montre ce que le pointeur valait réellement :
  `lr=0x821B9768 -> 0x821C5190`, une fois, au tick 1. C'est `D3D::SwapCallback`,
  pas la sentinelle. La branche du diagnostic n'a donc pas été prise ici.

L'unique interruption du processeur de commandes a donc appelé le gestionnaire
attendu, `SwapCallback`, normalement.

## Ce que cela laisse

L'interruption de tick 1 a fait ce qu'elle devait. Les deux threads d'attente
GPU restent parqués, et la raison n'est pas dans ce callback.

## Non établi

- D'où vient l'unique appel à `Hang::Out`.
- Pourquoi les deux threads en `0x821C4A28` ne sont pas réveillés alors que
  `SwapCallback` a été appelée.

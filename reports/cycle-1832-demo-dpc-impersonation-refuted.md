# Cycle 1832 — l'impersonation DPC est implémentée, et réfutée comme cause

## Qualification

XEX `de917873…405da8`, store neutre, 3000 ticks, headless, aucune entrée,
binaire `83f69af9e6a5d04f`. Offsets lus dans `X_KPCR`/`X_KPRCB` de Xenia Edge
(`src/xenia/kernel/xthread.h`). Aucun oracle.

## Ce qui a été implémenté

`begin_guest_dpc` / `end_guest_dpc` encadrent l'appel du callback graphique et
écrivent, comme `BeginDPCImpersonation` de Xenia :

```
current_irql          KPCR+0x18    <- 2
prcb_data.dpc_active  KPCR+0x150   <- 1
```

La validation croisée des offsets est `prcb_data.current_cpu` à
`KPCR+0x100+0x0C = 268` — exactement l'octet que le dispatcher du port écrivait
déjà. Le layout est donc partagé, ce n'est pas une transposition à l'aveugle.

## Les offsets sont qualifiés empiriquement

L'implémentation porte une précondition fail-closed : au moment où une
interruption est délivrée, un KPCR qui porte réellement ces champs ne peut être
ni déjà au niveau dispatch ni déjà dans un DPC. Des valeurs implausibles font
trapper plutôt que corrompre la mémoire invitée en silence.

**Elle a tenu sur les 2892 dispatchs des 3000 ticks, sans un seul trap.** Si
`0x18` ou `0x150` avaient été faux, le run se serait arrêté. Les deux offsets
sont donc vérifiés sur ce binaire, ce qui est acquis indépendamment du verdict.

## Verdict : `RÉFUTÉ`

Le chiffre décisif ne bouge pas d'une unité :

```
                        sans impersonation   avec impersonation   chemin qui progresse
thread 1 signal-and-wait   x5 ticks 0..177      x5 ticks 0..177      x5611 ticks 0..2999
bloqués / runnable            22 / 1               22 / 1               23 / 0
épuisements de tranche         2998                 2998                  215
transition de mode            aucune               aucune            Title au tick 2369
```

Identique trait pour trait. L'absence d'impersonation DPC est une vraie
divergence de contrat avec la référence, et elle reste corrigée à ce titre —
mais **elle n'est pas la cause de l'effondrement de la fence**.

Le cycle 1831 avait écrit que « bon chemin et bonne nature en font des
candidats, pas une démonstration ». La mesure tranche dans le sens que cette
réserve anticipait, et c'est la raison pour laquelle elle y était.

## Décision

Aucun run Vulkan n'a été lancé. Dépenser une heure de rendu pour capturer un
écran dont la mesure dit déjà qu'il est inchangé serait confirmer une absence
au prix du temps machine.

## Non établi

- La cause réelle. La paire (fence, cible) n'a toujours pas été lue à une base
  **vérifiée** ; la seule tentative a donné une cible de `1154359047343833104`,
  soit plus de sept cents ans à 50 MHz, ce qui est l'allure d'une lecture à
  côté.
- Que la cadence de l'interruption soit correcte. Le port tire `source=0` une
  fois par tick ; chez Xenia le vblank est cadencé par le temps réel et
  l'horloge invitée avance **entre** deux interruptions, alors qu'ici
  l'interruption et l'horloge sont la même variable `tick_`.

## État

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Une seule interruption du processeur de commandes — correction de `5a7c3511`

Date : 2026-08-18

## Ce que j'ai écrit

> « Le port **délivre** l'interruption : `0x821B9710` est atteinte 12 001 fois,
> une par tick. »

C'était vrai et trompeur. Le nombre lit comme « les interruptions passent
bien » ; il ne le montre pas.

## La première instruction du callback

```c
if (r3 != 1) goto loc_821B97A0;      // r3 = source
```

Le callback ne fait son travail que pour **source 1**. Tout le reste — effacer
le bit par processeur, appeler le gestionnaire enregistré en `[ctx+10900][16]`
— est derrière ce test.

Et le bridge émet deux sources (`lifecycle.hpp`) : la source 0 à chaque tick,
et la source 1 seulement quand `xenos_cp_interrupts_pending_` est non nul,
c'est-à-dire quand le processeur de commandes a consommé quelque chose.

## Le compte

Run de 1 500 ticks, `AC6_DEMO_WATCH_GRAPHICS_INTERRUPT` :

```text
source=0   1499 fois     vblank, écartée dès la première instruction
source=1      1 fois     tick 1
```

Le callback a donc fait son travail **une fois en 1 500 ticks**, et jamais
après le tick 1. Les 12 001 appels de `5a7c3511` étaient 12 000 vblanks
écartés par conception, plus un.

## Ce que cela tranche

`5a7c3511` laissait ouvert lequel des deux faits précède l'autre :

> « L'anneau n'avance pas donc les événements ne partent pas » et « les
> consommateurs n'avancent pas donc l'anneau n'est pas rempli » sont deux
> lectures du même cycle.

La seconde tombe. L'anneau **est rempli** — 163 930 écritures de buffer
indirect — et son pointeur d'écriture n'avance pas. Dans le modèle Xenos du
port, l'interruption du processeur de commandes est produite par cette avance :
pas de kick, pas d'interruption. L'anneau est donc **en amont** des événements,
pas en aval.

## Ce qui reste un cycle

Cela ne dit pas pourquoi le kick n'a pas lieu. La lecture cohérente reste un
inter-blocage : le D3D invité attend que le GPU rattrape avant de publier
davantage, et le GPU ne signale rien parce que rien n'a été publié. Sur
matériel réel, le travail du premier `KickOff` se termine, l'interruption part,
l'attente rend, et le cycle tourne. Ici, la seule interruption utile est celle
du tick 1.

## Non établi

- Pourquoi l'unique interruption du tick 1 n'a pas signalé les événements que
  les deux threads d'attente GPU réclament — ou pourquoi elle l'a fait sans
  suffire.
- Ce que fait la moitié de `0x821B9710` située après le verrou tournant.

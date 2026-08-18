# Cinq appels noyau que le runtime retail fait et que nous ne faisons pas

Date : 2026-08-18

## D'abord, un pixel

Toute la campagne dit « écran noir » en s'appuyant sur `submissions=2`, jamais
sur une sortie. Mesure directe du frontbuffer que `VdSwap` déclare
(`0x137A0000`, 1280×720) :

```text
AC6_FRONTBUFFER tick=1000 nonzero=0 of 921600 fnv=0x4BD87A07A4536325
AC6_FRONTBUFFER tick=2000 nonzero=0 of 921600 fnv=0x4BD87A07A4536325
AC6_FRONTBUFFER tick=3000 nonzero=0 of 921600 fnv=0x4BD87A07A4536325
```

Zéro mot non nul sur 921 600, hachage identique. C'est **vraiment noir**, pas
une image fixe non noire. Cette phrase n'avait jamais été vérifiée.

## Jusqu'où va l'oracle

Log Xenia, 479 796 lignes : `Cheap-skate exit!` à la frame 4183, textures
chargées, 142 `Draw`, 201 `Swap`, **zéro occurrence de `Mission`**. Son run est
donc le **frontend rendu** — précisément notre jalon. Le manque de 502
fonctions D3D n'est donc pas confondu par du code de mission.

## Le diff d'imports

Journal natif `AC6_DEMO_WATCH_IMPORTS` sur 4 000 ticks : 101 noms distincts,
16 821 782 appels. Comparé aux noms du log oracle, **cinq imports que le
retail appelle et nous jamais** :

```text
KeResetEvent               2246 fois, la première à la frame 3061
XGetAVPack                    1, frame 0
RtlImageXexHeaderField        1, frame 0
KeQueryBasePriorityThread     1, frame 0
KeResumeThread                1, frame 21
```

Et leurs appelants **tournent chez nous** : `sub_821A6F78` une fois,
`sub_821A9140` une fois, `sub_821C4970` deux fois. Ce ne sont pas des fonctions
mortes, ce sont des appels sautés.

## La garde, lue

```c
r3 = 10;
XexCheckExecutablePrivilege(10);
if (r3 == 0) goto loc_821A713C;      // saute tout le bloc
XGetAVPack();
if (r3 == 3) goto loc_821A713C;
```

Notre bridge rend **0 sans condition** pour `XexCheckExecutablePrivilege`, avec
le commentaire « le titre qualifié n'a pas d'exigence de privilège élevé ».

## Ce que le fichier dit

`XexCheckExecutablePrivilege(n)` teste le bit *n* des privilèges déclarés par
le XEX. En-têtes optionnels du `Default.xex` de la démo :

```text
key=0x00030000  val=0x00000600      <- XEX_HEADER_SYSTEM_FLAGS
```

`0x600` = bits 9 **et 10**. Le bit 10 est posé. Le test devrait donc réussir,
et le bloc s'exécuter — ce que fait l'oracle.

## Et une correction de plus, gratuite

```text
key=0x00010100  val=0x821A7160      <- point d'entrée du XEX
```

`0x821A7160` figurait dans ma liste des « onze fonctions D3D absentes du
port » (`92f76265`), puis parmi les refus du sondeur d'étendue (`52c5d07c`).
Ce n'est pas une fonction D3D manquante : c'est **le point d'entrée du
programme**, que l'oracle exécute forcément et que le port aborde autrement.
Deuxième entrée de cette liste à se dissoudre.

## Non établi

- Que corriger `XexCheckExecutablePrivilege` change quelque chose
  d'observable. Rien ici ne le montre ; c'est la prochaine expérience et elle
  est bornée.
- La sémantique exacte du champ : `0x00030000` est nommé *system flags* dans la
  structure XEX, et l'assimiler aux privilèges testés par cet import reste à
  confirmer autrement que par la coïncidence du bit 10.
- Pourquoi `KeResetEvent` ne démarre qu'à la frame 3061 chez l'oracle.

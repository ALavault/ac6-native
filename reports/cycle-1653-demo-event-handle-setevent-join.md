# Cycle 1653 — jointure du payload jusqu’à `NtSetEvent`

## Résultat

Le run A/B frais active simultanément les hooks consumer, payload, writer et
handoff. Il ferme la chaîne PAL suivante sans modifier l’état guest :

```text
0x822EEE30 load E0000048
  → 0x822EEE38 load E0000048
  → 0x822EEE3C std r30,16(r31) vers 0x82934758
  → 0x822EEE44 load E000004C
  → sub_821A6AB0
  → NtSetEvent(E000004C), caller LR 0x821A6AC4
  → event_wake / set_exit
```

Le reçu durable est
[`ac6-demo-event-handle-setevent-join-v1.json`](../analysis/demo/ac6-demo-event-handle-setevent-join-v1.json).

## A/B

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| payload writers | 374 | 374 |
| payload reads | 3 927 | 3 927 |
| handoff total | 4 485 | 4 485 |
| `set_enter` / `set_exit` total | 1 110 / 1 110 | 1 110 / 1 110 |
| `E000004C` `set_enter` / `set_exit` | 351 / 351 | 351 / 351 |
| writer→`set_enter` même tick/thread | 351 | 351 |
| SHA stderr | `c4262869…71fbb17` | `c4262869…71fbb17` |
| PRESENT | 163 | 163 |
| frontend / mission / terminal | non / non / non | non / non / non |

## Preuves directes

Le writer `0x822EEE3C` est vérifié par les bytes PAL `FB DF 00 10` et écrit
un `u64` à `0x82934758`. Pour chacune des 351 occurrences, un `set_enter`
postérieur au même tick et au même thread porte le handle `E000004C` et le LR
`0x821A6AC4`, qui est le retour de l’appel `NtSetEvent` dans
`sub_821A6AB0`. Les `set_exit` correspondants sont également présents.

Deux exemples reproduits dans les deux routes :

| tick | valeur writer | mot 7 après lecture `E000004C` | état `set_enter` / `set_exit` |
|---:|---:|---:|---:|
| 222 | `0xDC` | `0x000000DC` | `0x1 / 0x1` |
| 252 | `0x1D` | `0x0000001D` | `0x1 / 0x1` |

L’ordre est observé dans le flux stderr : la lecture postérieure, puis
`set_enter`, `event_wake`, `set_exit`. Une opération `E0000044` suit ensuite
dans le même thread ; son rôle reste seulement observé.

## Qualification et limites

- `demo-qualified` : séquence guest, bytes PAL, handle, LR de retour,
  ordonnancement même tick/thread et égalité A/B.
- `demo-observed` : états d’événement et wakes.
- `unknown` : consumer après `NtSetEvent`, transition scheduler→renderer,
  type sémantique de l’objet et pixels.
- `xenia-generic` : aucune preuve Xenia fusionnée.

Ce résultat ne produit pas de screencap et ne promeut toujours pas START :
aucun frontend guest-owned ni résultat de mission n’est atteint.

## Validation

- mapper writer : `374/374`, 19 sites, basefile PAL vérifié ;
- codegen ON CTest `17/17` et codegen OFF CTest `18/18` ;
- pytest build corpus : `47 passed`, 4 sous-tests ;
- capsule SHA : `dbd85daf6507d778da3c63af55e36c0f6a2e1ae725500665b88208265a687d4b`.

## Prochain checkpoint

Tracer le consumer suivant après `NtSetEvent` (wake/resume ou attente) et le
relier à un état PM4/Xenos déjà capturé. Toute absence de champ ABI ou de
writer exact doit interrompre le corridor avant effet.

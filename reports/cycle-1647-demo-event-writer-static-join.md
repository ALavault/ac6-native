# Cycle 1647 — jointure statique des writers d’événements PAL

## Résultat

Le probe ciblé du cycle 1646 a été corrigé pour conserver les handles avant
écrasement de `r3` par le statut de retour. Deux processus frais neutral/START
ont été rejoués jusqu'au tick 600.

| mesure | neutral | START |
|---|---:|---:|
| records ciblés | 13 785 | 13 785 |
| ticks couverts | 0–599 | 0–599 |
| SHA stderr | `613bf57f…610ed04` | `613bf57f…610ed04` |
| PRESENT | 463 | 463 |
| frontend / mission / terminal | non / non / non | non / non / non |

Les pulses sont maintenant joints au handle `0xE000002C`, threads 9, ticks
112/264, LR `0x821A688C`. Les 11 `NtResumeThread` portent les handles
`0xE1000000…0xE100001C` et `0xE1000044/48/4C`, thread 1, ticks 0 et 106,
LR `0x821A6B0C`. Les 55 clear ont 53 handles distincts et LR `0x821A6138`.

## Jointure au PAL statique

L’atlas statique PAL et le callgraph qualifient les callsites suivants :

| import | callsite / LR | fonction contenante | ordinal |
|---|---|---|---:|
| `NtSignalAndWaitForSingleObjectEx` | `0x821A69C8 / 0x821A69CC` | `0x821A6988..0x821A69F7` | 251 |
| `NtSetEvent` | `0x821A6AC0 / 0x821A6AC4` | `0x821A6AB0..0x821A6AEF` | 246 |
| `NtPulseEvent` | `0x821A6888 / 0x821A688C` | `0x821A6878..0x821A68B3` | 226 |
| `NtClearEvent` | `0x821A6134 / 0x821A6138` | `0x821A6128..0x821A615F` | 206 |
| `NtResumeThread` | `0x821A6B08 / 0x821A6B0C` | `0x821A6AF8..0x821A6B33` | 245 |
| `NtWaitForSingleObjectEx` | `0x821A8C84 / 0x821A8C88` | `0x821A8C50..0x821A8CB3` | 253 |
| `NtWaitForMultipleObjectsEx` | `0x821A6A78 / 0x821A6A7C` | `0x821A69F8..0x821A6AA7` | 254 |

Cette jointure s'appuie sur les bytes PAL, l'atlas et le callgraph ; elle ne
transforme pas les noms générés en preuve sémantique.

## Qualification et limites

- `demo-qualified` : handles conservés avant retour, ticks/threads/LR,
  callsites et bornes statiques exacts, A/B byte-identique ;
- `demo-observed` : valeurs `E000…`, comptes set/clear/pulse/resume et
  transitions du bridge ;
- `xenia-generic` : handles `F800…` et pile POSIX du run Xenia ;
- `unknown` : instruction guest qui écrit chaque handle et correspondance
  sémantique Xenia↔PAL.

Le renderer reste inchangé : readback noir, aucun frontend guest-owned, aucune
screencap ou progression START promue.

Le reçu durable est
[`ac6-demo-event-writer-static-join-v1.json`](../analysis/demo/ac6-demo-event-writer-static-join-v1.json).

## Prochain checkpoint

Utiliser ces sept callsites comme fenêtres rr/GDB PAL pour relever les stores
exactes des handles pulse/resume et leur PC/LR/thread/tick, puis joindre le
consumer de queue. Aucun changement de comportement ne doit être introduit.

# Cycle 1645 — PAL bridge event hand-off probe

## Résultat

Le rapport Xenia de cycle 1644 reste une autorité `xenia-generic` : il observe
le gel de deux threads sur `0x821A69C8/0x821A69CC` avec les handles
`0xF8000088 → 0xF800008C`, et la pile hôte
`pthread_cond_wait → PosixConditionBase::Wait → XObject::SignalAndWait`.
Ces handles ne sont pas copiés dans le runtime PAL.

Un hook read-only, désactivé par défaut, a été ajouté dans
[`event_handoff_trace.hpp`](../recompilation/ace-combat-6-demo/src/guest_bridge/event_handoff_trace.hpp).
Deux processus codegen-ON frais ont été exécutés sur la démo PAL seule,
neutral et START au tick 252, jusqu'au tick 600, avec
`AC6_DEMO_WATCH_EVENT_HANDOFF=1`.

| propriété | neutral | START |
|---|---:|---:|
| traces d'événements conservées | 4 096 | 4 096 |
| intervalle de ticks des traces | 0–285 | 0–285 |
| SHA stderr | `f5d2a9b7…a07f88` | `f5d2a9b7…a07f88` |
| ticks terminés | 600 | 600 |
| PRESENT | 463 | 463 |
| frontend / mission / terminal | non / non / non | non / non / non |

La limite de 4 096 records est volontaire et borne la sortie ; elle ne permet
pas de conclure sur les opérations après le tick 285.

## Preuves démo observées

Le bridge PAL alloue ses objets d'événement dans l'espace synthétique
`0xE0000000`. Le couple principal observé est
`0xE0000048 → 0xE000004C`, avec LR appelant `0x821A69CC`, thread 1,
71 entrées, 36 blocages et 35 reprises dans la fenêtre conservée. Un second
couple `0xE0000054 → 0xE0000058` compte 74 entrées, 38 blocages et 37 reprises,
threads 1 et 14, avec le même LR.

Les publications et réveils enregistrent le tick, le thread, le LR, l'état
signaled/granted et le résultat sans toucher au scheduler. Neutral et START
ont un stderr byte-identique et des sous-arbres de rapport
`outcome/milestones/graphics/scheduler` byte-identiques. START est donc livré
au guest mais ne cause aucune transition visuelle ou jalon persistant dans
cette fenêtre.

## Qualification et limites

- `demo-qualified` : identité PAL, LR `0x821A69CC`, couples de handles du
  bridge, comptes bornés, A/B déterministe et absence de mutation du réveil ;
- `demo-observed` : publications, blocs/reprises et handles `0xE000…` du
  runtime instrumenté ;
- `xenia-generic` : handles `0xF800…` et pile POSIX du rapport cycle 1644 ;
- `unknown` : jointure `F800 ↔ E000`, opérations après la limite de trace,
  propriétaire exact set/clear/pulse et toute relation avec un frontend.

Aucune lecture de pixels supplémentaire n'est promue : le readback reste noir,
et aucune screencap guest-owned n'est justifiée.

Le reçu durable est
[`ac6-demo-event-handoff-probe-v1.json`](../analysis/demo/ac6-demo-event-handoff-probe-v1.json).

## Prochain checkpoint

Répéter une seule exécution neutre avec un filtre ciblé sur les deux couples
`E0000048/4C` et `E0000054/58`, sans plafond qui masque les ticks tardifs, puis
joindre les writers/set-clear-pulse aux mêmes LR et threads. Ne pas assimiler
les handles Xenia aux handles PAL et ne pas promouvoir START, le readback ou la
screencap avant une transition guest persistante et une surface non noire.
